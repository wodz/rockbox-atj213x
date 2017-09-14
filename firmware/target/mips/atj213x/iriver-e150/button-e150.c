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

#include "kernel.h"
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
    atj213x_gpio_setup(BTN_POWER_PORT, BTN_POWER_PIN, GPIO_IN);

    /* A10 BUTTON_HOLD */
    atj213x_gpio_setup(BTN_HOLD_PORT, BTN_HOLD_PIN, GPIO_IN);

    /* A12 BUTTON_VOL_UP */
    atj213x_gpio_setup(BTN_VOLUP_PORT, BTN_VOLUP_PIN, GPIO_IN);

    /* A26 HEADPHONE_DETECT */
    atj213x_gpio_setup(BTN_HDPH_PORT, BTN_HDPH_PIN, GPIO_IN);

    /* B31 BUTTON_VOL_DOWN */
    atj213x_gpio_setup(BTN_VOLDOWN_PORT, BTN_VOLDOWN_PIN, GPIO_IN);

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
    static int idx = 0;
    static unsigned int adc[4] = {
        BTN_ADC_RELEASE, BTN_ADC_RELEASE, BTN_ADC_RELEASE, BTN_ADC_RELEASE
    };

    unsigned int adc_filtered;
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

    adc[idx++ & 3] = lradc_read(LRADC_CH_KEY);

    /* Debounce by requiring 4 consecutive samples to be the same
     * sampling frequency is dictated by tick
     */
    if (!(((adc[0] ^ adc[1]) ^ adc[2]) ^ adc[3]))
        adc_filtered = adc[0];
    else
        adc_filtered = BTN_ADC_RELEASE;

    if (adc_filtered < BTN_ADC_RELEASE)
    {
        if (adc_filtered < BTN_ADC_SELECT)
        {
            if (adc_filtered < BTN_ADC_LEFT)
                btn |= BUTTON_UP;
            else
                btn |= BUTTON_LEFT;
        }
        else
        {
            if (adc_filtered < BTN_ADC_DOWN)
                btn |= BUTTON_SELECT;
            else
            {
                if (adc_filtered < BTN_ADC_RIGHT)
                    btn |= BUTTON_DOWN;
                else
                    btn |= BUTTON_RIGHT;
            }
        }
    }

    if (!atj213x_gpio_get(BTN_POWER_PORT, BTN_POWER_PIN))
        btn |= BUTTON_POWER;

    if (!atj213x_gpio_get(BTN_VOLUP_PORT, BTN_VOLUP_PIN))
        btn |= BUTTON_VOLUP;

    if (atj213x_gpio_get(BTN_VOLDOWN_PORT, BTN_VOLDOWN_PIN))
        btn |= BUTTON_VOLDOWN;

    return btn;
}
