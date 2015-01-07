/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2015 Marcin Bukat
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
#include "cpu.h"
#include "system.h"
#include "button.h"

bool headphones_inserted(void)
{
    return !atj213x_gpio_get(GPIO_PORTA, 26);
}

bool button_hold(void)
{
    /* A10 active low */
    return !atj213x_gpio_get(GPIO_PORTA, 10);
}

void button_init_device(void)
{
    /* A8 BUTTON_ON */
    atj213x_gpio_setup(GPIO_PORTA, 8, GPIO_IN);

    /* A10 BUTTON_HOLD */
    atj213x_gpio_setup(GPIO_PORTA, 10, GPIO_IN);

    /* A12 BUTTON_VOL_UP */
    atj213x_gpio_setup(GPIO_PORTA, 12, GPIO_IN);

    /* A26 HEADPHONE_DETECT */
    atj213x_gpio_setup(GPIO_PORTA, 26, GPIO_IN);

    /* B31 BUTTON_VOL_DOWN */
    atj213x_gpio_setup(GPIO_PORTB, 31, GPIO_IN);

    /* BUTTON_LEFT, BUTTON_RIGHT, BUTTON_UP
     * BUTTON_DOWN, BUTTON_SELECT are sensed by 4bit ADC
     */
    lradc_init();
}

/*
 * Get button pressed from hardware
 */
int button_read_device(void)
{
    unsigned int adc;
    int btn = BUTTON_NONE;
    static bool hold_state = false;
    bool hold_state_old = hold_state;

    hold_state = button_hold();

    if (hold_state_old != hold_state)
        backlight_hold_changed(btn_hold);

    if (hold_state)
        return BUTTON_NONE;

    adc = lradc_read(LRADC_CH_KEY);
    if (adc < 0x0f)
    {
        if (adc < 0x06)
        {
            if (adc < 0x04)
                btn |= BUTTON_UP;
            else
                btn |= BUTTON_LEFT;
        }
        else
        {
            if (adc < 0x08)
                btn |= BUTTON_SELECT;
            else
            {
                if (adc < 0x0c)
                    btn |= BUTTON_DOWN;
                else
                    btn |= BUTTON_RIGHT;
            }
        }
    }

    if (!atj213x_gpio_get(GPIO_PORTA, 8)
        btn |= BUTTON_ON;

    if (!atj213x_gpio_get(GPIO_PORTA, 12)
        btn |= BUTTON_VOL_UP;

    if (atj213x_gpio_get(GPIO_PORTB, 31)
        btn |= BUTTON_VOL_DOWN;

    return btn;
}
