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
#ifndef __ATJ213X_H__
#define __ATJ213X_H__

#define IRAM_ORIG           0x94040000
#define IRAM_SIZE           (96 * 1024)

#define DRAM_ORIG           0x80000000
#define DRAM_SIZE           (MEMORYSIZE * 0x100000)

#define UNCACHED_DRAM_ADDR  0xa0000000
#define CACHED_DRAM_ADDR    0x80000000

/* 16 bytes per cache line */
#define CACHEALIGN_BITS     4

#define USB_NUM_ENDPOINTS   5
#define USB_DEVBSS_ATTR __attribute__((aligned(16)))
#endif /* __ATJ213X_H__ */
