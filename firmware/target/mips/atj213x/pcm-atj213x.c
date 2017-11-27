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

static void playback_cb(void);

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
    dma_stop(DMA_CH_PLAYBACK);
    locked = 1;
}

struct dst_t {
    int16_t reserved_l;
    int16_t l;
    int16_t reserved_r;
    int16_t r;
};

/* Rockbox uses packed S16 samples while ATJ DAC fifo expects S32 samples
 * (well actually it discards 16 least significant bits).
 * So we use hack here to convert rockbox samples and  samples suitable
 * for DMA transfer to ATJ DAC fifo. Ultimately support for S32 samples
 * should be added to rockbox.
 */
static void samples_convert(struct dst_t *dst, uint32_t *src, size_t size)
{
    uint32_t *end = (uint32_t *)((char *)src + size);
    while (src < end)
    {
        dst->r = *src;
        dst->l = *src >> 16;
        dst++;
        src++;
    }
}

static const void *orig_addr = NULL;
static void pcm_play_dma_run(const void *addr, size_t size)
{
    /* local sample buffer in format suitable for DMA transfer */
    static struct dst_t samples[256*2];

    /* DMA control struct */
    static struct dma_hwinfo_t hwinfo;

    hwinfo.mode = BF_DMAC_DMA_MODE_DBURLEN_V(SINGLE)   |
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
                  BF_DMAC_DMA_MODE_STRG_V(SDRAM)       |
                  BF_DMAC_DMA_MODE_STRANWID_V(WIDTH32) |
                  BF_DMAC_DMA_MODE_SFXS(0);

    hwinfo.dst = PHYSADDR((uint32_t)&DAC_DAT);
    hwinfo.src = PHYSADDR((uint32_t)&samples);

    /* account for the fact that converted samples
     * takes twice as much space in memory
     */
    hwinfo.cnt = size*2;

    samples_convert(samples, (uint32_t *)addr, size);

    /* needed for pcm_play_dma_get_peak_buffer() */
    orig_addr = addr;

    /* kick in DMA transfer */
    dma_setup(DMA_CH_PLAYBACK, &hwinfo, playback_cb);
    dma_start(DMA_CH_PLAYBACK);
}

void playback_cb(void)
{
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
    /* DACPLL, DACCLK */
    static const unsigned int pcm_freq_params[HW_NUM_FREQ][2] = 
    {
        [HW_FREQ_96] = {0, 0},
        [HW_FREQ_48] = {0, 1},
        [HW_FREQ_44] = {1, 1},
        [HW_FREQ_32] = {0, 2},
        [HW_FREQ_24] = {0, 3},
        [HW_FREQ_22] = {1, 3},
        [HW_FREQ_16] = {0, 4},
        [HW_FREQ_12] = {0, 5},
        [HW_FREQ_11] = {1, 5},
        [HW_FREQ_8]  = {0, 6}
    };

    CMU_AUDIOPLL = BF_CMU_AUDIOPLL_APEN(1) |
                   BF_CMU_AUDIOPLL_DACPLL(pcm_freq_params[freq][0]) |
                   BF_CMU_AUDIOPLL_DACCLK(pcm_freq_params[freq][1]);
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

    *count = rem/2;
    restore_interrupt(old);

    /* Warning!
     * Currently we use hack and convert packed S16 samples as provided
     * by rockbox into format suitable for DMA transfer to DAC.
     * We use here address of original pcm buffer and calculate index inside
     * this buffer based on DMA state. Since DMA sample is twice as big
     * as packed S16 we need to adjust offset.
     */
    return (void*)(orig_addr + (cnt - rem)/2);
}

/****************************************************************************
 ** Recording DMA transfer
 **/
/* TODO */
