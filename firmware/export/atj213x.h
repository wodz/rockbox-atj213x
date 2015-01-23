/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2011 by Amaury Pouly
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
#ifndef __IMX233_H__
#define __IMX233_H__

#define IRAM_ORIG           0x94040000
#define IRAM_SIZE           (96 * 1024)

#define DRAM_ORIG           0x80000000
#define DRAM_SIZE           (MEMORYSIZE * 0x100000)

#define UNCACHED_DRAM_ADDR  0xa0000000
#define CACHED_DRAM_ADDR    0x80000000

/* 16 bytes per cache line */
#define CACHEALIGN_BITS     4


#endif /* __ATJ213X_H__ */
