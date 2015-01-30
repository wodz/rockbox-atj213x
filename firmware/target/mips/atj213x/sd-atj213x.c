#include "gpio-atj213x.h"
#include "regs/regs-cmu.h"
#include "regs/regs-sd.h"

void sdc_init(void)
{
    /* ungate SD block clock */
    CMU_DEVCLKEN |= BM_CMU_DEVCLKEN_SD | BM_CMU_DEVCLKEN_DMAC;

    SD_CTL = BM_SD_CTL_RESE |
             BM_SD_CTL_EN |
             BF_SD_CTL_BSEL_V(BUS) |
             BF_SD_CTL_BUSWID_V(WIDTH_4BIT); /* 0x4c1 */

    SD_FIFOCTL = BM_SD_FIFOCTL_EMPTY |
                 BM_SD_FIFOCTL_RST |
                 BF_SD_FIFOCTL_THRH_V(THR_10_16) |
                 BM_SD_FIFOCTL_FULL |
                 BM_SD_FIFOCTL_IRQP; /* 0x25c */

    SD_BYTECNT = 0;

    SD_RW = BM_SD_RW_WCEF |
            BM_SD_RW_WCST |
            BM_SD_RW_RCST; /* 0x340 */

    /* B22 sd detect active low */
    atj213x_gpio_setup(GPIO_PORTB, 22, GPIO_IN);
}

void sdc_set_speed(unsigned sdfreq)
{
    unsigned int corefreq = atj213x_get_coreclk();
    unsigned int sddiv = (corefreq + sdfreq - 1)/sdfreq;

    if (sddive > 15)
    {
        /* when low clock is needed during initialization */
        sddiv = ((corefreq/128) + freq - 1)/freq;
        sddiv |= BM_CMU_SDCLK_D128;
    }

    CMU_SDCLK = BM_CMU_SDCLK_CKEN | sddiv;
}

void sdc_set_bus_width(unsigned width)
{
    if (width == 1)
        SD_CTL = (SD_CTL & ~BM_SD_CTL_BUSWID) |
                 BF_SD_CTL_BUSWID_V(WIDTH_1BIT);
    else
        SD_CTL = (SD_CTL & ~BM_SD_CTL_BUSWID) |
                 BF_SD_CTL_BUSWID_V(WIDTH_4BIT);
}

bool sdc_card_present(void)
{
    return !atj213x_gpio_get(GPIO_PORTB, 22);
}
