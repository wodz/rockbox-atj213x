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
#ifndef SYSTEM_TARGET_H
#define SYSTEM_TARGET_H

#include "mipsregs.h"

#define CACHE_SIZE      16*1024
#define CACHE_LINE_SIZE 16
#include "mmu-mips.h"

#define CPUFREQ_DEFAULT  50000000
#define CPUFREQ_NORMAL   50000000
#define CPUFREQ_MAX     200000000

#define STORAGE_WANTS_ALIGN
#define disable_irq() disable_interrupt()
#define enable_irq()  enable_interrupt()
#define HIGHEST_IRQ_LEVEL 0
#define set_irq_level(status)  set_interrupt_status((status), ST0_IE)
#define disable_irq_save()     disable_interrupt_save(ST0_IE)
#define restore_irq(c0_status) restore_interrupt(c0_status)
#define UNCACHED_ADDRESS(addr)    ((unsigned int)(addr) | 0xA0000000)
#define UNCACHED_ADDR(x)          UNCACHED_ADDRESS((x))
#define PHYSADDR(x)               ((x) & 0x1fffffff)

void udelay(unsigned usecs);
static inline void mdelay(unsigned msecs)
{
    udelay(1000 * msecs);
}



#if 0
/* Write DCache back to RAM for the given range and remove cache lines
 * from DCache afterwards */
void commit_discard_dcache_range(const void *base, unsigned int size);

static inline void commit_dcache(void) {}
void commit_discard_dcache(void);
void commit_discard_idcache(void);
#endif


/* MIPS32R2 variants */
static inline uint16_t swap16_hw(uint16_t value)
{
    asm (
         "wsbh   %0, %1\n"
         : "=r" (value)
         : "r"  (value)
        );

    return value;
}

static inline uint32_t swap32_hw(uint32_t value)
{
    asm (
         "wsbh   %0, %1\n"
         "rotr   %0, %0, 16\n"
         : "=r" (value)
         : "r"  (value)
        );

    return value;
}

static inline uint32_t swap_odd_even32_hw(uint32_t value)
{
    asm (
         "wsbh   %0, %1\n"
         : "=r" (value)
         : "r"  (value)
        );

    return value;
}

/* This one returns the old status */
static inline int set_interrupt_status(int status, int mask)
{
    unsigned int res, oldstatus;

    res = oldstatus = read_c0_status();
    res &= ~mask;
    res |= (status & mask);
    write_c0_status(res);

    return oldstatus;
}

static inline void enable_interrupt(void)
{
    /* Set IE bit */
    asm volatile("ei\n");
}

static inline void disable_interrupt(void)
{
    /* Clear IE bit */
    asm volatile("di\n");
}

static inline int disable_interrupt_save(int mask)
{
    return set_interrupt_status(0, mask);
}

static inline void restore_interrupt(int status)
{
    write_c0_status(status);
}

/* linux kernel does rather sophisticated things here
 * so maybe we will need a well
 */
static inline void core_sleep(void)
{
    asm volatile(
                 "ei\n"
                 "wait\n"
                );
}
#endif /* SYSTEM_TARGET_H */
