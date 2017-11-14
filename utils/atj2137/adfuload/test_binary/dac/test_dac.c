#include "atj213x.h"
#define PMA(x) ((x) & 0x1fffffff)

/* 1kHz sine @ 48kHz sampling rate
 * amplitude of right channel is 16 times lower
 * then left
 */
static const uint32_t tone[] = {
    0x00000000, 0x00000000, 0x10b40000, 0x010b4000,
    0x21200000, 0x02120000, 0x30fb0000, 0x030fb000,
    0x3fff0000, 0x03fff000, 0x4deb0000, 0x04deb000,
    0x5a810000, 0x05a81000, 0x658b0000, 0x0658b000,
    0x6ed90000, 0x06ed9000, 0x76400000, 0x07640000,
    0x7ba20000, 0x07ba2000, 0x7ee60000, 0x07ee6000,
    0x7fff0000, 0x07fff000, 0x7ee60000, 0x07ee6000,
    0x7ba20000, 0x07ba2000, 0x76400000, 0x07640000,
    0x6ed90000, 0x06ed9000, 0x658b0000, 0x0658b000,
    0x5a810000, 0x05a81000, 0x4deb0000, 0x04deb000,
    0x3fff0000, 0x03fff000, 0x30fb0000, 0x030fb000,
    0x21200000, 0x02120000, 0x10b40000, 0x010b4000,
    0x00000000, 0x00000000, 0xef4b0000, 0xfef4b000,
    0xdedf0000, 0xfdedf000, 0xcf040000, 0xfcf04000,
    0xc0000000, 0xfc000000, 0xb2140000, 0xfb214000,
    0xa57e0000, 0xfa57e000, 0x9a740000, 0xf9a74000,
    0x91260000, 0xf9126000, 0x89bf0000, 0xf89bf000,
    0x845d0000, 0xf845d000, 0x81190000, 0xf8119000,
    0x80010000, 0xf8001000, 0x81190000, 0xf8119000,
    0x845d0000, 0xf845d000, 0x89bf0000, 0xf89bf000,
    0x91260000, 0xf9126000, 0x9a740000, 0xf9a74000,
    0xa57e0000, 0xfa57e000, 0xb2140000, 0xfb214000,
    0xc0000000, 0xfc000000, 0xcf040000, 0xfcf04000,
    0xdedf0000, 0xfdedf000, 0xef4b0000, 0xfef4b000
};

void dac_init(void)
{
    /* ungate DAC and DMA */
    CMU_DEVCLKEN |= 0x20100;

    /* enable PLL and set to 48kHz */
    CMU_AUDIOPLL = 0x11;

    /* setup dac fifo, reset and enable */
    DAC_FIFOCTL = 0x141;

    /* DAC enable */
    DAC_CTL = 0x90b1;

    /* enable analog section, unmute DAC at mixer input */
    DAC_ANALOG = 0x8c7;
}

void play_tone(void)
{
    /* reset DMA channel 0 */
    DMAC_CTL = 0x10000;

    DMA_CNT(0) = sizeof(tone);
    DMA_MODE(0) = 0x11340084;
    DMA_SRC(0) = PMA((uint32_t)&tone);
    DMA_DST(0) = PMA((uint32_t)&DAC_DAT);
    DMA_CMD(0) = 1;
}

#define CPU_FREQ 60000000UL //???
void udelay(unsigned int usec)
{
    unsigned int i = ((usec * CPU_FREQ) / 2000000);
    asm volatile (
                  ".set noreorder    \n"
                  "1:                \n"
                  "bne  %0, $0, 1b   \n"
                  "addiu %0, %0, -1  \n"
                  ".set reorder      \n"
                  : "=r" (i)
                  : "0" (i)
                 );
}

void mdelay(unsigned int msec)
{
    unsigned int i;
    for(i=0; i<msec; i++)
        udelay(1000);
}

void backlight_init(void)
{
    /* backlight clock enable, select backlight clock as 32kHz */
    CMU_FMCLK = (CMU_FMCLK & ~(CMU_FMCLK_BCLK_MASK)) | CMU_FMCLK_BCKE | CMU_FMCLK_BCLK_32K;

    /* baclight enable */
    PMU_CTL |= PMU_CTL_BL_EN;

    /* pwm output, phase high, some initial duty cycle set as 24/32 */
    PMU_CHG = ((PMU_CHG & ~PMU_CHG_PDOUT_MASK)| PMU_CHG_PBLS_PWM | PMU_CHG_PPHS_HIGH | PMU_CHG_PDUT(24));
}

void backlight_set(int level)
{
    /* set duty cycle in 1/32 units */
    PMU_CHG = ((PMU_CHG & ~PMU_CHG_PDOUT_MASK) | PMU_CHG_PDUT(level));
}

static void wdt_feed(void)
{
    RTC_WDCTL |= RTC_WDCTL_CLR;
}

int main(void)
{
    int j = 0;

    backlight_init();
    dac_init();
    play_tone();

    while(1)
    {
        /* otherwise wdt will trigger reset */
        wdt_feed();

        backlight_set(j++);
        mdelay(1000);
    }

    return 0;
} 
