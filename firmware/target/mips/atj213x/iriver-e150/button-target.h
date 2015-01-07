/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2015 Marcin Bukat
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
#ifndef _BUTTON_TARGET_H_
#define _BUTTON_TARGET_H_

#define HAS_BUTTON_HOLD

/* Main unit's buttons */
#define BUTTON_ON          (1<<0)
#define BUTTON_LEFT        (1<<1)
#define BUTTON_RIGHT       (1<<2)
#define BUTTON_UP          (1<<3)
#define BUTTON_DOWN        (1<<4)
#define BUTTON_SELECT      (1<<5)
#define BUTTON_VOL_UP      (1<<6)
#define BUTTON_VOL_DOWN    (1<<7)


#define BUTTON_MAIN (BUTTON_ON|BUTTON_LEFT|BUTTON_RIGHT|\
                     BUTTON_UP|BUTTON_DOWN|BUTTON_SELECT|\
                     BUTTON_VOL_UP|BUTTON_VOL_DOWN)

#define POWEROFF_BUTTON BUTTON_ON
#define POWEROFF_COUNT 30

bool headphones_inserted(void);
bool button_hold(void);
#endif /* _BUTTON_TARGET_H_ */
