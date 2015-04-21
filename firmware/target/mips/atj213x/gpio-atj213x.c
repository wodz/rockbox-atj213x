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

/* returns previous 'saved' gpio muxsel value
 * this way you can switch back if needed
 */
enum gpio_mux_t atj213x_gpio_muxsel(enum gpio_mux_t module)
{
    static enum gpio_mux_t gpio_muxsel = GPIO_MUXSEL_FREE;
    enum gpio_mux_t gpio_muxsel_save = gpio_muxsel;

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

        case GPIO_MUXSEL_FREE:
            return gpio_muxsel_save;

        default:
            panicf("atj213x_gpio_muxsel() wrong module");
    }

    /* enable multifunction mux */
    GPIO_MFCTL1 = (1<<31);

    /* write multifunction mux selection */
    GPIO_MFCTL0 = mfctl0;

    gpio_muxsel = module;
    return gpio_muxsel_save;
}

static inline void check_gpio_port_valid(enum gpio_port_t port)
{
    if (port > GPIO_PORTB)
        panicf("atj213x_gpio_setup() invalid port");
}

void atj213x_gpio_setup(enum gpio_port_t port, unsigned pin, bool in)
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

void atj213x_gpio_set(enum gpio_port_t port, unsigned pin, bool val)
{
    check_gpio_port_valid(port);

    volatile uint32_t *dat=(port == GPIO_PORTA) ? &GPIO_ADAT : &GPIO_BDAT;

    if (val)
        *dat |= (1<<pin);
    else
        *dat &= ~(1<<pin);
}

bool atj213x_gpio_get(enum gpio_port_t port, unsigned pin)
{
    check_gpio_port_valid(port);

    volatile uint32_t *dat=(port == GPIO_PORTA) ? &GPIO_ADAT : &GPIO_BDAT;
    return (*dat & (1<<pin)) ? true : false;
}
