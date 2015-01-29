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

#include <stdbool.h>
#include "kernel.h"
#include "system.h"
#include "i2c-atj213x.h"
#include "regs/regs-cmu.h"
#include "regs/regs-i2c.h"
#include "regs/regs-gpio.h"

static struct mutex i2c_mtx[2];

static bool atj213x_i2c_wait_finish(unsigned iface)
{
    unsigned tmo = current_tick + HZ/10;

    while (!TIME_AFTER(current_tick, tmo))
    {
        if (I2C_STAT(iface) & BM_I2C_STAT_TRC)
        {
            yield();
        }
        else
        {
            I2C_STAT(iface) = BM_I2C_STAT_TRC;
            return true;
        }
    }
    return false;
}

static bool atj213x_i2c_start(unsigned iface, unsigned char address,
                              bool repeat)
{
    I2C_ADDR(iface) = address;

    if (repeat)
        I2C_CTL(iface) = BM_I2C_CTL_EN | 
                         BF_I2C_CTL_GBCC_V(REPEATED_START) |
                         BM_I2C_CTL_RB; /* 0x8e */
    else
        I2C_CTL(iface) = BM_I2C_CTL_EN |
                         BF_I2C_CTL_GBCC_V(START) |
                         BM_I2C_CTL_RB; /* 0x86 */

    return atj213x_i2c_wait_finish(iface);
}

static bool atj213x_i2c_stop(unsigned iface)
{
    unsigned tmo = current_tick + HZ/10;
    I2C_CTL(iface) = BM_I2C_CTL_EN |
              BF_I2C_CTL_GBCC_V(STOP) |
              BM_I2C_CTL_RB |
              BM_I2C_CTL_GRAS; /* 0x8b */

    while (!(I2C_STAT(iface) & BM_I2C_STAT_STPD))
    {
        if (TIME_AFTER(current_tick, tmo))
        {
            I2C_STAT(iface) = BM_I2C_STAT_STPD;
            return false;
        }
        yield();
    }

    I2C_STAT(iface) = BM_I2C_STAT_STPD;
    return true;
}

static bool atj213x_i2c_write_byte(unsigned iface, unsigned char data)
{
    I2C_DAT(iface) = data;
    I2C_CTL(iface) = 0x82;

    return atj213x_i2c_wait_finish(iface);
}

static bool atj213x_i2c_read_byte(unsigned iface, unsigned char *data,
                                  bool ack)
{
    if (ack)
        I2C_CTL(iface) = BM_I2C_CTL_EN |
                         BM_I2C_CTL_RB; /* 0x82 */
    else
        I2C_CTL(iface) = BM_I2C_CTL_EN |
                         BM_I2C_CTL_RB |
                         BM_I2C_CTL_GRAS; /* 0x83 */

    if (atj213x_i2c_wait_finish(iface))
    {
        *data = I2C_DAT(iface);
        return true;
    }

    return false;
}

static void atj213x_i2c_reset(unsigned iface)
{
    I2C_CTL(iface) = 0;
    udelay(10);
    I2C_CTL(iface) = BM_I2C_CTL_EN;
}

void atj213x_i2c_init(unsigned iface)
{
    CMU_DEVCLKEN |= BM_CMU_DEVCLKEN_I2C; /* enable i2c APB clock */

    if (iface == 1)
        GPIO_MFCTL1 &= ~BM_GPIO_MFCTL1_I2C1SS;
    else
        GPIO_MFCTL1 |= BF_GPIO_MFCTL1_U2TR_V(I2C2_SCL_SDA);

    atj213x_i2c_reset(iface);
    mutex_init(&i2c_mtx[iface]);
}

void atj213x_i2c_set_speed(unsigned iface, unsigned i2cfreq)
{
    unsigned int pclkfreq = atj213x_get_pclk();
    unsigned int clkdiv = (((pclkfreq + (i2cfreq - 1))/i2cfreq)+15)/16;

    I2C_CLKDIV(iface) = clkdiv;
}

int atj213x_i2c_read(unsigned iface, unsigned char slave, int address,
                     int len, unsigned char *data)
{
    int i, ret = 0;

    mutex_lock(&i2c_mtx[iface]);

    if (address >= 0)
    {
        /* START */
        if (! atj213x_i2c_start(iface, slave & ~1, false))
        {
            ret = 1;
            goto end;
        }

        /* write address */
        if (! atj213x_i2c_write_byte(iface, address))
        {
            ret = 2;
            goto end;
        }
    }

    /* REPEATED START */
    if (! atj213x_i2c_start(iface, slave | 1, true))
    {
        ret = 3;
        goto end;
    }

    for (i=0; i<len-1; i++)
    {
        if (! atj213x_i2c_read_byte(iface, data++, I2C_ACK))
        {
            ret = 4;
            goto end;
        }
    }

    if (! atj213x_i2c_read_byte(iface, data, I2C_NACK))
    {
        ret = 5;
        goto end;
    }


   /* STOP */
    if (! atj213x_i2c_stop(iface))
        ret = 6;

end:
    mutex_unlock(&i2c_mtx[iface]);
    return ret;
}

int atj213x_i2c_write(unsigned iface, unsigned char slave, int address,
                      int len, unsigned char *data)
{
    int ret = 0;

    mutex_lock(&i2c_mtx[iface]);

    /* START */
    if (! atj213x_i2c_start(iface, slave & ~1, false))
    {
        ret = 1;
        goto end;
    }

    if (address >= 0)
    {
        if (! atj213x_i2c_write_byte(iface, address))
        {
            ret = 2;
            goto end;
        }
    }

    /* write data */
    while (len--)
    {
        if (! atj213x_i2c_write_byte(iface, *data++))
        {
            ret = 4;
            goto end;
        }
    }

    /* STOP */
    if (! atj213x_i2c_stop(iface))
    {
        ret = 5;
    }

end:
    mutex_unlock(&i2c_mtx[iface]);

    return ret;
}
