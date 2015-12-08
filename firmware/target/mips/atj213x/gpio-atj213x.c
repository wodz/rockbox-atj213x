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
 
#include "kernel.h"
#include "panic.h"
#include "gpio-atj213x.h"
#include "regs/regs-gpio.h"

static int gpio_muxsel_owner = GPIO_MUXSEL_FREE;

void atj213x_gpio_muxsel(int module)
{
    uint32_t mfctl0 = GPIO_MFCTL0;
    switch (module)
    {
        case GPIO_MUXSEL_LCM:
            /* taken from WELCOME.BIN */
            mfctl0 &= 0xfe3f3f00; /* ~((7<<22) | (3<<14) | 0xff) */
            mfctl0 |= 0x00808092; /* (1<<23) | (1<<15) | (1<<7) | (1<<4) | (1<<1) */
            break;                /* RGB_RS, WD9, WD0, CEB3 - RGB_CE, RGB_WD[17:10], RGB_WD[8:1] */

        case GPIO_MUXSEL_SD:
            /* taken from CARD.DRV */
            mfctl0 &= 0xff3ffffc; /* ~((3<<22) | 0x03) */
            mfctl0 |= 0x01300004; /* (1<<24) | (1<<21) | (1<<20) | (1<<2) */
            break;                /* SD_CMD, CEB6 - SDCLK, SD_D[7:0] */

        case GPIO_MUXSEL_NAND:
            /* taken from BROM dump */
            mfctl0 &= 0xfe3ff300; /* ~((7<<22) | (3<<10) | 0xff) */
            mfctl0 |= 0x00400449; /* (1<<22) | (1<<10) | (1<<6) | (1<<3) | 1 */
            break;                /* NAND_CLE, NAND_RB, NAND_ALE, CEB1 - NAND_CEB1, NAND_D[7:0], NAND_D[15:8] */

        default:
            panicf("Wrong gpio muxsel argument");
    }

    atj213x_gpio_mux_lock(module);

    /* enable multifunction mux */
    GPIO_MFCTL1 = (1<<31);

    /* write multifunction mux selection */
    GPIO_MFCTL0 = mfctl0;
}

void atj213x_gpio_mux_lock(int module)
{
    long tmo = current_tick + HZ/10;
    int owner_save;

    while (gpio_muxsel_owner != GPIO_MUXSEL_FREE)
    {
        if (TIME_AFTER(current_tick, tmo))
        {
            owner_save = gpio_muxsel_owner;
            gpio_muxsel_owner = GPIO_MUXSEL_FREE;
            panicf("atj213x_gpio_mux_lock(%d) timeout, owner: %d",
                   module, owner_save);
        }
        yield();
    }
    gpio_muxsel_owner = module;
}

void atj213x_gpio_mux_unlock(int module)
{
    int owner_save;

    if (gpio_muxsel_owner != module)
    {
        owner_save = gpio_muxsel_owner;
        gpio_muxsel_owner = GPIO_MUXSEL_FREE;
        panicf("atj213x_gpio_mux_unlock(%d) not lock owner: %d",
               module, owner_save);
    }
    gpio_muxsel_owner = GPIO_MUXSEL_FREE;
}

static inline void check_gpio_port_valid(unsigned port)
{
    if (port > GPIO_PORTB)
        panicf("atj213x_gpio_setup() invalid port");
}

void atj213x_gpio_setup(unsigned port, unsigned pin, bool in)
{
    check_gpio_port_valid(port);

    volatile uint32_t  *inen=(port == GPIO_PORTA) ? &GPIO_AINEN : &GPIO_BINEN;
    volatile uint32_t *outen=(port == GPIO_PORTA) ? &GPIO_AOUTEN : &GPIO_BOUTEN;

    if (in)
    {
        *outen &= ~(1<<pin);
        *inen |= (1<<pin);
    }
    else
    {
        *inen &= ~(1<<pin);
        *outen |= (1<<pin);
    }
}

void atj213x_gpio_set(unsigned port, unsigned pin, bool val)
{
    check_gpio_port_valid(port);

    volatile uint32_t *dat=(port == GPIO_PORTA) ? &GPIO_ADAT : &GPIO_BDAT;

    if (val)
        *dat |= (1<<pin);
    else
        *dat &= ~(1<<pin);
}

bool atj213x_gpio_get(unsigned port, unsigned pin)
{
    check_gpio_port_valid(port);

    volatile uint32_t *dat=(port == GPIO_PORTA) ? &GPIO_ADAT : &GPIO_BDAT;
    return (*dat & (1<<pin)) ? true : false;
}
