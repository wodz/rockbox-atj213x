/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2016 by Marcin Bukat
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
#include "usb.h"
#include "usb_drv.h"

#include "cpu.h"
#include "system.h"
#include "kernel.h"
#include "panic.h"

#include "usb_ch9.h"
#include "usb_core.h"
#include <inttypes.h>
#include "power.h"

#include "regs/regs-pmu.h"
#include "logf.h"


#ifdef LOGF_ENABLE
#define XFER_DIR_STR(dir) ((dir) ? "IN" : "OUT")
#define XFER_TYPE_STR(type) \
    ((type) == USB_ENDPOINT_XFER_CONTROL ? "CTRL" : \
     ((type) == USB_ENDPOINT_XFER_ISOC ? "ISOC" : \
      ((type) == USB_ENDPOINT_XFER_BULK ? "BULK" : \
       ((type) == USB_ENDPOINT_XFER_INT ? "INTR" : "INVL"))))
#endif

/* return port speed FS=0, HS=1 */
int usb_drv_port_speed(void)
{
    return 1;
}

/* Reserve endpoint */
int usb_drv_request_endpoint(int type, int dir)
{
    (void)type;
    (void)dir;
    return -1;
}

/* Free endpoint */
void usb_drv_release_endpoint(int ep)
{
    (void)ep;
}

/* Set the address (usually it's in a register).
 * There is a problem here: some controller want the address to be set between
 * control out and ack and some want to wait for the end of the transaction.
 * In the first case, you need to write some code special code when getting
 * setup packets and ignore this function (have a look at other drives)
 */
void usb_drv_set_address(int address)
{
    (void)address;
}

/* Setup a send transfer. (blocking) */
int usb_drv_send(int endpoint, void *ptr, int length)
{
    (void)endpoint;
    (void)ptr;
    (void)length;
    return 0;
}

/* Setup a send transfer. (non blocking) */
int usb_drv_send_nonblocking(int endpoint, void *ptr, int length)
{
    (void)endpoint;
    (void)ptr;
    (void)length;
    return 0;
}

/* Setup a receive transfer. (non blocking) */
int usb_drv_recv(int endpoint, void* ptr, int length)
{
    (void)endpoint;
    (void)ptr;
    (void)length;
    return 0;
}

/* Kill all transfers. Usually you need to set a bit for each endpoint
 *  and flush fifos. You should also call the completion handler with 
 * error status for everything
 */
void usb_drv_cancel_all_transfers(void)
{
}

/* Set test mode, you can forget that for now, usually it's sufficient
 * to bit copy the argument into some register of the controller
 */
void usb_drv_set_test_mode(int mode)
{
    (void)mode;
}

/* Check if endpoint is in stall state */
bool usb_drv_stalled(int endpoint, bool in)
{
    (void)endpoint;
    (void)in;
    return true;
}

/* Stall the endpoint. Usually set a flag in the controller */
void usb_drv_stall(int endpoint, bool stall, bool in)
{
}

/* one time init (once per connection) - basicaly enable usb core */
void usb_drv_init(void)
{
}

/* turn off usb core */
void usb_drv_exit(void)
{
}

int usb_detect(void)
{
    if (PMU_LRADC & BM_PMU_LRADC_DC5V)
        return USB_INSERTED;
    else
        return USB_EXTRACTED;
}

