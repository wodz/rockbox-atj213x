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

#include <stdint.h>
#include "panic.h"
#include "gpio-atj213x.h"
#include "lradc-atj213x.h"
#include "regs/regs-pmu.h"

void lradc_init(void)
{
    PMU_CTL |= BF_PMU_CTL_LA6E(1)|BF_PMU_CTL_LA4E(1);
}

unsigned int lradc_read(unsigned channel)
{
    switch (channel)
    {
        case LRADC_CH_KEY:
            return (PMU_LRADC & BM_PMU_LRADC_REMOADC4)>>BP_PMU_LRADC_REMOADC4;
            break;

        case LRADC_CH_BAT:
            return (PMU_LRADC & BM_PMU_LRADC_BATADC6)>>BP_PMU_LRADC_BATADC6;
            break;

        case LRADC_CH_TEMP:
            return (PMU_LRADC & BM_PMU_LRADC_TEMPADC6)>>BP_PMU_LRADC_TEMPADC6;
            break;

        default:
            panicf("lradc_read() invalid channel");
    }

    return -1;
}
