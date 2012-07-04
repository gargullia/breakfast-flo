/*
 * Copyright (C) 2011-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/module.h>
#include <linux/hw_random.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mach/socinfo.h>
#include <mach/msm_bus.h>

/* Device specific register offsets */
#define PRNG_DATA_OUT_OFFSET    0x0000
#define PRNG_STATUS_OFFSET	0x0004
#define PRNG_LFSR_CFG_OFFSET	0x0100
#define PRNG_CONFIG_OFFSET	0x0104

/* Device specific register masks and config values */
#define PRNG_LFSR_CFG_MASK	0xFFFF0000
#define PRNG_LFSR_CFG_CLOCKS	0x0000DDDD
#define PRNG_CONFIG_MASK	0xFFFFFFFD
#define PRNG_HW_ENABLE		0x00000002

#define MAX_HW_FIFO_DEPTH (16)			   /* FIFO is 16 words deep */
#define MAX_HW_FIFO_SIZE  (MAX_HW_FIFO_DEPTH << 2) /* FIFO is 64 bits wide  */

struct msm_rng_device {
	struct platform_device *pdev;
	struct clk *prng_clk;
	uint32_t qrng_perf_client;
	void __iomem *base;
};

static inline void msm_bus_rng_update(struct msm_rng_device *msm_rng_dev)
{
	int ret;

	/* Return early if bus client is not registered */
	if (!msm_rng_dev->qrng_perf_client)
		return;

	ret = msm_bus_scale_client_update_request(
		msm_rng_dev->qrng_perf_client, 1);
	if (IS_ERR_VALUE(ret))
		dev_err(&(msm_rng_dev->pdev)->dev,
			"Unable to update bus request\n");
}

static int msm_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct msm_rng_device *msm_rng_dev = (struct msm_rng_device *)rng->priv;
	void __iomem *base = msm_rng_dev->base;
	unsigned long val, *retdata = data;
	size_t maxsize, currsize = 0;
	int ret;

	/* Calculate max size bytes to transfer back to caller */
	maxsize = min_t(size_t, MAX_HW_FIFO_SIZE, max);
	if (unlikely(maxsize < 4))
		return 0;

	/* Enable PRNG clock */
	ret = clk_prepare_enable(msm_rng_dev->prng_clk);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Unable to enable clk in callback\n");
		return 0;
	}

	/* Update bus request */
	msm_bus_rng_update(msm_rng_dev);

	/* Read random data from h/w */
	do {
		/* Check status bit if data is available */
		if (!(readl_relaxed(base + PRNG_STATUS_OFFSET) & 0x00000001))
			break; /* No data to read so just bail */

		/* Read FIFO */
		val = readl_relaxed(base + PRNG_DATA_OUT_OFFSET);
		if (!val)
			break; /* No data to read so just bail */

		/* Write data back to callers pointer */
		*(retdata++) = val;
		currsize += 4;

		/* Make sure we stay on 32bit boundary */
		if ((maxsize - currsize) < 4)
			break;
	} while (currsize < maxsize);

	/* Update bus request */
	msm_bus_rng_update(msm_rng_dev);

	/* Vote to turn off clock */
	clk_disable_unprepare(msm_rng_dev->prng_clk);

	return currsize;
}

static struct hwrng msm_rng = {
	.name = "msm_rng",
	.read = msm_rng_read,
	.quality = 700,
};

static int __devinit msm_rng_enable_hw(struct msm_rng_device *msm_rng_dev)
{
	unsigned long reg_val, val;
	int ret;

	/* Enable the PRNG CLK */
	ret = clk_prepare_enable(msm_rng_dev->prng_clk);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&(msm_rng_dev->pdev)->dev,
			"Unable to enable clk in probe\n");
		return -EPERM;
	}

	/* Update bus request */
	msm_bus_rng_update(msm_rng_dev);

	/* Enable PRNG H/W only if it is NOT ON */
	val  = readl_relaxed(msm_rng_dev->base + PRNG_CONFIG_OFFSET);
	val &= PRNG_HW_ENABLE;

	/* PRNG H/W is not ON */
	if (val != PRNG_HW_ENABLE) {
		val  = readl_relaxed(msm_rng_dev->base + PRNG_LFSR_CFG_OFFSET);
		val &= PRNG_LFSR_CFG_MASK;
		val |= PRNG_LFSR_CFG_CLOCKS;
		writel_relaxed(val, msm_rng_dev->base + PRNG_LFSR_CFG_OFFSET);

		/* The PRNG CONFIG register should be first written */
		mb();

		reg_val = readl_relaxed(msm_rng_dev->base + PRNG_CONFIG_OFFSET);
		reg_val &= PRNG_CONFIG_MASK;
		reg_val |= PRNG_HW_ENABLE;
		writel_relaxed(reg_val, msm_rng_dev->base + PRNG_CONFIG_OFFSET);

		/*
		 * The PRNG clk should be disabled only after we enable the
		 * PRNG h/w by writing to the PRNG CONFIG register.
		 */
		mb();
	}

	/* Update bus request */
	msm_bus_rng_update(msm_rng_dev);

	/* Vote to turn off clock */
	clk_disable_unprepare(msm_rng_dev->prng_clk);

	return 0;
}

static int __devinit msm_rng_probe(struct platform_device *pdev)
{
	struct msm_bus_scale_pdata *qrng_platform_support;
	struct device_node *rng_node = pdev->dev.of_node;
	struct msm_rng_device *msm_rng_dev;
	struct resource *res;
	void __iomem *base;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Invalid address\n");
		ret = -EFAULT;
		goto err_exit;
	}

	msm_rng_dev = kzalloc(sizeof(*msm_rng_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(msm_rng_dev)) {
		dev_err(&pdev->dev, "Unable to allocate memory for device\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	base = ioremap(res->start, resource_size(res));
	if (IS_ERR_OR_NULL(base)) {
		dev_err(&pdev->dev, "IORemap failed\n");
		ret = -ENOMEM;
		goto err_iomap;
	}

	msm_rng_dev->base = base;

	/* Create a handle for clk control */
	if (rng_node &&
	    of_property_read_bool(rng_node, "qcom,msm-rng-iface-clk"))
		msm_rng_dev->prng_clk = clk_get(&pdev->dev, "iface_clk");
	else
		msm_rng_dev->prng_clk = clk_get(&pdev->dev, "core_clk");

	if (IS_ERR_OR_NULL(msm_rng_dev->prng_clk)) {
		dev_err(&pdev->dev, "Unable to get clk source\n");
		ret = -EPERM;
		goto err_clk_get;
	}

	/* Save platform device and register driver data */
	msm_rng_dev->pdev = pdev;
	platform_set_drvdata(pdev, msm_rng_dev);

	if (rng_node) {
		/* Register bus client */
		qrng_platform_support = msm_bus_cl_get_pdata(pdev);
		msm_rng_dev->qrng_perf_client =
			msm_bus_scale_register_client(qrng_platform_support);
		if (!msm_rng_dev->qrng_perf_client)
			dev_err(&pdev->dev, "Unable to register bus client\n");
		else
			dev_info(&pdev->dev, "Bus client initialized\n");
	}

	/* Enable HW RNH */
	ret = msm_rng_enable_hw(msm_rng_dev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "Unable to enable HWRNG\n");
		goto rollback_clk;
	}

	/* Register with HWRNG framework */
	msm_rng.priv = (unsigned long)msm_rng_dev;
	ret = hwrng_register(&msm_rng);
	if (IS_ERR_VALUE(ret)) {
		dev_err(&pdev->dev, "Unable to register HWRNG\n");
		goto rollback_clk;
	}

	return 0;

rollback_clk:
	clk_put(msm_rng_dev->prng_clk);
err_clk_get:
	iounmap(msm_rng_dev->base);
err_iomap:
	kfree(msm_rng_dev);
err_exit:
	return ret;
}

static int __devexit msm_rng_remove(struct platform_device *pdev)
{
	struct msm_rng_device *msm_rng_dev = platform_get_drvdata(pdev);

	hwrng_unregister(&msm_rng);
	if (msm_rng_dev->qrng_perf_client)
		msm_bus_scale_unregister_client(msm_rng_dev->qrng_perf_client);

	clk_put(msm_rng_dev->prng_clk);
	iounmap(msm_rng_dev->base);

	platform_set_drvdata(pdev, NULL);
	kfree(msm_rng_dev);

	return 0;
}

static const struct of_device_id qrng_match[] = {
	{ .compatible = "qcom,msm-rng" },
	{ },
};

static struct platform_driver rng_driver = {
	.probe = msm_rng_probe,
	.remove = __devexit_p(msm_rng_remove),
	.driver = {
		.name = "msm_rng",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(qrng_match),
	},
};

static int __init msm_rng_init(void)
{
	return platform_driver_register(&rng_driver);
}

static void __exit msm_rng_exit(void)
{
	platform_driver_unregister(&rng_driver);
}

module_init(msm_rng_init);
module_exit(msm_rng_exit);

MODULE_AUTHOR("The Linux Foundation");
MODULE_DESCRIPTION("Qualcomm MSM Random Number Driver");
MODULE_LICENSE("GPL v2");
