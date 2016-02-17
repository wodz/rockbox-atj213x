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

#include "system-target.h" 
#include "cmu-atj213x.h"
#include "regs/regs-cmu.h"

void atj213x_clk_enable(unsigned int blockno)
{
    CMU_DEVCLKEN |= (1<<blockno);
    udelay(1);
}

void atj213x_clk_disable(unsigned int blockno)
{
    CMU_DEVCLKEN &= ~(1<<blockno);
    udelay(1);
}

void atj213x_block_reset(unsigned int blockno)
{
    CMU_DEVRST &= ~(1<<blockno);
    udelay(10);
    CMU_DEVRST |= (1<<blockno);
    udelay(10);
}
