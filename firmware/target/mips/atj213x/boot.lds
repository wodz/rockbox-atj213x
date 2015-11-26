#include "config.h"

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(MIPS)
ENTRY(start)
STARTUP(target/mips/atj213x/crt0.o)

#define DRAMSIZE (MEMORYSIZE * 0x100000)

#define DRAMORIG 0x80000000
#define IRAMORIG 0x94040000
#define IRAMSIZE 96k

MEMORY
{
   IRAM : ORIGIN = IRAMORIG, LENGTH = IRAMSIZE
}

SECTIONS
{
    .exception_vectors :
    {
        /* vectors needs to be 4k aligned */
        . = ALIGN(0x1000);

        _iramstart = .;
        _irqbase = .;
        KEEP(*(.vectors.tlb_refill));
        . = _iramstart + 0x100;
        KEEP(*(.vectors.cache_error));
        . = _iramstart + 0x180;
        KEEP(*(.vectors.exception));
        . = _iramstart + 0x200;
        KEEP(*(.vectors.interrupt));
        KEEP(*(.vectors));
    } > IRAM

    .text :
    {
        _relocstart = .;
        *(.init.text);
        *(.text*);
        *(.icode);
    } > IRAM

    .data :
    {
        . = ALIGN(16);
        *(.rodata*);
        *(.irodata);
        *(.data*);
        *(.idata);
        *(.sdata*);
        *(.rel.dyn);
    } > IRAM


    .bss (NOLOAD):
    {
         . = ALIGN(4);
         _relocend = .;
         _bss_start = .;
         *(.sbss*);
         *(.bss*);
         *(.ibss*);
         *(COMMON);
         *(.scommon*);
         _bss_end = .;
    } > IRAM

    .stack (NOLOAD):
    {
         . = ALIGN(16);
         *(.stack)
         stackbegin = .;
         . += 0x2000;
         stackend = .;
         irqstackbegin = .;
         . += 0x2000;
         irqstackend = .;
    } > IRAM
}
