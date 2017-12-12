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

#include "intc-atj213x.h"
#include "dma-atj213x.h"
#include "regs/regs-dmac.h"
#include "regs/regs-intc.h"

static volatile struct dma_ctl_t dma_ctl[8] = {
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL}
};

static void dma_hw_setup(unsigned int chan, struct dma_hwinfo_t *dma_hwinfo)
{
    DMAC_DMA_SRC(chan) = dma_hwinfo->src;
    DMAC_DMA_DST(chan) = dma_hwinfo->dst;
    DMAC_DMA_MODE(chan) = dma_hwinfo->mode;
    DMAC_DMA_CNT(chan) = dma_hwinfo->cnt;
}

static void dma_hw_start(unsigned int chan)
{
    /* DMA kick in */
    DMAC_DMA_CMD(chan) = 1;
}

static void dma_hw_reset(unsigned int chan)
{
    /* Reset DMA channel */
    DMAC_CTL |= (1 << (16 + chan));
}

static void dma_hw_tcirq_ack(unsigned int chan)
{
    /* Acknowledge DMA transfer complete irq */
    DMAC_IRQPD |= (3 << (chan));
}

void dma_tcirq_disable(unsigned int chan)
{
    /* Disable DMA transfer complete irq */
    DMAC_IRQEN &= ~(1 << (chan*2));
}

void dma_tcirq_enable(unsigned int chan)
{
    /* Enable DMA transfer complete irq */
    DMAC_IRQEN |= (1 << (chan*2));
}

void dma_setup(unsigned int chan, struct dma_hwinfo_t *hwinfo,
               void (*cb)(void))
{
    dma_ctl[chan].hwinfo = hwinfo;
    dma_ctl[chan].callback = cb;
}

void dma_start(unsigned int chan)
{
    /* reset DMA channel to kill any pending transfer */
    dma_hw_reset(chan);

    /* setup dst, src, cnt and mode of the DMA channel */
    dma_hw_setup(chan, dma_ctl[chan].hwinfo);

    /* cache coherency */
    commit_discard_dcache();

    /* unmask DMA channel transfer complete interrupt */
    dma_tcirq_enable(chan);

    /* enable DMA irq in INTC */
    atj213x_intc_unmask(BP_INTC_MSK_DMA);

    /* DMA kick in */
    dma_hw_start(chan);
}

void dma_stop(unsigned int chan)
{
    dma_tcirq_disable(chan);
    dma_hw_tcirq_ack(chan);

    dma_ctl[chan].hwinfo = NULL;
    dma_ctl[chan].callback = NULL;

    dma_hw_reset(chan);
}

void dma_pause(unsigned int chan, bool pause)
{
    if (pause)
    {
        DMAC_DMA_CMD(chan) |= 2;
    }
    else
    {
        DMAC_DMA_CMD(chan) &= ~2;
    }
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

void INT_DMA(void)
{
    unsigned int irqpd = DMAC_IRQPD;
    unsigned int chan = 0;

    /* Loop through all pending dma interrupts */
    while (irqpd)
    {
        /* this channel has pending interrupt */
        if (irqpd & 1)
        {
            /* ack this interrupt */
            dma_hw_tcirq_ack(chan);

            /* execute registered callback if any */
            if (dma_ctl[chan].callback)
            {
                dma_ctl[chan].callback();
            }
        }

        /* advance to next dma channel */
        irqpd >>= 2;
        chan++;
    }
}
