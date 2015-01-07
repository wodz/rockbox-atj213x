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
#include "adc.h"
#include "adc-target.h"
#include "powermgmt.h"

const unsigned short battery_level_dangerous[BATTERY_TYPES_COUNT] =
{
    /* TODO: this is just an initial guess */
    3400
};

const unsigned short battery_level_shutoff[BATTERY_TYPES_COUNT] =
{
    /* TODO: this is just an initial guess */
    3300
};

/* voltages (millivolt) of 0%, 10%, ... 100% when charging disabled */
const unsigned short percent_to_volt_discharge[BATTERY_TYPES_COUNT][11] =
{
    /* TODO: simple uncalibrated curve, linear except for first 10% */
    { 3300, 3600, 3665, 3730, 3795, 3860, 3925, 3990, 4055, 4120, 4185 }
};

/* voltages (millivolt) of 0%, 10%, ... 100% when charging enabled */
const unsigned short percent_to_volt_charge[11] =
    /* TODO: simple uncalibrated curve, linear except for first 10% */
    { 3300, 3600, 3665, 3730, 3795, 3860, 3925, 3990, 4055, 4120, 4185 };

/* Returns battery voltage from ADC [millivolts] */
/* according to datasheet 6bit adc value scales to 2.1-4.5V range */
int _battery_voltage(void)
{
    return ((lradc_read(LRADC_CH_BAT)*38 + 2100);
}

