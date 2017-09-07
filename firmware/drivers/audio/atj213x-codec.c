/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Driver for internal Actions ATJ213x audio codec
 *
 * Copyright (c) 2017 Marcin Bukat
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
#include "audio.h"
#include "audiohw.h"
#include "system.h"

static void audiohw_mute(bool mute)
{
    if (mute)
    {
        DAC_ANALOG &= ~BF_DAC_ANALOG_PBM(1)
    }
    else
    {
        DAC_ANALOG |= BF_DAC_ANALOG_PBM(1)
    }
}

/* public functions */
static int vol_tenthdb2hw(int tdb)
{
    /* codec has 31 steps of -1.8 dB
     * max attenuation is -55.8 dB */
    return -tdb/18;
}

void audiohw_preinit(void)
{
    /* ungate clock to DAC block */
    atj213x_clk_enable(BP_CMU_DEVCLKEN_DAC);
}

void audiohw_postinit(void)
{
    audiohw_mute(false);
}

void audiohw_close(void)
{
    /* stub */
}

/* mono volume control */
void audiohw_set_volume(int vol)
{
    vol = vol_tenthdb2hw(vol);

    if (vol > 30)
    {
        audiohw_mute(true);
    }
    else
    {
        audiohw_mute(false);
    }

    DAC_ANALOG = (DAC_ANALOG & ~BM_DAC_ANALOG_HAVC) | BF_DAC_ANALOG_HAVC(vol);
}

void audiohw_set_monitor(bool enable)
{
    (void)enable;
}
