/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2016 Marcin Bukat
 * Copyright (C) 2009 by Michael Sevakis
 * Copyright (C) 2008 by Bertrik Sikken
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

#include <stdint.h>
#include "config.h"
#include "powermgmt.h"

#include "lradc-atj213x.h"
#include "pmu-atj213x.h"

/* Parameters defined in powermgmt-target.h
 *
 * BATT_FULL_VOLTAGE -   Upon connect a charge cycle begins if the reading is
 *                       lower than this value (millivolts).
 *
 * BATT_VAUTO_RECHARGE - While left plugged after cycle completion, the
 *                       charger restarts automatically if the reading drops
 *                       below this value (millivolts). Must be less than
 *                       BATT_FULL_VOLTAGE.
 *
 * BATT_CHG_I -          Charger current regulation setting
 *
 * CHARGER_TOTAL_TIMER - Maximum allowed charging time (1/2-second steps)
 *
 * CV phase is fixed to 4.2V on this platform
 */

/* Timeout in algorithm steps */
static int charger_total_timer = 0;

/* Current battery threshold for (re)charge:
 * First plugged = BATT_FULL_VOLTAGE
 * After charge cycle or non-start = BATT_VAUTO_RECHARGE
 */
static unsigned int batt_threshold = 0;

/* Returns true if the unit is charging the batteries. */
bool charging_state(void)
{
    return (atj213x_charger_state() & 1) ? true : false;
}

/* Disable charger and minimize all settings. Reset timers, etc. */
static void disable_charger(bool error)
{
    unsigned tmo = current_tick + HZ/5;

    /* Disable charging hardware */
    atj213x_charger_disable();

    /* Wait for change to propagate */
    while (atj213x_charger_state() != 0)
    {
        if (TIME_AFTER(current_tick, tmo))
        {
            panicf("%s() state change tmo", __func__);
        }
    }

    if (error)
    {
        /* Error condition */
        charge_state = CHARGE_STATE_ERROR;
    }
    else
    {
        /* Not an error state already */
        if (charge_state > DISCHARGING)
        {
            charge_state = DISCHARGING;
        }
    }

    /* Reset charger timer */
    charger_total_timer = 0;
}

/* Enable charger with specified settings. Start timers, etc. */
static void enable_charger(void)
{
    unsigned tmo = current_tick + HZ/5;

    /* Set max charging current according to BATT_CHG_I in powermgmt-target.h */
    atj213x_charger_set_current(BATT_CHG_I);

    /* Enable charger circuit */
    atj213x_charger_enable();

    /* Allow charger turn-on time */
    while (atj213x_charger_state() == 0)
    {
        if (TIME_AFTER(current_tick, tmo))
        {
            panicf("%s() state change tmo", __func__);
        }
    }

    charge_state = CHARGING;

    /* Set charging timeout */
    charger_total_timer = CHARGER_TOTAL_TIMER;
}

void powermgmt_init_target(void)
{
    /* Start with disabled charger hardware */
    disable_charger(false);
}

static inline void charger_control(void)
{
    switch (charge_state)
    {
        case DISCHARGING:
        {
            unsigned int millivolts = battery_voltage();

            if (millivolts <= batt_threshold)
            {
                enable_charger();
            }

            break;
        } /* DISCHARGING: */

        case CHARGING:
        {
            if (PMU_CHG & BF_PMU_CHG_STAT_V(CHARGING))
            {
                if (--charger_total_timer > 0)
                {
                    break;
                }

                /* Timer ran out - require replug. */
                disable_charger(true);
            }
            else /* else end of charge */
            {
                disable_charger(false);

                if (batt_threshold == BATT_FULL_VOLTAGE)
                {
                    batt_threshold = BATT_VAUTO_RECHARGE;
                }
            }

            break;
        } /* CHARGING: */

        default:
        {
            /* DISABLED, ERROR */
            break;
        }
    }
}

static inline void charger_plugged(void)
{
    /* Start with max cutout */
    batt_threshold = BATT_FULL_VOLTAGE;
}

static inline void charger_unplugged(void)
{
    /* Reset error */
    if (charge_state >= CHARGE_STATE_ERROR)
        charge_state = DISCHARGING;
}

/* Main charging algorithm - called from powermgmt.c */
void charging_algorithm_step(void)
{
    switch (charger_input_state)
    {
        case NO_CHARGER:
            /* Nothing to do */
            break;

        case CHARGER_PLUGGED:
            charger_plugged();
            break;

        case CHARGER:
            charger_control();
            break;

        case CHARGER_UNPLUGGED:
            charger_unplugged();
            break;
    }
}

void charging_algorithm_close(void)
{
    disable_charger(false);
}

/* Returns battery voltage from ADC [millivolts]
 * according to datasheet 6bit adc value scales to 2.1-4.5V range
 */
int _battery_voltage(void)
{
    return ((lradc_read(LRADC_CH_BAT)*38 + 2100));
}
