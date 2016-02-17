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

#include <string.h>
#include "system.h"
#include "semaphore.h"
#include "panic.h"
#include "sdlib.h"
#include "gpio-atj213x.h"
#include "dma-atj213x.h"
#include "cmu-atj213x.h"
#include "regs/regs-dmac.h"
#include "regs/regs-cmu.h"
#include "regs/regs-sd.h"
#include "regs/regs-gpio.h"

//#define ATJ213X_SD_DEBUG

/* 16 bits of SD_BYTECNT */
#define SD_MAX_XFER_SIZE 0xffff

static struct semaphore sd_semaphore;

void sdc_init(void)
{
    /* ungate SD and DMA blocks clock */
    atj213x_clk_enable(BP_CMU_DEVCLKEN_SD);
    atj213x_clk_enable(BP_CMU_DEVCLKEN_DMAC);

    /* Setup pad drive strength ?
     * Without this sd transfers kill lcd communication
     */
    GPIO_PADDRV |= 0x23;

    SD_CTL = BM_SD_CTL_RESE |
             BM_SD_CTL_EN |
             BF_SD_CTL_BSEL_V(BUS) |
             BF_SD_CTL_BUSWID_V(WIDTH_4BIT); /* 0x481 */

    SD_FIFOCTL = BM_SD_FIFOCTL_EMPTY |
                 BM_SD_FIFOCTL_RST |
                 BF_SD_FIFOCTL_THRH_V(THR_10_16) |
                 BM_SD_FIFOCTL_FULL |
                 BM_SD_FIFOCTL_IRQP; /* 0x25c */

    SD_BYTECNT = 0;

    SD_RW = BM_SD_RW_WCEF |
            BM_SD_RW_WCST |
            BM_SD_RW_RCST; /* 0x340 */

    SD_CMDRSP = 1;

    /* B22 sd detect active low */
    atj213x_gpio_setup(GPIO_PORTB, 22, GPIO_IN);

    semaphore_init(&sd_semaphore, 1, 0);
}

void sdc_card_reset(void)
{
    for (int i=0; i<5; i++)
    {
        SD_CLK = 0xff;
        while (SD_CLK & 1) { udelay(10); }
    }
}

void sdc_set_speed(unsigned sdfreq)
{
    if (sdfreq == 0)
        panicf("sdc_set_speed() called with sdfreq == 0");

    unsigned int corefreq = atj213x_get_coreclk();
    unsigned int sddiv = ((corefreq + sdfreq - 1)/sdfreq) - 1;

    if (sddiv > 15)
    {
        /* when low clock is needed during initialization */
        sddiv = (((corefreq/128) + sdfreq - 1)/sdfreq) - 1;
        sddiv |= BM_CMU_SDCLK_D128;
    }

    CMU_SDCLK = BM_CMU_SDCLK_CKEN | sddiv;
}

void sdc_set_bus_width(unsigned width)
{
    if (width == 1)
        SD_CTL = (SD_CTL & ~BM_SD_CTL_BUSWID) |
                 BF_SD_CTL_BUSWID_V(WIDTH_1BIT);
    else
        SD_CTL = (SD_CTL & ~BM_SD_CTL_BUSWID) |
                 BF_SD_CTL_BUSWID_V(WIDTH_4BIT);
}

bool sdc_card_present(void)
{
    return !atj213x_gpio_get(GPIO_PORTB, 22);
}

/* called between DMA channel setup
 * and DMA channel start
 */
static void sdc_dma_rd_callback(struct ll_dma_t *ll)
{
    if (ll)
    {
        SD_BYTECNT = ll->hwinfo.cnt;

        SD_FIFOCTL = BF_SD_FIFOCTL_EMPTY(1)          |
                     BF_SD_FIFOCTL_RST(1)            |
                     BF_SD_FIFOCTL_THRH_V(THR_10_16) |
                     BF_SD_FIFOCTL_FULL(1)           |
                     BF_SD_FIFOCTL_DRQE(1); /* 0x259 */

        SD_RW = BF_SD_RW_WCEF(1) | 
                BF_SD_RW_WCST(1) |
                BF_SD_RW_STRD(1) |
                BF_SD_RW_RCST(1); /* 0x3c0 */
    }
}

/* called between DMA channel setup
 * and DMA channel start
 */
static void sdc_dma_wr_callback(struct ll_dma_t *ll)
{
    if (ll)
    {
        SD_BYTECNT = ll->hwinfo.cnt;

        SD_FIFOCTL = BF_SD_FIFOCTL_EMPTY(1)          |
                     BF_SD_FIFOCTL_RST(1)            |
                     BF_SD_FIFOCTL_THRH_V(THR_10_16) |
                     BF_SD_FIFOCTL_FULL(1)           |
                     BF_SD_FIFOCTL_DRQE(1); /* 0x259 */

        SD_RW = BF_SD_RW_STWR(1) |
                BF_SD_RW_WCEF(1) | 
                BF_SD_RW_WCST(1) |
                BF_SD_RW_STRD(0) |
                BF_SD_RW_RCST(1) |
                BF_SD_RW_RWST(1); /* 0x8341 */
    }
}

static void sdc_dma_rd(void *buf, int size)
{
    /* This allows for ~4MB chained transfer */
    static struct ll_dma_t sd_ll[4];
    int i, xsize;
    unsigned int mode = BF_DMAC_DMA_MODE_DBURLEN_V(SINGLE)   |
                        BF_DMAC_DMA_MODE_RELO(0)             |
                        BF_DMAC_DMA_MODE_DDSP(0)             |
                        BF_DMAC_DMA_MODE_DCOL(0)             |
                        BF_DMAC_DMA_MODE_DDIR_V(INCREASE)    |
                        BF_DMAC_DMA_MODE_DFXA_V(NOT_FIXED)   |
                        BF_DMAC_DMA_MODE_DFXS(0)             |
                        BF_DMAC_DMA_MODE_SBURLEN_V(SINGLE)   |
                        BF_DMAC_DMA_MODE_SDSP(0)             |
                        BF_DMAC_DMA_MODE_SCOL(0)             |
                        BF_DMAC_DMA_MODE_SFXA_V(FIXED)       |
                        BF_DMAC_DMA_MODE_STRG_V(SD)          |
                        BF_DMAC_DMA_MODE_STRANWID_V(WIDTH32) |
                        BF_DMAC_DMA_MODE_SFXS(0);

    /* dma destination DTRG depends on address (dram/iram */
    if (iram_address(buf))
        mode |= BF_DMAC_DMA_MODE_DTRG_V(IRAM);
    else
        mode |= BF_DMAC_DMA_MODE_DTRG_V(SDRAM);

    /* destination transfer width depends on alignement */
    if (((unsigned int)buf & 3) == 0)
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH32);
    else if (((unsigned int)buf & 3) == 2)
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH16);
    else
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH8);

    for (i=0; i<4; i++)
    {
        xsize = MIN(size, SD_MAX_XFER_SIZE);
        sd_ll[i].hwinfo.dst = PHYSADDR((uint32_t)buf);
        sd_ll[i].hwinfo.src = PHYSADDR((uint32_t)&SD_DAT);
        sd_ll[i].hwinfo.mode = mode;
        sd_ll[i].hwinfo.cnt = xsize;

        size -= xsize;
        buf = (void *)((char *)buf + xsize);
        sd_ll[i].next = (size) ? &sd_ll[i+1] : NULL;

        if (size == 0)
            break;
    }

    if (size)
    {
        /* requested transfer is bigger then we can handle */
        panicf("sdc_dma_rd(): can't transfer that much!");
    }

    ll_dma_setup(DMA_CH_SD, sd_ll, sdc_dma_rd_callback, &sd_semaphore);
    ll_dma_start(DMA_CH_SD);
}

static void sdc_dma_wr(void *buf, int size)
{

    /* This allows for ~4MB chained transfer */
    static struct ll_dma_t sd_ll[4];
    int i, xsize;
    unsigned int mode = BF_DMAC_DMA_MODE_DBURLEN_V(SINGLE)   |
                        BF_DMAC_DMA_MODE_RELO(0)             |
                        BF_DMAC_DMA_MODE_DDSP(0)             |
                        BF_DMAC_DMA_MODE_DCOL(0)             |
                        BF_DMAC_DMA_MODE_DFXA_V(FIXED)       |
                        BF_DMAC_DMA_MODE_DTRG_V(SD)          |
                        BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH32) |
                        BF_DMAC_DMA_MODE_DFXS(0)             |
                        BF_DMAC_DMA_MODE_SBURLEN_V(SINGLE)   |
                        BF_DMAC_DMA_MODE_SDSP(0)             |
                        BF_DMAC_DMA_MODE_SCOL(0)             |
                        BF_DMAC_DMA_MODE_SDIR_V(INCREASE)    |
                        BF_DMAC_DMA_MODE_SFXA_V(NOT_FIXED)   |
                        BF_DMAC_DMA_MODE_SFXS(0);

    /* dma destination DTRG depends on address (dram/iram */
    if (iram_address(buf))
        mode |= BF_DMAC_DMA_MODE_STRG_V(IRAM);
    else
        mode |= BF_DMAC_DMA_MODE_STRG_V(SDRAM);

    /* destination transfer width depends on alignement */
    if (((unsigned int)buf & 3) == 0)
        mode |= BF_DMAC_DMA_MODE_STRANWID_V(WIDTH32);
    else if (((unsigned int)buf & 3) == 2)
        mode |= BF_DMAC_DMA_MODE_STRANWID_V(WIDTH16);
    else
        mode |= BF_DMAC_DMA_MODE_STRANWID_V(WIDTH8);

    for (i=0; i<4; i++)
    {
        xsize = MIN(size, SD_MAX_XFER_SIZE);
        sd_ll[i].hwinfo.dst = PHYSADDR((uint32_t)&SD_DAT);
        sd_ll[i].hwinfo.src = PHYSADDR((uint32_t)buf);
        sd_ll[i].hwinfo.mode = mode;
        sd_ll[i].hwinfo.cnt = xsize;

        size -= xsize;
        buf = (void *)((char *)buf + xsize);
        sd_ll[i].next = (size) ? &sd_ll[i+1] : NULL;

        if (size == 0)
            break;
    }

    if (size)
    {
        /* requested transfer is bigger then we can handle */
        panicf("sdc_dma_rd(): can't transfer that much!");
    }

    ll_dma_setup(DMA_CH_SD, sd_ll, sdc_dma_wr_callback, &sd_semaphore);
    ll_dma_start(DMA_CH_SD);
}

int sdc_send_cmd(const uint32_t cmd, const uint32_t arg,
                 struct sd_rspdat_t *rspdat, int datlen)
{
    unsigned long tmo = current_tick + HZ;
    unsigned int cmdrsp, crc7, rescrc;
    unsigned rsp = SDLIB_RSP(cmd);

    memset(rspdat->response, 0, 4*sizeof(uint32_t));

#ifdef ATJ213X_SD_DEBUG
    printf("sdc_send_cmd(c=%d, a=%x, d=%d,%d)", SDLIB_CMD(cmd), arg, datlen, rsp);
#endif
    atj213x_gpio_muxsel(GPIO_MUXSEL_SD);

    SD_ARG = arg;
    SD_CMD = SDLIB_CMD(cmd);

    /* this sets bit0 to clear STAT and mark response type */
    switch (rsp)
    {
        case SDLIB_RSP_NRSP:
            cmdrsp = 0x05;
            break;

        case SDLIB_RSP_R1:
        case SDLIB_RSP_R6:
            cmdrsp = 0x03;
            break;

        case SDLIB_RSP_R2:
            cmdrsp = 0x11;
            break;

        case SDLIB_RSP_R3:
            cmdrsp = 0x09;
            break;

        default:
            atj213x_gpio_mux_unlock(GPIO_MUXSEL_SD);
            panicf("Invalid SD response requested: 0x%0x", rsp);
    }

    /* prepare DMA transfer */
    if (rspdat->data && datlen > 0)
    {
        if (rspdat->rd)
        {
            sdc_dma_rd(rspdat->data, datlen);
        }
        else
        {
            sdc_dma_wr(rspdat->data, datlen);
        }
    }

    /* Kick SD transaction in */
    SD_CMDRSP = cmdrsp;
    udelay(1);

    /* command finish wait */
    do
    {
        if (TIME_AFTER(current_tick, tmo))
        {
            atj213x_gpio_mux_unlock(GPIO_MUXSEL_SD);
#ifdef ATJ213X_SD_DEBUG
	    printf("sdc_send_cmd: timeout");
#endif
            return -1;
        }
    } while (SD_CMDRSP & cmdrsp);


    if (rsp != SDLIB_RSP_NRSP)
    {
        /* check CRC */
        if (rsp == SDLIB_RSP_R1)
        {
            crc7 = SD_CRC7;
            rescrc = SD_RSPBUF(0) & 0xff;

            if (crc7 ^ rescrc)
            {
                atj213x_gpio_mux_unlock(GPIO_MUXSEL_SD);
                panicf("Invalid SD CRC: 0x%02x != 0x%02x", rescrc, crc7);
            }
        }

        if (rsp == SDLIB_RSP_R2)
        {
            rspdat->response[0] = SD_RSPBUF(3);
            rspdat->response[1] = SD_RSPBUF(2);
            rspdat->response[2] = SD_RSPBUF(1);
            rspdat->response[3] = SD_RSPBUF(0);
        }
        else
        {
            rspdat->response[0] = (SD_RSPBUF(1)<<24) | (SD_RSPBUF(0)>>8);
        }
    }

    /* data stage */
    if (rspdat->data && datlen > 0)
    {
        semaphore_wait(&sd_semaphore, TIMEOUT_BLOCK);
    }

    atj213x_gpio_mux_unlock(GPIO_MUXSEL_SD);

#ifdef ATJ213X_SD_DEBUG
    printf("rsp %x %x %x %x", rspdat->response[0], rspdat->response[1], rspdat->response[2], rspdat->response[3]);
#endif

    return 0;
}
