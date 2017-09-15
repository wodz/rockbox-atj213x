/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (c) 2017 Marcin Bukat
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

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "system.h"
#include "kernel.h"
#include "button.h"
#include "lcd.h"
#include "adc.h"
#include "font.h"
#include "storage.h"
#include "lcd-target.h"
#include "lradc-atj213x.h"
#include "regs/regs-cmu.h"
#include "regs/regs-gpio.h"

#define DEBUG_CANCEL BUTTON_LEFT

/*  Skeleton for adding target specific debug info to the debug menu
 */

#define _DEBUG_PRINTF(a, varargs...) lcd_putsf(0, line++, (a), ##varargs)

bool dbg_hw_info(void)
{
    int line;
    lcd_clear_display();
    lcd_setfont(FONT_SYSFIXED);

    while(1)
    {
        lcd_clear_display();
        line = 0;

        /* _DEBUG_PRINTF statements can be added here to show debug info */
        _DEBUG_PRINTF("CMU_COREPLL:  0x%08x", CMU_COREPLL);
        _DEBUG_PRINTF("CMU_DSPPLL:   0x%08x", CMU_DSPPLL);
        _DEBUG_PRINTF("CMU_AUDIOPLL: 0x%08x", CMU_AUDIOPLL);
        _DEBUG_PRINTF("CMU_BUSCLK:   0x%08x", CMU_BUSCLK);
        _DEBUG_PRINTF("CMU_SDRCLK:   0x%08x", CMU_SDRCLK);
        _DEBUG_PRINTF("CMU_SDCLK:    0x%08x", CMU_SDCLK);
        _DEBUG_PRINTF("CMU_FMCLK:    0x%08x", CMU_FMCLK);
        _DEBUG_PRINTF("CMU_DEVCLKEN: 0x%08x", CMU_DEVCLKEN);

        lcd_update(); 
        switch(button_get_w_tmo(HZ/20))
        {
            case DEBUG_CANCEL|BUTTON_REL:
                lcd_setfont(FONT_UI);
                return false;
        }
    }

    lcd_setfont(FONT_UI);
    return false;
}

bool dbg_ports(void)
{
    int line;

    lcd_setfont(FONT_SYSFIXED);

    while(1)
    {
        lcd_clear_display();
        line = 0;
        
        _DEBUG_PRINTF("GPIO_AOUTEN:  0x%08x", GPIO_AOUTEN);
        _DEBUG_PRINTF("GPIO_AINEN:   0x%08x", GPIO_AINEN);
        _DEBUG_PRINTF("GPIO_ADAT:    0x%08x", GPIO_ADAT);
        _DEBUG_PRINTF("GPIO_BOUTEN:  0x%08x", GPIO_BOUTEN);
        _DEBUG_PRINTF("GPIO_BINEN:   0x%08x", GPIO_BINEN);
        _DEBUG_PRINTF("GPIO_BDAT:    0x%08x", GPIO_BDAT);
        _DEBUG_PRINTF("GPIO_MFCTL0: 0x%08x", GPIO_MFCTL0);
        _DEBUG_PRINTF("GPIO_MFCTL1: 0x%08x", GPIO_MFCTL1);
        _DEBUG_PRINTF("ADC_KEY: %d", lradc_read(LRADC_CH_KEY));
        _DEBUG_PRINTF("ADC_BAT: %d", lradc_read(LRADC_CH_BAT));
        _DEBUG_PRINTF("ADC_TEM: %d", lradc_read(LRADC_CH_TEMP));

        lcd_update();
        if (button_get_w_tmo(HZ/10) == (DEBUG_CANCEL|BUTTON_REL))
            break;
    }
    lcd_setfont(FONT_UI);
    return false;
}

