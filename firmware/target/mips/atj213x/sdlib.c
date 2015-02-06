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

static tCardInfo sdmmc_card_info[SDMMC_NUM_DRIVES];
static int disk_last_activity[SDMMC_NUM_DRIVES];

static long sdmmc_stack[(DEFAULT_STACK_SIZE*2 + 0x200)/sizeof(long)];
static const char sdmmc_thread_name[] = "sdmmc";
static struct event_queue sdmmc_queue;

int sd_wait_for_state(state)
{
    int retry = 50;
    struct sd_rspdat_t rspdat;

    while (retry-- > 0)
    {
        sdc_send_cmd(SD_SEND_STATUS, card_info.rca, &rspdat, 0);

        if (((rspdat.response[1] >> 9) & 0xf) == state)
             return 0;

        mdelay(100);
    }

    return -1;
}

int sd_card_init(void)
{
    bool sd_v2 = false;
    uint32_t arg;
    uint8_t buf[64];
    struct sd_rspdat_t rspdat;

    rspdat.data = buf;
		
    /* bomb out if the card is not present */
    if (!sdc_card_present())
        return -1;

    /* init at max 400kHz */
    sdc_set_speed(400000);

    sdc_send_cmd(SD_GO_IDLE_STATE, 0, &rspdat, 0);

    /* CMD8 Check for v2 sd card. Must be sent before using ACMD41
     * Non v2 cards will not respond to this command
     * bit [7:1] are crc, bit0 is 1
     */
    sdc_send_cmd(SD_SEND_IF_COND, 0x1AA, &rspdat, 0);

    if ((rspdat.response[1] & 0xfff) == 0x1aa)
        sd_v2 = true;

    arg = sd_v2 ? 0x40FF8000 : 0x00FF8000;

    /* 2s init timeout */
    while ((card_info.ocr & 0x80000000) == 0)
    {
        /* ACMD41 For v2 cards set HCS bit[30] & send host voltage range to all */
	sdc_send_cmd(SD_APP_OP_COND, arg, &rspdat, 0);
	card_info.ocr = rspdat.response[1];
    }

    sdc_send_cmd(SD_ALL_SEND_CID, 0, &rspdat, 0);
    card_info.cid[1] = rspdat.response[1];
    card_info.cid[2] = rspdat.response[2];
    card_info.cid[3] = rspdat.response[3];
    card_info.cid[4] = rspdat.response[4];

    sdc_send_cmd(SDLIB.CMD.SEND_RELATIVE_ADDR, 0, &rspdat, 0);
    card_info.rca = rspdat.response[1];

    /* End of Card Identification Mode */

    sdc_send_cmd(SD_SEND_CSD, SDLIB.card_info.rca, &rspdat, 0);
    card_info.csd[1] = rspdat.response[1];
    card_info.csd[2] = rspdat.response[2];
    card_info.csd[3] = rspdat.response[3];
    card_info.csd[4] = rspdat.response[4];

    sd_parse_csd(&card_info);

    sdc_send_cmd(SD_SELECT_CARD, card_info.rca, &rspdat, 0);

    /* wait for tran state */
    if (sd_wait_for_state(SD_STS_TRAN))
        return -2;

    /* switch card to 4bit interface
     * TODO: add some configuration here
     */
    sdc_send_cmd(SDLIB.CMD.SET_BUS_WIDTH, 2, rspdat, 0);
    sdc_set_bus_width(4);

    /* disconnect the pull-up resistor on CD/DAT3 */
    sdc_send_cmd(SDLIB.CMD.SET_CLR_CARD_DETECT, 0, rspdat, 0);

    /* try switching to HS timing, non-HS cards seems to ignore this
     * the command returns 64bytes of data
     */
    sdc_send_cmd(SDLIB.CMD.SWITCH_FUNC, 0x80fffff1, rspdat, 64);

    /* rise SD clock */
    sdc_set_speed(card_info.speed);

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
            if (TIME_BEFORE(current_tick, last_disk_activity+(3*HZ)))
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
            usb_wait_for_disconnect(&sd_queue);

            break;
        }
    }
}

#if CONFIG_STORAGE & STORAGE_SD
int sd_init(void)
{
    int ret;

    /* initialize controller */
    sdc_init();

    ret = sd_init_card();
    if(ret < 0)
        return ret;

    /* init mutex */
    mutex_init(&sd_mtx);

    queue_init(&sd_queue, true);
    create_thread(sdmmc_thread, sdmmc_stack, sizeof(sdmmc_stack), 0,
            sdmmc_thread_name IF_PRIO(, PRIORITY_USER_INTERFACE) IF_COP(, CPU));

    return 0;
}

int sd_read_sectors(IF_MD(int drive,) unsigned long start, int count,
                    void* buf)
{
}

int sd_write_sectors(IF_MD(int drive,) unsigned long start, int count,
                     const void* buf)
{
}

#ifndef BOOTLOADER
long sd_last_disk_activity(void)
{
    return last_disk_activity;
}

tCardInfo *card_get_info_target(int card_no)
{
    (void)card_no;
    return &card_info;
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
