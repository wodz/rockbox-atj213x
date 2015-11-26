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
    bltzal zero, load_addr    # ra = PC + 8, branch not taken
    nop

load_addr:
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

reloc:
    la     a1, _relocstart
    la     a2, _relocend
    beq    a0, a1, entry_point    # no relocation needed
    nop

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

core_irq_setup:
    li     t0, 0x00404000     # BEV=1 for C0_EBASE setup, IM6=1, IE=0
    mtc0   t0, C0_STATUS

    la     t0, _irqbase       # vectors base address must be 4k aligned
    mtc0   t0, C0_EBASE

    li     t0, 0x00004000
    mtc0   t0, C0_STATUS      # BEV=0, IM6=1, IE=0

    li     t1, 0x08800000
    mtc0   t1, C0_CAUSE       # DC=1, IV=1
    mtc0   zero, C0_INTCTL    # VS = 0

#if 0
iram_copy:
    la     a0, _iramcopy
    la     a1, _iramstart
    la     a2, _iramend
    subu   a3, a2, a1
    beqz   a3, clear_bss
    nop

    bal    code_copy
    nop
#endif

clear_bss:
    la     t0, _bss_start
    la     t1, _bss_end
    beq    t0, t1, clear_ibss
    nop

clear_bss_loop:
    sw     zero, 0(t0)
    bne    t0, t1, clear_bss_loop
    addiu  t0, 4

#if 0
clear_ibss:
    la     t0, _ibss_start
    la     t1, _ibss_end
    beq    t0, t1, stack_setup
    nop

clear_ibss_loop:
    sw     zero, 0(t0)
    bne    t0, t1, clear_ibss_loop
    addiu  t0, 4
#endif

stack_setup:
    # setup stack
    la     sp, stackend
    la     t0, stackbegin
    li     t1, 0xdeadbeef

stack_munge_loop:
    sw     t1, 0(t0)
    bne    t0, sp, stack_munge_loop
    addiu  t0, 4

irq_stack_setup:
    # setup irq stack
    la     k0, irqstackend
    la     t0, irqstackbegin

irq_stack_munge_loop:
    sw     t1, 0(t0)
    bne    t0, k0, irq_stack_munge_loop
    addiu  t0, 4

    # jump to C code with enabled interrupts
    la     t0, main
    jr.hb  t0
    ei

/* void code_copy(src, dst, dst_end)
 * naive version of memcpy() which does
 * move in 16bytes blocks and ensures
 * cache coherency
 *
 * dst should be 16bytes aligned otherwise
 * cache operations will have side  effects
 */
code_copy:
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
    bnez   a3, code_copy
    addiu  a0, a0, 16         # inc src addr
    jr     ra
    nop

    /* 0x000 - TLB refill handler
     * 0x100 - Cache error handler
     * 0x180 - Exception/Interrupt handler
     * 0x200 - Special Exception Interrupt handler (when IV is set in CP0_STATUS)
     */

    .section .vectors.tlb_refill,"ax",%progbits
    j      exception_handler
    nop

    .section .vectors.cache_error,"ax",%progbits
    j      exception_handler
    nop

    .section .vectors.exception,"ax",%progbits
    j      exception_handler
    nop

    /* this helper function prepares exception arguments,
     * switches to irqstack and finally call
     * system_exception(unsigned int sp, unsigned int cause, unsigned int epc)
     * exceptions are considered fatal and we don't bother to preserve anything
     */ 
    .extern system_exeption
    .section .vectors,"ax",%progbits

exception_handler:
    move   a0, sp
    la     sp, irqstackend
    mfc0   a1, C0_CAUSE
    mfc0   a2, C0_EPC
    la     k0, system_exception
    jr     k0
    nop

/* s0-s7 not saved as this are callee saved registers
 * CO_STATUS is saved to allow proper handling of exceptions
 * inside irq handlers
 *
 * Separate irqstack is used for context save and irq processing
 */
    .extern irqvector
    .section .vectors.interrupt,"ax",%progbits

irq_handler:
    # save SP and switch to irqstack
    move   k0, sp
    la     sp, irqstackend

    addiu  sp, sp, -96
    sw     k0, 0(sp)
    ehb
    mfc0   k0, C0_STATUS
    ehb
    sw     k0, 4(sp)

    # switch to kernel mode with irq disabled
    li     k0, 0x00004000
    mtc0   k0, C0_STATUS
    ehb

    # context save
    sw     AT,  8(sp)
    sw     v0, 12(sp)
    sw     v1, 16(sp)
    sw     a0, 20(sp)
    sw     a1, 24(sp)
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
    mfhi   k0
    mflo   k1
    sw     k0, 76(sp)
    sw     k1, 80(sp)
    sw     gp, 84(sp)
    sw     fp, 88(sp)
    sw     ra, 92(sp)

    # dispatch interrupt
    lui    t0, 0xb002         # INTC base
    lw     a0, 0(t0)          # INTC_PD
    lw     a1, 4(t0)          # INTC_MSK
    and    a0, a0, a1         # mask
    clz    a0, a0             # IRQ number

    # a0 holds irq number to be serviced
    # and is implicitely passed to UIRQ()
    # in case isr routine was not registered.
    #
    # In case of spurious interrupt there
    # is no active irq flagged in INTC_PD, a0 
    # will evaluate to 32 hence irqvector[] has
    # one more pointer then hw irqs and this
    # will trigger UIRQ()

    sll    a1, a0, 2          # offset
    la     a2, irqvector      # array address
    addu   a3, a2, a1         # irq handler pointer address
    lw     k1, 0(a3)
    jalr   k1                 # call handler function
    nop

    # context restore
    lw     ra, 92(sp)
    lw     fp, 88(sp)
    lw     gp, 84(sp)
    lw     k1, 80(sp)
    lw     k0, 76(sp)
    mtlo   k1
    mthi   k0
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
    lw     a1, 24(sp)
    lw     a0, 20(sp)
    lw     v1, 16(sp)
    lw     v0, 12(sp)
    lw     AT,  8(sp)
    lw     k0,  4(sp)
    mtc0   k0, C0_STATUS
    lw     sp,  0(sp)

    eret
    .set reorder
    .set at
