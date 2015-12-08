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
   DRAM : ORIGIN = DRAMORIG, LENGTH = DRAMSIZE
   IRAM : ORIGIN = IRAMORIG, LENGTH = IRAMSIZE
}

SECTIONS
{
    .text :
    {
        _relocstart = .;
        *(.init.text);
        *(.text*);
    } > DRAM

    .data :
    {
        . = ALIGN(16);
        *(.rodata*);
        *(.data*);
        *(.sdata*);
        *(.rel.dyn);
    } > DRAM

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

        . = ALIGN(16);
        *(.icode);
        *(.irodata);
        *(.idata);

        _iramend = .;
    } > IRAM AT> DRAM
    _iramcopy = LOADADDR(.iram);

    .bss (NOLOAD):
    {
         . = ALIGN(4);
         _relocend = .;
         _bss_start = .;
         *(.sbss*);
         *(.bss*);
         *(COMMON);
         *(.scommon*);
         _bss_end = .;
    } > DRAM

    .ibss (NOLOAD):
    {
        . = ALIGN(4);
        _ibss_start = .;
        *(.ibss*);
        _ibss_end = .;
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
