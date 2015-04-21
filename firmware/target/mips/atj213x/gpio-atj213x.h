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

#ifndef GPIO_ATJ213X_H
#define GPIO_ATJ213X_H
 
enum gpio_mux_t {
    GPIO_MUXSEL_FREE = -1,
    GPIO_MUXSEL_LCM,
    GPIO_MUXSEL_SD,
    GPIO_MUXSEL_NAND,
};

enum gpio_port_t {
    GPIO_PORTA,
    GPIO_PORTB
};

enum gpio_dir_t {
    GPIO_OUT,
    GPIO_IN
};

enum gpio_mux_t atj213x_gpio_muxsel(enum gpio_mux_t module);
void atj213x_gpio_setup(enum gpio_port_t port, unsigned pin, bool in);
void atj213x_gpio_set(enum gpio_port_t port, unsigned pin, bool val);
bool atj213x_gpio_get(enum gpio_port_t port, unsigned pin);

#endif /* GPIO_ATJ213X_H */
