/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2015 by Marcin Bukat
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

#include "semaphore.h"
#include "panic.h"
#include "dma-atj213x.h"
#include "regs/regs-dmac.h"

static volatile struct ll_dma_ctl_t dma_ll[8] = {
    {NULL, NULL, NULL}, {NULL, NULL, NULL},
    {NULL, NULL, NULL}, {NULL, NULL, NULL},
    {NULL, NULL, NULL}, {NULL, NULL, NULL},
    {NULL, NULL, NULL}, {NULL, NULL, NULL}
};

void dma_setup(unsigned int chan, struct dma_hwinfo_t *dma_hwinfo)
{
    DMAC_DMA_SRC(chan) = dma_hwinfo->src;
    DMAC_DMA_DST(chan) = dma_hwinfo->dst;
    DMAC_DMA_MODE(chan) = dma_hwinfo->mode;
    DMAC_DMA_CNT(chan) = dma_hwinfo->cnt;
}

void dma_start(unsigned int chan)
{
    DMAC_DMA_CMD(chan) = 1;
}

bool dma_wait_complete(unsigned int chan, unsigned tmo)
{
    tmo += current_tick;
    while (!TIME_AFTER(current_tick, tmo))
    {
        if (DMAC_DMA_CMD(chan) & 1)
            yield();
        else
            return true;
    }
    return false;
}

void dma_reset(unsigned int chan)
{
    DMAC_CTL |= (1<<(16+chan));
}

void dma_tcirq_ack(unsigned int chan)
{
    DMAC_IRQPD |= (3<<(chan));
}

void dma_tcirq_disable(unsigned int chan)
{
    DMAC_IRQEN &= ~(1<<chan*2);
}

void dma_tcirq_enable(unsigned int chan)
{
    DMAC_IRQEN |= (1<<chan*2);
}

static unsigned int dma_find_irqpd_chan(void)
{
    unsigned int irqpd = DMAC_IRQPD;
    unsigned int chan = 7;
    unsigned int mask = (1<<14);

    while (mask)
    {
        if (irqpd & mask)
            return chan;

        mask >>= 2;
        chan--;
    }

    return 0xffffffff;
}

void ll_dma_setup(unsigned int chan, struct ll_dma_t *ll,
                  void (*cb)(struct ll_dma_t *), struct semaphore *s)
{
    dma_ll[chan].ll = ll;
    dma_ll[chan].callback = cb;
    dma_ll[chan].semaphore = s;
}

void ll_dma_stop(unsigned int chan)
{
    dma_tcirq_disable(chan);
    dma_ll[chan].ll = NULL;
    dma_ll[chan].callback = NULL;
    dma_ll[chan].semaphore = NULL;
    dma_reset(chan);
}

void ll_dma_start(unsigned int chan)
{
    dma_reset(chan);
    dma_setup(chan, &dma_ll[chan].ll->hwinfo);

    if (dma_ll[chan].callback)
        (*dma_ll[chan].callback)(dma_ll[chan].ll);

    commit_discard_dcache();
    dma_ll[chan].ll = dma_ll[chan].ll->next;
    dma_tcirq_enable(chan);
    dma_start(chan);
}

void INT_DMA(void)
{
    unsigned int chan;

    /* find channel which caused interrupt */
    chan = dma_find_irqpd_chan();

    /* if something is THAT broken to report irq from nonexisting
     * dma channel... */
    if (UNLIKELY(chan > 7))
        panicf("Non existent DMA channel: %d", chan);

    /* linked transfer */
    if (dma_ll[chan].ll)
    {
        dma_setup(chan, &dma_ll[chan].ll->hwinfo);

        if (dma_ll[chan].callback)
            (*dma_ll[chan].callback)(dma_ll[chan].ll);

        commit_discard_dcache();
        dma_ll[chan].ll = dma_ll[chan].ll->next;
        dma_tcirq_ack(chan);
        dma_start(chan);
    }
    else
    {
        /* terminal node */
        if (dma_ll[chan].semaphore)
            semaphore_release(dma_ll[chan].semaphore);

        dma_tcirq_ack(chan);
    }
}
