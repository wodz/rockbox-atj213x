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

#include "panic.h"
#include "button.h"
#include "backlight-target.h"
#include "font.h"
#include "lcd.h"
#include "mips.h"
#include "system.h"

#include "wdt-atj213x.h"
#include "tmr-atj213x.h"
#include "pmu-atj213x.h"

#include "regs/regs-cmu.h"
#include "regs/regs-sdr.h"

__asm__("\n\
.section .text\n\
.global hwstub\n\
.align 4\n\
hwstub:\n\
.incbin \"/home/wodz/rockbox-wodz/utils/hwstub/stub/atj213x/build/hwstub.bin\"\n");
extern unsigned char hwstub[];

void call_hwstub(void)
{
    asm volatile (
        "la $8, hwstub\n"
        "jr.hb $8\n"
        "nop\n"
    );
}

#define default_interrupt(name) \
  extern __attribute__((weak,alias("UIRQ"))) void name (void)

default_interrupt(INT_MCA);
default_interrupt(INT_SD);
default_interrupt(INT_MHA);
default_interrupt(INT_USB);
default_interrupt(INT_DSP);
default_interrupt(INT_PCNT);
default_interrupt(INT_WD);
default_interrupt(INT_T1);
default_interrupt(INT_T0);
default_interrupt(INT_RTC);
default_interrupt(INT_DMA);
default_interrupt(INT_KEY);
default_interrupt(INT_EXT);
default_interrupt(INT_IIC2);
default_interrupt(INT_IIC1);
default_interrupt(INT_ADC);
default_interrupt(INT_DAC);
default_interrupt(INT_NAND);
default_interrupt(INT_YUV);

/* This alias makes it possible to define irqvector[]
 * without explicit casting every single occurance of UIRQ
 * We relay on the fact that ISR functions are called only
 * from irq_handler() which prepares evironment accordingly
 */
void __UIRQ(void) __attribute__((alias("UIRQ")));

/* TRICK ALERT !!!!
 * The table is organized in reversed order so
 * clz on INTC_PD returns the index in this table
 */
void (* const irqvector[])(void) __attribute__((used)) =
{
    __UIRQ, __UIRQ, __UIRQ, __UIRQ, __UIRQ, INT_YUV, __UIRQ, INT_NAND,
    __UIRQ, INT_DAC, INT_ADC, __UIRQ, __UIRQ, INT_IIC1, INT_IIC2, __UIRQ,
    __UIRQ, INT_EXT, INT_KEY, INT_DMA, INT_RTC, INT_T0, INT_T1, INT_WD,
    INT_PCNT, __UIRQ, INT_DSP, INT_USB, INT_MHA, INT_SD, __UIRQ, INT_MCA,
    __UIRQ
};

static const char * const irqname[] =
{
    "IRQ31","IRQ30","IRQ29","IRQ28","IRQ27","INT_YUV","IRQ25","INT_NAND",
    "IRQ23","INT_DAC","INT_ADC","IRQ20","IRQ19","INT_IIC1","INT_IIC2","IRQ16",
    "IRQ15","INT_EXT","INT_KEY","INT_DMA","INT_RTC","INT_T0","INT_T1","INT_WD",
    "INT_PCNT","IRQ6","INT_DSP","INT_USB","INT_MHA","INT_SD","UIRQ1","INT_MCA",
    "Spurious"
};

/* This is the actual function called by irq_handler()
 * irqno in a0
 */
static void UIRQ(unsigned int irqno)
{
    panicf("Unhandled IRQ: %s", irqname[irqno]);
}

void system_init(void)
{
    atj213x_wdt_disable();
}

void system_reboot(void)
{
    atj213x_wdt_enable();

    while(1);
}

void system_exception_wait(void)
{
    /* wait until button release (if a button is pressed) */
    while(button_read_device());
    /* then wait until next button press */
    while(!button_read_device());
}

static const char *exc_reg_names[] = {
    "c0status", "at", "v0", "v1",
    "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3",
    "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3",
    "s4", "s5", "s6", "s7",
    "t8", "t9", "hi", "lo",
    "gp", "sp", "fp", "ra"
};

static const char *irq_reg_names[] = {
    "c0status", "at", "v0", "v1",
    "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3",
    "t4", "t5", "t6", "t7",
    "t8", "t9", "hi", "lo",
    "gp", "sp", "fp", "ra"
};

#define EXC(x,y) case (x): return (y);
static char* parse_exception(unsigned int cause)
{
    switch(cause & M_CauseExcCode)
    {
        EXC(EXC_INT, "Interrupt");
        EXC(EXC_MOD, "TLB Modified");
        EXC(EXC_TLBL, "TLB Exception (Load or Ifetch)");
        EXC(EXC_ADEL, "Address Error (Load or Ifetch)");
        EXC(EXC_ADES, "Address Error (Store)");
        EXC(EXC_TLBS, "TLB Exception (Store)");
        EXC(EXC_IBE, "Instruction Bus Error");
        EXC(EXC_DBE, "Data Bus Error");
        EXC(EXC_SYS, "Syscall");
        EXC(EXC_BP, "Breakpoint");
        EXC(EXC_RI, "Reserved Instruction");
        EXC(EXC_CPU, "Coprocessor Unusable");
        EXC(EXC_OV, "Overflow");
        EXC(EXC_TR, "Trap Instruction");
        EXC(EXC_FPE, "Floating Point Exception");
        EXC(EXC_C2E, "COP2 Exception");
        EXC(EXC_MDMX, "MDMX Exception");
        EXC(EXC_WATCH, "Watch Exception");
        EXC(EXC_MCHECK, "Machine Check Exception");
        EXC(EXC_CacheErr, "Cache error caused re-entry to Debug Mode");
        default:
            return NULL;
    }
}

void system_exception(unsigned int cause,
                      unsigned int epc,
                      unsigned int *ef)
{
    int line = 1;

    atj213x_gpio_mux_reset();

    lcd_set_backdrop(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(LCD_BLACK);
    lcd_set_background(LCD_WHITE);
    lcd_setfont(FONT_SYSFIXED);
    lcd_set_viewport(NULL);
    lcd_clear_display();
    backlight_hw_on();

    lcd_putsf(1, line++, "Exception: %s", parse_exception(cause));
    lcd_putsf(1, line++, "pc:%08x cause:%08x", epc, cause);

    for (int i=0; i<32; i+=2)
    {
        lcd_putsf(1, line++, "%s:%08x %s:%08x", exc_reg_names[i], ef[i], exc_reg_names[i+1], ef[i+1]);
    }

    lcd_putsf(1, line++, "");

    lcd_putsf(1, line++, "IRQ frame");
    for (int i=0; i<24; i+=2)
    {
        lcd_putsf(1, line++, "%s:%08x %s:%08x", irq_reg_names[i], ef[33+i], irq_reg_names[i+1], ef[33+i+1]);
    }

    lcd_update();
    system_exception_wait(); /* if this returns, try to reboot */

    lcd_clear_display();
    line = 1;
    lcd_putsf(1,line++,"Calling hwstub...");
    call_hwstub();

    system_reboot();
    while (1);       /* halt */

}

int system_memory_guard(int newmode)
{
    (void)newmode;
    return 0;
}

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
void set_cpu_frequency(long frequency)
{
    //return;

    if (cpu_frequency == frequency)
        return;

    if (frequency == CPUFREQ_MAX)
    {
        /* COREPLL:CCLK:SCLK:PCLK = 180:180:90:11.25
         * sdram refresh 700
         */

        /* value taken from SYSCFG.EXE disassembly */
        atj213x_vdd_set(VDD_1800MV);

        CMU_BUSCLK = (7<<8)|(2<<6)|(1<<4)|(0<<2);
        SDR_RFSH = 700;
    }
    else
    {
        /* COREPLL:CCLK:SCLK:PCLK = 180:45:45:11.25
         * sdram refresh 350
         */
        SDR_RFSH = 350;
        CMU_BUSCLK = (3<<8)|(2<<6)|(0<<4)|(3<<2);

        /* value taken from SYSCFG.EXE disassembly */
        atj213x_vdd_set(VDD_1600MV);
    }

    cpu_frequency = frequency;
}
#endif

void udelay(unsigned int usec)
{
    unsigned cycles_per_usec = (cpu_frequency + 999999) / 1000000;
    unsigned i = (usec * cycles_per_usec)/2;
    asm volatile (
                  ".set noreorder    \n"
                  "1:                \n"
                  "bne  %0, $0, 1b   \n"
                  "addiu %0, %0, -1  \n"
                  ".set reorder      \n"
                  : : "r" (i)
                 );
}
