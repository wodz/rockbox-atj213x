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

#include "wdt-atj213x.h"

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
    INT_PCNT, __UIRQ, INT_DSP, INT_USB, INT_MHA, INT_SD, __UIRQ, INT_MCA
};

static const char * const irqname[] =
{
    "IRQ31","IRQ30","IRQ29","IRQ28","IRQ27","INT_YUV","IRQ25","INT_NAND",
    "IRQ23","INT_DAC","INT_ADC","IRQ20","IRQ19","INT_IIC1","INT_IIC2","IRQ16",
    "IRQ15","INT_EXT","INT_KEY","INT_DMA","INT_RTC","INT_T0","INT_T1","INT_WD",
    "INT_PCNT","IRQ6","INT_DSP","INT_USB","INT_MHA","INT_SD","UIRQ1","INT_MCA"
};

/* This the actual function called by irq_handler()
 * which sets irqno in a0 properly
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

void system_exception(unsigned int sp, unsigned int cause, unsigned int epc)
{
    lcd_set_backdrop(NULL);
    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(LCD_BLACK);
    lcd_set_background(LCD_WHITE);
    lcd_setfont(FONT_SYSFIXED);
    lcd_set_viewport(NULL);
    lcd_clear_display();
    //backlight_hw_on();
    backlight_hw_brightness(1);

    panicf("Exception occurred! pc: 0x%08x sp: 0x%08x cause: 0x%08x)",
           epc, sp, cause);
}

int system_memory_guard(int newmode)
{
    (void)newmode;
    return 0;
}

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
void set_cpu_frequency(long frequency)
{
    if (cpu_frequency == frequency)
        return;

    if (frequency == CPUFREQ_MAX)
    {
    }
    else
    {
    }

    cpu_frequency = frequency;
}
#endif

void udelay(unsigned int usec)
{
    unsigned cycles_per_usec = (CPU_FREQ + 999999) / 1000000;
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
