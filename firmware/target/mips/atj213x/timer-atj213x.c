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
 
#include "config.h"
#include "system.h"
#include "kernel.h"
#include "timer.h"
#include "rtctmr-atj213x.h"

bool timer_set(long cycles, bool start)
{
   atj213x_timer_stop(TIMER_USER);

   /* optionally unregister any previously registered timer user */
    if (start)
    {
        if (pfn_unregister != NULL)
        {
            pfn_unregister();
            pfn_unregister = NULL;
        }
    }

    atj213x_timer_set(TIMER_USER, cycles*1000/HZ, pfn_timer);
    return true;
}

bool timer_start(void)
{
    atj213x_timer_start(TIMER_USER);
    return true;
}

void timer_stop(void)
{
    atj213x_timer_stop(TIMER_USER);
}
