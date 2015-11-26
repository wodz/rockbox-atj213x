/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2011 by Amaury Pouly
 *               2015 by Marcin Bukat
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

#include "config.h"
#include "system.h"
#include "panic.h"
#include "disk.h"
#include "usb.h"
#include "ata_idle_notify.h"
#include "led.h"
#include "sdmmc.h"
#include "sdlib.h"

#ifdef HAVE_MULTIDRIVE
#define SDMMC_INFO(drive) sdmmc_card_info[drive]
#else
#define SDMMC_INFO(drive) sdmmc_card_info[0]
#endif

#define SDMMC_RCA(drive) SDMMC_INFO(drive).rca
#define SDMMC_CSD(drive) SDMMC_INFO(drive).csd
#define SDMMC_CID(drive) SDMMC_INFO(drive).cid
#define SDMMC_OCR(drive) SDMMC_INFO(drive).ocr
#define SDMMC_SPEED(drive) SDMMC_INFO(drive).speed

static tCardInfo sdmmc_card_info[SDMMC_NUM_DRIVES];
static int disk_last_activity[SDMMC_NUM_DRIVES];
static struct mutex sdmmc_mtx[SDMMC_NUM_DRIVES];

static long sdmmc_stack[(DEFAULT_STACK_SIZE*2 + 0x200)/sizeof(long)];
static const char sdmmc_thread_name[] = "sdmmc";
static struct event_queue sdmmc_queue;

static int sdlib_send_cmd(IF_MD(int drive,) const uint32_t cmd, const uint32_t arg,
                        struct sd_rspdat_t *rspdat, int datlen)
{
    printf("sdlib_send_cmd(%d, %x, %d)", SDLIB_CMD(cmd), arg, datlen);

    if (SDLIB_ACMD(cmd))
    {
        if (sdc_send_cmd(IF_MD(drive,) SDLIB_APP_CMD, SDMMC_RCA(drive), rspdat, 0) != 0)
            return -1;
    }

    return sdc_send_cmd(IF_MD(drive,) cmd, arg, rspdat, datlen);
}

int sd_wait_for_state(IF_MD(int drive,) unsigned state)
{
    int retry = 50;
    struct sd_rspdat_t rspdat;

    while (retry-- > 0)
    {
        sdlib_send_cmd(IF_MD(drive,) SDLIB_SEND_STATUS, SDMMC_RCA(drive), &rspdat, 0);

        if (((rspdat.response[0] >> 9) & 0xf) == state)
             return 0;

        mdelay(100);
    }

    return -1;
}

int sd_card_init(IF_MD(int drive))
{
asm volatile("nop");
asm volatile("nop");

    //bool sd_v2 = false;
    uint32_t arg;
    uint8_t buf[64];
    struct sd_rspdat_t rspdat = {
        .response = {0,0,0,0},
        .data = buf,
        .rd = true
    };

    long init_tmo;

    /* bomb out if the card is not present */
    if (!sdc_card_present(IF_MD(drive)))
    {
        //return -1;
        printf("sd_card_init(): card not present");
        while(1);
    }

    /* init at max 400kHz */
    sdc_set_speed(400000);

    sdlib_send_cmd(IF_MD(drive,) SDLIB_GO_IDLE_STATE, 0, &rspdat, 0);
    /* CMD8 Check for v2 sd card. Must be sent before using ACMD41
     * Non v2 cards will not respond to this command
     * bit [7:1] are crc, bit0 is 1
     */
    sdlib_send_cmd(IF_MD(drive,) SDLIB_SEND_IF_COND, 0x1aa, &rspdat, 0);
    if ((rspdat.response[0] & 0xfff) == 0x1aa)
    {
        /* v2 card */
        arg = 0x40FF8000;
    }
    else
    {
        arg = 0x00FF8000;
    }

    /* 1s init timeout according to SD spec 2.0 */
    init_tmo = current_tick + HZ;
    do {
        if(TIME_AFTER(current_tick, init_tmo))
        {
            printf("SD spec 1s timeout");
            while(1);
            //return -2;
        }

        /* ACMD41 For v2 cards set HCS bit[30] & send host voltage range to all */
	sdlib_send_cmd(IF_MD(drive,) SDLIB_APP_OP_COND, arg, &rspdat, 0);
	SDMMC_OCR(drive) = rspdat.response[0];
    } while ((SDMMC_OCR(drive) & 0x80000000) == 0);


    sdlib_send_cmd(IF_MD(drive,) SDLIB_ALL_SEND_CID, 0, &rspdat, 0);
    SDMMC_CID(drive)[0] = rspdat.response[0];
    SDMMC_CID(drive)[1] = rspdat.response[1];
    SDMMC_CID(drive)[2] = rspdat.response[2];
    SDMMC_CID(drive)[3] = rspdat.response[3];

    sdlib_send_cmd(IF_MD(drive,) SDLIB_SEND_RELATIVE_ADDR, 0, &rspdat, 0);
    SDMMC_RCA(drive) = rspdat.response[0];

    /* End of Card Identification Mode */

    sdlib_send_cmd(IF_MD(drive,) SDLIB_SEND_CSD, SDMMC_RCA(drive), &rspdat, 0);
    SDMMC_CSD(drive)[0] = rspdat.response[0];
    SDMMC_CSD(drive)[1] = rspdat.response[1];
    SDMMC_CSD(drive)[2] = rspdat.response[2];
    SDMMC_CSD(drive)[3] = rspdat.response[3];

    sd_parse_csd(&SDMMC_INFO(drive));

    sdlib_send_cmd(IF_MD(drive,) SDLIB_SELECT_CARD, SDMMC_RCA(drive), &rspdat, 0);

    /* wait for tran state */
    if (sd_wait_for_state(IF_MD(drive,) SDLIB_STS_TRAN))
    {
        printf("wait for tran timeout");
        while(1);
        //return -2;
    }

    /* switch card to 4bit interface
     * TODO: add some configuration here
     */
    sdlib_send_cmd(IF_MD(drive,) SDLIB_SET_BUS_WIDTH, 2, &rspdat, 0);
    sdc_set_bus_width(4);

    /* disconnect the pull-up resistor on CD/DAT3 */
    sdlib_send_cmd(IF_MD(drive,) SDLIB_SET_CLR_CARD_DETECT, 0, &rspdat, 0);

    /* try switching to HS timing, non-HS cards seems to ignore this
     * the command returns 64bytes of data
     */
    sdlib_send_cmd(IF_MD(drive,) SDLIB_SWITCH_FUNC, 0x80fffff1, &rspdat, 64);

    /* rise SD clock */
    sdc_set_speed(SDMMC_SPEED(IF_MD(drive)));

    printf("sd_card_init(): OK");

    return 0;

}

static void sdmmc_thread(void) NORETURN_ATTR;
static void sdmmc_thread(void)
{
    struct queue_event ev;
    bool idle_notified = false;

    while (1)
    {
        queue_wait_w_tmo(&sdmmc_queue, &ev, HZ);

        switch ( ev.id )
        {
#ifdef HAVE_HOTSWAP
        case SYS_HOTSWAP_INSERTED:
        case SYS_HOTSWAP_EXTRACTED:;
            int success = 1;

            disk_unmount(sd_first_drive); /* release "by force" */

            mutex_lock(&sd_mtx); /* lock-out card activity */

            /* Force card init for new card, re-init for re-inserted one or
             * clear if the last attempt to init failed with an error. */
            card_info.initialized = 0;

            if (ev.id == SYS_HOTSWAP_INSERTED)
            {
                success = 0;
                sd_enable(true);
                int rc = sd_init_card(sd_first_drive);
                sd_enable(false);
                if (rc >= 0)
                    success = 2;
                else /* initialisation failed */
                    panicf("microSD init failed : %d", rc);
            }

            /* Access is now safe */
            mutex_unlock(&sd_mtx);

            if (success > 1)
                success = disk_mount(sd_first_drive); /* 0 if fail */

            /*
             * Mount succeeded, or this was an EXTRACTED event,
             * in both cases notify the system about the changed filesystems
             */
            if (success)
                queue_broadcast(SYS_FS_CHANGED, 0);

            break;
#endif /* HAVE_HOTSWAP */

        case SYS_TIMEOUT:
            if (TIME_BEFORE(current_tick, disk_last_activity+(3*HZ)))
            {
                idle_notified = false;
            }
            else if (!idle_notified)
            {
                call_storage_idle_notifys(false);
                idle_notified = true;
            }
            break;

        case SYS_USB_CONNECTED:
            usb_acknowledge(SYS_USB_CONNECTED_ACK);
            /* Wait until the USB cable is extracted again */
            usb_wait_for_disconnect(&sdmmc_queue);

            break;
        }
    }
}

#if CONFIG_STORAGE & STORAGE_SD
int sd_init(void)
{
    int i, rc;

    /* initialize controller */
    sdc_init();

    rc = sd_card_init();

    if(rc < 0)
        return rc;

    /* init mutex */
    for (i=0; i<SDMMC_NUM_DRIVES; i++)
        mutex_init(&sdmmc_mtx[i]);

    queue_init(&sdmmc_queue, true);
    create_thread(sdmmc_thread, sdmmc_stack, sizeof(sdmmc_stack), 0,
            sdmmc_thread_name IF_PRIO(, PRIORITY_USER_INTERFACE) IF_COP(, CPU));

    return SDLIB_OK;
}

int sd_read_sectors(IF_MD(int drive,) unsigned long start, int count,
                    void* buf)
{
    int rc = SDLIB_OK;
    struct sd_rspdat_t rspdat = {
        .response = {0,0,0,0},
        .data = buf,
        .rd = true
    };

    if (count == 0)
        return rc;

#ifdef HAVE_MULTIDRIVE
    mutex_lock(&sdmmc_mtx[drive]);
#else
    mutex_lock(&sdmmc_mtx[0]);
#endif

    if (count < 0 || (start + count > SDMMC_INFO(drive).numblocks))
        panicf("SD out of bound read request"
               " start: %ld count: %d while numblocks: %ld",
               start, count, SDMMC_INFO(drive).numblocks);

    led(true);

    if(!(SDMMC_OCR(drive) & 0x40000000))
        start = start * 512;    /* not SDHC */

    if(sdlib_send_cmd(IF_MD(drive,) SDLIB_READ_MULTIPLE_BLOCK,
                      start, &rspdat, count*512) != SDLIB_OK)
    {
        SDLIB_ERR(rc, SDLIB_READ_MULTIPLE_BLOCK);
    }

    if(sdlib_send_cmd(IF_MD(drive,) SDLIB_STOP_TRANSMISSION,
                      0, &rspdat, 0) != SDLIB_OK)
    {
        SDLIB_ERR(rc, SDLIB_STOP_TRANSMISSION);
    }

    led(false);
#ifdef HAVE_MULTIDRIVE
    mutex_unlock(&sdmmc_mtx[drive]);
#else
    mutex_unlock(&sdmmc_mtx[0]);
#endif
    return rc;
}

int sd_write_sectors(IF_MD(int drive,) unsigned long start, int count,
                     const void* buf)
{
    IF_MD((void)drive;)
    (void)start;
    (void)count;
    (void)buf;
    return -1;
}

#ifndef BOOTLOADER
long sd_last_disk_activity(void)
{
    return last_disk_activity;
}

tCardInfo *card_get_info_target(int card_no)
{
    return &SDMMC_INFO(card_no);
}
#endif /* BOOTLOADER */

#ifdef HAVE_HOTSWAP
/* Not complete and disabled in config */
bool sd_removable(IF_MD_NONVOID(int drive))
{
    (void)drive;
    return true;
}

bool sd_present(IF_MD_NONVOID(int drive))
{
    (void)drive;
    return card_detect_target();
}

static int sd_oneshot_callback(struct timeout *tmo)
{
    (void)tmo;

    /* This is called only if the state was stable for 300ms - check state
     * and post appropriate event. */
    if (card_detect_target())
    {
        queue_broadcast(SYS_HOTSWAP_INSERTED, 0);
    }
    else
        queue_broadcast(SYS_HOTSWAP_EXTRACTED, 0);

    return 0;
}
#endif /* HAVE_HOTSWAP */
#endif /* CONFIG_STORAGE & STORAGE_SD */
