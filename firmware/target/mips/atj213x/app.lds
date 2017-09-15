#include "config.h"

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(MIPS)
ENTRY(start)
STARTUP(target/mips/atj213x/crt0.o)

#define DRAMORIG      0x80000000
#define DRAMSIZE      (MEMORYSIZE * 0x100000)
#define IRAMORIG      0x94040000
#define IRAMSIZE      48K

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
    . = DRAMORIG;

    .text :
    {
//        _relocstart = .;
        loadaddress = .;
//        _loadaddress = .;
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
    } > DRAM

    /* Set the load address of .iram at the same address as .bss
     * so RAM won't be wasted as .iram in the end will get copied
     * to IRAM. */
    .iram IRAMORIG: AT (_edata)
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
        _iramend = .;
    } > IRAM
    _iramcopy = LOADADDR(.iram);

    .ibss (NOLOAD) :
    {
//        _relocend = .;
//        _iedata = .;
        _ibss_start = .;
        *(.ibss*);
        _ibss_end = .;
//        _iend = .;
    } > IRAM

    . = ALIGN(4);

    .stack (NOLOAD):
    {
         *(.stack);
         stackbegin = .;
         . += 0x2000;
         stackend = .;
         irqstackbegin = .;
         . += 0x400;
         irqstackend = .;
    } > IRAM

    .bss (NOLOAD):
    {
         _edata = .;
         _bss_start = .;
         *(.sbss*);
         *(.bss*);
         *(COMMON);
         *(.scommon*);
         _bss_end = .;
//         _end = .;
    } > DRAM

    .audiobuf :
    {
        . = ALIGN(4);
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
