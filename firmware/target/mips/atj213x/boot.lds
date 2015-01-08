#include "config.h"

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(MIPS)
ENTRY(_start)
STARTUP(target/mips/atj213x/crt0.o)

#define DRAMSIZE (MEMORYSIZE * 0x100000)

#define DRAMORIG 0x80000000
#define IRAMORIG 0x94040000
#define IRAMSIZE 96k

MEMORY
{
   DRAM : ORIGIN = DRAMORIG, LENGTH = DRAMSIZE
   IRAM : ORIGIN = IRAMORIG, LENGTH = IRAMSIZE
}

SECTIONS
{
    . = DRAMORIG;

    .text :
    {
        relocstart = .;
        *(.init.text);
        *(.text*);
    } > DRAM

    . = ALIGN(4);

    .rodata :
    {
        *(.rodata*);
    } > DRAM

    . = ALIGN(4);

    .data :
    {
        *(.data*);
        *(.sdata*);
        *(.rel.dyn);
    } > DRAM

    . = ALIGN(16);

    .iram IRAMORIG:
    {
        _iramstart = .;
        _irqbase = .;
        KEEP(*(.vectors.tlb_refill));
        . = 0x100;
        KEEP(*(.vectors.cache_error));
        . = 0x180;
        KEEP(*(.vectors.exception));
        . = 0x200;
        KEEP(*(.vectors.interrupt));
        KEEP(*(.vectors));

        *(.icode);
        *(.irodata);
        *(.idata);

        _iramend = .;
        relocend = .;
    } > IRAM AT> DRAM
    _iramcopy = LOADADDR(.iram);

    .ibss (NOLOAD):
    {
        _iedata = .;
        *(.ibss*);
        _iend = .;
    } > IRAM

    . = ALIGN(4);

    .bss (NOLOAD):
    {
         _edata = .;
         *(.sbss*);
         *(.bss*);
         *(COMMON);
         *(.scommon*);
         _end = .;
    } > DRAM

    .stack (NOLOAD):
    {
         *(.stack)
         stackbegin = .;
         . += 0x2000;
         stackend = .;
         irqstackbegin = .;
         . += 0x400;
         irqstackend = .;
    } > IRAM
}
