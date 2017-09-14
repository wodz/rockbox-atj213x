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
#ifndef BUTTON_TARGET_H
#define BUTTON_TARGET_H

#define HAS_BUTTON_HOLD

/* Button 4bit ADC threshold */
#define BTN_ADC_RELEASE    0x0f
#define BTN_ADC_RIGHT      0x0c
#define BTN_ADC_DOWN       0x08
#define BTN_ADC_SELECT     0x06
#define BTN_ADC_LEFT       0x04
#define BTN_ADC_UP         0x02

/* Button GPIO pins */
#define BTN_HOLD_PORT      GPIO_PORTA
#define BTN_HOLD_PIN       10

#define BTN_HDPH_PORT      GPIO_PORTA
#define BTN_HDPH_PIN       26

#define BTN_POWER_PORT     GPIO_PORTA
#define BTN_POWER_PIN      8

#define BTN_VOLUP_PORT     GPIO_PORTA
#define BTN_VOLUP_PIN      12

#define BTN_VOLDOWN_PORT   GPIO_PORTB
#define BTN_VOLDOWN_PIN    31

/* Main unit's button codes*/
#define BUTTON_POWER       (1<<0)
#define BUTTON_LEFT        (1<<1)
#define BUTTON_RIGHT       (1<<2)
#define BUTTON_UP          (1<<3)
#define BUTTON_DOWN        (1<<4)
#define BUTTON_SELECT      (1<<5)
#define BUTTON_VOLUP       (1<<6)
#define BUTTON_VOLDOWN     (1<<7)


#define BUTTON_MAIN (BUTTON_POWER|BUTTON_LEFT|BUTTON_RIGHT|\
                     BUTTON_UP|BUTTON_DOWN|BUTTON_SELECT|\
                     BUTTON_VOLUP|BUTTONVOL_DOWN)

#define POWEROFF_BUTTON BUTTON_POWER
#define POWEROFF_COUNT 30

bool headphones_inserted(void);
bool button_hold(void);
#endif /* _BUTTON_TARGET_H */
