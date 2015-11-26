/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2015 by Marcin Bukat
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "config.h"
#include "inttypes.h"
#include "cpu.h"
#include "system.h"
#include "lcd.h"
#include "../kernel-internal.h"
#include "storage.h"
#include "fat.h"
#include "disk.h"
#include "font.h"
#include "backlight.h"
#include "backlight-target.h"
#include "button.h"
#include "panic.h"
#include "power.h"
#include "file.h"
#include "common.h"

#include "regs/regs-intc.h"
#include "regs/regs-rtcwdt.h"
#include "tmr-atj213x.h"

extern void show_logo( void );
void main(void)
{
    unsigned int button = BUTTON_NONE;
    system_init();
    kernel_init();
    lcd_init();
    button_init();
    backlight_hw_init();

//    show_logo();

//    sleep(HZ*2);
    int ret = storage_init();
    lcd_clear_display();
    lcd_update();

    printf("storage_init(): %d", ret);

    while(1)
    {
        button = button_get_w_tmo(HZ);
        switch (button)
        {
            case BUTTON_ON:
                printf("Button ON");
                break;
            case BUTTON_LEFT:
                printf("Button left");
                break;
            case BUTTON_RIGHT:
                printf("Button right");
                break;
            case BUTTON_UP:
                printf("Button up");
                break;
            case BUTTON_DOWN:
                printf("Button down");
                break;
            case BUTTON_SELECT:
                printf("Button select");
                break;
            case BUTTON_VOL_UP:
                printf("Button vol+");
                break;
            case BUTTON_VOL_DOWN:
                printf("Button vol-");
                break;
        }
    }
}
