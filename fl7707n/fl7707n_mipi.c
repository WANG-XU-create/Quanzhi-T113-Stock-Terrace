/* drivers/video/sunxi/disp2/disp/lcd/fl7707n_mipi.c
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
&lcd0 {
        lcd_used            = <1>;                            //# 启用lcd
        lcd_driver_name     = "fl7707n_mipi";                 //# 使用 default_lcd 驱动
        lcd_backlight       = <50>;
        lcd_if              = <4>;                            //# 0:rgb 4:dsi
        lcd_dsi_if          = <0>;
        
        lcd_dsi_lane        = <2>;
        pinctrl-0 = <&dsi2lane_pins_a>;
        pinctrl-1 = <&dsi2lane_pins_a>;

        lcd_x               = <720>;                          //# 宽度
        lcd_y               = <720>;                         //# 高度
        lcd_width           = <72>;                           //# 屏幕物理宽度，单位 mm
        lcd_height          = <72>;                          //# 屏幕物理高度，单位 mm
        lcd_dclk_freq       = <60>;                           //# 屏幕时钟，单位 MHz lcd_ht * lcd_vt * 60

        lcd_hbp             = <166>;                           //# hsync back porch(pixel) + hsync plus width(pixel);
        lcd_ht              = <1006>;                          //# hsync total cycle(pixel)
        lcd_hspw            = <60>;                           //# hsync plus width(pixel)
        lcd_vbp             = <24>;                            //# vsync back porch(line) + vysnc plus width(line)        
        lcd_vt              = <764>;                         //# vsync total cycle(line)
        lcd_vspw            = <4>;                            //# vsync plus width(pixel)

        lcd_pwm_used        = <1>;                            //# 启用背光 PWM
        lcd_pwm_ch          = <7>;                            //# 使用 PWM 通道 9 
        lcd_pwm_freq        = <10000>;                        //# PWM 频率，单位 Hz
        lcd_pwm_pol         = <0>;                            //# 背光 PWM 的极性
        lcd_pwm_max_limit   = <255>;
        
        lcd_dsi_format      = <0>;
        lcd_dsi_te          = <0>;
        
        lcd_frm             = <0>;
        lcd_gamma_en        = <0>;
        lcd_cmap_en         = <0>;
        
        lcd_gpio_0 = <&pio PD 20 GPIO_ACTIVE_HIGH>;        
};
*/
#include "fl7707n_mipi.h"

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

static s32 lcd_open_flow(u32 sel)
{
    printk("=====================lcd_open_flow\n");
    LCD_OPEN_FUNC(sel, lcd_power_on, 100);
    LCD_OPEN_FUNC(sel, lcd_panel_init, 120);
    LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 120);
    LCD_OPEN_FUNC(sel, lcd_bl_open, 0);
    return 0;
}

static s32 lcd_close_flow(u32 sel)
{
    printk("=====================lcd_close_flow\n");
    LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
    LCD_CLOSE_FUNC(sel, lcd_panel_exit, 200);
    LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
    LCD_CLOSE_FUNC(sel, lcd_power_off, 500);

    return 0;
}

static void lcd_power_on(u32 sel)
{
    printk("=====================lcd_power_on\n");
    sunxi_lcd_pin_cfg(sel, 1);
    sunxi_lcd_power_enable(sel, 0);
    // sunxi_lcd_power_enable(sel, 1);
    sunxi_lcd_delay_ms(50);

    /* reset lcd by gpio */
    panel_reset(sel, 1);
    sunxi_lcd_delay_ms(5);
    panel_reset(sel, 0);
    sunxi_lcd_delay_ms(10);
    panel_reset(sel, 1);
    sunxi_lcd_delay_ms(120);
    sunxi_lcd_delay_ms(10);
}

static void lcd_power_off(u32 sel)
{
    printk("=====================lcd_power_off\n");
    sunxi_lcd_pin_cfg(sel, 0);
    sunxi_lcd_delay_ms(20);
    panel_reset(sel, 0);
    sunxi_lcd_delay_ms(5);
    sunxi_lcd_power_disable(sel, 0);
}

static void lcd_bl_open(u32 sel)
{
    printk("=====================lcd_bl_open\n");
    sunxi_lcd_pwm_enable(sel);
    sunxi_lcd_backlight_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
    printk("=====================lcd_bl_close\n");
    sunxi_lcd_backlight_disable(sel);
    sunxi_lcd_pwm_disable(sel);
}

#define REGFLAG_DELAY 0XFC
#define REGFLAG_END_OF_TABLE 0xFD /* END OF REGISTERS MARKER */

struct LCM_setting_table {
    u8 cmd;
    u32 count;
    u8 para_list[100];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xB9, 3,  {0xF1,0x12,0x87} },
    {0xB2, 3,  {0x64,0x05,0x78} },
    {0xB3, 10, {0x10,0x10,0x28,0x28,0x03,0xFF,0x00,0x00,0x00,0x00} },
    {0xB4, 1,  {0x80} },
    {0xB5, 2,  {0x0D,0x0D} },
    {0xB6, 2,  {0x4F,0x4F} },
    {0xB8, 4,  {0x26,0x22,0xF0,0x13} },

    {0xBA, 27, {0x33,0x81,0x05,0xF9,0x0E,0x0E,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x25,0x00,0x91,0x0A,0x00,0x00,0x01,0x4F,0x01,0x00,0x00,0x37} },
    {0xBC, 1,  {0x47} },
    {0xBF, 5,  {0x02,0x10,0x00,0x80,0x04} },

    {0xC0, 9,  {0x73,0x73,0x50,0x50,0x00,0x00,0x12,0x73,0x00} },
    {0xC1, 17, {0x54,0x00,0x32,0x32,0x99,0xE4,0x77,0x77,0xCC,0xCC,0xFF,0xFF,0x11,0x11,0x00,0x00,0x32} },
    {0xC7, 12, {0x10,0x00,0x0A,0x00,0x00,0x00,0x00,0x00,0xED,0xC5,0x00,0xA5} },
    {0xC8, 4,  {0x10,0x40,0x1E,0x03} },
    {0xCC, 1,  {0x0B} },

    {0xE0, 34, {0x00,0x0B,0x12,0x29,0x3C,0x3F,0x47,0x3D,0x06,0x0C,0x0D,0x12,0x13,0x11,0x13,0x13,0x1B,0x00,0x0B,0x12,0x29,0x3C,0x3F,0x47,0x3D,0x06,0x0C,0x0D,0x12,0x13,0x11,0x13,0x13,0x1B} },
    {0xE1, 7,  {0x11,0x11,0x91,0x00,0x00,0x00,0x00} },
    {0xE3, 14, {0x07,0x07,0x0B,0x0B,0x0B,0x0B,0x00,0x00,0x00,0x00,0xFF,0x04,0xC0,0x10} },
    {0xE9, 63, {0xC8,0x10,0x0A,0x10,0x0D,0x80,0x38,0x12,0x31,0x23,0x4F,0x86,0x80,0x38,0x47,0x08,0x00,0x00,0x4F,0x00,0x00,0x04,0x00,0x00,0x4F,0x00,0x00,0x04,0x94,0xA3,0xF8,0x18,0x13,0x57,0x88,0x88,0x88,0x88,0x88,0x94,0xA2,0xF8,0x08,0x02,0x46,0x88,0x88,0x88,0x88,0x88,0x00,0x00,0x00,0x01,0x00,0x80,0x38,0x00,0x00,0x00,0x00,0x00,0x00} },
    {0xEA, 61, {0x00,0x1A,0x00,0x00,0x00,0x00,0x01,0x0A,0x41,0x01,0x02,0x00,0x94,0xA0,0x8F,0x28,0x64,0x20,0x88,0x88,0x88,0x88,0x88,0x94,0xA1,0x8F,0x38,0x75,0x31,0x88,0x88,0x88,0x88,0x88,0x23,0x00,0x00,0x00,0xD3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0xAA,0x00,0x00,0x40,0x80,0x38,0x40,0x80,0x38,0x00} },

    {0xEF, 3,  {0xFF,0xFF,0x01} },

    {0x11, 0, {} },
    {REGFLAG_DELAY, REGFLAG_DELAY, {250} },

    {0x29, 0, {} },
    {REGFLAG_DELAY, REGFLAG_DELAY, {50} },
    {REGFLAG_END_OF_TABLE, REGFLAG_END_OF_TABLE, {} }
};

static void lcd_panel_init(u32 sel)
{
    u32 i = 0;

    sunxi_lcd_dsi_clk_enable(sel);
    sunxi_lcd_delay_ms(100);

    for (i = 0;; i++) {
        if (lcm_initialization_setting[i].cmd == REGFLAG_END_OF_TABLE)
            break;
        else if (lcm_initialization_setting[i].cmd == REGFLAG_DELAY)
            sunxi_lcd_delay_ms(lcm_initialization_setting[i].count);
        else {
            dsi_dcs_wr(0, lcm_initialization_setting[i].cmd,
                   lcm_initialization_setting[i].para_list,
                   lcm_initialization_setting[i].count);
        }
    }
}

static void lcd_panel_exit(u32 sel)
{
    sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
    sunxi_lcd_delay_ms(80);
    sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
    sunxi_lcd_delay_ms(50);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
    return 0;
}

__lcd_panel_t fl7707n_mipi_panel = {
    /* panel driver name, must mach the name of
     * lcd_drv_name in sys_config.fex
     */
    .name = "fl7707n_mipi",
    .func = {
        .cfg_panel_info = lcd_cfg_panel_info,
        .cfg_open_flow = lcd_open_flow,
        .cfg_close_flow = lcd_close_flow,
        .lcd_user_defined_func = lcd_user_defined_func,
    },
};
