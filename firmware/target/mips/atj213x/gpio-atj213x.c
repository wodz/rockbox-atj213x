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
 
#include "config.h"
#include "system.h"
#include "gpio-atj213x.h"

static struct mutex muxsel_mtx;
static unsigned gpio_muxsel_owner = GPIO_MUXSEL_FREE;

void atj213x_gpio_muxsel(unsigned module)
{
    uint32_t mfctl0 = GPIO_MFCTL0;
    switch (module)
    {
        case GPIO_MUXSEL_LCM:
            /* taken from WELCOME.BIN */
            mfctl0 &= 0xfe3f3f00;
            mfctl0 |= 0x00808092;
            break;

        case GPIO_MUXSEL_SD:
            /* taken from CARD.DRV */
            mfctl0 &= 0xff3ffffc;
            mfctl0 |= 0x01300004;
            break;

        case GPIO_MUXSEL_NAND:
            /* taken from BROM dump */
            mfctl0 &= 0xfe3ff300;
            mfctl0 |= 0x00400449;
            break;

        default:
            panicf("Wrong gpio muxsel argument");
    }

    atj213x_gpio_mux_lock(module);

    /* enable multifunction mux */
    GPIO_MFCTL1 = (1<<31);

    /* write multifunction mux selection */
    GPIO_MFCTL0 = mfctl0;
}

void atj213x_gpio_mux_lock(unsigned module)
{
    unsigned long timeout = current_tick + HZ/10;

    while (gpio_mux_owner != GPIO_MUXSEL_FREE)
    {
        if (TIME_AFTER(current_tick, timeout)
            panicf("atj213x_gpio_mux_lock() timeout");
    }

    // disable irqs
    mutex_lock(&muxsel_mtx);
    gpio_mux_owner = module;
    // enable irqs
}

void atj213x_gpio_mux_unlock(unsigned module)
{
    if (gpio_mux_owner == module)
        mutex_unlock(&muxsel_mtx);
    else
        panicf("atj213x_gpio_mux_unlock() not an owner");
}
