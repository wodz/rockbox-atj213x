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
 
#include "system.h"
#include "intc-atj213x.h"
#include "tmr-atj213x.h"
#include "regs/regs-intc.h"
#include "regs/regs-rtcwdt.h"
#include "regs/regs-cmu.h"

static void (*timer_fns[])(void) = {NULL, NULL};

#define timer_irq_handler(nr) \
void INT_T##nr(void) \
{ \
    if(timer_fns[nr]) \
        timer_fns[nr](); \
    atj213x_timer_irq_clear(nr); \
}

timer_irq_handler(0)
timer_irq_handler(1)

void atj213x_timer_irq_clear(unsigned timer_nr)
{
    /* clear pending irq flag in timer module */
    while (RTCWDT_TxCTL(timer_nr) & 1)
        RTCWDT_TxCTL(timer_nr) |= 1;
}

void atj213x_timer_set(unsigned timer_nr, unsigned interval_ms, void (*cb)(void))
{
    uint32_t corepllfreq = (CMU_COREPLL & 0x3f) * 6000000;
    uint32_t cclkdiv = ((CMU_BUSCLK >> 2) & 0x03) + 1;
    uint32_t sclkdiv = ((CMU_BUSCLK >> 4) & 0x03) + 1;
    uint32_t pclkdiv = ((CMU_BUSCLK >> 8) & 0x0f);
    pclkdiv = pclkdiv ? (pclkdiv + 1) : 2;

    uint32_t pclkfreq = ((corepllfreq/cclkdiv)/sclkdiv)/pclkdiv;

    unsigned int old_irq = disable_irq_save();

    /* install callback */
    timer_fns[timer_nr] = cb;

    /* timer disable, timer reload, timer irq, clear irq pending bit */
    RTCWDT_TxCTL(timer_nr) = (1<<2) | (1<<1) | (1<<0);

    RTCWDT_Tx(timer_nr) = interval_ms*(pclkfreq/1000);

    restore_irq(old_irq);
}

void atj213x_timer_start(unsigned timer_nr)
{
    /* unmask interrupt in INTC */
    atj213x_intc_unmask(timer_nr ? BP_INTC_MSK_T1 : BP_INTC_MSK_T0);

    /* timer enable bit */
    RTCWDT_TxCTL(timer_nr) |= (1<<5);
}

void atj213x_timer_stop(unsigned timer_nr)
{
    /* mask interrupt in INTC */
    atj213x_intc_mask(timer_nr ? BP_INTC_MSK_T1 : BP_INTC_MSK_T0);

    /* clear enable bit */
    RTCWDT_TxCTL(timer_nr) &= ~(1<<5);
}
