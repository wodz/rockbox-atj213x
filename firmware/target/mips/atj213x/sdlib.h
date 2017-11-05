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
#ifndef SDLIB_H
#define SDLIB_H

#include <stdint.h>
#include <mv.h>
#include "sdmmc.h"

#define RSP(r) ((r)<<9)
#define CMD(c,r) ((r)|(c))
#define ACMD(c,r) (CMD(c,r) | 0x100)

#define SDLIB_RSP_NRSP RSP(0)
#define SDLIB_RSP_R1   RSP(1)
#define SDLIB_RSP_R1b  RSP(1)
#define SDLIB_RSP_R2   RSP(2)
#define SDLIB_RSP_R3   RSP(3)
#define SDLIB_RSP_R6   RSP(6)

/* sd command coding:
 * [31:10] [11:9] [8] [7:0]
      x     rsp   app cmd
 * where:
 * x - don't care
 * rsp - response type
 * app - ACMD flag
 * cmd - command code
 */
#define SDLIB_CMD(x)  ((x) & 0xff)
#define SDLIB_ACMD(x) ((x) & 0x100)
#define SDLIB_RSP(x)  ((x) & (0x07<<9))

#define SDLIB_GO_IDLE_STATE            CMD(0, SDLIB_RSP_NRSP)
#define SDLIB_ALL_SEND_CID             CMD(2, SDLIB_RSP_R2)
#define SDLIB_SEND_RELATIVE_ADDR       CMD(3, SDLIB_RSP_R6)
#define SDLIB_SET_DSR                  CMD(4, SDLIB_RSP_NRSP)
#define SDLIB_SWITCH_FUNC              CMD(6, SDLIB_RSP_R1)
#define SDLIB_SET_BUS_WIDTH           ACMD(6, SDLIB_RSP_R1)
#define SDLIB_SELECT_CARD              CMD(7, SDLIB_RSP_R1b)  /* with card's rca  */
#define SDLIB_DESELECT_CARD            CMD(7, SDLIB_RSP_NRSP)  /* with rca = 0  */
#define SDLIB_SEND_IF_COND             CMD(8, SDLIB_RSP_R6)
#define SDLIB_SEND_CSD                 CMD(9, SDLIB_RSP_R2)
#define SDLIB_SEND_CID                 CMD(10,SDLIB_RSP_R2)
#define SDLIB_STOP_TRANSMISSION        CMD(12,SDLIB_RSP_R1b)
#define SDLIB_SEND_STATUS              CMD(13,SDLIB_RSP_R1)
#define SDLIB_SD_STATUS               ACMD(13,SDLIB_RSP_R1)
#define SDLIB_GO_INACTIVE_STATE        CMD(15,SDLIB_RSP_NRSP)
#define SDLIB_SET_BLOCKLEN             CMD(16,SDLIB_RSP_R1)
#define SDLIB_READ_SINGLE_BLOCK        CMD(17,SDLIB_RSP_R1)
#define SDLIB_READ_MULTIPLE_BLOCK      CMD(18,SDLIB_RSP_R1)
#define SDLIB_SEND_NUM_WR_BLOCKS      ACMD(22,SDLIB_RSP_R1)
#define SDLIB_SET_BLOCK_COUNT          CMD(23,SDLIB_RSP_R1)
#define SDLIB_SET_WR_BLK_ERASE_COUNT  ACMD(23,SDLIB_RSP_R1)
#define SDLIB_WRITE_BLOCK              CMD(24,SDLIB_RSP_R1)
#define SDLIB_WRITE_MULTIPLE_BLOCK     CMD(25,SDLIB_RSP_R1)
#define SDLIB_PROGRAM_CSD              CMD(27,SDLIB_RSP_R1)
#define SDLIB_ERASE_WR_BLK_START       CMD(32,SDLIB_RSP_R1)
#define SDLIB_ERASE_WR_BLK_END         CMD(33,SDLIB_RSP_R1)
#define SDLIB_ERASE                    CMD(38,SDLIB_RSP_R1b)
#define SDLIB_APP_OP_COND             ACMD(41,SDLIB_RSP_R3)
#define SDLIB_LOCK_UNLOCK              CMD(42,SDLIB_RSP_R1)
#define SDLIB_SET_CLR_CARD_DETECT     ACMD(42,SDLIB_RSP_R1)
#define SDLIB_SEND_SCR                ACMD(51,SDLIB_RSP_R1)
#define SDLIB_APP_CMD                  CMD(55,SDLIB_RSP_R1)

/* SD States */
#define SDLIB_STS_IDLE             0
#define SDLIB_STS_READY            1
#define SDLIB_STS_IDENT            2
#define SDLIB_STS_STBY             3
#define SDLIB_STS_TRAN             4
#define SDLIB_STS_DATA             5
#define SDLIB_STS_RCV              6
#define SDLIB_STS_PRG              7
#define SDLIB_STS_DIS              8

/* library error codes
 * we build up to 4 commands backtrace by shifting
 * previous value up by 8 bits
 */
#define SDLIB_OK 0
#define SDLIB_ERR_BIT (1<<31)
#define SDLIB_ERR(rc, cmd) rc = (SDLIB_ERR_BIT | ((rc)<<8) | SDLIB_CMD(cmd))

struct sd_rspdat_t {
    uint32_t response[4];
    uint8_t *data;
    bool rd;
};

#define SDLIB_SD_V2    1
#define SDLIB_SD_HS    2
#define SDLIB_SD_CMD23 4


struct sdlib_card_info_t {
    tCardInfo info;
    uint32_t flags;
};

/* prototypes */
void sdc_init(void);
void sdc_set_speed(unsigned sdfreq);
void sdc_set_bus_width(unsigned width);
#ifdef HAVE_MULTIDRIVE
bool sdc_card_present(int drive);
int sd_card_init(int drive);
#else
bool sdc_card_present(void);
int sd_card_init(void);
#endif
void sdc_card_reset(void);
int sdc_send_cmd(IF_MD(int drive,) const uint32_t cmd, const uint32_t arg,
                 struct sd_rspdat_t *rspdat, int datlen);
#endif /* SDLIB_H */
