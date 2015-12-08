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

#include "system.h"
#include "semaphore.h"
#include "panic.h"
#include "sdlib.h"
#include "gpio-atj213x.h"
#include "dma-atj213x.h"
#include "regs/regs-dmac.h"
#include "regs/regs-cmu.h"
#include "regs/regs-sd.h"

static struct semaphore sd_semaphore;

void sdc_init(void)
{
    /* ungate SD block clock */
    CMU_DEVCLKEN |= BM_CMU_DEVCLKEN_SD | BM_CMU_DEVCLKEN_DMAC;

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

    SD_CMDRSP = 0x10000;

    /* Send 127 dummy clocks to the card */
    SD_CLK = 0xff;

    /* B22 sd detect active low */
    atj213x_gpio_setup(GPIO_PORTB, 22, GPIO_IN);

    semaphore_init(&sd_semaphore, 1, 0);
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
    // TODO remove magic values
    SD_BYTECNT = ll->hwinfo.cnt;
    SD_FIFOCTL = 0x259;
    SD_RW = 0x3c0;
}

static bool iram_address(void *buf)
{
    return (PHYSADDR((uint32_t)buf) >= 0x14040000);
}

static void sdc_dma_rd(void *buf, int size)
{
    /* This allows for ~4MB chained transfer */
    static struct ll_dma_t sd_ll[4];
    int i, xsize;
    unsigned int mode = BF_DMAC_DMA_MODE_DDIR_V(INCREASE) |
                        BF_DMAC_DMA_MODE_SFXA_V(FIXED) |
                        BF_DMAC_DMA_MODE_STRG_V(SD);

    /* dma mode depends on dst address (dram/iram)
     * and buffer alignment
     */
    if (iram_address(buf))
        mode |= BF_DMAC_DMA_MODE_DTRG_V(IRAM);
    else
        mode |= BF_DMAC_DMA_MODE_DTRG_V(SDRAM);

    if (((unsigned int)buf & 3) == 0)
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH32);
    else if (((unsigned int)buf & 3) == 2)
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH16);
    else
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH8);

    for (i=0; i<4; i++)
    {
        xsize = MIN(size, DMA_MAX_XFER_SIZE);
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

    ll_dma_setup(DMA_CH_SD, sd_ll,
                 sdc_dma_rd_callback, &sd_semaphore);
    ll_dma_start(DMA_CH_SD);

    semaphore_wait(&sd_semaphore, TIMEOUT_BLOCK);
}

static void sdc_dma_wr(void *buf, int size)
{
    (void)buf;
    (void)size;
}

int sdc_send_cmd(const uint32_t cmd, const uint32_t arg,
                 struct sd_rspdat_t *rspdat, int datlen)
{
    unsigned long tmo = current_tick + HZ/5;
    unsigned int cmdrsp, crc7, rescrc;
    unsigned rsp = SDLIB_RSP(cmd);

    atj213x_gpio_muxsel(GPIO_MUXSEL_SD);

//CHECK_MUX();
    SD_ARG = arg;
//CHECK_MUX();
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

    /* prepare transfer to sd in case of wr data command */
//    if (rspdat->data && datlen > 0 && !rspdat->rd)
//        sdc_dma_wr(rspdat->data, datlen);

//CHECK_MUX();
    SD_CMDRSP = cmdrsp;

    /* command finish wait */
    do
    {
        if (TIME_AFTER(current_tick, tmo))
        {
//            panicf("sdc_send_cmd: timeout");
            return -1; /// error code?
        }
//CHECK_MUX();
    } while (SD_CMDRSP & cmdrsp);


    if (rsp != SDLIB_RSP_NRSP)
    {
        /* check CRC */
        if (rsp == SDLIB_RSP_R1)
        {
//CHECK_MUX();
            crc7 = SD_CRC7;
            rescrc = SD_RSPBUF(0) & 0xff;

            if (crc7 ^ rescrc)
            {
                atj213x_gpio_mux_unlock(GPIO_MUXSEL_SD);
                panicf("Invalid SD CRC: 0x%02x != 0x%02x", rescrc, crc7);
//                printf("Invalid SD CRC: 0x%02x != 0x%02x", rescrc, crc7);
            }
        }

        if (rsp == SDLIB_RSP_R2)
        {
//CHECK_MUX();
            rspdat->response[0] = SD_RSPBUF(3);
            rspdat->response[1] = SD_RSPBUF(2);
            rspdat->response[2] = SD_RSPBUF(1);
            rspdat->response[3] = SD_RSPBUF(0);
            //printf("rsp: %0x %0x %0x %0x", rspdat->response[0], rspdat->response[1], rspdat->response[2], rspdat->response[3]);
        }
        else
        {
//CHECK_MUX();
            rspdat->response[0] = (SD_RSPBUF(1)<<24) | (SD_RSPBUF(0)>>8);
            //printf("rsp: %0x", rspdat->response[0]);
        }
    }

    /* data stage */
    if (rspdat->data && datlen > 0 && rspdat->rd)
        //sdc_dma_rd(rspdat->data, datlen);

//CHECK_MUX();
    atj213x_gpio_mux_unlock(GPIO_MUXSEL_SD);
//CHECK_MUX_FREE();
    return 0;
}
