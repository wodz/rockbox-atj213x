#include "config.h"

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(MIPS)
ENTRY(start)
STARTUP(target/mips/atj213x/crt0.o)

#define DRAMORIG_KSEG0    0x80000000
#define DRAMORIG_KSEG2    0xc0000000
#define DRAMSIZE          (MEMORYSIZE * 0x100000)

#define IRAMORIG_KSEG0    0x94040000
#define IRAMORIG_KSEG2    0xc1000000
#define IRAMSIZE          40K

/* End of the audio buffer, where the codec buffer starts */
#define ENDAUDIOADDR  (DRAMORIG_KSEG2 + DRAMSIZE - PLUGIN_BUFFER_SIZE - CODEC_SIZE)

/* Where the codec buffer ends, and the plugin buffer starts */
#define ENDCODECADDR  (ENDAUDIOADDR + CODEC_SIZE)

MEMORY
{
   DRAM_KSEG0 : ORIGIN = DRAMORIG_KSEG0, LENGTH = DRAMSIZE
   DRAM_KSEG2 : ORIGIN = DRAMORIG_KSEG2, LENGTH = DRAMSIZE
   IRAM_KSEG0 : ORIGIN = IRAMORIG_KSEG0, LENGTH = IRAMSIZE
   IRAM_KSEG2 : ORIGIN = IRAMORIG_KSEG2, LENGTH = IRAMSIZE
}

SECTIONS
{
    .inittext :
    {
        loadaddress = .;
        *(.init.text);
    } > DRAM_KSEG0 AT> DRAM_KSEG2

    .text :
    {
        *(.text*);
    } > DRAM_KSEG2

    .data :
    {
        *(.rodata*);
        *(.data*);
        *(.sdata*);
        . = ALIGN(16);
    } > DRAM_KSEG2

    .iram_kseg0 :
    {
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
    } > IRAM_KSEG0 AT> DRAM_KSEG2

    .iram IRAMORIG_KSEG2 + SIZEOF(.iram_kseg0) :
    {
        *(.icode*);
        *(.irodata*);
        *(.idata*);
        . = ALIGN(16);
    } > IRAM_KSEG2 AT> DRAM_KSEG2

    _iramsrc = (LOADADDR(.iram_kseg0) & 0x0fffffff) | 0x80000000;
    _iramdst = ADDR(.iram_kseg0);
    _iramsize = SIZEOF(.iram_kseg0) + SIZEOF(.iram);

    .irqstack IRAMORIG_KSEG0 + SIZEOF(.iram_kseg0) + SIZEOF(.iram) (NOLOAD) :
    {
        irqstackbegin = .;
        . += 0x400;
        irqstackend = .;
    } > IRAM_KSEG0

    .bss (NOLOAD):
    {
         _bss_start = .;
         *(.sbss*);
         *(.bss*);
         *(COMMON);
         *(.scommon*);
         _bss_end = .;
    } > DRAM_KSEG2

    _relocdst = ADDR(.inittext);
    _relocsize = ADDR(.bss) - LOADADDR(.inittext);

    .ibss IRAMORIG_KSEG2 + SIZEOF(.iram_kseg0) + SIZEOF(.iram) + SIZEOF(.irqstack) (NOLOAD) :
    {
        . = ALIGN(16);
        _ibss_start = .;
        *(.ibss*);
        _ibss_end = .;
    } > IRAM_KSEG2

    .stack IRAMORIG_KSEG2 + SIZEOF(.iram_kseg0) + SIZEOF(.iram) + SIZEOF(.irqstack) + SIZEOF(.ibss) (NOLOAD):
    {
         . = ALIGN(16);
         *(.stack);
         stackbegin = .;
         . += 0x2000;
         stackend = .;
    } > IRAM_KSEG2

    .audiobuf :
    {
        . = ALIGN(16);
        audiobuffer = .;
    } > DRAM_KSEG2

    audiobufend = ENDAUDIOADDR;
    codecbuf = ENDAUDIOADDR;
    pluginbuf = ENDCODECADDR;

    /DISCARD/ :
    {
        *(.eh_frame);
        *(.rel.dyn);
    }
}
