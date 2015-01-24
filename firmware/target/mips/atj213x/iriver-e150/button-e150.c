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

#include "button.h"
#include "backlight.h"
#include "gpio-atj213x.h"
#include "lradc-atj213x.h"

bool headphones_inserted(void)
{
    return !atj213x_gpio_get(BTN_HDPH_PORT, BTN_HDPH_PIN);
}

bool button_hold(void)
{
    /* A10 active low */
    return !atj213x_gpio_get(BTN_HOLD_PORT, BTN_HOLD_PIN);
}

void button_init_device(void)
{
    /* A8 BUTTON_ON */
    atj213x_gpio_setup(BTN_ON_PORT, BTN_ON_PIN, GPIO_IN);

    /* A10 BUTTON_HOLD */
    atj213x_gpio_setup(BTN_HOLD_PORT, BTN_HOLD_PIN, GPIO_IN);

    /* A12 BUTTON_VOL_UP */
    atj213x_gpio_setup(BTN_VOL_UP_PORT, BTN_VOL_UP_PIN, GPIO_IN);

    /* A26 HEADPHONE_DETECT */
    atj213x_gpio_setup(BTN_HDPH_PORT, BTN_HDPH_PIN, GPIO_IN);

    /* B31 BUTTON_VOL_DOWN */
    atj213x_gpio_setup(BTN_VOL_DOWN_PORT, BTN_VOL_DOWN_PIN, GPIO_IN);

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
#ifndef BOOTLOADER
    bool hold_state_old = hold_state;
#endif

    hold_state = button_hold();

#ifndef BOOTLOADER
    if (hold_state_old != hold_state)
        backlight_hold_changed(hold_state);
#endif

    if (hold_state)
        return BUTTON_NONE;

    adc = lradc_read(LRADC_CH_KEY);
    if (adc < BTN_ADC_RELEASE)
    {
        if (adc < BTN_ADC_SELECT)
        {
            if (adc < BTN_ADC_LEFT)
                btn |= BUTTON_UP;
            else
                btn |= BUTTON_LEFT;
        }
        else
        {
            if (adc < BTN_ADC_DOWN)
                btn |= BUTTON_SELECT;
            else
            {
                if (adc < BTN_ADC_RIGHT)
                    btn |= BUTTON_DOWN;
                else
                    btn |= BUTTON_RIGHT;
            }
        }
    }

    if (!atj213x_gpio_get(BTN_ON_PORT, BTN_ON_PIN))
        btn |= BUTTON_ON;

    if (!atj213x_gpio_get(BTN_VOL_UP_PORT, BTN_VOL_UP_PIN))
        btn |= BUTTON_VOL_UP;

    if (atj213x_gpio_get(BTN_VOL_DOWN_PORT, BTN_VOL_DOWN_PIN))
        btn |= BUTTON_VOL_DOWN;

    return btn;
}
