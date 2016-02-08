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
 
#include "kernel.h"
#include "panic.h"
#include "gpio-atj213x.h"
#include "lcm-atj213x.h"
#include "regs/regs-yuv2rgb.h"
#include "regs/regs-cmu.h"

void lcm_wait_fifo_empty()
{
    unsigned long timeout = current_tick + HZ/10;
    while(!(YUV2RGB_CTL & 0x04))
    {
        if (TIME_AFTER(current_tick, timeout))
            panicf("atj213x_lcm_wait_fifo_empty() timeout");
    }
}

void lcm_rs_command(void)
{
    lcm_wait_fifo_empty();

    YUV2RGB_CTL = 0x802ae;
}

void lcm_rs_data(void)
{
    lcm_wait_fifo_empty();

    YUV2RGB_CTL = 0x902ae;
}

void lcm_fb_data(void)
{
    lcm_rs_command();
    YUV2RGB_FIFODATA = 0x22; /* write GRAM command */
    YUV2RGB_CTL = 0xa02ae; /* AHB bus write fifo */
}

void lcm_init(void)
{
    CMU_DEVCLKEN |= (1<<8)|(1<<1); /* dma clk, lcm clk */
    udelay(10);

    lcm_rs_command();   /* this has side effect of enabling whole block */
    YUV2RGB_CLKCTL = 0x102; /* lcm clock divider */
}
