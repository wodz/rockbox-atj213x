/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2016 Marcin Bukat
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
#include "kernel.h"
#include "audio.h"
#include "string.h"
#include "panic.h"
#include "audiohw.h"
#include "sound.h"
#include "pcm-internal.h"
#include "dma-atj213x.h"

#include "regs/regs-dac.h"
#include "regs/regs-cmu.h"

static void playback_callback(struct ll_dma_t *ll);

static int locked = 0;

/* Mask the DMA interrupt */
void pcm_play_lock(void)
{
    if (++locked == 1)
    {
        int old = disable_irq_save();
        /* mask Transfer complete IRQ */
        dma_tcirq_disable(DMA_CH_PLAYBACK);
        restore_irq(old);
    }
}

/* Unmask the DMA interrupt if enabled */
void pcm_play_unlock(void)
{
    if(--locked == 0)
    {
        int old = disable_irq_save();
        /* unmask Transfer complete IRQ */
        dma_tcirq_enable(DMA_CH_PLAYBACK);
        restore_irq(old);
    }
}

void pcm_play_dma_stop(void)
{
    ll_dma_stop(DMA_CH_PLAYBACK);
    locked = 1;
}

static void pcm_play_dma_run(const void *addr, size_t size)
{
    static struct ll_dma_t playback_ll;

    unsigned int mode = BF_DMAC_DMA_MODE_DBURLEN_V(SINGLE)   |
                        BF_DMAC_DMA_MODE_RELO(0)             |
                        BF_DMAC_DMA_MODE_DDSP(0)             |
                        BF_DMAC_DMA_MODE_DCOL(0)             |
                        BF_DMAC_DMA_MODE_DDIR(0)             |
                        BF_DMAC_DMA_MODE_DFXA_V(FIXED)       |
                        BF_DMAC_DMA_MODE_DTRG_V(DAC)         |
                        BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH32) |
                        BF_DMAC_DMA_MODE_DFXS(0)             |
                        BF_DMAC_DMA_MODE_SBURLEN_V(SINGLE)   |
                        BF_DMAC_DMA_MODE_SDSP(0)             |
                        BF_DMAC_DMA_MODE_SCOL(0)             |
                        BF_DMAC_DMA_MODE_SDIR_V(INCREASE)    |
                        BF_DMAC_DMA_MODE_SFXA_V(NOT_FIXED)   |
                        BF_DMAC_DMA_MODE_STRANWID_V(WIDTH32) |
                        BF_DMAC_DMA_MODE_SFXS(0);

    if (iram_address((void *)addr))
    {
        mode |= BF_DMAC_DMA_MODE_STRG_V(IRAM);
    }
    else
    {
        mode |= BF_DMAC_DMA_MODE_STRG_V(SDRAM);
    }

    playback_ll.hwinfo.dst = PHYSADDR((uint32_t)&DAC_DAT);
    playback_ll.hwinfo.src = PHYSADDR((uint32_t)addr);
    playback_ll.hwinfo.mode = mode;
    playback_ll.hwinfo.cnt = size;
    playback_ll.next = NULL;

    /* kick in DMA transfer */
    ll_dma_setup(DMA_CH_PLAYBACK, &playback_ll, playback_callback, NULL);
    ll_dma_start(DMA_CH_PLAYBACK);
}

void playback_callback(struct ll_dma_t *ll)
{
    (void)ll;
    const void *start;
    size_t size;

    if (pcm_play_dma_complete_callback(PCM_DMAST_OK, &start, &size))
    {
        pcm_play_dma_run(start, size);
        pcm_play_dma_status_callback(PCM_DMAST_STARTED);
    }
}

void pcm_play_dma_start(const void *addr, size_t size)
{

    /* Stop any DMA in progress */
    pcm_play_dma_stop();

    /* Initiate DMA transfer */
    pcm_play_dma_run(addr, size);
}

void pcm_play_dma_pause(bool pause)
{
    dma_pause(DMA_CH_PLAYBACK, pause);
    locked = pause ? 1 : 0;
}

static void set_codec_freq(unsigned int freq)
{
    /* 24Mhz base clock */
    static const unsigned int pcm_freq_params[HW_NUM_FREQ] = 
    {
        [HW_FREQ_96] = 0,
        [HW_FREQ_48] = 1,
        [HW_FREQ_32] = 2,
        [HW_FREQ_24] = 3,
        [HW_FREQ_16] = 4,
        [HW_FREQ_12] = 5,
        [HW_FREQ_8]  = 6,
    };

    CMU_AUDIOPLL = BF_CMU_AUDIOPLL_APEN(1) |
                   BF_CMU_AUDIOPLL_DACPLL(0) |
                   BF_CMU_AUDIOPLL_DACCLK(pcm_freq_params[freq]);
}

void pcm_play_dma_init(void)
{
    audiohw_preinit();
}

void pcm_play_dma_postinit(void)
{
    audiohw_postinit();
}

void pcm_dma_apply_settings(void)
{
    set_codec_freq(pcm_fsel);
    audiohw_set_frequency(pcm_fsel);
}

size_t pcm_get_bytes_waiting(void)
{
    return dma_get_remining(DMA_CH_PLAYBACK);
}

const void * pcm_play_dma_get_peak_buffer(int *count)
{
    int old = disable_irq_save();
    uint32_t cnt = dma_get_cnt(DMA_CH_PLAYBACK);
    uint32_t rem = dma_get_remining(DMA_CH_PLAYBACK);
    uint32_t src = dma_get_src(DMA_CH_PLAYBACK);

    *count = rem;
    restore_interrupt(old);

    return (void*)(src + cnt - rem);
}

/****************************************************************************
 ** Recording DMA transfer
 **/
/* TODO */
