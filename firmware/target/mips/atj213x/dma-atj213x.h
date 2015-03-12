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

#ifndef DMA_ATJ213X_H
#define DMA_ATJ213X_H

#include "kernel.h"

/* 20 bits so 1 Mb - 1 */
#define DMA_MAX_XFER_SIZE 0xfffff

/* Static dma channels allocation
 * channels 0-3 use regular AHB bus
 * channels 4-7 are special DMA bus
 */
#define DMA_CH_SD 1

/* data to setup DMAC hardware */
struct dma_hwinfo_t {
    uint32_t src;
    uint32_t dst;
    uint32_t mode;
    uint32_t cnt;     /* 20bits actually */
};

/* linked list dma transfer node */
struct ll_dma_t {
    struct dma_hwinfo_t hwinfo;
    struct ll_dma_t *next;
};

struct ll_dma_ctl_t {
    struct ll_dma_t *ll;
    void (*callback)(struct ll_dma_t *);
    struct semaphore *semaphore;
};

void dma_setup(unsigned int chan, struct dma_hwinfo_t *dma_hwinfo);
void dma_start(unsigned int chan);
bool dma_wait_complete(unsigned int chan, unsigned tmo);
void dma_reset(unsigned int chan);
void dma_tcirq_ack(unsigned int chan);
void dma_tcirq_enable(unsigned int chan);
void dma_tcirq_disable(unsigned int chan);
void ll_dma_setup(unsigned int chan, struct ll_dma_t *ll,
                  void (*cb)(struct ll_dma_t *), struct semaphore *s);
void ll_dma_stop(unsigned int chan);
void ll_dma_start(unsigned int chan);

#endif /* DMA_ATJ213X_H */
