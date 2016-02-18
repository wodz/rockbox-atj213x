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

#define USB_FULL_SPEED 0
#define USB_HIGH_SPEED 1

static volatile bool setup_data_valid = false;
static volatile int udc_speed = USB_FULL_SPEED;

/* return port speed FS=0, HS=1 */
int usb_drv_port_speed(void)
{
    return udc_speed;
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
    int ep_num = EP_NUM(endpoint);
    volatile unsigned char *reg;

    if (ep_num == 0)
    {
        return (UDC_EP0CS & 1) ? true : false;
    }
    else
    {
        /* OUT1CON, IN1CON, OUT2CON, IN2CON */
        reg = (volatile unsigned char *)
                  (REGS_UDC_BASE + (in ? 0x6 : 0x2) + ep_num * 8);

        return (*reg & 0x40) ? true : false;
    }
}

/* Stall the endpoint. Usually set a flag in the controller */
void usb_drv_stall(int endpoint, bool stall, bool in)
{
    int ep_num = EP_NUM(endpoint);
    volatile unsigned char *reg;

    /* EP0 */
    if (ep_num == 0)
    {
        if (stall)
        {
            UDC_EP0CS |= 1;
        }
        else
        {
            UDC_EP0CS &= ~1;
        }
    }
    else
    {
        /* OUT1CON, IN1CON, OUT2CON, IN2CON */
        reg = (volatile unsigned char *)
                  (REGS_UDC_BASE + (in ? 0x6 : 0x2) + ep_num * 8);

        if (stall)
        {
            *reg |= 0x40;
        }
        else
        {
            *reg &= ~0x40;
        }
    }
}

/* one time init (once per connection) - basicaly enable usb core */
void usb_drv_init(void)
{
    /* ungate udc clock */
    ajt213x_clk_enable(BM_CMU_DEVCLKEN_USBC);

    /* soft disconnect */
    UDC_USBCS |= BM_UDC_USBCS_SOFT_CONNECT;

    /* reset EP0 IN fifo */
    UDC_ENDPRST = BF_UDC_ENDPRST_FIFO_RESET(0) |
                  BF_UDC_ENDPRST_TOGGLE_RESET(0) |
                  BF_UDC_ENDPRST_DIR_V(IN) |
                  BF_UDC_ENDPRST_EP_NUM(0);

    UDC_ENDPRST = BF_UDC_ENDPRST_FIFO_RESET(1) |
                  BF_UDC_ENDPRST_TOGGLE_RESET(1) |
                  BF_UDC_ENDPRST_DIR_V(IN) |
                  BF_UDC_ENDPRST_EP_NUM(0); /* 0x70 */

    /* reset EP0 OUT fifo */
    UDC_ENDPRST = BF_UDC_ENDPRST_FIFO_RESET(0) |
                  BF_UDC_ENDPRST_TOGGLE_RESET(0) |
                  BF_UDC_ENDPRST_DIR_V(OUT) |
                  BF_UDC_ENDPRST_EP_NUM(0);

    UDC_ENDPRST = BF_UDC_ENDPRST_FIFO_RESET(1) |
                  BF_UDC_ENDPRST_TOGGLE_RESET(1) |
                  BF_UDC_ENDPRST_DIR_V(OUT) |
                  BF_UDC_ENDPRST_EP_NUM(0); /* 0x60 */

    /* clear all pending interrupts */
    UDC_USBIRQ = 0xff;
    UDC_OTGIRQ = 0xff;
    UDC_IN04IRQ = 0xff;
    UDC_OUT04IRQ = 0xff;
    UDC_USBEIRQ = 0x50; /* UDC ? with 0x40 there is irq storm */

    /* enable HS, Reset, Setup_data interrupts */
    UDC_USBIEN = BF_UDC_USBIEN_HS(1) |
                 BF_UDC_USBIEN_RESET(1) |
                 BF_UDC_USBIEN_SETUP_DATA(1);

    UDC_OTGIEN = 0;

    /* disable interrupts from EPs */
    UDC_IN04IEN = 0;
    UDC_OUT04IEN = 0;

    /* unmask UDC interrupt in interrupt controller */
    atj213x_intc_unmask(BP_INTC_MSK_USB);

    mdelay(100);

    /* soft connect */
    UDC_USBCS &= ~BM_UDC_USBCS_SOFT_CONNECT;
}

/* turn off usb core */
void usb_drv_exit(void)
{
    /* soft disconnect */
    UDC_USBCS |= BM_UDC_USBCS_SOFT_CONNECT;

    /* mask UDC interrupt in interrupt controller */
    atj213x_intc_mask(BP_INTC_MSK_USB);

    /* gate udc clock */
    ajt213x_clk_disable(BM_CMU_DEVCLKEN_USBC);
}

int usb_detect(void)
{
    if (PMU_LRADC & BM_PMU_LRADC_DC5V)
        return USB_INSERTED;
    else
        return USB_EXTRACTED;
}

/* UDC interrupt handler */
void INT_USB(void)
{
    /* get possible sources */
    unsigned int usbirq = UDC_USBIRQ;
    unsigned int otgirq = UDC_OTGIRQ;
    unsigned int epinirq = UDC_IN04IRQ;
    unsigned int epoutirq = UDC_OUT04IRQ;

    /* HS, Reset, Setup */
    if (usbirq)
    {
        if (usbirq & BM_UDC_USBIRQ_HS)
        {
            /* HS irq */
            udc_speed = USB_HIGH_SPEED;
        }
        else if (usbirq & BM_UDC_USBIRQ_RESET)
        {
            /* reset */
            udc_speed = USB_FULL_SPEED;

            /* clear all pending irqs */
            UDC_OUT04IRQ = 0xff;
            UDC_IN04IRQ = 0xff;
        }
        else if (usbirq & BM_UDC_USBIRQ_SETUP_DATA)
        {
            /* Setup data valid */
            setup_data_valid = true;
        }

        /* clear irq flags */
        UDC_USBIRQ = usbirq;
    }

    if (epoutirq)
    {
        UDC_OUT04IRQ = epoutirq;
    }

    if (epinirq)
    {
        UDC_IN04IRQ = epinirq;
    }

    if (otgirq)
    {
        UDC_OTGIRQ = otgirq;
    }

    UDC_USBEIRQ = 0x50;
}
