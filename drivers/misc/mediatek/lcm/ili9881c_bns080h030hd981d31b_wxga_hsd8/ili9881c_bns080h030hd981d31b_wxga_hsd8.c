/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID (0x98)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH		(800)
#define FRAME_HEIGHT	(1280)
#define LCM_DENSITY		(320)

#define LCM_PHYSICAL_WIDTH		(64800)
#define LCM_PHYSICAL_HEIGHT		(129600)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x4F, 1, {0x01} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_cmd[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x03} },
};

/* Recovered from Acer stock kernel
 * Base offset: 0x01867780
 * Record stride: 0x48
 * Entries: 204
 */
static struct LCM_setting_table init_setting_vdo[] = {
    {0x0FF, 3, {0x98, 0x81, 0x03}},
    {0x001, 1, {0x00}},
    {0x002, 1, {0x00}},
    {0x003, 1, {0x53}},
    {0x004, 1, {0x13}},
    {0x005, 1, {0x00}},
    {0x006, 1, {0x04}},
    {0x007, 1, {0x00}},
    {0x008, 1, {0x00}},
    {0x009, 1, {0x22}},
    {0x00A, 1, {0x22}},
    {0x00B, 1, {0x00}},
    {0x00C, 1, {0x01}},
    {0x00D, 1, {0x00}},
    {0x00E, 1, {0x00}},
    {0x00F, 1, {0x25}},
    {0x010, 1, {0x25}},
    {0x011, 1, {0x00}},
    {0x012, 1, {0x00}},
    {0x013, 1, {0x00}},
    {0x014, 1, {0x00}},
    {0x015, 1, {0x00}},
    {0x016, 1, {0x00}},
    {0x017, 1, {0x00}},
    {0x018, 1, {0x00}},
    {0x019, 1, {0x00}},
    {0x01A, 1, {0x00}},
    {0x01B, 1, {0x00}},
    {0x01C, 1, {0x00}},
    {0x01D, 1, {0x00}},
    {0x01E, 1, {0x44}},
    {0x01F, 1, {0x80}},
    {0x020, 1, {0x02}},
    {0x021, 1, {0x03}},
    {0x022, 1, {0x00}},
    {0x023, 1, {0x00}},
    {0x024, 1, {0x00}},
    {0x025, 1, {0x00}},
    {0x026, 1, {0x00}},
    {0x027, 1, {0x00}},
    {0x028, 1, {0x33}},
    {0x029, 1, {0x03}},
    {0x02A, 1, {0x00}},
    {0x02B, 1, {0x00}},
    {0x02C, 1, {0x00}},
    {0x02D, 1, {0x00}},
    {0x02E, 1, {0x00}},
    {0x02F, 1, {0x00}},
    {0x030, 1, {0x00}},
    {0x031, 1, {0x00}},
    {0x032, 1, {0x00}},
    {0x033, 1, {0x00}},
    {0x034, 1, {0x04}},
    {0x035, 1, {0x00}},
    {0x036, 1, {0x00}},
    {0x037, 1, {0x00}},
    {0x038, 1, {0x3C}},
    {0x039, 1, {0x00}},
    {0x03A, 1, {0x40}},
    {0x03B, 1, {0x40}},
    {0x03C, 1, {0x00}},
    {0x03D, 1, {0x00}},
    {0x03E, 1, {0x00}},
    {0x03F, 1, {0x00}},
    {0x040, 1, {0x00}},
    {0x041, 1, {0x00}},
    {0x042, 1, {0x00}},
    {0x043, 1, {0x00}},
    {0x044, 1, {0x00}},
    {0x050, 1, {0x01}},
    {0x051, 1, {0x23}},
    {0x052, 1, {0x45}},
    {0x053, 1, {0x67}},
    {0x054, 1, {0x89}},
    {0x055, 1, {0xAB}},
    {0x056, 1, {0x01}},
    {0x057, 1, {0x23}},
    {0x058, 1, {0x45}},
    {0x059, 1, {0x67}},
    {0x05A, 1, {0x89}},
    {0x05B, 1, {0xAB}},
    {0x05C, 1, {0xCD}},
    {0x05D, 1, {0xEF}},
    {0x05E, 1, {0x11}},
    {0x05F, 1, {0x01}},
    {0x060, 1, {0x00}},
    {0x061, 1, {0x15}},
    {0x062, 1, {0x14}},
    {0x063, 1, {0x0C}},
    {0x064, 1, {0x0D}},
    {0x065, 1, {0x0E}},
    {0x066, 1, {0x0F}},
    {0x067, 1, {0x06}},
    {0x068, 1, {0x02}},
    {0x069, 1, {0x02}},
    {0x06A, 1, {0x02}},
    {0x06B, 1, {0x02}},
    {0x06C, 1, {0x02}},
    {0x06D, 1, {0x02}},
    {0x06E, 1, {0x08}},
    {0x06F, 1, {0x02}},
    {0x070, 1, {0x02}},
    {0x071, 1, {0x02}},
    {0x072, 1, {0x02}},
    {0x073, 1, {0x02}},
    {0x074, 1, {0x02}},
    {0x075, 1, {0x01}},
    {0x076, 1, {0x00}},
    {0x077, 1, {0x15}},
    {0x078, 1, {0x14}},
    {0x079, 1, {0x0C}},
    {0x07A, 1, {0x0D}},
    {0x07B, 1, {0x0E}},
    {0x07C, 1, {0x0F}},
    {0x07D, 1, {0x08}},
    {0x07E, 1, {0x02}},
    {0x07F, 1, {0x02}},
    {0x080, 1, {0x02}},
    {0x081, 1, {0x02}},
    {0x082, 1, {0x02}},
    {0x083, 1, {0x02}},
    {0x084, 1, {0x06}},
    {0x085, 1, {0x02}},
    {0x086, 1, {0x02}},
    {0x087, 1, {0x02}},
    {0x088, 1, {0x02}},
    {0x089, 1, {0x02}},
    {0x08A, 1, {0x02}},
    {0x0FF, 3, {0x98, 0x81, 0x04}},
    {0x06C, 1, {0x15}},
    {0x06E, 1, {0x2A}},
    {0x06F, 1, {0x35}},
    {0x03A, 1, {0x24}},
    {0x08D, 1, {0x14}},
    {0x087, 1, {0xBA}},
    {0x026, 1, {0x76}},
    {0x0B2, 1, {0xD1}},
    {0x035, 1, {0x1F}},
    {0x039, 1, {0x00}},
    {0x0B5, 1, {0x27}},
    {0x031, 1, {0x75}},
    {0x03B, 1, {0x98}},
    {0x030, 1, {0x03}},
    {0x033, 1, {0x14}},
    {0x038, 1, {0x02}},
    {0x07A, 1, {0x00}},
    {0x0FF, 3, {0x98, 0x81, 0x01}},
    {0x022, 1, {0x0A}},
    {0x031, 1, {0x00}},
    {0x035, 1, {0x07}},
    {0x053, 1, {0x6E}},
    {0x055, 1, {0x40}},
    {0x050, 1, {0x85}},
    {0x051, 1, {0x85}},
    {0x060, 1, {0x1F}},
    {0x062, 1, {0x07}},
    {0x063, 1, {0x00}},
    {0x0A0, 1, {0x08}},
    {0x0A1, 1, {0x14}},
    {0x0A2, 1, {0x1E}},
    {0x0A3, 1, {0x12}},
    {0x0A4, 1, {0x13}},
    {0x0A5, 1, {0x24}},
    {0x0A6, 1, {0x18}},
    {0x0A7, 1, {0x1A}},
    {0x0A8, 1, {0x56}},
    {0x0A9, 1, {0x18}},
    {0x0AA, 1, {0x25}},
    {0x0AB, 1, {0x57}},
    {0x0AC, 1, {0x22}},
    {0x0AD, 1, {0x24}},
    {0x0AE, 1, {0x58}},
    {0x0AF, 1, {0x2B}},
    {0x0B0, 1, {0x2E}},
    {0x0B1, 1, {0x4D}},
    {0x0B2, 1, {0x5B}},
    {0x0B3, 1, {0x3F}},
    {0x0C0, 1, {0x08}},
    {0x0C1, 1, {0x14}},
    {0x0C2, 1, {0x1E}},
    {0x0C3, 1, {0x12}},
    {0x0C4, 1, {0x13}},
    {0x0C5, 1, {0x24}},
    {0x0C6, 1, {0x17}},
    {0x0C7, 1, {0x1A}},
    {0x0C8, 1, {0x56}},
    {0x0C9, 1, {0x18}},
    {0x0CA, 1, {0x25}},
    {0x0CB, 1, {0x56}},
    {0x0CC, 1, {0x22}},
    {0x0CD, 1, {0x24}},
    {0x0CE, 1, {0x58}},
    {0x0CF, 1, {0x2B}},
    {0x0D0, 1, {0x2D}},
    {0x0D1, 1, {0x4E}},
    {0x0D2, 1, {0x5C}},
    {0x0D3, 1, {0x3F}},
    {0x0FF, 3, {0x98, 0x81, 0x00}},
    {0x011, 0, {}},
    {0xFFE, 120, {}},
    {0x029, 0, {}},
    {0xFFE, 20, {}},
    {0x035, 0, {}},
    {0xFFF, 0, {}},
};

#if 0
static struct LCM_setting_table lcm_set_window[] = {
	{0x2A, 4, {0x00, 0x00, (FRAME_WIDTH >> 8), (FRAME_WIDTH & 0xFF)} },
	{0x2B, 4, {0x00, 0x00, (FRAME_HEIGHT >> 8), (FRAME_HEIGHT & 0xFF)} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
#if 0
static struct LCM_setting_table lcm_sleep_out_setting[] = {
	/* Sleep Out */
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	/* Display ON */
	{0x29, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	/* Display off sequence */
	{0x28, 1, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	/* Sleep Mode On */
	{0x10, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd,
				table[i].count,
				table[i].para_list,
				force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
#endif
	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 16;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 4;
	params->dsi.vertical_frontporch = 4;
	params->dsi.vertical_frontporch_for_low_power = 540;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 48;
	params->dsi.horizontal_backporch = 16;
	params->dsi.horizontal_frontporch = 800;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 220;
#else
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 255;
#endif
	params->dsi.PLL_CK_CMD = 220;
	params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x87;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->full_content = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
	params->corner_pattern_height_bot = 32;
#endif
}

static void lcm_init_power(void)
{
	/*LCM_LOGD("lcm_init_power\n");*/

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO2_LCM_1V8_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO2_LCM_1V8_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO2_LCM_1V8_EN, GPIO_OUT_ONE);
	MDELAY(2);

	mt_set_gpio_mode(GPIO25_LCM_PO_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO25_LCM_PO_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO25_LCM_PO_EN, GPIO_OUT_ONE);
	MDELAY(2);

	mt_set_gpio_mode(GPIO26_LCM_NO_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO26_LCM_NO_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO26_LCM_NO_EN, GPIO_OUT_ONE);

	MDELAY(20);
#else
	display_bias_enable();

#endif
}

static void lcm_suspend_power(void)
{
	/*LCM_LOGD("lcm_suspend_power\n");*/

#ifdef BUILD_LK
	mt_set_gpio_out(GPIO2_LCM_1V8_EN, GPIO_OUT_ZERO);
	MDELAY(2);

	mt_set_gpio_out(GPIO26_LCM_NO_EN, GPIO_OUT_ZERO);
	MDELAY(2);

	mt_set_gpio_out(GPIO25_LCM_PO_EN, GPIO_OUT_ZERO);
#else
	display_bias_disable();

#endif
}

static void lcm_resume_power(void)
{
	/*LCM_LOGD("lcm_resume_power\n");*/

	/*lcm_init_power();*/
	SET_RESET_PIN(0);
	display_bias_enable();
}

static void lcm_init(void)
{
	int size;

	/*LCM_LOGD("lcm_init\n");*/

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO45_LCM_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO45_LCM_RESET, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO45_LCM_RESET, GPIO_OUT_ONE);
	MDELAY(1);
	mt_set_gpio_out(GPIO45_LCM_RESET, GPIO_OUT_ZERO);
	MDELAY(1);
	mt_set_gpio_out(GPIO45_LCM_RESET, GPIO_OUT_ONE);
	MDELAY(20);
#else
	SET_RESET_PIN(0);
	MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);
	if (lcm_dsi_mode == CMD_MODE) {
		size = sizeof(init_setting_cmd) /
			sizeof(struct LCM_setting_table);
		push_table(NULL, init_setting_cmd, size, 1);
		LCM_LOGI(
			"ili9881c----rt5081----lcm mode = cmd mode :%d----\n",
			lcm_dsi_mode);
	} else {
		size = sizeof(init_setting_vdo) /
			sizeof(struct LCM_setting_table);
		push_table(NULL, init_setting_vdo, size, 1);
		LCM_LOGI(
			"ili9881c----rt5081----lcm mode = vdo mode :%d----\n",
			lcm_dsi_mode);
	}

#endif



}

static void lcm_suspend(void)
{
	/*LCM_LOGD("lcm_suspend\n");*/

	push_table(NULL, lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	MDELAY(10);
	/* SET_RESET_PIN(0); */
}

static void lcm_resume(void)
{
	/*LCM_LOGD("lcm_resume\n");*/

	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y,
	unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0, version_id = 0;
	unsigned char buffer[2];
	unsigned int array[16];
	struct LCM_setting_table switch_table_page1[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x01} }
	};
	struct LCM_setting_table switch_table_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(NULL,
		switch_table_page1,
		sizeof(switch_table_page1) / sizeof(struct LCM_setting_table),
		1);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x00, buffer, 1);
	id = buffer[0];		/* we only need ID */

	read_reg_v2(0x01, buffer, 1);
	version_id = buffer[0];

	LCM_LOGI("%s,ili9881c_id=0x%08x,version_id=0x%x\n",
		__func__, id, version_id);
	push_table(NULL,
		switch_table_page0,
		sizeof(switch_table_page0) / sizeof(struct LCM_setting_table),
		1);

	if (id == LCM_ID && version_id == 0x81)
		return 1;
	else
		return 0;

}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x53, buffer, 1);

	if (buffer[0] != 0x24) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",
		x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	/* read id return two byte,version and id */
	data_array[0] = 0x00043700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,ili9881c backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle,
		bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table),
		1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value;
 *                      2. config mode control register
 */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		/* mode control addr */
		lcm_switch_mode_cmd.addr = 0xBB;
		/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[0] = 0x13;
		/* disable video mode secondly */
		lcm_switch_mode_cmd.val[1] = 0x10;
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		/* disable GRAM and enable video mode */
		lcm_switch_mode_cmd.val[0] = 0x03;
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (LCM_DSI_CMD_MODE)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty. */
	/* In this case we need to use minimu roi */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		LCM_LOGD("%s calc error,assign full roi:y=%d,h=%d\n",
			__func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*LCM_LOGD("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",*/
	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif

#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER ili9881c_bns080h030hd981d31b_wxga_hsd8_cmd_lcm_drv = {
	.name = "ili9881c_bns080h030hd981d31b_wxga_hsd8_cmd_drv",
#else

struct LCM_DRIVER ili9881c_bns080h030hd981d31b_wxga_hsd8_lcm_drv = {
	.name = "ili9881_bns080h030hd981d31d_wxga_hsd8",
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
