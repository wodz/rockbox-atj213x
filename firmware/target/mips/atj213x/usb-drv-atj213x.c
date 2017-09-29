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

#include "intc-atj213x.h"
#include "cmu-atj213x.h"
#include "regs/regs-pmu.h"
#include "regs/regs-udc.h"
#include "regs/regs-intc.h"
#include "logf.h"

typedef volatile uint8_t reg8;
typedef volatile uint16_t reg16;
typedef volatile uint32_t reg32;

#define REGS_UDC_BASE 0xb00e0000
#define EP0_BC(in) (*(reg8 *)(REGS_UDC_BASE + ((in) ? 0x1 : 0x0)))

/* OUT1BCL, IN1BCL, OUT2BCL, IN2BCL */
#define EP_BCL(ep_num, in) (*(reg8 *)(REGS_UDC_BASE + ((in) ? 0x4 : 0x0) + (ep_num) * 8))

/* OUT1BCH, IN1BCH, OUT2BCH, IN2BCH */
#define EP_BCH(ep_num, in) (*(reg8 *)(REGS_UDC_BASE + ((in) ? 0x5 : 0x1) + (ep_num) * 8))

/* OUT1CON, IN1CON, OUT2CON, IN2CON */
#define EP_CON(ep_num, in) (*(reg8 *)(REGS_UDC_BASE + ((in) ? 0x6 : 0x2) + (ep_num) * 8))

/* OUT1CS, IN1CS, OUT2CS, IN2CS */
#define EP_CS(ep_num, in) (*(reg8 *)(REGS_UDC_BASE + ((in) ? 0x7 : 0x3) + (ep_num) * 8))

/* Fixme!!! FIFO1DAT, FIFO2DAT */
#define EP_FIFO(ep_num, in) (*(reg32 *)(REGS_UDC_BASE + 0x80 + (ep_num) * 4))

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

#define ENDPOINT(_num, _dir) \
    {.ep_num = (_num), .type = 0, .dir = (_dir), .allocated = false, .buf = NULL, .len = 0, .cnt = 0, .zlp = true, .block = false}

struct endpoint_t
{
    const int ep_num;             /* EP number */
    int type;                     /* EP type */
    int dir;                      /* DIR_IN/DIR_OUT */
    bool allocated;               /* flag to mark EPs taken */
    volatile void *buf;           /* tx/rx buffer address */
    volatile int len;             /* size of the transfer (bytes) */
    volatile int cnt;             /* number of bytes transfered/received  */
    bool zlp;                     /* mark the need for ZLP at the end */
    volatile bool block;          /* flag indicating that transfer is blocking */ 
    struct semaphore complete;    /* semaphore for blocking transfers */
};

static struct endpoint_t endpoints[][2] =
{
    {ENDPOINT(0, DIR_OUT), ENDPOINT(0, DIR_IN)},
    {ENDPOINT(1, DIR_OUT), ENDPOINT(1, DIR_IN)},
    {ENDPOINT(2, DIR_OUT), ENDPOINT(2, DIR_IN)}
};

static volatile int udc_speed = USB_FULL_SPEED;

static uint8_t read_epcs(struct endpoint_t *endp)
{
    return (endp->ep_num == 0) ?
               UDC_EP0CS : EP_CS(endp->ep_num, endp->dir);
}

static uint16_t read_epbc(struct endpoint_t *endp)
{
    return (endp->ep_num == 0) ?
               EP0_BC(endp->dir) : 
               (EP_BCH(endp->ep_num, endp->dir) << 8) |
                   EP_BCL(endp->ep_num, endp->dir);
}

static void write_epcs(struct endpoint_t *endp, uint8_t val)
{
    if (endp->ep_num == 0)
    {
        UDC_EP0CS = val;
    }
    else
    {
        EP_CS(endp->ep_num, endp->dir) = val;
    }
}

static void write_epbc(struct endpoint_t *endp, uint16_t val)
{
    if (endp->ep_num == 0)
    {
        EP0_BC(endp->dir) = (uint8_t)val;
    }
    else
    {
        EP_BCH(endp->ep_num, endp->dir) = (val >> 8);
        EP_BCL(endp->ep_num, endp->dir) = val & 0xff;
    }
}

/* Copy from UDC core buffers to memory */
static void copy_from_udc(void *ptr, volatile void *reg,
                          size_t sz, bool fifo_mode)
{
    uint32_t *p = ptr;
    volatile uint32_t *rp = reg;
    int inc = fifo_mode ? 0 : 1;

    /* do not overflow the destination buffer ! */
    while(sz >= 4)
    {
        *p++ = *rp;
        rp += inc;
        sz -= 4;
    }

    if(sz == 0)
        return;

    /* reminder */
    uint32_t cache = *rp;
    uint8_t *p8 = (void *)p;
    while(sz-- > 0)
    {
        *p8++ = cache;
        cache >>= 8;
    }
}

/* Copy from memory to UDC buffers */
static void copy_to_udc(volatile void *reg, void *ptr,
                        size_t sz, bool fifo_mode)
{
    uint32_t *p = ptr;
    volatile uint32_t *rp = reg;
    sz = (sz + 3) / 4;
    int inc = fifo_mode ? 0 : 1;

    /* read may overflow the source buffer but
     * it will not overwrite anything
     */
    while(sz-- > 0)
    {
        *rp = *p++;
        rp += inc;
    }
}

static void udc_ep_reset(int ep, bool in)
{
    UDC_ENDPRST = BF_UDC_ENDPRST_FIFO_RESET(0)   |
                  BF_UDC_ENDPRST_TOGGLE_RESET(0) |
                  BF_UDC_ENDPRST_DIR(in ? 1 : 0) |
                  BF_UDC_ENDPRST_EP_NUM(ep);

    UDC_ENDPRST = BF_UDC_ENDPRST_FIFO_RESET(1)   |
                  BF_UDC_ENDPRST_TOGGLE_RESET(1) |
                  BF_UDC_ENDPRST_DIR(in ? 1 : 0) |
                  BF_UDC_ENDPRST_EP_NUM(ep);
}

static int max_pkt_size(struct endpoint_t *endp)
{
    switch(endp->type)
    {
        case USB_ENDPOINT_XFER_CONTROL: return 64;
        case USB_ENDPOINT_XFER_BULK: return usb_drv_port_speed() ? 512 : 64;
        case USB_ENDPOINT_XFER_INT: return usb_drv_port_speed() ? 1024 : 64;
        default: panicf("max_pkt_size() invalid EP type"); return 0;
    }
}


/* write to EP fifo */
static void ep_write(struct endpoint_t *endp)
{
    int xfer_size = MIN(max_pkt_size(endp), endp->cnt);
    unsigned int timeout = current_tick + HZ/10;

    /* wait while EP is busy */
    while (read_epcs(endp) & 2)
    {
        if(TIME_AFTER(current_tick, timeout))
            break;
    }

    /* Copy data to EP fifo */
    copy_to_udc((volatile void *)EP_FIFO(endp->ep_num, endp->dir), (void *)endp->buf, xfer_size, true);

    /* Decrement by max packet size is intentional.
     * This way if we have final packet short one we will get negative len
     * after transfer, which in turn indicates we *don't* need to send
     * zero length packet. If the final packet is max sized packet we will
     * get zero len after transfer which indicates we need to send
     * zero length packet to signal host end of the transfer.
     */
    endp->cnt -= max_pkt_size(endp);
    endp->buf += xfer_size;

    write_epbc(endp, xfer_size);

    /* arm IN buffer as ready to send */
    write_epcs(endp, 0);
}

/* read from EP fifo */
static void ep_read(struct endpoint_t *endp)
{
    int xfer_size = read_epbc(endp);

    copy_from_udc((void *)endp->buf, (volatile void *)EP_FIFO(endp->ep_num, endp->dir), xfer_size, true);

    /* arm receiving buffer */
    write_epcs(endp, 0);
}

/* helper functions called in IRQ context
 * for Rest, HS and SETUP interrupts
 */
static void usbirq_handler(unsigned int usbirq)
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
        static struct usb_ctrlrequest setup_data;

        /* Copy request received. Beware that ONLY 32bit access is allowed
         * to hardware buffer or data get corrupted.
         */
        copy_from_udc(&setup_data, &UDC_SETUPDAT,
                      sizeof(struct usb_ctrlrequest), false);

        /* pass setup data to the upper layer */
        usb_core_control_request(&setup_data);
    }
}

/* Helper function called in IRQ context
 * for handling IN transactions.
 *
 * It is called when host ACKed the transfer
 * and FIFO is ready for new data.
 */
static void epin_handler(unsigned int epinirq)
{
    if (epinirq & 1)
    {
        /* EP0 IN irq */
    }

    if (epinirq & 2)
    {
        /* EP1 IN irq */
    }

    if (epinirq & 4)
    {
        /* EP2 IN irq */
    }
}

/* Helper function called in IRQ context
 * for handling OUT transactions.
 *
 * It is called when data reach UDC buffer.
 */
static void epout_handler(unsigned int epoutirq)
{
    if (epoutirq & 1)
    {
        /* EP0 OUT irq */
    }

    if (epoutirq & 2)
    {
        /* EP1 OUT irq */
    }

    if (epoutirq & 4)
    {
        /* EP2 OUT irq */
    }
}

/* return port speed FS=0, HS=1 */
int usb_drv_port_speed(void)
{
    return udc_speed;
}

#define BF_UDC_CON_EP_ENABLE(v) BF_UDC_OUT1CON_EP_ENABLE(v)
#define BF_UDC_CON_EP_TYPE(v) BF_UDC_OUT1CON_EP_TYPE(v)
#define BF_UDC_CON_SUBFIFOS_V(e) BF_UDC_OUT1CON_SUBFIFOS_V(e)
/* Reserve endpoint */
int usb_drv_request_endpoint(int type, int dir)
{
    int ep_dir = EP_DIR(dir);
    int ep_type = type & USB_ENDPOINT_XFERTYPE_MASK;

    /* EP1, EP2 */
    for (int ep_num=1; ep_num<USB_NUM_ENDPOINTS; ep_num++)
    {
        struct endpoint_t *endp = &endpoints[ep_num][ep_dir];
        if (endp->allocated)
            continue;

        /* mark EP as allocated */
        endp->type = ep_type;
        endp->dir = ep_dir;
        endp->allocated = true;

        /* enable and configure EP */
        EP_CON(ep_num, ep_dir) = BF_UDC_CON_EP_ENABLE(1) |
                                 BF_UDC_CON_EP_TYPE(ep_type) |
                                 BF_UDC_CON_SUBFIFOS_V(SINGLE);

        /* reset EP fifo */
        udc_ep_reset(ep_num, (ep_dir == DIR_IN) ? true : false);

        /* enable EP interrupt */
        if (ep_dir == DIR_IN)
        {
            UDC_IN04IEN |= (1 << ep_num);
        }
        else
        {
            UDC_OUT04IEN |= (1 << ep_num);
        }

        return ((dir & USB_ENDPOINT_DIR_MASK) | ep_num);
    }

    return -1;
}

/* Free endpoint */
void usb_drv_release_endpoint(int ep)
{
    int ep_num = EP_NUM(ep);
    int ep_dir = EP_DIR(ep);

    /* disable EP */
    EP_CON(ep_num, ep_dir) &= ~BF_UDC_CON_EP_ENABLE(1);

    if (ep_dir == DIR_IN)
    {
        UDC_IN04IEN &= ~(1 << ep_num);
    }
    else
    {
        UDC_OUT04IEN &= ~(1 << ep_num);
    }

    /* mark EP as free */
    endpoints[ep_num][ep_dir].allocated = false;
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

static int _usb_drv_send(int endpoint, void *ptr, int length, bool block)
{
    int ep_num = EP_NUM(endpoint);
    struct endpoint_t *ep = &endpoints[ep_num][DIR_IN];

    ep->buf = ptr;
    ep->len = ep->cnt = length;
    ep->block = block;
    ep->zlp = (length % max_pkt_size(ep) == 0) ?
                  true : false;

    /* prime transfer */
    ep_write(ep);

    if(block)
        semaphore_wait(&ep->complete, TIMEOUT_BLOCK);

    return 0;
}

/* Setup a send transfer. (blocking) */
int usb_drv_send(int endpoint, void *ptr, int length)
{
    return _usb_drv_send(endpoint, ptr, length, true);
}

/* Setup a send transfer. (non blocking) */
int usb_drv_send_nonblocking(int endpoint, void *ptr, int length)
{
    return _usb_drv_send(endpoint, ptr, length, false);
}

/* Setup a receive transfer. (non blocking) */
int usb_drv_recv(int endpoint, void* ptr, int length)
{
    int ep_num = EP_NUM(endpoint);
    struct endpoint_t *ep = &endpoints[ep_num][DIR_OUT];

    ep->buf = ptr;
    ep->len = ep->cnt = length;

    /* arm receive buffer */
    write_epcs(ep, 0);

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

#define BM_UDC_CON_STALL BM_UDC_OUT1CON_STALL
/* Check if endpoint is in stall state */
bool usb_drv_stalled(int endpoint, bool in)
{
    int ep_num = EP_NUM(endpoint);

    if (ep_num == 0)
    {
        return (UDC_EP0CS & BM_UDC_EP0CS_STALL) ? true : false;
    }
    else
    {
        return (EP_CON(ep_num, in) & BM_UDC_CON_STALL) ? true : false;
    }
}

/* Stall the endpoint. Usually set a flag in the controller */
void usb_drv_stall(int endpoint, bool stall, bool in)
{
    int ep_num = EP_NUM(endpoint);

    /* EP0 */
    if (ep_num == 0)
    {
        if (stall)
        {
            UDC_EP0CS |= BM_UDC_EP0CS_STALL;
        }
        else
        {
            UDC_EP0CS &= ~BM_UDC_EP0CS_STALL;
        }
    }
    else
    {
        if (stall)
        {
            EP_CON(ep_num, in) |= BM_UDC_CON_STALL;
        }
        else
        {
            EP_CON(ep_num, in) &= ~BM_UDC_CON_STALL;
        }
    }
}

/* one time init (once per connection) - basicaly enable usb core */
void usb_drv_init(void)
{
    /* ungate udc clock */
    atj213x_clk_enable(BM_CMU_DEVCLKEN_USBC);

    /* soft disconnect */
    UDC_USBCS |= BM_UDC_USBCS_SOFT_CONNECT;

    /* reset EP0 IN fifo */
    udc_ep_reset(0, true); /* IN0 */
    udc_ep_reset(0, false); /* OUT0 */

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

    /* enable interrupts from EP0 only */
    UDC_IN04IEN = 1;
    UDC_OUT04IEN = 1;

    /* unmask UDC interrupt in interrupt controller */
    atj213x_intc_unmask(BP_INTC_MSK_USB);

    /* is it actually needed that long? */
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
    atj213x_clk_disable(BM_CMU_DEVCLKEN_USBC);
}

int usb_detect(void)
{
    //if (PMU_LRADC & BM_PMU_LRADC_DC5V)
    //    return USB_INSERTED;
    //else
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
        usbirq_handler(usbirq);

        /* clear irq flags */
        UDC_USBIRQ = usbirq;
    }

    if (epoutirq)
    {
        epout_handler(epoutirq);

        /* clear irq flags */
        UDC_OUT04IRQ = epoutirq;
    }

    if (epinirq)
    {
        epin_handler(epinirq);

        /* clear irq flags */
        UDC_IN04IRQ = epinirq;
    }

    if (otgirq)
    {
        UDC_OTGIRQ = otgirq;
    }

    UDC_USBEIRQ = 0x50;
}
