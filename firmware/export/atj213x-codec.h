/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Driver for internal Actions ATJ213x audio codec
 *
 * Copyright (c) 2017 Marcin Bukat
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
#ifndef _ATJ213X_CODEC_H_
#define _ATJ213X_CODEC_H_

#define AUDIOHW_CAPS    (MONO_VOL_CAP)

/* 31 steps of -1.8 dB which gives -55.8 to 0 dB range */
AUDIOHW_SETTING(VOLUME,     "dB", 1,  18,   -558,    0, -270)

#endif /* _ATJ213X_CODEC_H_ */
