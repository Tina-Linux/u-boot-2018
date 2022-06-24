/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/video/sunxi/disp2/disp/lcd/jd9161z.c
 *
 * Copyright (c) 2017 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * he0801a-068 panel driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
[lcd0]
 * lcd_used            = 1
 *
 * lcd_driver_name     = "jd9161z"
 *
 * lcd_bl_0_percent    = 0
 * lcd_bl_40_percent   = 23
 * lcd_bl_100_percent  = 100
 * lcd_backlight       = 88
 *
 * lcd_if              = 4
 * lcd_x               = 480
 * lcd_y               = 640
 * lcd_width           = 36
 * lcd_height          = 65
 * lcd_dclk_freq       = 25
 *
 * lcd_pwm_used        = 1
 * lcd_pwm_ch          = 8
 * lcd_pwm_freq        = 50000
 * lcd_pwm_pol         = 1
 * lcd_pwm_max_limit   = 255
 *
 * lcd_hbp             = 70
 * lcd_ht              = 615
 * lcd_hspw            = 8
 * lcd_vbp             = 30
 * lcd_vt              = 690
 * lcd_vspw            = 10
 *
 * lcd_dsi_if          = 0
 * lcd_dsi_lane        = 2
 * lcd_dsi_format      = 0
 * lcd_dsi_te          = 0
 * lcd_dsi_eotp        = 0
 *
 * lcd_frm             = 0
 * lcd_io_phase        = 0x0000
 * lcd_hv_clk_phase    = 0
 * lcd_hv_sync_polarity= 0
 * lcd_gamma_en        = 0
 * lcd_bright_curve_en = 0
 * lcd_cmap_en         = 0
 *
 * lcdgamma4iep        = 22
 *
 * ;lcd_bl_en           = port:PD09<1><0><default><1>
 * lcd_power            = "vcc-lcd"
 * lcd_pin_power        = "vcc18-dsi"
 * lcd_pin_power1		= "vcc-pd"
 *
 * ;reset
 * lcd_gpio_0          = port:PD09<1><0><default><1>
 */
#include "jd9161z_mipi.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)

static void lcd_cfg_panel_info(panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
	    {0, 0},     {15, 15},   {30, 30},   {45, 45},   {60, 60},
	    {75, 75},   {90, 90},   {105, 105}, {120, 120}, {135, 135},
	    {150, 150}, {165, 165}, {180, 180}, {195, 195}, {210, 210},
	    {225, 225}, {240, 240}, {255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
		{LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
		{LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
		{LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
		},
		{
		{LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
		{LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
		{LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
		},
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
			    lcd_gamma_tbl[i][1] +
			    ((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1]) *
			     j) /
				num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
			    (value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8) +
				   lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));
}

static void lcd_panel_try_switch(u32 sel)
{
	u8 result[16] = {0};
	u32 num = 0, ret = 0;

	/* read st7701_g5 lcd id */
	sunxi_lcd_dsi_dcs_read(sel, 0xa1, result, &num);
	if (result[0] == 0x88) {
		tick_printf("[LCD] id is 0x%x, driver is st7701s_g5\n", result[0]);
		sunxi_lcd_switch_compat_panel(sel, 1);
		printf("%s:%d\n", __func__, __LINE__);
	} else {
		/* read jd9161z lcd id */
		sunxi_lcd_dsi_dcs_read(sel, 0x04, result, &num);
		if (result[0] == 0x91 || result[0] == 0x61)
			tick_printf("[LCD] id is 0x%x, driver is jd9161z_mipi\n", result[0]);
		else
			tick_printf("[LCD] unknown lcd id, use default driver is jd9161z_mipi\n");
	}

	/* update lcdid command line */
	char lcdid[8];

	snprintf(lcdid, sizeof(lcdid), "0x%x", result[0]);
	ret = env_set("lcd_id", lcdid);
	if (ret)
		tick_printf("[LCD] set lcdid cmdline fail!\n");
}

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 0);
	LCD_OPEN_FUNC(sel, lcd_panel_try_switch, 0);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 0);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 40);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);
	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 10);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 0);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_power_enable(sel, 1);
	sunxi_lcd_delay_ms(10);

	/* reset lcd by gpio */
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(5);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(120);
}

static void lcd_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(1);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_power_enable(sel, 1);
	sunxi_lcd_power_disable(sel, 0);
}

static void lcd_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	sunxi_lcd_backlight_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

#define REGFLAG_DELAY         0XFE
#define REGFLAG_END_OF_TABLE  0xFC   /* END OF REGISTERS MARKER */

struct LCM_setting_table {
	u8 cmd;
	u32 count;
	u8 para_list[40];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xdf, 3, {0x91, 0x62, 0xF3}},
	{0xb7, 6, {0x10, 0x16, 0x58, 0x00, 0x5F, 0x21}},
	{0xb9, 4,  {0x43, 0x03, 0x00, 0x77}},
	{0xbb, 11,  {0x59, 0x19, 0x32, 0x32, 0x32, 0xC0, 0xD0, 0x20, 0x40, 0x50, 0x50}},
	{0xbc, 2,  {0x73, 0x34}},
	{0xc0, 1,  {0x22}},
	{0xc3, 14,  {0x74, 0x04, 0x04, 0x08, 0x00, 0x08, 0x04, 0x08, 0x08, 0x00,
				0x08, 0x82, 0x04, 0x82}},
	{0xc4, 5,  {0x10, 0xE0, 0x92, 0x0E, 0x12}},
	{0xc8, 38,  {0x7F, 0x62, 0x50, 0x43, 0x3F, 0x2E, 0x32, 0x1A, 0x31, 0x2D,
				0x2D, 0x4B, 0x39, 0x3F, 0x2F, 0x2C, 0x1C, 0x0E, 0x04, 0x7F,
				0x62, 0x50, 0x43, 0x3F, 0x2E, 0x32, 0x1A, 0x31, 0x2D, 0x2D,
				0x4B, 0x39, 0x3F, 0x2F, 0x2C, 0x1C, 0x0E, 0x04}},
	{0xd0, 16,  {0x1F, 0x1F, 0x1F, 0x1E, 0x15, 0x10, 0x08, 0x0A, 0x04, 0x06,
				0x1F, 0x35, 0x00, 0x02, 0x1F, 0x1F}},
	{0xd1, 16,  {0x1F, 0x1F, 0x1F, 0x1E, 0x15, 0x10, 0x09, 0x0B, 0x05, 0x07,
				0x1F, 0x35, 0x03, 0x01, 0x1F, 0x1F}},
	{0xd2, 16,  {0x1F, 0x1F, 0x1E, 0x1F, 0x15, 0x10, 0x07, 0x05, 0x0B, 0x09,
				0x1F, 0x35, 0x03, 0x01, 0x1F, 0x1F}},
	{0xd3, 16,  {0x1F, 0x1F, 0x1E, 0x1F, 0x15, 0x10, 0x06, 0x04, 0x0A, 0x08,
				0x1F, 0x35, 0x00, 0x02, 0x1F, 0x1F}},
	{0xd4, 29,  {0x30, 0x00, 0x00, 0x00, 0x00, 0x08, 0x30, 0x00, 0x0A, 0x03,
				0x6F, 0x03, 0x6F, 0xD8, 0x33, 0x80, 0x00, 0x03, 0x6F, 0x73,
				0x0C, 0x03, 0x6F, 0x08, 0x25, 0x00, 0x63, 0x03, 0x00}},
	{0xd5, 36,  {0x30, 0x08, 0x80, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
				0x06, 0x60, 0x00, 0x07, 0x50, 0x00, 0x83, 0xC0, 0x00, 0x60,
				0x03, 0x00, 0x00, 0x00, 0x04, 0x6F, 0x06, 0x10, 0x00, 0x00,
				0x0F, 0x4F, 0x00, 0x10, 0x1F, 0x3F}},
	{0xde, 1,  {0x02}},
	{0xc1, 1,  {0x60}},
	{0xde, 1,  {0x00}},
	{0x11, 1,  {0x00}},
	{REGFLAG_DELAY, 120, {} },
	{0x29, 1,  {0x00}},
	{REGFLAG_DELAY, 5, {} },
	{0x35, 1, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void lcd_panel_init(u32 sel)
{
	u32 i = 0;

	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(10);

	for (i = 0;; i++) {
		if (lcm_initialization_setting[i].cmd == REGFLAG_END_OF_TABLE) {
			break;
		} else if (lcm_initialization_setting[i].cmd == REGFLAG_DELAY) {
			sunxi_lcd_delay_ms(lcm_initialization_setting[i].count);

		} else {
			dsi_dcs_wr(0, lcm_initialization_setting[i].cmd,
				   lcm_initialization_setting[i].para_list,
				   lcm_initialization_setting[i].count);
		}
	}
}

static void lcd_panel_exit(u32 sel)
{
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(20);
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	sunxi_lcd_delay_ms(120);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

__lcd_panel_t jd9161z_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "jd9161z",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
			.cfg_open_flow = lcd_open_flow,
			.cfg_close_flow = lcd_close_flow,
			.lcd_user_defined_func = lcd_user_defined_func,
	},
};
