/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2014 by Marcin Bukat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
 
#include "system.h"
#include "lcd.h"
#include "gpio-atj213x.h"
#include "lcm-atj213x.h"
#include "regs/regs-yuv2rgb.h"

static inline void lcd_reg_write(int reg, int val)
{
     lcm_rs_command();
     YUV2RGB_FIFODATA = reg;
     lcm_rs_data();
     YUV2RGB_FIFODATA = val;
}

static void lcd_set_gram_area(int x_start, int y_start,
                       int x_end, int y_end)
{
    /* here we exploit the fact that width fits in LSB only */
    lcd_reg_write(0x02, 0x00); /* column start MSB */
    lcd_reg_write(0x03, x_start); /* column start LSB */
    lcd_reg_write(0x04, 0x00); /* column end MSB */
    lcd_reg_write(0x05, x_end); /* column end LSB */

    lcd_reg_write(0x06, y_start>>8); /* row start MSB */
    lcd_reg_write(0x07, y_start&0xff); /* row start LSB */
    lcd_reg_write(0x08, y_end>>8); /* row end MSB */
    lcd_reg_write(0x09, y_end&0xff); /* row end LSB */
}


/* google says COG-IZT2298 is 2.8" lcd module
 * The init sequence matches  HX8347-D lcd controller in rgb565 mode
 * with 16bit parallel interface.
 */
void hx8347d_init(void)
{
    int x,y;

    lcd_reg_write(0xea, 0x00);
    lcd_reg_write(0xeb, 0x20);
    lcd_reg_write(0xec, 0x0f);
    lcd_reg_write(0xed, 0xc4);
    lcd_reg_write(0xe8, 0xc4);
    lcd_reg_write(0xe9, 0xc4);
    lcd_reg_write(0xf1, 0xc4);
    lcd_reg_write(0xf2, 0xc4);
    lcd_reg_write(0x27, 0xc4);
    lcd_reg_write(0x40, 0x00); /* gamma block start */
    lcd_reg_write(0x41, 0x00);
    lcd_reg_write(0x42, 0x01);
    lcd_reg_write(0x43, 0x13);
    lcd_reg_write(0x44, 0x10);
    lcd_reg_write(0x45, 0x26);
    lcd_reg_write(0x46, 0x08);
    lcd_reg_write(0x47, 0x51);
    lcd_reg_write(0x48, 0x02);
    lcd_reg_write(0x49, 0x12);
    lcd_reg_write(0x4a, 0x18);
    lcd_reg_write(0x4b, 0x19);
    lcd_reg_write(0x4c, 0x14);
    lcd_reg_write(0x50, 0x19);
    lcd_reg_write(0x51, 0x2f);
    lcd_reg_write(0x52, 0x2c);
    lcd_reg_write(0x53, 0x3e);
    lcd_reg_write(0x54, 0x3f);
    lcd_reg_write(0x55, 0x3f);
    lcd_reg_write(0x56, 0x2e);
    lcd_reg_write(0x57, 0x77);
    lcd_reg_write(0x58, 0x0b);
    lcd_reg_write(0x59, 0x06);
    lcd_reg_write(0x5a, 0x07);
    lcd_reg_write(0x5b, 0x0d);
    lcd_reg_write(0x5c, 0x1d);
    lcd_reg_write(0x5d, 0xcc); /* gamma block end */
    lcd_reg_write(0x1b, 0x1b);
    lcd_reg_write(0x1a, 0x01);
    lcd_reg_write(0x24, 0x2f);
    lcd_reg_write(0x25, 0x57);
    lcd_reg_write(0x23, 0x86);
    lcd_reg_write(0x18, 0x36); /* 70Hz framerate */
    lcd_reg_write(0x19, 0x01); /* osc enable */
    lcd_reg_write(0x01, 0x00);
    lcd_reg_write(0x1f, 0x88);
    mdelay(5);
    lcd_reg_write(0x1f, 0x80);
    mdelay(5);
    lcd_reg_write(0x1f, 0x90);
    mdelay(5);
    lcd_reg_write(0x1f, 0xd0);
    mdelay(5);
    lcd_reg_write(0x17, 0x05); /* 16bpp */
    lcd_reg_write(0x36, 0x00);
    lcd_reg_write(0x28, 0x38);
    mdelay(40);
    lcd_reg_write(0x28, 0x3c);

    lcd_set_gram_area(0, 0, LCD_WIDTH, LCD_HEIGHT);

    lcm_fb_data(); /* prepare for data write to fifo */

    /* clear lcd gram */
    for (y=0; y<LCD_HEIGHT; y++)
        for (x=0; x<(LCD_WIDTH/2); x++)
            YUV2RGB_FIFODATA = 0;
}

/* enter sleep */
void lcd_sleep(bool sleep)
{
    if (sleep)
    {
        lcd_reg_write(0x1f, 0x10);
        mdelay(40);
        lcd_reg_write(0x1f, 0x19);
        mdelay(40);
        lcd_reg_write(0x19, 0x00);
    }
    else
    {
        lcd_reg_write(0x19, 0x01);
        lcd_reg_write(0x1f, 0x88);
        mdelay(5);
        lcd_reg_write(0x1f, 0x80);
        mdelay(5);
        lcd_reg_write(0x1f, 0x90);
        mdelay(5);
        lcd_reg_write(0x1f, 0xd0);
        mdelay(5);
    }
}

/* rockbox API functions */
void lcd_init_device(void)
{
    /* lcd reset pin GPIOA16 */
    atj213x_gpio_setup(GPIO_PORTA, 16, GPIO_OUT);
    atj213x_gpio_set(GPIO_PORTA, 16, 1);
    mdelay(10);
    atj213x_gpio_set(GPIO_PORTA, 16, 0);
    mdelay(10);
    atj213x_gpio_set(GPIO_PORTA, 16, 1);
    mdelay(10);

    atj213x_gpio_muxsel(GPIO_MUXSEL_LCM);

    lcm_init();
    hx8347d_init();

    atj213x_gpio_mux_unlock(GPIO_MUXSEL_LCM);
}

void lcd_update_rect(int x, int y, int width, int height)
{
    unsigned int end_x, end_y, row, col, *fbptr;

    x &= ~1; /* we transfer 2px at once */
    width = (width + 1) & ~1; /* adjust the width */

    end_x = x + width - 1;
    end_y = y + height - 1;

    atj213x_gpio_muxsel(GPIO_MUXSEL_LCM);

    lcd_set_gram_area(x, y, end_x, end_y); /* set GRAM window */
    lcm_fb_data(); /* prepare for AHB write to fifo */

    for (row=y; row<=end_y; row++)
    {
        fbptr = (unsigned int *)FBADDR(x,row);
        for (col=x; col<end_x; col+=2)
            YUV2RGB_FIFODATA = *fbptr++;
    }

    atj213x_gpio_mux_unlock(GPIO_MUXSEL_LCM);
}

void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}
