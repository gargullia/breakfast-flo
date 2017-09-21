/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013, LGE Inc. All rights reserved.
 * Copyright (c) 2014, Savoca <adeddo27@gmail.com>
 * Copyright (c) 2017-2018, Alex Saiko <solcmdr@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "msm_fb.h"
#include "mdp4.h"

#define DEF_ENABLE	(1)
#define DEF_INVERT	(0)
#define DEF_MIN		(35)
#define DEF_PCC		(256)

struct kcal_lut_data {
	uint32_t enable:1;
	uint32_t invert:1;
	uint32_t min;
	uint32_t red;
	uint32_t green;
	uint32_t blue;
};

static uint32_t igc_inverted[256] = {
	0x0FF00FF0, 0x0FE00FE0, 0x0FD00FD0, 0x0FC00FC0, 0x0FB00FB0, 0x0FA00FA0,
	0x0F900F90, 0x0F800F80, 0x0F700F70, 0x0F600F60, 0x0F500F50, 0x0F400F40,
	0x0F300F30, 0x0F200F20, 0x0F100F10, 0x0F000F00, 0x0EF00EF0, 0x0EE00EE0,
	0x0ED00ED0, 0x0EC00EC0, 0x0EB00EB0, 0x0EA00EA0, 0x0E900E90, 0x0E800E80,
	0x0E700E70, 0x0E600E60, 0x0E500E50, 0x0E400E40, 0x0E300E30, 0x0E200E20,
	0x0E100E10, 0x0E000E00, 0x0DF00DF0, 0x0DE00DE0, 0x0DD00DD0, 0x0DC00DC0,
	0x0DB00DB0, 0x0DA00DA0, 0x0D900D90, 0x0D800D80, 0x0D700D70, 0x0D600D60,
	0x0D500D50, 0x0D400D40, 0x0D300D30, 0x0D200D20, 0x0D100D10, 0x0D000D00,
	0x0CF00CF0, 0x0CE00CE0, 0x0CD00CD0, 0x0CC00CC0, 0x0CB00CB0, 0x0CA00CA0,
	0x0C900C90, 0x0C800C80, 0x0C700C70, 0x0C600C60, 0x0C500C50, 0x0C400C40,
	0x0C300C30, 0x0C200C20, 0x0C100C10, 0x0C000C00, 0x0BF00BF0, 0x0BE00BE0,
	0x0BD00BD0, 0x0BC00BC0, 0x0BB00BB0, 0x0BA00BA0, 0x0B900B90, 0x0B800B80,
	0x0B700B70, 0x0B600B60, 0x0B500B50, 0x0B400B40, 0x0B300B30, 0x0B200B20,
	0x0B100B10, 0x0B000B00, 0x0AF00AF0, 0x0AE00AE0, 0x0AD00AD0, 0x0AC00AC0,
	0x0AB00AB0, 0x0AA00AA0, 0x0A900A90, 0x0A800A80, 0x0A700A70, 0x0A600A60,
	0x0A500A50, 0x0A400A40, 0x0A300A30, 0x0A200A20, 0x0A100A10, 0x0A000A00,
	0x09F009F0, 0x09E009E0, 0x09D009D0, 0x09C009C0, 0x09B009B0, 0x09A009A0,
	0x09900990, 0x09800980, 0x09700970, 0x09600960, 0x09500950, 0x09400940,
	0x09300930, 0x09200920, 0x09100910, 0x09000900, 0x08F008F0, 0x08E008E0,
	0x08D008D0, 0x08C008C0, 0x08B008B0, 0x08A008A0, 0x08900890, 0x08800880,
	0x08700870, 0x08600860, 0x08500850, 0x08400840, 0x08300830, 0x08200820,
	0x08100810, 0x08000800, 0x07F007F0, 0x07E007E0, 0x07D007D0, 0x07C007C0,
	0x07B007B0, 0x07A007A0, 0x07900790, 0x07800780, 0x07700770, 0x07600760,
	0x07500750, 0x07400740, 0x07300730, 0x07200720, 0x07100710, 0x07000700,
	0x06F006F0, 0x06E006E0, 0x06D006D0, 0x06C006C0, 0x06B006B0, 0x06A006A0,
	0x06900690, 0x06800680, 0x06700670, 0x06600660, 0x06500650, 0x06400640,
	0x06300630, 0x06200620, 0x06100610, 0x06000600, 0x05F005F0, 0x05E005E0,
	0x05D005D0, 0x05C005C0, 0x05B005B0, 0x05A005A0, 0x05900590, 0x05800580,
	0x05700570, 0x05600560, 0x05500550, 0x05400540, 0x05300530, 0x05200520,
	0x05100510, 0x05000500, 0x04F004F0, 0x04E004E0, 0x04D004D0, 0x04C004C0,
	0x04B004B0, 0x04A004A0, 0x04900490, 0x04800480, 0x04700470, 0x04600460,
	0x04500450, 0x04400440, 0x04300430, 0x04200420, 0x04100410, 0x04000400,
	0x03F003F0, 0x03E003E0, 0x03D003D0, 0x03C003C0, 0x03B003B0, 0x03A003A0,
	0x03900390, 0x03800380, 0x03700370, 0x03600360, 0x03500350, 0x03400340,
	0x03300330, 0x03200320, 0x03100310, 0x03000300, 0x02F002F0, 0x02E002E0,
	0x02D002D0, 0x02C002C0, 0x02B002B0, 0x02A002A0, 0x02900290, 0x02800280,
	0x02700270, 0x02600260, 0x02500250, 0x02400240, 0x02300230, 0x02200220,
	0x02100210, 0x02000200, 0x01F001F0, 0x01E001E0, 0x01D001D0, 0x01C001C0,
	0x01B001B0, 0x01A001A0, 0x01900190, 0x01800180, 0x01700170, 0x01600160,
	0x01500150, 0x01400140, 0x01300130, 0x01200120, 0x01100110, 0x01000100,
	0x00F000F0, 0x00E000E0, 0x00D000D0, 0x00C000C0, 0x00B000B0, 0x00A000A0,
	0x00900090, 0x00800080, 0x00700070, 0x00600060, 0x00500050, 0x00400040,
	0x00300030, 0x00200020, 0x00100010, 0x00000000,
};

static uint32_t igc_rgb[256] = {
	0x0FF0, 0x0FE0, 0x0FD0, 0x0FC0, 0x0FB0, 0x0FA0, 0x0F90, 0x0F80,
	0x0F70, 0x0F60, 0x0F50, 0x0F40, 0x0F30, 0x0F20, 0x0F10, 0x0F00,
	0x0EF0, 0x0EE0, 0x0ED0, 0x0EC0, 0x0EB0, 0x0EA0, 0x0E90, 0x0E80,
	0x0E70, 0x0E60, 0x0E50, 0x0E40, 0x0E30, 0x0E20, 0x0E10, 0x0E00,
	0x0DF0, 0x0DE0, 0x0DD0, 0x0DC0, 0x0DB0, 0x0DA0, 0x0D90, 0x0D80,
	0x0D70, 0x0D60, 0x0D50, 0x0D40, 0x0D30, 0x0D20, 0x0D10, 0x0D00,
	0x0CF0, 0x0CE0, 0x0CD0, 0x0CC0, 0x0CB0, 0x0CA0, 0x0C90, 0x0C80,
	0x0C70, 0x0C60, 0x0C50, 0x0C40, 0x0C30, 0x0C20, 0x0C10, 0x0C00,
	0x0BF0, 0x0BE0, 0x0BD0, 0x0BC0, 0x0BB0, 0x0BA0, 0x0B90, 0x0B80,
	0x0B70, 0x0B60, 0x0B50, 0x0B40, 0x0B30, 0x0B20, 0x0B10, 0x0B00,
	0x0AF0, 0x0AE0, 0x0AD0, 0x0AC0, 0x0AB0, 0x0AA0, 0x0A90, 0x0A80,
	0x0A70, 0x0A60, 0x0A50, 0x0A40, 0x0A30, 0x0A20, 0x0A10, 0x0A00,
	0x09F0, 0x09E0, 0x09D0, 0x09C0, 0x09B0, 0x09A0, 0x0990, 0x0980,
	0x0970, 0x0960, 0x0950, 0x0940, 0x0930, 0x0920, 0x0910, 0x0900,
	0x08F0, 0x08E0, 0x08D0, 0x08C0, 0x08B0, 0x08A0, 0x0890, 0x0880,
	0x0870, 0x0860, 0x0850, 0x0840, 0x0830, 0x0820, 0x0810, 0x0800,
	0x07F0, 0x07E0, 0x07D0, 0x07C0, 0x07B0, 0x07A0, 0x0790, 0x0780,
	0x0770, 0x0760, 0x0750, 0x0740, 0x0730, 0x0720, 0x0710, 0x0700,
	0x06F0, 0x06E0, 0x06D0, 0x06C0, 0x06B0, 0x06A0, 0x0690, 0x0680,
	0x0670, 0x0660, 0x0650, 0x0640, 0x0630, 0x0620, 0x0610, 0x0600,
	0x05F0, 0x05E0, 0x05D0, 0x05C0, 0x05B0, 0x05A0, 0x0590, 0x0580,
	0x0570, 0x0560, 0x0550, 0x0540, 0x0530, 0x0520, 0x0510, 0x0500,
	0x04F0, 0x04E0, 0x04D0, 0x04C0, 0x04B0, 0x04A0, 0x0490, 0x0480,
	0x0470, 0x0460, 0x0450, 0x0440, 0x0430, 0x0420, 0x0410, 0x0400,
	0x03F0, 0x03E0, 0x03D0, 0x03C0, 0x03B0, 0x03A0, 0x0390, 0x0380,
	0x0370, 0x0360, 0x0350, 0x0340, 0x0330, 0x0320, 0x0310, 0x0300,
	0x02F0, 0x02E0, 0x02D0, 0x02C0, 0x02B0, 0x02A0, 0x0290, 0x0280,
	0x0270, 0x0260, 0x0250, 0x0240, 0x0230, 0x0220, 0x0210, 0x0200,
	0x01F0, 0x01E0, 0x01D0, 0x01C0, 0x01B0, 0x01A0, 0x0190, 0x0180,
	0x0170, 0x0160, 0x0150, 0x0140, 0x0130, 0x0120, 0x0110, 0x0100,
	0x00F0, 0x00E0, 0x00D0, 0x00C0, 0x00B0, 0x00A0, 0x0090, 0x0080,
	0x0070, 0x0060, 0x0050, 0x0040, 0x0030, 0x0020, 0x0010, 0x0000,
};

static inline void mdp4_kcal_update_lut(struct kcal_lut_data *lut_data)
{
	int ret;
	uint16_t r = lut_data->enable ?
		max_t(uint16_t, (lut_data->red - 1), lut_data->min) : 255;
	uint16_t g = lut_data->enable ?
		max_t(uint16_t, (lut_data->green - 1), lut_data->min) : 255;
	uint16_t b = lut_data->enable ?
		max_t(uint16_t, (lut_data->blue - 1), lut_data->min) : 255;
	struct fb_cmap cmap = {
		.start = 0,
		.len = 256,
		.transp = NULL,
		.red = (uint16_t *)&r,
		.green = (uint16_t *)&g,
		.blue = (uint16_t *)&b,
	};

	ret = mdp_preset_lut_update_lcdc(&cmap);
	if (IS_ERR_VALUE(ret))
		pr_err("Unable to update display LUT\n");
}

static inline void mdp4_kcal_update_igc(struct kcal_lut_data *lut_data)
{
	int ret;
	struct mdp_igc_lut_data igc_config = {
		.block = MDP_BLOCK_DMA_P,
		.len = 256,
		.ops = MDP_PP_OPS_WRITE | MDP_PP_OPS_KSPACE,
		.c0_c1_data = igc_inverted,
		.c2_data = igc_rgb,
	};

	igc_config.ops |= (lut_data->enable && lut_data->invert) ?
		MDP_PP_OPS_ENABLE : MDP_PP_OPS_DISABLE;

	ret = mdp4_igc_lut_config(&igc_config);
	if (IS_ERR_VALUE(ret))
		pr_err("Unable to update display IGC\n");
}

#define create_one_rw_node(name)					\
static DEVICE_ATTR(name, 0644, show_##name, store_##name);

#define define_one_kcal_node(name, object, min, max, update_igc)	\
static ssize_t show_##name(struct device *dev,				\
			   struct device_attribute *attr,		\
			   char *buf)					\
{									\
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);		\
									\
	return scnprintf(buf, 5, "%u\n", lut_data->object);		\
}									\
									\
static ssize_t store_##name(struct device *dev,				\
			    struct device_attribute *attr,		\
			    const char *buf, size_t count)		\
{									\
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);		\
	uint32_t val;							\
	int ret;							\
									\
	ret = kstrtouint(buf, 10, &val);				\
	if (ret || val < min || val > max || lut_data->object == val)	\
		return -EINVAL;						\
									\
	lut_data->object = val;						\
									\
	mdp4_kcal_update_lut(lut_data);					\
	if (update_igc)							\
		mdp4_kcal_update_igc(lut_data);				\
									\
	return count;							\
}									\
									\
create_one_rw_node(name);

static ssize_t show_kcal(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return scnprintf(buf, 13, "%u %u %u\n",
			 lut_data->red, lut_data->green, lut_data->blue);
}

static ssize_t store_kcal(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);
	uint32_t kcal_r, kcal_g, kcal_b;
	int ret;

	ret = sscanf(buf, "%u %u %u", &kcal_r, &kcal_g, &kcal_b);
	if (ret != 3 ||
	   (int32_t)kcal_r < 1 || kcal_r > 256 ||
	   (int32_t)kcal_g < 1 || kcal_g > 256 ||
	   (int32_t)kcal_b < 1 || kcal_b > 256)
		return -EINVAL;

	lut_data->red = kcal_r;
	lut_data->green = kcal_g;
	lut_data->blue = kcal_b;

	mdp4_kcal_update_lut(lut_data);

	return count;
}

create_one_rw_node(kcal);
define_one_kcal_node(kcal_enable, enable, 0, 1, true);
define_one_kcal_node(kcal_invert, invert, 0, 1, true);
define_one_kcal_node(kcal_min, min, 1, 256, false);

static int __devinit kcal_ctrl_probe(struct platform_device *pdev)
{
	struct kcal_lut_data *lut_data;
	int ret;

	lut_data = devm_kzalloc(&pdev->dev, sizeof(*lut_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(lut_data)) {
		pr_err("Unable to allocate memory for LUT data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, lut_data);

	lut_data->enable = DEF_ENABLE;
	lut_data->invert = DEF_INVERT;
	lut_data->min = DEF_MIN;
	lut_data->red = DEF_PCC;
	lut_data->green = DEF_PCC;
	lut_data->blue = DEF_PCC;

	ret  = device_create_file(&pdev->dev, &dev_attr_kcal);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_enable);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_invert);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_min);
	if (IS_ERR_VALUE(ret)) {
		lut_data->enable = lut_data->invert = 0;
		pr_err("Unable to create sysfs nodes\n");
	}

	mdp4_kcal_update_lut(lut_data);
	mdp4_kcal_update_igc(lut_data);

	return ret;
}

static int __devexit kcal_ctrl_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_kcal_min);
	device_remove_file(&pdev->dev, &dev_attr_kcal_invert);
	device_remove_file(&pdev->dev, &dev_attr_kcal_enable);
	device_remove_file(&pdev->dev, &dev_attr_kcal);

	return 0;
}

static struct platform_driver kcal_ctrl_driver = {
	.probe = kcal_ctrl_probe,
	.remove = __devexit_p(kcal_ctrl_remove),
	.driver = {
		.name = "kcal_ctrl",
		.owner = THIS_MODULE,
	},
};

static struct platform_device kcal_ctrl_device = {
	.name = "kcal_ctrl",
};

static int __init kcal_ctrl_init(void)
{
	int ret;

	ret = platform_driver_register(&kcal_ctrl_driver);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Unable to register platform driver\n");
		return ret;
	}

	ret = platform_device_register(&kcal_ctrl_device);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Unable to register platform device\n");
		platform_driver_unregister(&kcal_ctrl_driver);
		return ret;
	}

	return ret;
}

static void __exit kcal_ctrl_exit(void)
{
	platform_device_unregister(&kcal_ctrl_device);
	platform_driver_unregister(&kcal_ctrl_driver);
}

module_init(kcal_ctrl_init);
module_exit(kcal_ctrl_exit);
