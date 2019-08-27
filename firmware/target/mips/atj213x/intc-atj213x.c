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
#include "intc-atj213x.h"
#include "regs/regs-intc.h"

void atj213x_intc_set_prio(unsigned int irqno, unsigned int prio)
{
    if (prio & 1)
    {
        INTC_CFGx(0) |= (1<<irqno);
    }

    if (prio & 2)
    {
        INTC_CFGx(1) |= (1<<irqno);
    }

    if (prio & 4)
    {
        INTC_CFGx(2) |= (1<<irqno);
    }
}

void atj213x_intc_unmask(unsigned int irqno)
{
    INTC_MSK |= (1<<irqno);
}

void atj213x_intc_mask(unsigned int irqno)
{
    INTC_MSK &= ~(1<<irqno);
}
