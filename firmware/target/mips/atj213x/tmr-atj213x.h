/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2014 by Marcin Bukat
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

#ifndef TMR_ATJ213X_H
#define TMR_ATJ213X_H
 
enum
{
    TIMER_TICK, /* for tick task */
    TIMER_USER, /* for user timer */
};

void atj213x_timer_irq_clear(unsigned timer_nr);
void atj213x_timer_set(unsigned timer_nr, unsigned interval_ms, void (*cb)(void));
void atj213x_timer_start(unsigned timer_nr);
void atj213x_timer_stop(unsigned timer_nr);

#endif /* RTCTMR_ATJ213X_H */
