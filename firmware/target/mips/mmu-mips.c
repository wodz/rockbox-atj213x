/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2009 by Maurus Cuelenaere
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

#include "config.h"
#include "mips.h"
#include "mipsregs.h"
#include "system.h"
#include "mmu-mips.h"

#define BARRIER                            \
    __asm__ __volatile__(                  \
    "    .set    noreorder          \n"    \
    "    nop                        \n"    \
    "    nop                        \n"    \
    "    nop                        \n"    \
    "    nop                        \n"    \
    "    nop                        \n"    \
    "    nop                        \n"    \
    "    .set    reorder            \n");

#define DEFAULT_PAGE_SHIFT       PL_4K
#define DEFAULT_PAGE_MASK        PM_4K
#define UNIQUE_ENTRYHI(idx, ps)  (A_K0BASE + ((idx) << (ps + 1)))
#define ASID_MASK                M_EntryHiASID
#define VPN2_SHIFT               S_EntryHiVPN2
#define PFN_SHIFT                S_EntryLoPFN
#define PFN_MASK                 0xffffff
static void local_flush_tlb_all(void)
{
    unsigned long old_ctx;
    int entry;
    unsigned int old_irq = disable_irq_save();
    
    /* Save old context and create impossible VPN2 value */
    old_ctx = read_c0_entryhi();
    write_c0_entrylo0(0);
    write_c0_entrylo1(0);
    BARRIER;

    /* Blast 'em all away. */
    for(entry = 0; entry < 32; entry++)
    {
        /* Make sure all entries differ. */
        write_c0_entryhi(UNIQUE_ENTRYHI(entry, DEFAULT_PAGE_SHIFT));
        write_c0_index(entry);
        BARRIER;
        tlb_write_indexed();
    }
    BARRIER;
    write_c0_entryhi(old_ctx);
    
    restore_irq(old_irq);
}

static void add_wired_entry(unsigned long entrylo0, unsigned long entrylo1,
                            unsigned long entryhi,  unsigned long pagemask)
{
    unsigned long wired;
    unsigned long old_pagemask;
    unsigned long old_ctx;
    unsigned int  old_irq = disable_irq_save();
    
    old_ctx = read_c0_entryhi() & ASID_MASK;
    old_pagemask = read_c0_pagemask();
    wired = read_c0_wired();
    write_c0_wired(wired + 1);
    write_c0_index(wired);
    BARRIER;
    write_c0_pagemask(pagemask);
    write_c0_entryhi(entryhi);
    write_c0_entrylo0(entrylo0);
    write_c0_entrylo1(entrylo1);
    BARRIER;
    tlb_write_indexed();
    BARRIER;

    write_c0_entryhi(old_ctx);
    BARRIER;
    write_c0_pagemask(old_pagemask);
    local_flush_tlb_all();
    restore_irq(old_irq);
}

void map_address(unsigned long virtual, unsigned long physical,
                 unsigned long length, unsigned int cache_flags)
{
    unsigned long entry0  = (physical & PFN_MASK) << PFN_SHIFT;
    unsigned long entry1  = ((physical+length) & PFN_MASK) << PFN_SHIFT;
    unsigned long entryhi = virtual & ~VPN2_SHIFT;
    
    entry0 |= (M_EntryLoG | M_EntryLoV | (cache_flags << S_EntryLoC) );
    entry1 |= (M_EntryLoG | M_EntryLoV | (cache_flags << S_EntryLoC) );
    
    add_wired_entry(entry0, entry1, entryhi, DEFAULT_PAGE_MASK);
}

void mmu_init(void)
{
    write_c0_pagemask(DEFAULT_PAGE_MASK);
    write_c0_wired(0);
    write_c0_framemask(0);
    
    local_flush_tlb_all();
/*
    map_address(0x80000000, 0x80000000, 0x4000, K_CacheAttrC);
    map_address(0x80004000, 0x80004000, MEMORYSIZE * 0x100000, K_CacheAttrC);
*/
}

#if CONFIG_CPU == JZ4732
#define INVALIDATE_BTB()                     \
do {                                         \
        unsigned long tmp;                   \
        __asm__ __volatile__(                \
        "    .set noreorder       \n"        \
        "    .set mips32          \n"        \
        "    mfc0 %0, $16, 7      \n"        \
        "    nop                  \n"        \
        "    ori  %0, 2           \n"        \
        "    mtc0 %0, $16, 7      \n"        \
        "    nop                  \n"        \
        "    .set mips0           \n"        \
        "    .set reorder         \n"        \
        : "=&r"(tmp));                       \
    } while (0)

#define SYNC_WB() __asm__ __volatile__ ("sync")
#else
#define INVALIDATE_BTB() do { } while(0)
#define SYNC_WB() do { } while(0)
#endif
#define __CACHE_OP(op, addr)                 \
    __asm__ __volatile__(                    \
    "    .set    noreorder        \n"        \
    "    .set    mips32\n\t       \n"        \
    "    cache   %0, %1           \n"        \
    "    .set    mips0            \n"        \
    "    .set    reorder          \n"        \
    :                                        \
    : "i" (op), "m"(*(unsigned char *)(addr)))

/* rockbox cache api */

/* Writeback whole D-cache
 * on MIPS alias to commit_discard_dcache() as there is no index type
 * variant of writeback only operation
 */
void commit_dcache(void) __attribute__((alias("commit_discard_dcache")));

/* Writeback whole D-cache and invalidate D-cache lines */
void commit_discard_dcache(void)
{
    unsigned int i;

    asm volatile (".set   noreorder  \n"
                  ".set   mips32     \n"
                  "mtc0   $0, $28    \n" /* TagLo */
                  "mtc0   $0, $29    \n" /* TagHi */
                  ".set   mips0      \n"
                  ".set   reorder    \n"
                  );

    /* Use index type operation and iterate whole cache */
    for (i=A_K0BASE; i<A_K0BASE+CACHE_SIZE; i+=CACHE_LINE_SIZE)
        __CACHE_OP(DCIndexWBInv, i);

    SYNC_WB();
}

/* Writeback lines of D-cache corresponding to address range and 
 * invalidate those D-cache lines
 */
void commit_discard_dcache_range(const void *base, unsigned int size)
{
    char *s;

    if (size > CACHE_SIZE)
    {
        commit_discard_dcache();
        return;
    }

    for (s=(char *)base; s<(char *)base+size; s+=CACHE_LINE_SIZE)
        __CACHE_OP(DCHitWBInv, s);

    SYNC_WB();
}

/* Invalidate D-cache lines corresponding to address range
 * WITHOUT writeback
 */
void discard_dcache_range(const void *base, unsigned int size)
{
    char *s;

    if (size > CACHE_SIZE)
    {
        commit_discard_dcache();
        return;
    }

    for (s=(char *)base; s<(char *)base+size; s+=CACHE_LINE_SIZE)
        __CACHE_OP(DCHitInv, s);
}

/*
 * Writeback + invalidate the entire D-cache
 * Invalidate the entire I-cache
 */
void commit_discard_idcache(void)
{
    unsigned int i;

    for(i=A_K0BASE; i<A_K0BASE+CACHE_SIZE; i+=CACHE_LINE_SIZE)
    {
        __CACHE_OP(DCIndexWBInv, i);
        __CACHE_OP(ICIndexInv, i);
    }

    INVALIDATE_BTB();
    SYNC_WB();
}
