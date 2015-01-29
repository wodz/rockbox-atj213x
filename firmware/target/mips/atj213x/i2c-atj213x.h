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

#ifndef I2C_ATJ213X_H
#define I2C_ATJ213X_H
 
enum
{
    I2C_NACK,
    I2C_ACK
};

enum
{
    I2C1 = 1,
    I2C2 = 2
};

void atj213x_i2c_init(unsigned iface);
void atj213x_i2c_set_speed(unsigned iface, unsigned i2cfreq);
int atj213x_i2c_read(unsigned iface, unsigned char slave, int address,
                     int len, unsigned char *data);
int atj213x_i2c_write(unsigned iface, unsigned char slave, int address,
                      int len, unsigned char *data);
#endif /* I2C_ATJ213X_H */
