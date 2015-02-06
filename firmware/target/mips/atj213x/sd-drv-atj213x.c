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

    semaphore_init(&sd_semaphore, 1, 0);
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

/* called between DMA channel setup
 * and DMA channel start
 */
static void sdc_dma_rd_callback(void)
{
    // TODO remove magic values
    SD_BYTECNT = size;
    SD_FIFOCTL = 0x259;
    SD_RW = 0x3c0;
}

void sdc_dma_rd(void *buf, int size)
{
    /* This allows for ~4MB chained transfer */
    static struct ll_dma_t sd_ll[4];
    int xsize;
    unsigned int mode = BF_DMAC_DMA_MODE_DDIR_V(INCREASE) |
                        BF_DMAC_DMA_MODE_SFXA_V(FIXED) |
                        BF_DMAC_DMA_MODE_STRG_V(SD);

    /* dma mode depends on dst address (dram/iram)
     * and buffer alignment
     */
    if (iram) // TODO
        mode |= BF_DMAC_DMA_MODE_DTRG_V(IRAM);
    else
        mode |= BF_DMAC_DMA_MODE_DTRG_V(DRAM);

    if (((unsigned int)buf & 3) == 0)
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH32);
    else if (((unsigned int)buf & 3) == 2)
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH16);
    else
        mode |= BF_DMAC_DMA_MODE_DTRANWID_V(WIDTH8);

    for (i=0; i<4; i++)
    {
        xsize = MIN(size, DMA_MAX_XFER_SIZE);
        sd_ll[i].hwinfo.dst = PHYSADDR((uint32_t)buf);
        sd_ll[i].hwinfo.src = PHYSADDR((uint32_t)&SD_DAT);
        sd_ll[i].hwinfo.mode = mode;
        sd_ll[i].hwinfo.size = xsize;

        size -= xsize;
        buf = (void *)((char *)buf + xsize);
        sd_ll[].next = (size) ? &sd_ll[i+1] : NULL;

        if (size == 0)
            break;
    }

    if (size)
    {
        /* requested transfer is bigger then we can handle */
        panicf("sdc_dma_rd(): can't transfer that much!");
    }

    ll_dma_setup(DMA_CH_SD, struct ll_dma_t *ll,
                 sdc_dma_rd_callback, &sd_semaphore);
    ll_dma_start(DMA_CH_SD);

    semaphore_wait(&sd_semaphore, TIMEOUT_BLOCK);
}

void sdc_dma_wr(void *buf, int size)
{
}

struct sd_cmd_t {
    uint32_t cmd;
    enum sd_rsp_t rsp;
};

struct sd_rspdat_t {
    uint32_t response[4];
    void *data;
    bool rd;
};

int sdc_send_cmd(const uint32_t cmd, const uint32_t arg,
                 struct sd_rspdat_t *rspdat, int datlen)
{
    unsigned long tmo = current_tick + HZ;
    unsigned int cmdrsp;
    struct sd_cmd_t *sd = &sd_cmd_table[cmd];

    if (sd->opcode > 256)
        sdc_send_cmd(SD_APP_CMD, card_info.rca, rspdat, 0);

    SD_ARG = arg;
    SD_CMD = (sd->cmd % 256);

    /* this sets bit0 to clear STAT and mark response type */
    switch (sd->rsp)
    {
        case SD_RSP_NRSP:
            cmdrsp = 0x05;
            break;

        case SD_RSP_R1:
        case SD_RSP_R6:
            cmdrsp = 0x03;
            break;

        case SD_RSP_R2:
            cmdrsp = 0x11;
            break;

        case SD_RSP_R3:
            cmdrsp = 0x09;
            break;

        default:
            panicf("Invalid SD response requested: 0x%0x", sd->rsp);
    }

    /* prepare transfer to sd in case of wr data command */
    if (rspdat->data && datlen > 0 && !rspdat->rd)
        sdc_dma_wr(rspdat->data, datlen);

    SD_CMDRSP = cmdrsp;

    /* command finish wait */
    while (SD_CMDRSP & cmdrsp)
        if (TIME_AFTER(current_tick, tmo))
            return -1; /// error code?

    if (sd->rsp != SD_RSP_NRSP)
    {
        /* check CRC */
        if (sd->rsp == SD_RSP_R1)
        {
            crc7 = SD_CRC7;
            rescrc = SD_RSPBUF(0) & 0xff;

            if (crc7 ^ rescrc)
                panicf("Invalid SD CRC: 0x%02x != 0x%02x", rescrc, crc7);
        }

        if (sd->rsp == SD_RSP_R2)
        {
            rspdat->response[0] = SD_RSPBUF(3);
            rspdat->response[1] = SD_RSPBUF(2);
            rspdat->response[2] = SD_RSPBUF(1);
            rspdat->response[3] = SD_RSPBUF(0);
        }
        else
        {
            rspdat->response[0] = (SD_RSPBUF(1)<<24) | (SD_RSPBUF(0)>>8);
        }
    }

    /* data stage */
    if (rspdat->data && datlen > 0 && rspdat->rd)
        sdc_dma_rd(rspdat->data, datlen);
}
