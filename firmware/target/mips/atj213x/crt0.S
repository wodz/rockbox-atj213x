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

#include "mips.h"

    .extern main
    .global start

    .set mips32r2
    .set noreorder
    .set noat

    .section .init.text,"ax",%progbits

start:
    di                        # disable interrupts
    bltzal zero, 1f           # ra = PC + 8, branch not taken
    nop
1:
    addiu  a0, ra, -12        # calc real load address
                              # account for branch delay slot
                              # and very first 'di' instruction

core_clk_setup:
    la     t0, 0xb0010000     # CMU base
    li     t1, 0x440          # HOSC enable, bypass
    sw     t1, 0(t0)          # CMU_COREPLL

    li     t1, 0x350          # CORECLKS 24M, CCLKDIV = 1, SCLKDIV = 2,
                              # PCLKDIV = 4
    sw     t1, 0x0c(t0)       # CMU_BUSCLK
    li     t1, 0xc6           # HOSC enable, PLL enable, 6*6M = 36M
    sw     t1, 0(t0)          # CMU_COREPLL
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop                       # arbitrary 300ns delay as there is no
                              # PLL lock feedback
    li     t1, 0x390          # CORECLKS COREPLL, CCLKDIV = 1, SCLKDIV = 2,
                              # PCLKDIV = 4
    sw     t1, 0x0c(t0)       # CMU_BUSCLK

#ifndef BOOTLOADER
mmu_init:
    mfc0    t0, C0_Config1
    ext     t1, t0, 25, 6      # extract number of TLB entries

    mtc0    zero, C0_EntryLo0
    mtc0    zero, C0_EntryLo1
    mtc0    zero, C0_PageMask
    mtc0    zero, C0_Wired
    li      t2, 0x80000000

1:
    mtc0    t1, C0_Index       # store TLB index
    mtc0    t2, C0_EntryHi
    ehb                        # clear hazard
    tlbwi                      # write TLB entry
    addiu   t2, t2, 0x2000     # 4k * 2 per entry
    bnez    t1, 1b
    addiu   t1, t1, -1

// Last 12kB of DRAM is used for PTE
// each entry is 8 bytes struct:
// struct pte_t {
//     uint32_t entrylo0;
//     uint32_t entrylo1;
// }
//
// first 8kB of PTE is mapping DRAM and is itself mapped in kseg2
// 0xf0000000
// last 4kB of PTE is mapping IRAM and is itself mapped in kseg2
// 0xf1000000
// default 4kB * 2 page size is used
//
// Here we build PTE and write it to uncached address
mmu_pte_dram:
    li      t0, 0x1f
    la      t1, 0xa07fd000     # PTE base in kseg1
    li      t2, 1024           # number of DRAM entries
2:
    sw      t0, 0(t1)
    addiu   t0, t0, 0x40
    sw      t0, 4(t1)
    addiu   t0, t0, 0x40
    addiu   t2, t2, -1
    bnez    t2, 2b
    addiu   t1, t1, 8

mmu_pte_iram:
    li      t0, 0x50101f
    li      t2, 12             # number of IRAM entries
3:
    sw      t0, 0(t1)
    addiu   t0, t0, 0x40
    sw      t0, 4(t1)
    addiu   t0, t0, 0x40
    addiu   t2, t2, -1
    bnez    t2, 3b
    addiu   t1, t1, 8

mmu_map_pte_dram:
    mtc0    zero, C0_PageMask
    li      t0, 0
    mtc0    t0, C0_Index
    li      t1, 0xf0300000
    mtc0    t1, C0_EntryHi
    li      t2, 0x1ff5f
    mtc0    t2, C0_EntryLo0
    addiu   t2, 0x40
    mtc0    t2, C0_EntryLo1
    ehb
    tlbwi

    addiu   t0, t0, 1
    mtc0    t0, C0_Index

mmu_map_pte_iram:
    li      t1, 0xf0304000
    mtc0    t1, C0_EntryHi
    addiu   t2, 0x40
    mtc0    t2, C0_EntryLo0
    li      t2, 1            # set G bit so whole TLB entry will be global
    mtc0    t2, C0_EntryLo1
    ehb
    tlbwi

    addiu   t0, t0, 1
    mtc0    t0, C0_Wired
    ehb
#endif

cache_setup:
    la     t0, 0x80000000     # an idx op should use an unmappable address
    ori    t1, t0, 0x4000     # 16kB cache
    mtc0   zero, C0_TAGLO
    mtc0   zero, C0_TAGHI

cache_init_loop:
    cache  ICIndexStTag, 0(t0)    # index store icache tag
    cache  DCIndexStTag, 0(t0)    # index store dcache tag
    bne    t0, t1, cache_init_loop
    addiu  t0, t0, 0x10

    li     t0, 3              # enable cache for kseg0 accesses
    mtc0   t0, C0_CONFIG
    ehb

#ifndef BOOTLOADER
# unconditionaly reloc code
reloc:
    la     a1, _relocdst
    la     a2, _relocsize

    bal    code_copy
    nop
#endif

core_irq_setup:
    li     t0, 0x0040ff00 #0x00404000     # BEV=1 for C0_EBASE setup, IM6=1, IE=0
    mtc0   t0, C0_STATUS

    la     t0, _irqbase       # vectors base address must be 4k aligned
    mtc0   t0, C0_EBASE

    li     t0, 0x0000ff00 #0x00004000
    mtc0   t0, C0_STATUS      # BEV=0, IM6=1, IE=0

    li     t1, 0x08800000
    mtc0   t1, C0_CAUSE       # DC=1, IV=1
    mtc0   zero, C0_INTCTL    # VS = 0

iram_copy:
    la     a1, _iramdst
    la     a2, _iramsize
    beqz   a2, entry_point_jump
    nop

    la     a0, _iramsrc

    bal    code_copy
    nop

entry_point_jump:
    la     t0, entry_point
    sync
    jr.hb  t0
    nop

entry_point:
intc_setup:
    li     t0, 0xb0020000     # INTC base
    sw     zero, 4(t0)        # INTC_MSK mask all interrupt sources

clear_bss:
    la     t0, _bss_start
    la     t1, _bss_end
    beq    t0, t1, clear_ibss
    nop
1:
    addiu  t0, t0, 4
    bne    t0, t1, 1b
    sw     zero, -4(t0)

clear_ibss:
    la     t0, _ibss_start
    la     t1, _ibss_end
    beq    t0, t1, stack_setup
    nop
1:
    addiu  t0, t0, 4
    bne    t0, t1, 1b
    sw     zero, -4(t0)

stack_setup:
    # setup stack
    la     sp, stackend
    la     t0, stackbegin
    li     t1, 0xdeadbeef
1:
    addiu  t0, t0, 4
    bne    t0, sp, 1b
    sw     t1, -4(t0)

irq_stack_setup:
    # setup irq stack
    la     t3, irqstackend
    la     t0, irqstackbegin
1:
    addiu  t0, t0, 4
    bne    t0, t3, 1b
    sw     t1, -4(t0)

main_jump:
    # jump to C code with enabled interrupts
    la     t0, main
    jr.hb  t0
    ei

/* void code_copy(src, dst, size)
 * naive version of memcpy() which does
 * move in 16bytes blocks and ensures
 * cache coherency
 *
 * dst should be 16bytes aligned otherwise
 * cache operations will have side  effects
 *
 * clobbers t0-t3, a0-a3
 */
code_copy:
    beq    a0, a1, 2f
    addu   a2, a1, a2         # calc end address
                              # end = dst + size
1:
    lw     t0, 0(a0)          # src
    lw     t1, 4(a0)
    lw     t2, 8(a0)
    lw     t3, 12(a0)

    sw     t0, 0(a1)          # dst
    sw     t1, 4(a1)
    sw     t2, 8(a1)
    sw     t3, 12(a1)

    synci  0(a1)              # dcache writeback invalidate
                              # icache invalidate

    addiu  a1, a1, 16         # inc dst addr
    slt    a3, a1, a2
    bnez   a3, 1b             # if (dst < end_addr) goto 1
    addiu  a0, a0, 16         # inc src addr
2:
    jr     ra
    nop

    /* 0x000 - TLB refill handler
     * 0x100 - Cache error handler
     * 0x180 - Exception/Interrupt handler
     * 0x200 - Special Exception Interrupt handler (when IV is set in CP0_STATUS)
     */

// TLB refill handler for non standard pte entry
// We need 8*1024*1024/(2*4096) = 1024 entries
// 8 bytes each => 8kB for DRAM
// 96*1024/(2*4096) = 12
// 8 bytes each = 96 bytes for IRAM
// hence 1 wired entry for DRAM PTE and 1 wired entry for IRAM PTE
// this saves 1 wired entry compared to standard MIPS 16b PTE entry
// and shrinks PTE table size from 20kB down to 12kB
// PMA for PTE table is then 0x7fd000
//
// we don't need to setup C0_Context register
// DRAM PTE should be mapped @ 0xf0300000
// IRAM PTE should be mapped @ 0xf0304000
// struct pte_t {
//     uint32_t entrylo0;
//     uint32_t entrylo1;
// }

    .section .vectors.tlb_refill,"ax",%progbits
tlb_refill:
    mfc0   k1, C0_BadVAddr    # failing VMA
    lui    k0, 0xf000         # PTE base
    srl    k1, k1, 13         # pgnum = VMA >> 13; pte_entry_offset = pgnum * 8
    sll    k1, k1, 3
    or     k1, k1, k0         # PTE entry address = PTE_base | pte_entry_offset
    lw     k0, 0(k1)          # entrylo0
    lw     k1, 4(k1)          # entrylo1
    mtc0   k0, C0_EntryLo0
    mtc0   k1, C0_EntryLo1
    ehb
    tlbwr
    eret

    .section .vectors.cache_error,"ax",%progbits
cache_error:
    b      exception_handler
    nop

    .section .vectors.exception,"ax",%progbits
general_exception:
    b      exception_handler
    nop

    /* this helper function prepares exception arguments,
     * switches to irqstack and finally call
     * system_exception(unsigned int sp, unsigned int cause, unsigned int epc)
     * exceptions are considered fatal and we don't bother to preserve anything
     */ 
    .extern system_exeption
    .global exception_handler
    .section .vectors,"ax",%progbits

exception_handler:
    /* save SP */
    move k0, sp

    /* switch to irqstack
     * exception frame is 128 bytes long
     * irq frame is 96 bytes long
     * so we move 228 bytes up in order to preserve irq frame
     */
    la     sp, irqstackend
    addiu  sp, sp, -228

    /* save register file */
    sw     k0, 116(sp)
    mfc0   k1, C0_STATUS
    ehb
    sw     k1, 0(sp)

    sw     AT,   4(sp)
    sw     v0,   8(sp)
    sw     v1,  12(sp)
    sw     a0,  16(sp)
    sw     a1,  20(sp)
    sw     a2,  24(sp)
    sw     a3,  28(sp)
    sw     t0,  32(sp)
    sw     t1,  36(sp)
    sw     t2,  40(sp)
    sw     t3,  44(sp)
    sw     t4,  48(sp)
    sw     t5,  52(sp)
    sw     t6,  56(sp)
    sw     t7,  60(sp)
    sw     s0,  64(sp)
    sw     s1,  68(sp)
    sw     s2,  72(sp)
    sw     s3,  76(sp)
    sw     s4,  80(sp)
    sw     s5,  84(sp)
    sw     s6,  88(sp)
    sw     s7,  92(sp)
    sw     t8,  96(sp)
    sw     t9, 100(sp)
    mfhi   k0
    mflo   k1
    sw     k0, 104(sp)
    sw     k1, 108(sp)
    sw     gp, 112(sp)
    /* sp saved early */
    sw     fp, 120(sp)
    sw     ra, 124(sp)


    mfc0   a0, C0_CAUSE
    mfc0   a1, C0_EPC
    ehb
    move   a2, sp
    /* void system_exception(unsigned int cause,
     *                       unsigned int epc,
     *                       unsigned int *ef)
     */
    la     k0, system_exception
    jr     k0
    nop

/* s0-s7 not saved as this are callee saved registers
 * C0_STATUS and C0_EPC are saved to allow proper handling of exceptions
 * inside irq handlers
 *
 * Separate irqstack is used for context save and irq processing
 *
 * Code flow:
 * 1) Save SP in k0. Check nesting level. If level is 0 switch to irqstack.
 *    irqstack needs to be in kseg0 to prevent TLB miss on context save.
 * 2) make room on SP for irq frame
 * 3) save context part I and increment nestlevel
 *    SR(EXL) = 1 so irqs are suppressed
 *    TLB misses are avoided as we run in kseg0 and SP is in kseg0
 *    save enough context to reenable irqs:
 *    - C0_STATUS, C0_EPC, original sp, a0, a1
 * 4) mask all irqs with lower level then current one
 * 5) clear SR(EXL) to allow nested interrupts with higher priority
 * 6) save context part II (this can be interrupted)
 *    - a2-a3, v0-v1, at, hi, lo, gp, fp, ra
 * 7) dispatch interrupt
 *    this can be:
 *    a) interrupted by higher priority interrupt
 *    b) generate TLB miss as handlers are in kseg2 generaly
 * 8) restore context part I (can still be interrupted)
 * 9) restore C0_STATUS which effectively sets SR(EXL) = 1
 *    and protects us from interrupts
 *    restore part II
 * 10) decrement nestlevel
 * 11) return from interrupt which clears hazards and SR(EXL) bit
 *     atomicaly
 *
 */
    .extern irqvector
    .section .vectors.interrupt,"ax",%progbits

irq_handler:
    # 1) save original SP and switch to irqstack if required
    la     k0, nestlevel
    lw     k1, 0(k0)
    bnez   k1, 1f              # this is nested exception
    move   k0, sp
    
    la     sp, irqstackend

1:
    # 2) make room for exception frame
    addiu  sp, sp, -100

    # 3) save context part I, increment nestlevel
    sw     k0, 88(sp)      # save original SP
    sw     a0, 20(sp)      # free a0,a1 for internal use
    sw     a1, 24(sp)
    mfc0   k0, C0_STATUS
    addiu  k1, k1, 1       # increment nestlevel
    sw     k0, 0(sp)       # save C0_STATUS
    mfc0   a0, C0_EPC
    la     a1, nestlevel
    sw     a0, 4(sp)       # save C0_EPC
    sw     k1, 0(a1)       # save incremented nestlevel

    # 4) mask all irqs with lower level then current one
    mfc0   k1, C0_CAUSE
    and    k1, k1, k0      # C0_CAUSE & C0_STATUS we look for CAUSE(IP) only
    ext    k1, k1, 8, 8    # extract CAUSE(IP) bitfield
    sll    k1, k1, 24      # move it left so we can use clz
    clz    k1, k1          # calculate active IP position
    negu   k1, k1
    addiu  k1, k1, 8   
    li     a0, 0xff
    sll    a0, a0, k1      # now we have SR(IM) mask with only higher
                           # then current irq source unmasked
    ins    k0, a0, 8, 8    # now k0 holds SR with IM set properly
    ins    k0, zero, 1, 4  # clear SR(EXL), SR(ERL), SR(KSU) bits

    # 5) put prepared value back to SR
    #    since SR(EXL) is cleared this effectively reenables interrupts
    #    from now on k0, k1 cannot be used as nested interrupt or
    #    TLB miss will clobber the content
    mtc0   k0, C0_STATUS

    # 6) save context part II (this can be interrupted)
    sw     AT,  8(sp)
    sw     v0, 12(sp)
    sw     v1, 16(sp)
#    sw     a0, 20(sp) saved already in step 3)
#    sw     a1, 24(sp) saved already in step 3)
    sw     a2, 28(sp)
    sw     a3, 32(sp)
    sw     t0, 36(sp)
    sw     t1, 40(sp)
    sw     t2, 44(sp)
    sw     t3, 48(sp)
    sw     t4, 52(sp)
    sw     t5, 56(sp)
    sw     t6, 60(sp)
    sw     t7, 64(sp)
    sw     t8, 68(sp)
    sw     t9, 72(sp)
    mfhi   a0
    mflo   a1
    sw     a0, 76(sp)
    sw     a1, 80(sp)
    sw     gp, 84(sp)
    /* sp saved early */
    sw     fp, 92(sp)
    sw     ra, 96(sp)

    # 7) dispatch interrupt
    lui    t0, 0xb002         # INTC base
    lw     t1, 0(t0)          # INTC_PD
    lw     t2, 4(t0)          # INTC_MSK
    and    t3, t1, t2         # mask
    clz    a0, t3             # IRQ number

    # a0 holds irq number to be serviced
    # and is implicitely passed to UIRQ()
    # in case isr routine was not registered.
    #
    # In case of spurious interrupt there
    # is no active irq flagged in INTC_PD, a0 
    # will evaluate to 32 hence irqvector[] has
    # one more pointer then hw irqs and this
    # will trigger UIRQ()

    sll    t1, a0, 2          # offset
    la     t2, irqvector      # array address
    addu   t3, t2, t1         # irq handler pointer address
    lw     t0, 0(t3)
    jalr   t0                 # call handler function
    nop

    # 8) context restore part I
    lw     ra, 96(sp)
    lw     fp, 92(sp)
    lw     gp, 84(sp)
    lw     a1, 80(sp)
    lw     a0, 76(sp)
    mtlo   a1
    mthi   a0
    lw     t9, 72(sp)
    lw     t8, 68(sp)
    lw     t7, 64(sp)
    lw     t6, 60(sp)
    lw     t5, 56(sp)
    lw     t4, 52(sp)
    lw     t3, 48(sp)
    lw     t2, 44(sp)
    lw     t1, 40(sp)
    lw     t0, 36(sp)
    lw     a3, 32(sp)
    lw     a2, 28(sp)
#    lw     a1, 24(sp)
#    lw     a0, 20(sp)
    lw     v1, 16(sp)
    lw     v0, 12(sp)
    lw     AT,  8(sp)
    lw     a0,  0(sp)

    mtc0   a0, C0_STATUS
    ehb

    # 9) now C0_STATUS is restored so SR(EXL) = 1
    #    and we are protected from nested interrupts
    #    restore part II

    lw     a1, 24(sp)
    lw     a0, 20(sp)

    lw     k0, 4(sp)
    mtc0   k0, C0_EPC    # restore C0_EPC

    lw     sp,  88(sp)   # restore SP

    # 10) decrement nestlevel
    la     k0, nestlevel
    lw     k1, 0(k0)
    subu   k1, k1, 1
    sw     k1, 0(k0)

    # 11) return from interrupt clearing any hazard
    #     and clearing SR(EXL) bit

    eret
    .set reorder
    .set at

nestlevel:
    .word 0
