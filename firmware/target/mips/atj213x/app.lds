#include "config.h"

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(MIPS)
ENTRY(start)
STARTUP(target/mips/atj213x/crt0.o)

#define DRAMORIG    0xc0000000 //0x80000000
#define DRAMSIZE          (MEMORYSIZE * 0x100000)

#define IRAMORIG    0xc1000000
#define IRAMSIZE          40K

/* End of the audio buffer, where the codec buffer starts */
#define ENDAUDIOADDR  (DRAMORIG + DRAMSIZE - PLUGIN_BUFFER_SIZE - CODEC_SIZE)

/* Where the codec buffer ends, and the plugin buffer starts */
#define ENDCODECADDR  (ENDAUDIOADDR + CODEC_SIZE)

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
        loadaddress = .;
        *(.init.text);
        *(.text*);
    } > DRAM

    .data :
    {
        . = ALIGN(16);
        *(.rodata*);
        *(.data*);
        *(.sdata*);
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

        *(.icode*);
        *(.irodata*);
        *(.idata*);
        . = ALIGN(16);
        _iramend = .;
    } > IRAM AT> DRAM
    _iramcopy = LOADADDR(.iram);

    .bss (NOLOAD):
    {
         . = ALIGN(16);
         _relocend = .;
         _bss_start = .;
         *(.sbss*);
         *(.bss*);
         *(COMMON);
         *(.scommon*);
         _bss_end = .;
    } > DRAM

    .ibss (NOLOAD) :
    {
        . = ALIGN(16);
        _ibss_start = .;
        *(.ibss*);
        _ibss_end = .;
    } > IRAM

    .stack (NOLOAD):
    {
         . = ALIGN(16);
         *(.stack);
         stackbegin = .;
         . += 0x2000;
         stackend = .;
         irqstackbegin = .;
         . += 0x400;
         irqstackend = .;
    } > IRAM

    .audiobuf :
    {
        . = ALIGN(16);
        audiobuffer = .;
    } > DRAM

    audiobufend = ENDAUDIOADDR;
    codecbuf = ENDAUDIOADDR;
    pluginbuf = ENDCODECADDR;

    /DISCARD/ :
    {
        *(.eh_frame);
        *(.rel.dyn);
    }
}
