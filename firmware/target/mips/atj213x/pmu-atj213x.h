/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2016 by Marcin Bukat
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
#ifndef PMU_ATJ213X_H
#define PMU_ATJ213X_H
 
#include "pmu-atj213x.h"
#include "regs/regs-pmu.h"

enum pmu_charge_current_t {
    CHARGE_CURRENT_50MA,
    CHARGE_CURRENT_100MA,
    CHARGE_CURRENT_150MA,
    CHARGE_CURRENT_200MA,
    CHARGE_CURRENT_250MA,
    CHARGE_CURRENT_300MA,
    CHARGE_CURRENT_400MA,
    CHARGE_CURRENT_500MA,
};

void atj213x_charger_enable(void);
void atj213x_charger_disable(void);
void atj213x_charger_set_current(enum pmu_charge_current_t current);
unsigned int atj213x_charger_state(void);

#endif /* PMU_ATJ213X_H */
