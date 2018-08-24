/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/printk.h>
#include <asm/page.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>

#include <mt-plat/mt_boot.h>

#include "mt_sd.h"
#include <core/core.h>
#include <card/queue.h>
#include <linux/mmc/card.h>

#ifdef CONFIG_MMC_FFU
#include <linux/mmc/ffu.h>
#endif

#ifdef MTK_MSDC_BRINGUP_DEBUG
#include <mach/mt_pmic_wrap.h>
#endif

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

#ifdef CONFIG_PWR_LOSS_MTK_TEST
#include <mach/power_loss_test.h>
#else
#define MVG_EMMC_CHECK_BUSY_AND_RESET(...)
#define MVG_EMMC_SETUP(...)
#define MVG_EMMC_RESET(...)
#define MVG_EMMC_WRITE_MATCH(...)
#define MVG_EMMC_ERASE_MATCH(...)
#define MVG_EMMC_ERASE_RESET(...)
#define MVG_EMMC_DECLARE_INT32(...)
#endif

#include "dbg.h"
#include "msdc_tune.h"
#include "autok.h"
#include "autok_dvfs.h"

#define CAPACITY_2G             (2 * 1024 * 1024 * 1024ULL)

u32 g_emmc_mode_switch; 


#define MSDC_MAX_FLUSH_COUNT    (3)
#define CACHE_UN_FLUSHED        (0)
#define CACHE_FLUSHED           (1)
#ifdef MTK_MSDC_USE_CACHE
static unsigned int g_cache_status = CACHE_UN_FLUSHED;
#endif
static unsigned long long g_flush_data_size;
static unsigned int g_flush_error_count;
static int g_flush_error_happend;

unsigned char g_emmc_cache_quirk[256];

#if (MSDC_DATA1_INT == 1)
static u16 u_sdio_irq_counter;
static u16 u_msdc_irq_counter;
#endif

struct msdc_host *ghost;
int src_clk_control;

bool emmc_sleep_failed;
static struct workqueue_struct *wq_init;

#ifdef MSDC_WQ_ERROR_TUNE
static struct workqueue_struct *wq_tune;
#endif

bool sdio_lock_dvfs;

#define DRV_NAME                "mtk-msdc"

#define MSDC_COOKIE_PIO         (1<<0)
#define MSDC_COOKIE_ASYNC       (1<<1)

#define msdc_use_async(x)       (x & MSDC_COOKIE_ASYNC)
#define msdc_use_async_dma(x)   (msdc_use_async(x) && (!(x & MSDC_COOKIE_PIO)))
#define msdc_use_async_pio(x)   (msdc_use_async(x) && ((x & MSDC_COOKIE_PIO)))

#define HOST_MAX_BLKSZ          (2048)

#define MSDC_OCR_AVAIL          (MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 \
				| MMC_VDD_31_32 | MMC_VDD_32_33)

#define DEFAULT_DTOC            (3)     

#define MAX_DMA_CNT             (64 * 1024 - 512)
				
#define MAX_DMA_CNT_SDIO        (0xFFFFFFFF - 255)
				

#define MAX_HW_SGMTS            (MAX_BD_NUM)
#define MAX_PHY_SGMTS           (MAX_BD_NUM)
#define MAX_SGMT_SZ             (MAX_DMA_CNT)
#define MAX_SGMT_SZ_SDIO        (MAX_DMA_CNT_SDIO)

#define HTC_MAX_CRCERR_COUNT             (2)

u8 g_emmc_id;
unsigned int cd_gpio = 0;

struct msdc_host *mtk_msdc_host[] = { NULL, NULL, NULL, NULL};
EXPORT_SYMBOL(mtk_msdc_host);

int g_dma_debug[HOST_MAX_NUM] = { 0, 0, 0, 0};
u32 latest_int_status[HOST_MAX_NUM] = { 0, 0, 0, 0};

unsigned int msdc_latest_transfer_mode[HOST_MAX_NUM] = {
	
	TRAN_MOD_NUM,
	TRAN_MOD_NUM,
	TRAN_MOD_NUM,
	TRAN_MOD_NUM,
};

unsigned int msdc_latest_op[HOST_MAX_NUM] = {
	
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
};

unsigned int sd_debug_zone[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

unsigned int sd_register_zone[HOST_MAX_NUM] = {
	1,
	1,
	1,
	1,
};

u32 dma_size[HOST_MAX_NUM] = {
	512,
	8,
	512,
	512,
};

u32 drv_mode[HOST_MAX_NUM] = {
	MODE_SIZE_DEP, 
	MODE_SIZE_DEP,
	MODE_SIZE_DEP,
	MODE_SIZE_DEP,
};

u8 msdc_clock_src[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

u32 msdc_host_mode[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

u32 msdc_host_mode2[HOST_MAX_NUM] = {
	0,
	0,
	0,
	0,
};

int msdc_rsp[] = {
	0,                      
	1,                      
	2,                      
	3,                      
	4,                      
	1,                      
	1,                      
	1,                      
	7,                      
};

#define msdc_init_gpd_ex(gpd, extlen, cmd, arg, blknum) \
	do { \
		((struct gpd_t *)gpd)->extlen = extlen; \
		((struct gpd_t *)gpd)->cmd    = cmd; \
		((struct gpd_t *)gpd)->arg    = arg; \
		((struct gpd_t *)gpd)->blknum = blknum; \
	} while (0)

#define msdc_init_bd(bd, blkpad, dwpad, dptr, dlen) \
	do { \
		BUG_ON(dlen > 0xFFFFFFUL); \
		((struct bd_t *)bd)->blkpad = blkpad; \
		((struct bd_t *)bd)->dwpad = dwpad; \
		((struct bd_t *)bd)->ptr = (u32)dptr; \
		((struct bd_t *)bd)->buflen = dlen; \
	} while (0)

#ifdef CONFIG_NEED_SG_DMA_LENGTH
#define msdc_sg_len(sg, dma)    ((dma) ? (sg)->dma_length : (sg)->length)
#else
#define msdc_sg_len(sg, dma)    sg_dma_len(sg)
#endif

#define msdc_dma_on()           MSDC_CLR_BIT32(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_off()          MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_status()       ((MSDC_READ32(MSDC_CFG) & MSDC_CFG_PIO) >> 3)

#define pr_reg(OFFSET, VAL)     \
	pr_err("%d R[%x]=0x%.8x", id, OFFSET, VAL)

static u16 msdc_offsets[] = {
	OFFSET_MSDC_CFG,
	OFFSET_MSDC_IOCON,
	OFFSET_MSDC_PS,
	OFFSET_MSDC_INT,
	OFFSET_MSDC_INTEN,
	OFFSET_MSDC_FIFOCS,
	OFFSET_SDC_CFG,
	OFFSET_SDC_CMD,
	OFFSET_SDC_ARG,
	OFFSET_SDC_STS,
	OFFSET_SDC_RESP0,
	OFFSET_SDC_RESP1,
	OFFSET_SDC_RESP2,
	OFFSET_SDC_RESP3,
	OFFSET_SDC_BLK_NUM,
	OFFSET_SDC_VOL_CHG,
	OFFSET_SDC_CSTS,
	OFFSET_SDC_CSTS_EN,
	OFFSET_SDC_DCRC_STS,
	OFFSET_EMMC_CFG0,
	OFFSET_EMMC_CFG1,
	OFFSET_EMMC_STS,
	OFFSET_EMMC_IOCON,
	OFFSET_SDC_ACMD_RESP,
	OFFSET_SDC_ACMD19_TRG,
	OFFSET_SDC_ACMD19_STS,
	OFFSET_MSDC_DMA_SA_HIGH,
	OFFSET_MSDC_DMA_SA,
	OFFSET_MSDC_DMA_CA,
	OFFSET_MSDC_DMA_CTRL,
	OFFSET_MSDC_DMA_CFG,
	OFFSET_MSDC_DMA_LEN,
	OFFSET_MSDC_DBG_SEL,
	OFFSET_MSDC_DBG_OUT,
	OFFSET_MSDC_PATCH_BIT0,
	OFFSET_MSDC_PATCH_BIT1,
	OFFSET_MSDC_PATCH_BIT2,

	OFFSET_DAT0_TUNE_CRC,
	OFFSET_DAT0_TUNE_CRC,
	OFFSET_DAT0_TUNE_CRC,
	OFFSET_DAT0_TUNE_CRC,
	OFFSET_CMD_TUNE_CRC,
	OFFSET_SDIO_TUNE_WIND,

	OFFSET_MSDC_PAD_TUNE0,
	OFFSET_MSDC_PAD_TUNE1,
	OFFSET_MSDC_DAT_RDDLY0,
	OFFSET_MSDC_DAT_RDDLY1,
	OFFSET_MSDC_DAT_RDDLY2,
	OFFSET_MSDC_DAT_RDDLY3,
	OFFSET_MSDC_HW_DBG,
	OFFSET_MSDC_VERSION,

	OFFSET_EMMC50_PAD_DS_TUNE,
	OFFSET_EMMC50_PAD_CMD_TUNE,
	OFFSET_EMMC50_PAD_DAT01_TUNE,
	OFFSET_EMMC50_PAD_DAT23_TUNE,
	OFFSET_EMMC50_PAD_DAT45_TUNE,
	OFFSET_EMMC50_PAD_DAT67_TUNE,
	OFFSET_EMMC51_CFG0,
	OFFSET_EMMC50_CFG0,
	OFFSET_EMMC50_CFG1,
	OFFSET_EMMC50_CFG2,
	OFFSET_EMMC50_CFG3,
	OFFSET_EMMC50_CFG4
};

void msdc_dump_register_core(u32 id, void __iomem *base)
{
	u16 i;

	for (i = 0; i < sizeof(msdc_offsets)/sizeof(msdc_offsets[0]); i++) {

		if (((id != 2) && (id != 3))
		 && (msdc_offsets[i] >= OFFSET_DAT0_TUNE_CRC)
		 && (msdc_offsets[i] <= OFFSET_SDIO_TUNE_WIND))
			continue;

		if ((id != 0)
		 && (msdc_offsets[i] >= OFFSET_EMMC50_PAD_DS_TUNE))
			break;

		pr_reg(msdc_offsets[i], MSDC_READ32(base + msdc_offsets[i]));
	}

	return;
}

void msdc_dump_register(struct msdc_host *host)
{
	void __iomem *base = host->base;

	msdc_dump_register_core(host->id, base);
}

void msdc_dump_dbg_register_core(u32 id, void __iomem *base)
{
	u32 i;

	for (i = 0; i <= 0x27; i++) {
		MSDC_WRITE32(MSDC_DBG_SEL, i);
		SIMPLE_INIT_MSG("SEL:r[%x]=0x%x", OFFSET_MSDC_DBG_SEL, i);
		SIMPLE_INIT_MSG("OUT:r[%x]=0x%x", OFFSET_MSDC_DBG_OUT,
			 MSDC_READ32(MSDC_DBG_OUT));
	}

	MSDC_WRITE32(MSDC_DBG_SEL, 0);
}

static void msdc_dump_dbg_register(struct msdc_host *host)
{
	void __iomem *base = host->base;

	msdc_dump_dbg_register_core(host->id, base);
}

void msdc_dump_info(u32 id)
{
	struct msdc_host *host = mtk_msdc_host[id];
	void __iomem *base;

	if (host == NULL) {
		pr_err("msdc host<%d> null\r\n", id);
		return;
	}

	if (host->async_tuning_in_progress || host->legacy_tuning_in_progress)
		return;

	
	if (!sd_register_zone[id]) {
		pr_err("msdc host<%d> is timeout when detect, so don't dump register\n", id);
		return;
	}

	host->prev_cmd_cause_dump++;
	if (host->prev_cmd_cause_dump > 1)
		return;

	base = host->base;

	
	msdc_dump_register(host);
	INIT_MSG("latest_INT_status<0x%.8x>", latest_int_status[id]);

	
	mdelay(10);
	msdc_dump_clock_sts();

	
	msdc_dump_ldo_sts(host);

	
	msdc_dump_padctl(host);

	
	mdelay(10);
	msdc_dump_dbg_register(host);
}

int msdc_get_dma_status(int host_id)
{
	int result = -1;

	if (host_id < 0 || host_id >= HOST_MAX_NUM) {
		pr_err("[%s] failed to get dma status, invalid host_id %d\n",
			__func__, host_id);
	} else if (msdc_latest_transfer_mode[host_id] == TRAN_MOD_DMA) {
		if (msdc_latest_op[host_id] == OPER_TYPE_READ)
			return 1;       
		else if (msdc_latest_op[host_id] == OPER_TYPE_WRITE)
			return 2;       
	} else if (msdc_latest_transfer_mode[host_id] == TRAN_MOD_PIO) {
		return 0;               
	}

	return result;
}
EXPORT_SYMBOL(msdc_get_dma_status);

void msdc_clr_fifo(unsigned int id)
{
	int retry = 3, cnt = 1000;
	void __iomem *base;

	if (id < 0 || id >= HOST_MAX_NUM)
		return;
	base = mtk_msdc_host[id]->base;

	if (MSDC_READ32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS) {
		pr_err("<<<WARN>>>: msdc%d, clear FIFO when DMA active, MSDC_DMA_CFG=0x%x\n",
			id, MSDC_READ32(MSDC_DMA_CFG));
		show_stack(current, NULL);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
		msdc_retry((MSDC_READ32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS),
			retry, cnt, id);
		if (retry == 0) {
			pr_err("<<<WARN>>>: msdc%d, faield to stop DMA before clear FIFO, MSDC_DMA_CFG=0x%x\n",
				id, MSDC_READ32(MSDC_DMA_CFG));
			return;
		}
	}

	retry = 3;
	cnt = 1000;
	MSDC_SET_BIT32(MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	msdc_retry(MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, retry, cnt, id);
}

int msdc_clk_stable(struct msdc_host *host, u32 mode, u32 div,
	u32 hs400_div_dis)
{
	void __iomem *base = host->base;
	int retry = 0;
	int cnt = 1000;
	int retry_cnt = 1;

#if defined(CFG_DEV_MSDC3)
	
	
	if (host->id == 3) {
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, 0);
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, 1);
		MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_SDR104CKS, 1);
		return 0;
	}
#endif

	do {
		retry = 3;
		MSDC_SET_FIELD(MSDC_CFG,
			MSDC_CFG_CKMOD_HS400 | MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(hs400_div_dis << 14) | (mode << 12) |
				((div + retry_cnt) % 0xfff));
		
		msdc_retry(!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB), retry,
			cnt, host->id);
		if (retry == 0) {
			pr_err("msdc%d host->onclock(%d)\n", host->id,
				host->core_clkon);

			pr_err("msdc%d on clock failed ===> retry twice\n",
				host->id);

			msdc_clk_disable(host);
			msdc_clk_enable(host);

			msdc_dump_info(host->id);
			host->prev_cmd_cause_dump = 0;
		}
		retry = 3;
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, div);
		msdc_retry(!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB), retry,
			cnt, host->id);
		msdc_reset_hw(host->id);
		if (retry_cnt == 2)
			break;
		retry_cnt++;
	} while (!retry);

	return 0;
}

#define msdc_irq_save(val) \
	do { \
		val = MSDC_READ32(MSDC_INTEN); \
		MSDC_CLR_BIT32(MSDC_INTEN, val); \
	} while (0)

#define msdc_irq_restore(val) \
	MSDC_SET_BIT32(MSDC_INTEN, val) \

void msdc_set_smpl(struct msdc_host *host, u32 clock_mode, u8 mode, u8 type, u8 *edge)
{
	void __iomem *base = host->base;
	int i = 0;

	switch (type) {
	case TYPE_CMD_RESP_EDGE:
		if (clock_mode == 3) {
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_PADCMD_LATCHCK, 0);
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_CMD_RESP_SEL, 0);
		}

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, mode);
		} else {
			ERR_MSG("invalid resp parameter: type=%d, mode=%d\n",
				type, mode);
		}
		break;
	case TYPE_WRITE_CRC_EDGE:
		if (clock_mode == 3) {
			
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_CRC_STS_SEL, 1);
		} else {
			
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_CRC_STS_SEL, 0);
		}

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			if (clock_mode == 3) {
				MSDC_SET_FIELD(EMMC50_CFG0,
					MSDC_EMMC50_CFG_CRC_STS_EDGE, mode);
			} else {
				MSDC_SET_FIELD(MSDC_PATCH_BIT2,
					MSDC_PB2_CFGCRCSTSEDGE, mode);
			}
		} else if ((mode == MSDC_SMPL_SEPARATE) &&
			   (edge != NULL) &&
			   (sizeof(edge) == 8)) {
			pr_err("Shall not enter here\n");

		} else {
			ERR_MSG("invalid crc parameter: type=%d, mode=%d\n",
				type, mode);
		}

		break;

	case TYPE_READ_DATA_EDGE:
		if (clock_mode == 3) {
			
			MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_START_BIT,
				START_AT_RISING_AND_FALLING);
		} else {
			MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_START_BIT,
				START_AT_RISING);
		}
		if (clock_mode == 2)
			mode = 0;

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL, 0);
			MSDC_SET_FIELD(MSDC_PATCH_BIT0,
				MSDC_PB0_RD_DAT_SEL, mode);
		} else if ((mode == MSDC_SMPL_SEPARATE) &&
			   (edge != NULL) &&
			   (sizeof(edge) == 8)) {
			pr_err("Shall not enter here\n");
		} else {
			ERR_MSG("invalid read parameter: type=%d, mode=%d\n",
				type, mode);
		}

		break;

	case TYPE_WRITE_DATA_EDGE:
		
		MSDC_SET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_SEL, 0);

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL, 0);
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, mode);
		} else if ((mode == MSDC_SMPL_SEPARATE) &&
			   (edge != NULL) &&
			   (sizeof(edge) >= 4)) {
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL, 1);
			for (i = 0; i < 4; i++) {
				
				MSDC_SET_FIELD(MSDC_IOCON,
					(MSDC_IOCON_W_D0SPL << i), edge[i]);
			}
		} else {
			ERR_MSG("invalid write parameter: type=%d, mode=%d\n",
				type, mode);
		}
		break;
	default:
		ERR_MSG("invalid parameter: type=%d, mode=%d\n", type, mode);
		break;
	}
	

}

void msdc_set_smpl_all(struct msdc_host *host, u32 clock_mode)
{
	struct msdc_hw *hw = host->hw;

	msdc_set_smpl(host, clock_mode, hw->cmd_edge, TYPE_CMD_RESP_EDGE,
		NULL);
	msdc_set_smpl(host, clock_mode, hw->rdata_edge, TYPE_READ_DATA_EDGE,
		NULL);
	msdc_set_smpl(host, clock_mode, hw->wdata_edge, TYPE_WRITE_CRC_EDGE,
		NULL);
}

#define msdc_set_vol_change_wait_count(count) \
	MSDC_SET_FIELD(SDC_VOL_CHG, SDC_VOL_CHG_CNT, (count))

static void msdc_clksrc_onoff(struct msdc_host *host, u32 on)
{
	void __iomem *base = host->base;
	u32 div, mode, hs400_div_dis;

	if ((on) && (0 == host->core_clkon)) {
		msdc_clk_enable(host);

		host->core_clkon = 1;
		udelay(10);

		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, div);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD_HS400, hs400_div_dis);
		msdc_clk_stable(host, mode, div, hs400_div_dis);

	} else if ((!on)
		&& (!((host->hw->flags & MSDC_SDIO_IRQ) && src_clk_control))) {
		if (host->core_clkon == 1) {

			MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_MODE, MSDC_MS);

			msdc_clk_disable(host);

			host->core_clkon = 0;
		}
	}
}

void msdc_gate_clock(struct msdc_host *host, int delay)
{
	unsigned long flags;
	unsigned int suspend;

	
	if (delay < 0) {
		suspend = 1;
		delay = 0;
	} else {
		suspend = 0;
	}

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if ((suspend == 0) && (host->clk_gate_count > 0))
		host->clk_gate_count--;
	if (delay) {
		mod_timer(&host->timer, jiffies + CLK_TIMEOUT);
		N_MSG(CLK, "[%s]: msdc%d, clk_gate_count=%d, delay=%d",
			__func__, host->id, host->clk_gate_count, delay);
	} else if (host->clk_gate_count == 0) {
		del_timer(&host->timer);
		msdc_clksrc_onoff(host, 0);
		N_MSG(CLK, "[%s]: msdc%d, clock gated done",
			__func__, host->id);
	} else {
		if (is_card_sdio(host))
			host->error = -EBUSY;
		ERR_MSG("[%s]: msdc%d, clock is needed, clk_gate_count=%d",
			__func__, host->id, host->clk_gate_count);
	}
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

void msdc_ungate_clock(struct msdc_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	host->clk_gate_count++;
	N_MSG(CLK, "[%s]: msdc%d, clk_gate_count=%d", __func__, host->id,
		host->clk_gate_count);
	if (host->clk_gate_count == 1)
		msdc_clksrc_onoff(host, 1);
	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

#if 0
static void msdc_dump_card_status(struct msdc_host *host, u32 status)
{
	static const char const *state[] = {
		"Idle",         
		"Ready",        
		"Ident",        
		"Stby",         
		"Tran",         
		"Data",         
		"Rcv",          
		"Prg",          
		"Dis",          
		"Reserved",     
		"Reserved",     
		"Reserved",     
		"Reserved",     
		"Reserved",     
		"Reserved",     
		"I/O mode",     
	};
	if (status & R1_OUT_OF_RANGE)
		N_MSG(RSP, "[CARD_STATUS] Out of Range");
	if (status & R1_ADDRESS_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Address Error");
	if (status & R1_BLOCK_LEN_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Block Len Error");
	if (status & R1_ERASE_SEQ_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Erase Seq Error");
	if (status & R1_ERASE_PARAM)
		N_MSG(RSP, "[CARD_STATUS] Erase Param");
	if (status & R1_WP_VIOLATION)
		N_MSG(RSP, "[CARD_STATUS] WP Violation");
	if (status & R1_CARD_IS_LOCKED)
		N_MSG(RSP, "[CARD_STATUS] Card is Locked");
	if (status & R1_LOCK_UNLOCK_FAILED)
		N_MSG(RSP, "[CARD_STATUS] Lock/Unlock Failed");
	if (status & R1_COM_CRC_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Command CRC Error");
	if (status & R1_ILLEGAL_COMMAND)
		N_MSG(RSP, "[CARD_STATUS] Illegal Command");
	if (status & R1_CARD_ECC_FAILED)
		N_MSG(RSP, "[CARD_STATUS] Card ECC Failed");
	if (status & R1_CC_ERROR)
		N_MSG(RSP, "[CARD_STATUS] CC Error");
	if (status & R1_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Error");
	if (status & R1_UNDERRUN)
		N_MSG(RSP, "[CARD_STATUS] Underrun");
	if (status & R1_OVERRUN)
		N_MSG(RSP, "[CARD_STATUS] Overrun");
	if (status & R1_CID_CSD_OVERWRITE)
		N_MSG(RSP, "[CARD_STATUS] CID/CSD Overwrite");
	if (status & R1_WP_ERASE_SKIP)
		N_MSG(RSP, "[CARD_STATUS] WP Eraser Skip");
	if (status & R1_CARD_ECC_DISABLED)
		N_MSG(RSP, "[CARD_STATUS] Card ECC Disabled");
	if (status & R1_ERASE_RESET)
		N_MSG(RSP, "[CARD_STATUS] Erase Reset");
	if ((status & R1_READY_FOR_DATA) == 0)
		N_MSG(RSP, "[CARD_STATUS] Not Ready for Data");
	if (status & R1_SWITCH_ERROR)
		N_MSG(RSP, "[CARD_STATUS] Switch error");
	if (status & R1_APP_CMD)
		N_MSG(RSP, "[CARD_STATUS] App Command");

	N_MSG(RSP, "[CARD_STATUS] '%s' State", state[R1_CURRENT_STATE(status)]);
}
#endif

static void msdc_set_timeout(struct msdc_host *host, u32 ns, u32 clks)
{
	void __iomem *base = host->base;
	u32 timeout, clk_ns;
	u32 mode = 0;

	host->timeout_ns = ns;
	host->timeout_clks = clks;
	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000UL / host->sclk;
		timeout = (ns + clk_ns - 1) / clk_ns + clks;
		
		timeout = (timeout + (1 << 20) - 1) >> 20;
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, timeout);

	N_MSG(OPS, "msdc%d, Set read data timeout: %dns %dclks -> %d x 1048576 cycles, mode:%d, clk_freq=%dKHz\n",
		host->id, ns, clks, timeout + 1, mode, (host->sclk / 1000));
}

static void msdc_eirq_sdio(void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;

	N_MSG(INT, "SDIO EINT");
#ifdef SDIO_ERROR_BYPASS
	if (host->sdio_error != -EILSEQ) {
#endif
		mmc_signal_sdio_irq(host->mmc);
#ifdef SDIO_ERROR_BYPASS
	}
#endif
}

volatile int sdio_autok_processed = 0;

void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	void __iomem *base = host->base;
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	u32 hclk = host->hclk;
	u32 hs400_div_dis = 0; 

	if (!hz) { 
		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			host->saved_para.hz = hz;
#ifdef SDIO_ERROR_BYPASS
			host->sdio_error = 0;
#endif
		}
		host->mclk = 0;
		msdc_reset_hw(host->id);
		return;
	}

	msdc_irq_save(flags);

	if (timing == MMC_TIMING_MMC_HS400) {
		mode = 0x3; 
		if (hz >= hclk/2) {
			hs400_div_dis = 1;
			div = 0;
			sclk = hclk/2;
		} else {
			hs400_div_dis = 0;
			if (hz >= (hclk >> 2)) {
				div  = 0;         
				sclk = hclk >> 2; 
			} else {
				div  = (hclk + ((hz << 2) - 1)) / (hz << 2);
				sclk = (hclk >> 2) / div;
				div  = (div >> 1);
			}
		}

	} else if ((timing == MMC_TIMING_UHS_DDR50)
		|| (timing == MMC_TIMING_MMC_DDR52)) {
		mode = 0x2; 
		if (hz >= (hclk >> 2)) {
			div  = 0;         
			sclk = hclk >> 2; 
		} else {
			div  = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
			div  = (div >> 1);
		}
#if !defined(FPGA_PLATFORM)
	} else if (hz >= hclk) {
		mode = 0x1; 
		div  = 0;
		sclk = hclk;
#endif
	} else {
		mode = 0x0; 
		if (hz >= (hclk >> 1)) {
			div  = 0;         
			sclk = hclk >> 1; 
		} else {
			div  = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
		}
	}

	msdc_clk_stable(host, mode, div, hs400_div_dis);

	host->sclk = sclk;
	host->mclk = hz;
	host->timing = timing;

	
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);

	msdc_set_smpl_all(host, mode);

	pr_err("msdc%d -> !!! Set<%dKHz> Source<%dKHz> -> sclk<%dKHz> timing<%d> mode<%d> div<%d> hs400_div_dis<%d>\n",
		host->id, hz/1000, hclk/1000, sclk/1000, (int)timing, mode, div,
		hs400_div_dis);

	msdc_irq_restore(flags);
}

void msdc_send_stop(struct msdc_host *host)
{
	struct mmc_command stop = {0};
	struct mmc_request mrq = {0};
	u32 err;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &stop;
	stop.mrq = &mrq;
	stop.data = NULL;

	err = msdc_do_command(host, &stop, 0, CMD_TIMEOUT);
}

static int msdc_app_cmd(struct mmc_host *mmc, struct msdc_host *host)
{
	struct mmc_command cmd = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err;

	cmd.opcode = MMC_APP_CMD;
	cmd.arg = host->app_cmd_arg;    
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;

	err = msdc_do_command(host, &cmd, 0, CMD_TIMEOUT);
	return err;
}

int msdc_get_card_status(struct mmc_host *mmc, struct msdc_host *host,
	u32 *status)
{
	struct mmc_command cmd = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err;

	cmd.opcode = MMC_SEND_STATUS;   
	cmd.arg = host->app_cmd_arg;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;

	
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (host->mmc->card->ext_csd.cmdq_mode_en)
		err = msdc_do_cmdq_command(host, &cmd, 0, CMD_TIMEOUT);
	else
#endif
		err = msdc_do_command(host, &cmd, 0, CMD_TIMEOUT);

	if (status)
		*status = cmd.resp[0];

	return err;
}

#if 0
static void msdc_remove_card(struct work_struct *work)
{
	struct msdc_host *host = container_of(work, struct msdc_host,
		remove_card.work);

	BUG_ON(!host || !host->mmc);
	ERR_MSG("Need remove card");
	if (host->mmc->card) {
		if (mmc_card_present(host->mmc->card)) {
			ERR_MSG("1.remove card");
			mmc_remove_card(host->mmc->card);
		} else {
			ERR_MSG("card was not present can not remove");
			host->block_bad_card = 0;
			host->card_inserted = 1;
			host->mmc->card->state &= ~MMC_CARD_REMOVED;
			return;
		}
		mmc_claim_host(host->mmc);
		ERR_MSG("2.detach bus");
		host->mmc->card = NULL;
		mmc_detach_bus(host->mmc);
		ERR_MSG("3.Power off");
		mmc_power_off(host->mmc);
		ERR_MSG("4.Gate clock");
		msdc_gate_clock(host, 0);
		mmc_release_host(host->mmc);
	}
	ERR_MSG("Card removed");
}
#endif

int msdc_reinit(struct msdc_host *host)
{
	int ret = -1;
	u32 err = 0;
	u32 status = 0;
	unsigned long tmo = 12;

	BUG_ON(!host || !host->mmc || !host->mmc->card);

	if (host->hw->host_function != MSDC_SD)
		goto skip_reinit2;

	if (host->block_bad_card)
		ERR_MSG("Need block this bad SD card from re-initialization");

	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE) || (host->block_bad_card != 0))
		goto skip_reinit1;

	
	ERR_MSG("SD card Re-Init!");
	mmc_claim_host(host->mmc);
	ERR_MSG("SD card Re-Init get host!");
	spin_lock(&host->lock);
	ERR_MSG("SD card Re-Init get lock!");
	msdc_clksrc_onoff(host, 1);
	if (host->app_cmd_arg) {
		while ((err = msdc_get_card_status(host->mmc, host, &status))) {
			ERR_MSG("SD card Re-Init in get card status!err(%d)",
				err);
			if (err == (unsigned int)-EILSEQ) {
				if (msdc_tune_cmdrsp(host)) {
					ERR_MSG("update cmd para failed");
					break;
				}
			} else {
				break;
			}
		}
		if (err == 0) {
			if (status == 0) {
				msdc_dump_info(host->id);
			} else {
				msdc_clksrc_onoff(host, 0);
				spin_unlock(&host->lock);
				mmc_release_host(host->mmc);
				ERR_MSG("SD Card is ready");
				return 0;
			}
		}
	}
	msdc_clksrc_onoff(host, 0);
	ERR_MSG("Reinit start..");
	host->mmc->ios.clock = HOST_MIN_MCLK;
	host->mmc->ios.bus_width = MMC_BUS_WIDTH_1;
	host->mmc->ios.timing = MMC_TIMING_LEGACY;
	host->card_inserted = 1;
	msdc_clksrc_onoff(host, 1);
	msdc_set_mclk(host, MMC_TIMING_LEGACY, HOST_MIN_MCLK);
	msdc_clksrc_onoff(host, 0);
	spin_unlock(&host->lock);
	mmc_release_host(host->mmc);
	if (host->mmc->card) {
		mmc_remove_card(host->mmc->card);
		host->mmc->card = NULL;
		mmc_claim_host(host->mmc);
		mmc_detach_bus(host->mmc);
		mmc_release_host(host->mmc);
	}
	mmc_power_off(host->mmc);
	mmc_detect_change(host->mmc, 0);
	while (tmo) {
		if (host->mmc->card && mmc_card_present(host->mmc->card)) {
			ret = 0;
			break;
		}
		msleep(50);
		tmo--;
	}
	ERR_MSG("Reinit %s", ret == 0 ? "success" : "fail");

skip_reinit1:
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE) && (host->mmc->card)
		&& mmc_card_present(host->mmc->card)
		&& (!mmc_card_removed(host->mmc->card))
		&& (host->block_bad_card == 0))
		ret = 0;
skip_reinit2:
	return ret;
}

static void msdc_pin_reset(struct msdc_host *host, int mode, int force_reset)
{
	struct msdc_hw *hw = (struct msdc_hw *)host->hw;
	void __iomem *base = host->base;

	
	if ((hw->flags & MSDC_RST_PIN_EN) || force_reset) {
		if (mode == MSDC_PIN_PULL_UP)
			MSDC_CLR_BIT32(EMMC_IOCON, EMMC_IOCON_BOOTRST);
		else
			MSDC_SET_BIT32(EMMC_IOCON, EMMC_IOCON_BOOTRST);
	}
}

static void msdc_card_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	msdc_pin_reset(host, MSDC_PIN_PULL_DOWN, 1);
	udelay(2);
	msdc_pin_reset(host, MSDC_PIN_PULL_UP, 1);
	usleep_range(200, 500);
}

static void msdc_set_power_mode(struct msdc_host *host, u8 mode)
{
	N_MSG(CFG, "Set power mode(%d)", mode);
	if (host->power_mode == MMC_POWER_OFF && mode != MMC_POWER_OFF) {
		msdc_pin_reset(host, MSDC_PIN_PULL_UP, 0);
		msdc_pin_config(host, MSDC_PIN_PULL_UP);

		if (host->power_control)
			host->power_control(host, 1);

		mdelay(10);

		msdc_oc_check(host);

	} else if (host->power_mode != MMC_POWER_OFF && mode == MMC_POWER_OFF) {

		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
		} else {

			if (host->power_control)
				host->power_control(host, 0);

			msdc_pin_config(host, MSDC_PIN_PULL_DOWN);
		}
		mdelay(20); 
		if (!host->mmc->disable_pull_down)
			msdc_pin_reset(host, MSDC_PIN_PULL_DOWN, 0);
	}
	host->power_mode = mode;
}

#ifdef CONFIG_PM
static void msdc_pm(pm_message_t state, void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;
	void __iomem *base = host->base;

	int evt = state.event;

	msdc_ungate_clock(host);

	if (evt == PM_EVENT_SUSPEND || evt == PM_EVENT_USER_SUSPEND) {
		if (host->suspend)
			goto end;

		host->suspend = 1;
		host->pm_state = state;

		N_MSG(PWR, "msdc%d -> %s Suspend", host->id,
			evt == PM_EVENT_SUSPEND ? "PM" : "USR");
		if (host->hw->flags & MSDC_SYS_SUSPEND) {
			if (host->hw->host_function == MSDC_EMMC) {
				msdc_save_timing_setting(host, 1);
				msdc_set_power_mode(host, MMC_POWER_OFF);
			}

			msdc_set_tdsel(host, 1, 0);

		} else {
			host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
			mmc_remove_host(host->mmc);
		}

		host->power_cycle = 0;
	} else if (evt == PM_EVENT_RESUME || evt == PM_EVENT_USER_RESUME) {
		if (!host->suspend)
			goto end;

		if (evt == PM_EVENT_RESUME
			&& host->pm_state.event == PM_EVENT_USER_SUSPEND) {
			ERR_MSG("PM Resume when in USR Suspend");
			goto end;
		}

		host->suspend = 0;
		host->pm_state = state;

		N_MSG(PWR, "msdc%d -> %s Resume", host->id,
			evt == PM_EVENT_RESUME ? "PM" : "USR");

		if (!(host->hw->flags & MSDC_SYS_SUSPEND)) {
			host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
			mmc_add_host(host->mmc);
			goto end;
		}

		
		msdc_set_tdsel(host, 1, 0);

		if (host->hw->host_function == MSDC_EMMC) {
			msdc_reset_hw(host->id);
			msdc_set_power_mode(host, MMC_POWER_ON);
			msdc_restore_timing_setting(host);

			if (emmc_sleep_failed) {
				msdc_card_reset(host->mmc);
				mdelay(200);
				mmc_card_clr_sleep(host->mmc->card);
				emmc_sleep_failed = 0;
				host->mmc->ios.timing = MMC_TIMING_LEGACY;
				mmc_set_clock(host->mmc, 260000);
			}
		}
	}

end:
#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host))
		host->sdio_error = 0;
#endif
	if ((evt == PM_EVENT_SUSPEND) || (evt == PM_EVENT_USER_SUSPEND)) {
		if ((host->hw->host_function == MSDC_SDIO) &&
		    (evt == PM_EVENT_USER_SUSPEND)) {
			pr_err("msdc%d -> MSDC Device Request Suspend",
				host->id);
		}
		msdc_gate_clock(host, 0);
	} else {
		msdc_gate_clock(host, 1);
	}

	if (host->hw->host_function == MSDC_SDIO) {
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		host->mmc->rescan_entered = 0;
	}
}
#endif

struct msdc_host *msdc_get_host(int host_function, bool boot, bool secondary)
{
	int host_index = 0;
	struct msdc_host *host = NULL, *host2;

	for (; host_index < HOST_MAX_NUM; ++host_index) {
		host2 = mtk_msdc_host[host_index];
		if (!host2)
			continue;
		if ((host_function == host2->hw->host_function) &&
		    (boot == host2->hw->boot)) {
			host = host2;
			break;
		}
	}
	if (secondary && (host_function == MSDC_SD))
		host = mtk_msdc_host[2];
	if (host == NULL) {
		pr_err("[MSDC] This host(<host_function:%d> <boot:%d><secondary:%d>) isn't in MSDC host config list",
			 host_function, boot, secondary);
		
	}

	return host;
}
EXPORT_SYMBOL(msdc_get_host);

int msdc_switch_part(struct msdc_host *host, char part_id)
{
	int ret = 0;
	struct mmc_card *card;

	if ((host != NULL) && (host->mmc != NULL) && (host->mmc->card != NULL))
		card = host->mmc->card;
	else
		return -ENOMEDIUM;

	if (mmc_card_mmc(card)) {
		u8 part_config = card->ext_csd.part_config;

		part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		part_config |= part_id;

		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, part_config,
			card->ext_csd.part_time);
		if (ret)
			return ret;

		card->ext_csd.part_config = part_config;
	}

	return ret;
}

static int msdc_cache_onoff(struct mmc_data *data)
{
	u8 *ptr;
#ifdef MTK_MSDC_USE_CACHE
	int i;
	enum boot_mode_t mode;
#endif
	struct scatterlist *sg;

	sg = data->sg;
	ptr = (u8 *) sg_virt(sg);

#ifndef MTK_MSDC_USE_CACHE
	*(ptr + 252) = *(ptr + 251) = *(ptr + 250) = *(ptr + 249) = 0;
	return 0;
#else
	mode = get_boot_mode();
	if ((mode != NORMAL_BOOT) && (mode != ALARM_BOOT)
		&& (mode != SW_REBOOT)) {
		
		*(ptr + 252) = *(ptr + 251) = *(ptr + 250) = *(ptr + 249) = 0;
		return 0;
	}

	for (i = 0; i < sizeof(g_emmc_cache_quirk); i++) {
		if (g_emmc_cache_quirk[i] == g_emmc_id) {
			*(ptr + 252) = *(ptr + 251) = 0;
			*(ptr + 250) = *(ptr + 249) = 0;
		}
	}
	return 0;
#endif
}

#ifdef MTK_MSDC_USE_CACHE
static void msdc_set_cache_quirk(struct msdc_host *host)
{
	int i;


	for (i = 0; i < sizeof(g_emmc_cache_quirk); i++) {
		if (g_emmc_cache_quirk[i] == 0) {
			
			pr_debug("msdc%d total emmc cache quirk count=%d\n",
				host->id, i);
			break;
		}
		pr_debug("msdc%d,add emmc cache quirk[%d]=0x%x\n",
			host->id, i, g_emmc_cache_quirk[i]);
	 }
}
#endif

int msdc_cache_ctrl(struct msdc_host *host, unsigned int enable,
	u32 *status)
{
	struct mmc_command cmd = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err;

	cmd.opcode = MMC_SWITCH;        
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24)
		| (EXT_CSD_CACHE_CTRL << 16) | (!!enable << 8)
		| EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;

	ERR_MSG("do disable Cache, cmd=0x%x, arg=0x%x\n", cmd.opcode, cmd.arg);
	
	err = msdc_do_command(host, &cmd, 0, CMD_TIMEOUT);

	if (status)
		*status = cmd.resp[0];
	if (!err) {
		host->mmc->card->ext_csd.cache_ctrl = !!enable;
		host->autocmd |= MSDC_AUTOCMD23;
		N_MSG(CHE, "enable AUTO_CMD23 because Cache feature is disabled\n");
	}

	return err;
}

#ifdef MTK_MSDC_USE_CACHE
static void msdc_update_cache_flush_status(struct msdc_host *host,
	struct mmc_request *mrq, struct mmc_data *data,
	u32 l_bypass_flush)
{
	struct mmc_command *cmd = mrq->cmd;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	struct mmc_command *sbc;
	unsigned int task_id;
#endif

	if (!check_mmc_cache_ctrl(host->mmc->card))
		return;

	if (check_mmc_cmd2425(cmd->opcode)) {
		if ((host->error == 0)
		 && mrq->sbc
		 && (((mrq->sbc->arg >> 24) & 0x1) ||
		     ((mrq->sbc->arg >> 31) & 0x1))) {
			if (g_cache_status == CACHE_UN_FLUSHED) {
				g_cache_status = CACHE_FLUSHED;
				N_MSG(CHE, "reliable/force prg write happend, update g_cache_status = %d",
					g_cache_status);
				N_MSG(CHE, "reliable/force prg write happend, update g_flush_data_size=%lld",
					g_flush_data_size);
				g_flush_data_size = 0;
			}
		} else if (host->error == 0) {
			if (g_cache_status == CACHE_FLUSHED) {
				g_cache_status = CACHE_UN_FLUSHED;
				N_MSG(CHE, "normal write happend, update g_cache_status = %d",
					g_cache_status);
			}
			g_flush_data_size += data->blocks;
		} else if (host->error) {
			g_flush_data_size += data->blocks;
			ERR_MSG("write error happend, g_flush_data_size=%lld",
				g_flush_data_size);
		}
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	} else if (cmd->opcode == MMC_WRITE_REQUESTED_QUEUE) {
		if (host->error == 0) {
			task_id = (cmd->arg >> 16) & 0x1f;
			sbc = host->mmc->areq_que[task_id]->mrq_que->sbc;
			if (sbc	&& (((sbc->arg >> 24) & 0x1) ||
				((sbc->arg >> 31) & 0x1))) {
				if (g_cache_status == CACHE_UN_FLUSHED) {
					g_cache_status = CACHE_FLUSHED;
					N_MSG(CHE, "reliable/force prg write happend, update g_cache_status = %d",
						g_cache_status);
					N_MSG(CHE, "reliable/force prg write happend, update g_flush_data_size=%lld",
						g_flush_data_size);
					g_flush_data_size = 0;
				}
			} else {
				if (g_cache_status == CACHE_FLUSHED) {
					g_cache_status = CACHE_UN_FLUSHED;
					N_MSG(CHE, "normal write happend, update g_cache_status = %d",
						g_cache_status);
				}
				g_flush_data_size += data->blocks;
			}
		} else if (host->error) {
			g_flush_data_size += data->blocks;
			ERR_MSG("write error happend, g_flush_data_size=%lld",
				g_flush_data_size);
		}
#endif
	} else if (l_bypass_flush == 0) {
		if (host->error == 0) {
			g_cache_status = CACHE_FLUSHED;
			N_MSG(CHE, "flush happend, update g_cache_status = %d, g_flush_data_size=%lld",
				g_cache_status, g_flush_data_size);
			g_flush_data_size = 0;
		} else {
			g_flush_error_happend = 1;
		}
	}
}
#endif

void msdc_check_cache_flush_error(struct msdc_host *host,
	struct mmc_command *cmd)
{
	if (g_flush_error_happend &&
	    check_mmc_cache_ctrl(host->mmc->card) &&
	    check_mmc_cache_flush_cmd(cmd)) {
		g_flush_error_count++;
		g_flush_error_happend = 0;
		ERR_MSG("the %d time flush error happned, g_flush_data_size=%lld",
			g_flush_error_count, g_flush_data_size);
		if (g_flush_error_count >= MSDC_MAX_FLUSH_COUNT) {
			if (!msdc_cache_ctrl(host, 0, NULL)) {
				g_emmc_cache_quirk[0] = g_emmc_id;
				host->mmc->card->ext_csd.cache_size = 0;
			}
			pr_err("msdc%d:flush cache error count=%d,Disable cache\n",
				host->id, g_flush_error_count);
		}
	}
}

static u32 wints_cmd = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO |
		       MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR |
		       MSDC_INT_ACMDTMO;
static unsigned int msdc_command_start(struct msdc_host   *host,
	struct mmc_command *cmd,
	int                 tune,
	unsigned long       timeout)
{
	void __iomem *base = host->base;
	u32 opcode = cmd->opcode;
	u32 rawcmd;
	u32 rawarg;
	u32 resp;
	unsigned long tmo;
	struct mmc_command *sbc = NULL;
	char *str;

	if (host->data && host->data->mrq && host->data->mrq->sbc
	 && (host->autocmd & MSDC_AUTOCMD23))
		sbc = host->data->mrq->sbc;

	switch (opcode) {
	case MMC_SEND_OP_COND:
	case SD_APP_OP_COND:
		resp = RESP_R3;
		break;
	case MMC_SET_RELATIVE_ADDR:
	
		resp = (mmc_cmd_type(cmd) == MMC_CMD_BCR) ? RESP_R6 : RESP_R1;
		break;
	case MMC_FAST_IO:
		resp = RESP_R4;
		break;
	case MMC_GO_IRQ_STATE:
		resp = RESP_R5;
		break;
	case MMC_SELECT_CARD:
		resp = (cmd->arg != 0) ? RESP_R1 : RESP_NONE;
		host->app_cmd_arg = cmd->arg;
		N_MSG(PWR, "msdc%d select card<0x%.8x>", host->id, cmd->arg);
		break;
	case SD_IO_RW_DIRECT:
	case SD_IO_RW_EXTENDED:
		
		resp = RESP_R1;
		break;
	case SD_SEND_IF_COND:
		resp = RESP_R1;
		break;
	case MMC_SEND_STATUS:
		resp = RESP_R1;
		break;
	default:
		switch (mmc_resp_type(cmd)) {
		case MMC_RSP_R1:
			resp = RESP_R1;
			break;
		case MMC_RSP_R1B:
			resp = RESP_R1B;
			break;
		case MMC_RSP_R2:
			resp = RESP_R2;
			break;
		case MMC_RSP_R3:
			resp = RESP_R3;
			break;
		case MMC_RSP_NONE:
		default:
			resp = RESP_NONE;
			break;
		}
	}

	cmd->error = 0;

	rawcmd = opcode | msdc_rsp[resp] << 7 | host->blksz << 16;

	switch (opcode) {
	case MMC_READ_MULTIPLE_BLOCK:
	case MMC_WRITE_MULTIPLE_BLOCK:
		rawcmd |= (2 << 11);
		if (opcode == MMC_WRITE_MULTIPLE_BLOCK)
			rawcmd |= (1 << 13);
		if (host->autocmd & MSDC_AUTOCMD12) {
			rawcmd |= (1 << 28);
			N_MSG(CMD, "AUTOCMD12 is set, addr<0x%x>", cmd->arg);
#ifdef MTK_MSDC_USE_CMD23
		} else if ((host->autocmd & MSDC_AUTOCMD23)) {
			unsigned int reg_blk_num;

			rawcmd |= (1 << 29);
			if (sbc) {
				reg_blk_num = MSDC_READ32(SDC_BLK_NUM);
				if (reg_blk_num != (sbc->arg & 0xFFFF))
					pr_err("msdc%d: acmd23 arg(0x%x) fail to match block num(0x%x), SDC_BLK_NUM(0x%x)\n",
						host->id, sbc->arg,
						host->mrq->cmd->data->blocks,
						reg_blk_num);
				else
					MSDC_WRITE32(SDC_BLK_NUM, sbc->arg);
				N_MSG(CMD, "AUTOCMD23 addr<0x%x>, arg<0x%x> ",
					cmd->arg, sbc->arg);
			}
#endif 
		}
		break;

	case MMC_READ_SINGLE_BLOCK:
	case MMC_SEND_TUNING_BLOCK:
	case MMC_SEND_TUNING_BLOCK_HS200:
		rawcmd |= (1 << 11);
		break;
	case MMC_WRITE_BLOCK:
		rawcmd |= ((1 << 11) | (1 << 13));
		break;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	case MMC_READ_REQUESTED_QUEUE:
		rawcmd |= (2 << 11);
		break;
	case MMC_WRITE_REQUESTED_QUEUE:
		rawcmd |= ((2 << 11) | (1 << 13));
		break;
	case MMC_CMDQ_TASK_MGMT:
		break;
#endif
	case SD_IO_RW_EXTENDED:
		if (cmd->data->flags & MMC_DATA_WRITE)
			rawcmd |= (1 << 13);
		if (cmd->data->blocks > 1)
			rawcmd |= (2 << 11);
		else
			rawcmd |= (1 << 11);
		break;
	case SD_IO_RW_DIRECT:
		if (cmd->flags == (unsigned int)-1)
			rawcmd |= (1 << 14);
		break;
	case SD_SWITCH_VOLTAGE:
		rawcmd |= (1 << 30);
		break;
	case SD_APP_SEND_SCR:
	case SD_APP_SEND_NUM_WR_BLKS:
		rawcmd |= (1 << 11);
		break;
	case SD_SWITCH:
	case SD_APP_SD_STATUS:
	case MMC_SEND_EXT_CSD:
		if (mmc_cmd_type(cmd) == MMC_CMD_ADTC)
			rawcmd |= (1 << 11);
		break;
	case MMC_STOP_TRANSMISSION:
		rawcmd |= (1 << 14);
		rawcmd &= ~(0x0FFF << 16);
		break;
	}

	N_MSG(CMD, "CMD<%d><0x%.8x> Arg<0x%.8x>", opcode, rawcmd, cmd->arg);

	tmo = jiffies + timeout;

	if (opcode == MMC_SEND_STATUS) {
		for (;;) {
			if (!sdc_is_cmd_busy())
				break;
			if (time_after(jiffies, tmo)) {
				str = "cmd_busy";
				goto err;
			}
		}
	} else {
		for (;;) {
			if (!sdc_is_busy())
				break;
			if (time_after(jiffies, tmo)) {
				str = "sdc_busy";
				goto err;
			}
		}
	}

	host->cmd = cmd;
	host->cmd_rsp = resp;

	
	MSDC_CLR_BIT32(MSDC_INTEN, wints_cmd);
	rawarg = cmd->arg;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	dbg_add_host_log(host->mmc, 0, cmd->opcode, cmd->arg);
#endif

	sdc_send_cmd(rawcmd, rawarg);

	return 0;

err:
	ERR_MSG("XXX %s timeout: before CMD<%d>", str, opcode);
	cmd->error = (unsigned int)-ETIMEDOUT;
	msdc_dump_register(host);
	msdc_reset_hw(host->id);
	return cmd->error;

}

static u32 msdc_command_resp_polling(struct msdc_host *host,
	struct mmc_command *cmd,
	int                 tune,
	unsigned long       timeout)
{
	void __iomem *base = host->base;
	u32 intsts;
	u32 resp;
	unsigned long tmo;
	
	u32 cmdsts = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;
#ifdef MTK_MSDC_USE_CMD23
	struct mmc_command *sbc = NULL;

	if (host->autocmd & MSDC_AUTOCMD23) {
		if (host->data && host->data->mrq && host->data->mrq->sbc)
			sbc = host->data->mrq->sbc;

		
		cmdsts |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO;
	}
#endif

	resp = host->cmd_rsp;

	
	tmo = jiffies + timeout;
	while (1) {
		intsts = MSDC_READ32(MSDC_INT);
		if ((intsts & cmdsts) != 0) {
			
#ifdef MTK_MSDC_USE_CMD23
			
			intsts &= (cmdsts | MSDC_INT_ACMDRDY);
#else
			intsts &= cmdsts;
#endif
			MSDC_WRITE32(MSDC_INT, intsts);
			break;
		}

		if (time_after(jiffies, tmo)) {
			pr_err("[%s]: msdc%d CMD<%d> polling_for_completion timeout ARG<0x%.8x>\n",
				__func__, host->id, cmd->opcode, cmd->arg);
			cmd->error = (unsigned int)-ETIMEDOUT;
			host->sw_timeout++;
			msdc_dump_info(host->id);
			msdc_reset_hw(host->id);
			goto out;
		}
	}

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	msdc_error_tune_debug1(host, cmd, sbc, &intsts);
#endif

	
	if  (!(intsts & cmdsts))
		goto out;

#ifdef MTK_MSDC_USE_CMD23
	if (intsts & (MSDC_INT_CMDRDY | MSDC_INT_ACMD19_DONE)) {
#else
	if (intsts & (MSDC_INT_CMDRDY | MSDC_INT_ACMD19_DONE
		| MSDC_INT_ACMDRDY)) {
#endif
		u32 *rsp = NULL;

		rsp = &cmd->resp[0];
		switch (host->cmd_rsp) {
		case RESP_NONE:
			break;
		case RESP_R2:
			*rsp++ = MSDC_READ32(SDC_RESP3);
			*rsp++ = MSDC_READ32(SDC_RESP2);
			*rsp++ = MSDC_READ32(SDC_RESP1);
			*rsp++ = MSDC_READ32(SDC_RESP0);
			break;
		default: 
			*rsp = MSDC_READ32(SDC_RESP0);

			if ((cmd->opcode == 13) || (cmd->opcode == 25)) {
				if (*rsp & R1_WP_VIOLATION) {
					pr_err("[%s]: msdc%d XXX CMD<%d> resp<0x%.8x>, write protection violation\n",
						__func__, host->id, cmd->opcode,
						*rsp);
				}

				
				if ((*rsp & R1_OUT_OF_RANGE)
				 && (host->hw->host_function != MSDC_SDIO)) {
					pr_err("[%s]: msdc%d XXX CMD<%d> resp<0x%.8x>,bit31=1,force make crc error\n",
						__func__, host->id, cmd->opcode,
						*rsp);
					cmd->error = (unsigned int)-EILSEQ;
				}
			}
			break;
		}
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		dbg_add_host_log(host->mmc, 1, cmd->opcode, cmd->resp[0]);
#endif
	} else if (intsts & MSDC_INT_RSPCRCERR) {
		cmd->error = (unsigned int)-EILSEQ;
		if ((cmd->opcode != 19) && (cmd->opcode != 21))
			pr_err("[%s]: msdc%d CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		if (((MMC_RSP_R1B == mmc_resp_type(cmd)) || (cmd->opcode == 13))
			&& (host->hw->host_function != MSDC_SDIO)) {
			pr_err("[%s]: msdc%d CMD<%d> ARG<0x%.8X> is R1B, CRC not reset hw\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		} else {
			msdc_reset_hw(host->id);
		}
	} else if (intsts & MSDC_INT_CMDTMO) {
		cmd->error = (unsigned int)-ETIMEDOUT;
		if ((cmd->opcode != 19) && (cmd->opcode != 21))
			pr_err("[%s]: msdc%d CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		if ((cmd->opcode != 52) && (cmd->opcode != 8) &&
		    (cmd->opcode != 5) && (cmd->opcode != 55) &&
		    (cmd->opcode != 19) && (cmd->opcode != 21) &&
		    (cmd->opcode != 1) &&
		    ((cmd->opcode != 13) || (g_emmc_mode_switch == 0))) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			mmc_cmd_dump(host->mmc);
#endif
			msdc_dump_info(host->id);
		}

		if (((MMC_RSP_R1B == mmc_resp_type(cmd)) || (cmd->opcode == 13))
			&& (host->hw->host_function != MSDC_SDIO)) {
			pr_err("[%s]: msdc%d XXX CMD<%d> ARG<0x%.8X> is R1B, TMO not reset hw\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		} else {
			msdc_reset_hw(host->id);
		}
	}
#ifdef MTK_MSDC_USE_CMD23
	if ((sbc != NULL) && (host->autocmd & MSDC_AUTOCMD23)) {
		if (intsts & MSDC_INT_ACMDRDY) {
			u32 *arsp = &sbc->resp[0];
			*arsp = MSDC_READ32(SDC_ACMD_RESP);
		} else if (intsts & MSDC_INT_ACMDCRCERR) {
			pr_err("[%s]: msdc%d, autocmd23 crc error\n",
				__func__, host->id);
			sbc->error = (unsigned int)-EILSEQ;
			
			cmd->error = (unsigned int)-EILSEQ;
			
			msdc_reset_hw(host->id);
		} else if (intsts & MSDC_INT_ACMDTMO) {
			pr_err("[%s]: msdc%d, autocmd23 tmo error\n",
				__func__, host->id);
			sbc->error = (unsigned int)-ETIMEDOUT;
			
			cmd->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(host->id);
			
			msdc_reset_hw(host->id);
		}
	}
#endif 

 out:
	host->cmd = NULL;

	if (!cmd->data && !cmd->error)
		host->prev_cmd_cause_dump = 0;

	return cmd->error;
}

unsigned int msdc_do_command(struct msdc_host *host,
	struct mmc_command *cmd,
	int tune,
	unsigned long timeout)
{
	MVG_EMMC_DECLARE_INT32(delay_ns);
	MVG_EMMC_DECLARE_INT32(delay_us);
	MVG_EMMC_DECLARE_INT32(delay_ms);

	if ((cmd->opcode == MMC_GO_IDLE_STATE) &&
	    (host->hw->host_function == MSDC_SD)) {
		mdelay(10);
	}

	MVG_EMMC_ERASE_MATCH(host, (u64)cmd->arg, delay_ms, delay_us,
		delay_ns, cmd->opcode);

	if (msdc_command_start(host, cmd, tune, timeout))
		goto end;

	MVG_EMMC_ERASE_RESET(delay_ms, delay_us, cmd->opcode);

	if (msdc_command_resp_polling(host, cmd, tune, timeout))
		goto end;

 end:
	N_MSG(CMD, "        return<%d> resp<0x%.8x>", cmd->error, cmd->resp[0]);
	return cmd->error;
}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static unsigned int msdc_cmdq_command_start(struct msdc_host *host,
	struct mmc_command *cmd,
	int tune,
	unsigned long timeout)
{
	void __iomem *base = host->base;
	u32 opcode = cmd->opcode;
	u32 rawarg;
	u32 resp;
	unsigned long tmo;
	u32 wints_cq_cmd = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;

	switch (opcode) {
	case MMC_SET_QUEUE_CONTEXT:
	case MMC_QUEUE_READ_ADDRESS:
	case MMC_SEND_STATUS:
		break;
	default:
		pr_err("[%s]: ERROR, only CMD44/CMD45/CMD13 can issue\n",
			__func__);
		break;
	}

	resp = RESP_R1;
	cmd->error = 0;

	N_MSG(CMD, "CMD<%d>        Arg<0x%.8x>", opcode, cmd->arg);

	tmo = jiffies + timeout;

	for (;;) {
		if (!sdc_is_cmd_busy())
			break;

		if (time_after(jiffies, tmo)) {
			ERR_MSG("[%s]: XXX cmd_busy timeout: before CMD<%d>",
				__func__ , opcode);
			cmd->error = (unsigned int)-ETIMEDOUT;
			msdc_reset_hw(host->id);
			return cmd->error;
		}
	}

	host->cmd	  = cmd;
	host->cmd_rsp = resp;

	
	MSDC_CLR_BIT32(MSDC_INTEN, wints_cq_cmd);
	rawarg = cmd->arg;

	dbg_add_host_log(host->mmc, 0, cmd->opcode, cmd->arg);
	sdc_send_cmdq_cmd(opcode, rawarg);

	return 0;
}

static unsigned int msdc_cmdq_command_resp_polling(struct msdc_host *host,
	struct mmc_command *cmd,
	int tune,
	unsigned long timeout)
{
	void __iomem *base = host->base;
	u32 intsts;
	u32 resp;
	unsigned long tmo;
	u32 cmdsts = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;

	resp = host->cmd_rsp;

	
	tmo = jiffies + timeout;
	while (1) {
		intsts = MSDC_READ32(MSDC_INT);
		if ((intsts & cmdsts) != 0) {
			
			intsts &= cmdsts;
			MSDC_WRITE32(MSDC_INT, intsts);
			break;
		}

		if (time_after(jiffies, tmo)) {
			pr_err("[%s]: msdc%d CMD<%d> polling_for_completion timeout ARG<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			cmd->error = (unsigned int)-ETIMEDOUT;
			host->sw_timeout++;
			msdc_dump_info(host->id);
			
			goto out;
		}
	}

	
	if (intsts & cmdsts) {
		if (intsts & MSDC_INT_CMDRDY) {
			u32 *rsp = NULL;

			rsp = &cmd->resp[0];
			switch (host->cmd_rsp) {
			case RESP_NONE:
				break;
			case RESP_R2:
				*rsp++ = MSDC_READ32(SDC_RESP3);
				*rsp++ = MSDC_READ32(SDC_RESP2);
				*rsp++ = MSDC_READ32(SDC_RESP1);
				*rsp++ = MSDC_READ32(SDC_RESP0);
				break;
			default: 
				*rsp = MSDC_READ32(SDC_RESP0);
				break;
			}
			dbg_add_host_log(host->mmc, 1, cmd->opcode, cmd->resp[0]);
		} else if (intsts & MSDC_INT_RSPCRCERR) {
			cmd->error = (unsigned int)-EILSEQ;
			pr_err("[%s]: msdc%d XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			msdc_dump_info(host->id);
			
		} else if (intsts & MSDC_INT_CMDTMO) {
			cmd->error = (unsigned int)-ETIMEDOUT;
			pr_err("[%s]: msdc%d XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			mmc_cmd_dump(host->mmc);
			msdc_dump_info(host->id);
			
		}
	}
out:
	host->cmd = NULL;
	MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_CMDQEN, (0));

	if (!cmd->data && !cmd->error)
		host->prev_cmd_cause_dump = 0;

	return cmd->error;
}

unsigned int msdc_do_cmdq_command(struct msdc_host *host,
	struct mmc_command *cmd,
	int tune,
	unsigned long timeout)
{
	if (msdc_cmdq_command_start(host, cmd, tune, timeout))
		goto end;

	if (msdc_cmdq_command_resp_polling(host, cmd, tune, timeout))
		goto end;
end:
	N_MSG(CMD, "		return<%d> resp<0x%.8x>", cmd->error, cmd->resp[0]);
	return cmd->error;
}
#endif

static int msdc_pio_abort(struct msdc_host *host, struct mmc_data *data,
	unsigned long tmo)
{
	int  ret = 0;
	void __iomem *base = host->base;

	if (atomic_read(&host->abort))
		ret = 1;

	if (time_after(jiffies, tmo)) {
		data->error = (unsigned int)-ETIMEDOUT;
		ERR_MSG("XXX PIO Data Timeout: CMD<%d>",
			host->mrq->cmd->opcode);
		msdc_dump_info(host->id);
		ret = 1;
	}

	if (ret) {
		msdc_reset_hw(host->id);
		ERR_MSG("msdc pio find abort");
	}
	return ret;
}

int msdc_pio_read(struct msdc_host *host, struct mmc_data *data)
{
	struct scatterlist *sg = data->sg;
	void __iomem *base = host->base;
	u32 num = data->sg_len;
	u32 *ptr;
	u8 *u8ptr;
	u32 left = 0;
	u32 count, size = 0;
	u32 wints = MSDC_INTEN_DATTMO | MSDC_INTEN_DATCRCERR
		| MSDC_INTEN_XFER_COMPL;
	u32 ints = 0;
	bool get_xfer_done = 0;
	unsigned long tmo = jiffies + DAT_TIMEOUT;
	struct page *hmpage = NULL;
	int i = 0, subpage = 0, totalpages = 0;
	int flag = 0;
	ulong kaddr[DIV_ROUND_UP(MAX_SGMT_SZ, PAGE_SIZE)];

	BUG_ON(sg == NULL);
	
	while (1) {
		if (!get_xfer_done) {
			ints = MSDC_READ32(MSDC_INT);
			latest_int_status[host->id] = ints;
			ints &= wints;
			MSDC_WRITE32(MSDC_INT, ints);
		}
		if (ints & MSDC_INT_DATTMO) {
			data->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(host->id);
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_DATCRCERR) {
			data->error = (unsigned int)-EILSEQ;
			
			msdc_reset_hw(host->id);
			
			
			break;
		} else if (ints & MSDC_INT_XFER_COMPL) {
			get_xfer_done = 1;
		}
		if (get_xfer_done && (num == 0) && (left == 0))
			break;
		if (msdc_pio_abort(host, data, tmo))
			goto end;
		if ((num == 0) && (left == 0))
			continue;
		left = msdc_sg_len(sg, host->dma_xfer);
		ptr = sg_virt(sg);
		flag = 0;

		if  ((ptr != NULL) &&
		     !(PageHighMem((struct page *)(sg->page_link & ~0x3))))
			goto check_fifo1;

		hmpage = (struct page *)(sg->page_link & ~0x3);
		totalpages = DIV_ROUND_UP((left + sg->offset), PAGE_SIZE);
		subpage = (left + sg->offset) % PAGE_SIZE;

		if (subpage != 0 || (sg->offset != 0))
			N_MSG(OPS, "msdc%d: read size or start not align %x,%x, hmpage %lx,sg offset %x\n",
				host->id, subpage, left, (ulong)hmpage,
				sg->offset);

		for (i = 0; i < totalpages; i++) {
			kaddr[i] = (ulong) kmap(hmpage + i);
			if ((i > 0) && ((kaddr[i] - kaddr[i - 1]) != PAGE_SIZE))
				flag = 1;
			if (!kaddr[i])
				ERR_MSG("msdc0:kmap failed %lx", kaddr[i]);
		}

		ptr = sg_virt(sg);

		if (ptr == NULL)
			ERR_MSG("msdc0:sg_virt %p", ptr);

		if (flag == 0)
			goto check_fifo1;

		for (i = 0; i < totalpages; i++) {
			left = PAGE_SIZE;
			ptr = (u32 *) kaddr[i];

			if (i == 0) {
				left = PAGE_SIZE - sg->offset;
				ptr = (u32 *) (kaddr[i] + sg->offset);
			}
			if ((subpage != 0) && (i == (totalpages-1)))
				left = subpage;

check_fifo1:
			if ((flag == 1) && (left == 0))
				continue;
			else if ((flag == 0) && (left == 0))
				goto check_fifo_end;

			if ((msdc_rxfifocnt() >= MSDC_FIFO_THD) &&
			    (left >= MSDC_FIFO_THD)) {
				count = MSDC_FIFO_THD >> 2;
				do {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read32());
#else
					*ptr++ = msdc_fifo_read32();
#endif
				} while (--count);
				left -= MSDC_FIFO_THD;
			} else if ((left < MSDC_FIFO_THD) &&
				    msdc_rxfifocnt() >= left) {
				while (left > 3) {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read32());
#else
					*ptr++ = msdc_fifo_read32();
#endif
					left -= 4;
				}

				u8ptr = (u8 *) ptr;
				while (left) {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read8());
#else
					*u8ptr++ = msdc_fifo_read8();
#endif
					left--;
				}
			} else {
				ints = MSDC_READ32(MSDC_INT);
				latest_int_status[host->id] = ints;

				if (ints & MSDC_INT_DATCRCERR) {
					ERR_MSG("[msdc%d] DAT CRC error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-EILSEQ;
				} else if (ints & MSDC_INT_DATTMO) {
					ERR_MSG("[msdc%d] DAT TMO error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-ETIMEDOUT;
				} else {
					goto skip_msdc_dump_and_reset1;
				}

				if (ints & MSDC_INT_DATTMO)
					msdc_dump_info(host->id);

				MSDC_WRITE32(MSDC_INT, ints);
				msdc_reset_hw(host->id);
				goto end;
			}

skip_msdc_dump_and_reset1:
			if (msdc_pio_abort(host, data, tmo))
				goto end;

			goto check_fifo1;
		}

check_fifo_end:
		if (hmpage != NULL) {
			
			for (i = 0; i < totalpages; i++)
				kunmap(hmpage + i);

			hmpage = NULL;
		}
		size += msdc_sg_len(sg, host->dma_xfer);
		sg = sg_next(sg);
		num--;
	}
 end:
	if (hmpage != NULL) {
		for (i = 0; i < totalpages; i++)
			kunmap(hmpage + i);
		
	}
	data->bytes_xfered += size;
	N_MSG(FIO, "        PIO Read<%d>bytes", size);

	if (data->error)
		ERR_MSG("read pio data->error<%d> left<%d> size<%d>",
			data->error, left, size);

	if (!data->error)
		host->prev_cmd_cause_dump = 0;

	return data->error;
}

int msdc_pio_write(struct msdc_host *host, struct mmc_data *data)
{
	void __iomem *base = host->base;
	struct scatterlist *sg = data->sg;
	u32 num = data->sg_len;
	u32 *ptr;
	u8 *u8ptr;
	u32 left = 0;
	u32 count, size = 0;
	u32 wints = MSDC_INTEN_DATTMO | MSDC_INTEN_DATCRCERR
		| MSDC_INTEN_XFER_COMPL;
	bool get_xfer_done = 0;
	unsigned long tmo = jiffies + DAT_TIMEOUT;
	u32 ints = 0;
	struct page *hmpage = NULL;
	int i = 0, totalpages = 0;
	int flag, subpage = 0;
	ulong kaddr[DIV_ROUND_UP(MAX_SGMT_SZ, PAGE_SIZE)];

	
	while (1) {
		if (!get_xfer_done) {
			ints = MSDC_READ32(MSDC_INT);
			latest_int_status[host->id] = ints;
			ints &= wints;
			MSDC_WRITE32(MSDC_INT, ints);
		}
		if (ints & MSDC_INT_DATTMO) {
			data->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(host->id);
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_DATCRCERR) {
			data->error = (unsigned int)-EILSEQ;
			
			msdc_reset_hw(host->id);
			break;
		} else if (ints & MSDC_INT_XFER_COMPL) {
			get_xfer_done = 1;
		}
		if ((get_xfer_done == 1) && (num == 0) && (left == 0))
			break;
		if (msdc_pio_abort(host, data, tmo))
			goto end;
		if ((num == 0) && (left == 0))
			continue;
		left = msdc_sg_len(sg, host->dma_xfer);
		ptr = sg_virt(sg);

		flag = 0;

		if  ((ptr != NULL) &&
		     !(PageHighMem((struct page *)(sg->page_link & ~0x3))))
			goto check_fifo1;

		hmpage = (struct page *)(sg->page_link & ~0x3);
		totalpages = DIV_ROUND_UP(left + sg->offset, PAGE_SIZE);
		subpage = (left + sg->offset) % PAGE_SIZE;

		if ((subpage != 0) || (sg->offset != 0))
			N_MSG(OPS, "msdc%d: write size or start not align %x,%x, hmpage %lx,sg offset %x\n",
				host->id, subpage, left, (ulong)hmpage,
				sg->offset);

		
		for (i = 0; i < totalpages; i++) {
			kaddr[i] = (ulong) kmap(hmpage + i);
			if ((i > 0) && ((kaddr[i] - kaddr[i - 1]) != PAGE_SIZE))
				flag = 1;
			if (!kaddr[i])
				ERR_MSG("msdc0:kmap failed %lx\n", kaddr[i]);
		}

		ptr = sg_virt(sg);

		if (ptr == NULL)
			ERR_MSG("msdc0:write sg_virt %p\n", ptr);

		if (flag == 0)
			goto check_fifo1;

		for (i = 0; i < totalpages; i++) {
			left = PAGE_SIZE;
			ptr = (u32 *) kaddr[i];

			if (i == 0) {
				left = PAGE_SIZE - sg->offset;
				ptr = (u32 *) (kaddr[i] + sg->offset);
			}
			if (subpage != 0 && (i == (totalpages - 1)))
				left = subpage;

check_fifo1:
			if ((flag == 1) && (left == 0))
				continue;
			else if ((flag == 0) && (left == 0))
				goto check_fifo_end;

			if (left >= MSDC_FIFO_SZ && msdc_txfifocnt() == 0) {
				count = MSDC_FIFO_SZ >> 2;
				do {
					msdc_fifo_write32(*ptr);
					ptr++;
				} while (--count);
				left -= MSDC_FIFO_SZ;
			} else if (left < MSDC_FIFO_SZ &&
				   msdc_txfifocnt() == 0) {
				while (left > 3) {
					msdc_fifo_write32(*ptr);
					ptr++;
					left -= 4;
				}
				u8ptr = (u8 *) ptr;
				while (left) {
					msdc_fifo_write8(*u8ptr);
					u8ptr++;
					left--;
				}
			} else {
				ints = MSDC_READ32(MSDC_INT);
				latest_int_status[host->id] = ints;

				if (ints & MSDC_INT_DATCRCERR) {
					ERR_MSG("[msdc%d] DAT CRC error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-EILSEQ;
				} else if (ints & MSDC_INT_DATTMO) {
					ERR_MSG("[msdc%d] DAT TMO error (0x%x), Left DAT: %d bytes\n",
						host->id, ints, left);
					data->error = (unsigned int)-ETIMEDOUT;
				} else {
					goto skip_msdc_dump_and_reset1;
				}

				msdc_dump_info(host->id);

				MSDC_WRITE32(MSDC_INT, ints);
				msdc_reset_hw(host->id);
				goto end;
			}

skip_msdc_dump_and_reset1:
			if (msdc_pio_abort(host, data, tmo))
				goto end;

			goto check_fifo1;
		}

check_fifo_end:
		if (hmpage != NULL) {
			for (i = 0; i < totalpages; i++)
				kunmap(hmpage + i);

			hmpage = NULL;

		}
		size += msdc_sg_len(sg, host->dma_xfer);
		sg = sg_next(sg);
		num--;
	}
 end:
	if (hmpage != NULL) {
		for (i = 0; i < totalpages; i++)
			kunmap(hmpage + i);
		pr_err("msdc0 write unmap 0x%x:\n", left);
	}
	data->bytes_xfered += size;
	N_MSG(FIO, "        PIO Write<%d>bytes", size);

	if (data->error)
		ERR_MSG("write pio data->error<%d> left<%d> size<%d>",
			data->error, left, size);

	if (!data->error)
		host->prev_cmd_cause_dump = 0;

	
	return data->error;
}

static void msdc_dma_start(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 wints = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO
		| MSDC_INTEN_DATCRCERR;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	host->mmc->is_data_dma = 1;
#endif

	if (host->autocmd & MSDC_AUTOCMD12)
		wints |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO
			| MSDC_INT_ACMDRDY;
	MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);

	MSDC_SET_BIT32(MSDC_INTEN, wints);

	N_MSG(DMA, "DMA start");
	
	if (host->data && (host->data->flags & MMC_DATA_WRITE)) {
		host->write_timeout_ms = min_t(u32, max_t(u32,
			host->data->blocks * 500,
			host->data->timeout_ns / 1000000), 10 * 1000);
		schedule_delayed_work(&host->write_timeout,
			msecs_to_jiffies(host->write_timeout_ms));
		N_MSG(DMA, "DMA Data Busy Timeout:%u ms, schedule_delayed_work",
			host->write_timeout_ms);
	}
}

static void msdc_dma_stop(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int retry = 500;
	int count = 1000;
	u32 wints = MSDC_INTEN_XFER_COMPL | MSDC_INTEN_DATTMO
		| MSDC_INTEN_DATCRCERR;

	
	if (host->data && (host->data->flags & MMC_DATA_WRITE)) {
		cancel_delayed_work(&host->write_timeout);
		N_MSG(DMA, "DMA Data Busy Timeout:%u ms, cancel_delayed_work",
			host->write_timeout_ms);
		host->write_timeout_ms = 0; 
	}

	
	if (host->autocmd & MSDC_AUTOCMD12)
		wints |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO
			| MSDC_INT_ACMDRDY;
	N_MSG(DMA, "DMA status: 0x%.8x", MSDC_READ32(MSDC_DMA_CFG));

	MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	msdc_retry((MSDC_READ32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS), retry,
		count, host->id);
	if (retry == 0) {
		msdc_dump_info(host->id);
		mdelay(10);
		msdc_dump_gpd_bd(host->id);
	}

	MSDC_CLR_BIT32(MSDC_INTEN, wints); 

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	host->mmc->is_data_dma = 0;
#endif

	N_MSG(DMA, "DMA stop");
}

static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xFF - (u8) sum;
}

static int msdc_dma_config(struct msdc_host *host, struct msdc_dma *dma)
{
	void __iomem *base = host->base;
	u32 sglen = dma->sglen;
	u32 j, num, bdlen;
	dma_addr_t dma_address;
	u32 dma_len;
	u8  blkpad, dwpad, chksum;
	struct scatterlist *sg = dma->sg;
	struct gpd_t *gpd;
	struct bd_t *bd, vbd = {0};

	switch (dma->mode) {
	case MSDC_MODE_DMA_BASIC:
		if (host->hw->host_function == MSDC_SDIO)
			BUG_ON(dma->xfersz > 0xFFFFFFFF);
		else
			BUG_ON(dma->xfersz > 65535);

		BUG_ON(dma->sglen != 1);
		dma_address = sg_dma_address(sg);
		dma_len = msdc_sg_len(sg, host->dma_xfer);

		N_MSG(DMA, "BASIC DMA len<%x> dma_address<%llx>",
			dma_len, (u64)dma_address);

		MSDC_WRITE32(MSDC_DMA_SA, dma_address);

		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_LASTBUF, 1);
		MSDC_WRITE32(MSDC_DMA_LEN, dma_len);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
			dma->burstsz);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 0);
		break;

	case MSDC_MODE_DMA_DESC:
		blkpad = (dma->flags & DMA_FLAG_PAD_BLOCK) ? 1 : 0;
		dwpad  = (dma->flags & DMA_FLAG_PAD_DWORD) ? 1 : 0;
		chksum = (dma->flags & DMA_FLAG_EN_CHKSUM) ? 1 : 0;

		
		num = (sglen + MAX_BD_PER_GPD - 1) / MAX_BD_PER_GPD;
		BUG_ON(num != 1);

		gpd = dma->gpd;
		bd  = dma->bd;
		bdlen = sglen;

		
		gpd->hwo = 1;   
		gpd->bdp = 1;
		gpd->chksum = 0;        
		gpd->chksum = (chksum ? msdc_dma_calcs((u8 *) gpd, 16) : 0);

		
		for (j = 0; j < bdlen; j++) {
#ifdef MSDC_DMA_VIOLATION_DEBUG
			if (g_dma_debug[host->id] &&
			    (msdc_latest_op[host->id] == OPER_TYPE_READ)) {
				pr_debug("[%s] msdc%d do write 0x10000\n",
					__func__, host->id);
				dma_address = 0x10000;
			} else {
				dma_address = sg_dma_address(sg);
			}
#else
			dma_address = sg_dma_address(sg);
#endif

			dma_len = msdc_sg_len(sg, host->dma_xfer);

			N_MSG(DMA, "DESC DMA len<%x> dma_address<%llx>",
				dma_len, (u64)dma_address);

			memcpy(&vbd, &bd[j], sizeof(struct bd_t));

			msdc_init_bd(&vbd, blkpad, dwpad, dma_address,
				dma_len);

			if (j == bdlen - 1)
				vbd.eol = 1;  
			else
				vbd.eol = 0;

			
			vbd.chksum = 0;
			vbd.chksum = (chksum ?
				msdc_dma_calcs((u8 *) (&vbd), 16) : 0);

			memcpy(&bd[j], &vbd, sizeof(struct bd_t));

			sg++;
		}
#ifdef MSDC_DMA_VIOLATION_DEBUG
		if (g_dma_debug[host->id] &&
		    (msdc_latest_op[host->id] == OPER_TYPE_READ))
			g_dma_debug[host->id] = 0;
#endif

		dma->used_gpd += 2;
		dma->used_bd += bdlen;

		MSDC_SET_FIELD(MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, chksum);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
			dma->burstsz);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 1);

		MSDC_WRITE32(MSDC_DMA_SA, (u32) dma->gpd_addr);
		break;

	default:
		break;
	}

	N_MSG(DMA, "DMA_CTRL = 0x%x", MSDC_READ32(MSDC_DMA_CTRL));
	N_MSG(DMA, "DMA_CFG  = 0x%x", MSDC_READ32(MSDC_DMA_CFG));
	N_MSG(DMA, "DMA_SA   = 0x%x", MSDC_READ32(MSDC_DMA_SA));

	return 0;
}

static void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
	struct scatterlist *sg, unsigned int sglen)
{
	u32 max_dma_len = 0;

	BUG_ON(sglen > MAX_BD_NUM);     

	dma->sg = sg;
	dma->flags = DMA_FLAG_EN_CHKSUM;
	 
	dma->sglen = sglen;
	dma->xfersz = host->xfer_size;
	dma->burstsz = MSDC_BRUST_64B;

	if (host->hw->host_function == MSDC_SDIO)
		max_dma_len = MAX_DMA_CNT_SDIO;
	else
		max_dma_len = MAX_DMA_CNT;

	if (sglen == 1 &&
	     msdc_sg_len(sg, host->dma_xfer) <= max_dma_len)
		dma->mode = MSDC_MODE_DMA_BASIC;
	else
		dma->mode = MSDC_MODE_DMA_DESC;

	N_MSG(DMA, "DMA mode<%d> sglen<%d> xfersz<%d>", dma->mode, dma->sglen,
		dma->xfersz);

	msdc_dma_config(host, dma);
}

static void msdc_dma_clear(struct msdc_host *host)
{
	void __iomem *base = host->base;

	host->data = NULL;
	host->mrq = NULL;
	host->dma_xfer = 0;
	msdc_dma_off();
	host->dma.used_bd = 0;
	host->dma.used_gpd = 0;
	host->blksz = 0;
}

static void msdc_set_blknum(struct msdc_host *host, u32 blknum)
{
	void __iomem *base = host->base;

	MSDC_WRITE32(SDC_BLK_NUM, blknum);
}

static void msdc_log_cmd(struct msdc_host *host, struct mmc_command *cmd,
	struct mmc_data *data)
{
	N_MSG(OPS, "CMD<%d> data<%s %s> blksz<%d> block<%d> error<%d>",
		cmd->opcode, (host->dma_xfer ? "dma" : "pio"),
		((data->flags & MMC_DATA_READ) ? "read " : "write"),
		data->blksz, data->blocks, data->error);

	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		if (!check_mmc_cmd2425(cmd->opcode) &&
		    !check_mmc_cmd1718(cmd->opcode)) {
			N_MSG(NRW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> data<%s> size<%d>",
				cmd->opcode, cmd->arg, cmd->resp[0],
				((data->flags & MMC_DATA_READ)
					? "read " : "write"),
				data->blksz * data->blocks);
		} else if (cmd->opcode != 13) { 
			N_MSG(NRW, "CMD<%3d> arg<0x%8x> resp<%8x %8x %8x %8x>",
				cmd->opcode, cmd->arg, cmd->resp[0],
				cmd->resp[1], cmd->resp[2], cmd->resp[3]);
		} else {
			N_MSG(RW, "CMD<%3d> arg<0x%8x> Resp<0x%8x> block<%d>",
				cmd->opcode, cmd->arg, cmd->resp[0],
				data->blocks);
		}
	}
}

void msdc_sdio_restore_after_resume(struct msdc_host *host)
{
	void __iomem *base = host->base;

	if (host->saved_para.hz) {
		if ((host->saved_para.suspend_flag)
		 || ((host->saved_para.msdc_cfg != 0) &&
		     ((host->saved_para.msdc_cfg&0xFFFFFF9F) !=
		      (MSDC_READ32(MSDC_CFG)&0xFFFFFF9F)))) {
			ERR_MSG("msdc resume[ns] cur_cfg=%x, save_cfg=%x, cur_hz=%d, save_hz=%d",
				MSDC_READ32(MSDC_CFG),
				host->saved_para.msdc_cfg, host->mclk,
				host->saved_para.hz);
			host->saved_para.suspend_flag = 0;
			msdc_restore_timing_setting(host);
		}
	}
}

int msdc_if_send_stop(struct msdc_host *host,
	struct mmc_command *cmd, struct mmc_data *data)
{
	if (!data || !data->stop)
		return 0;

	if ((cmd->error != 0)
	 || (data->error != 0)
	 || !(host->autocmd & MSDC_AUTOCMD12)
	 || !(check_mmc_cmd1825(cmd->opcode))) {
		if (msdc_do_command(host, data->stop, 0, CMD_TIMEOUT) != 0)
			return 1;
	}

	return 0;
}

void msdc_if_set_err(struct msdc_host *host, struct mmc_request *mrq,
	struct mmc_command *cmd)
{
	if (mrq->cmd->error == (unsigned int)-EILSEQ) {
		if (((cmd->opcode == MMC_SELECT_CARD) ||
		     (cmd->opcode == MMC_SLEEP_AWAKE))
		 && ((host->hw->host_function == MSDC_EMMC) ||
		     (host->hw->host_function == MSDC_SD))) {
			mrq->cmd->error = 0x0;
		} else {
			host->error |= REQ_CMD_EIO;
		}
	}
	if (mrq->cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;
	if (mrq->data && (mrq->data->error))
		host->error |= REQ_DAT_ERR;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-EILSEQ))
		host->error |= REQ_STOP_EIO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-ETIMEDOUT))
		host->error |= REQ_STOP_TMO;
}

int msdc_rw_cmd_dma(struct mmc_host *mmc, struct mmc_command *cmd,
	struct mmc_data *data, struct mmc_request *mrq, int tune)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	int map_sg = 0;
	int dir;

	msdc_dma_on();  

	init_completion(&host->xfer_done);

	if (msdc_command_start(host, cmd, 0, CMD_TIMEOUT) != 0)
		return -1;

	if (tune == 0) {
		dir = data->flags & MMC_DATA_READ ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE;
		(void)dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
		map_sg = 1;
	}

	
	if (msdc_command_resp_polling(host, cmd, 0, CMD_TIMEOUT) != 0)
		return -2;

	msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);
	msdc_dma_start(host);

	spin_unlock(&host->lock);
	if (!wait_for_completion_timeout(&host->xfer_done, DAT_TIMEOUT)) {
		ERR_MSG("XXX CMD<%d> ARG<0x%x> wait xfer_done<%d> timeout!!",
			cmd->opcode, cmd->arg, data->blocks * data->blksz);

		host->sw_timeout++;

		msdc_dump_info(host->id);
		data->error = (unsigned int)-ETIMEDOUT;
		msdc_reset(host->id);
	}
	spin_lock(&host->lock);

	msdc_dma_stop(host);

	if (((host->autocmd & MSDC_AUTOCMD12) && mrq->stop && mrq->stop->error)
	 || (mrq->data && mrq->data->error)
	 || (mrq->sbc && (mrq->sbc->error != 0) &&
	    (host->autocmd & MSDC_AUTOCMD23))) {
		msdc_clr_fifo(host->id);
		msdc_clr_int();
	}

	if (tune)
		return 0;
	else
		return map_sg;
}

#define PREPARE_NON_ASYNC       0
#define PREPARE_ASYNC           1
#define PREPARE_TUNE            2
int msdc_do_request_prepare(struct msdc_host *host,
	struct mmc_request *mrq,
	struct mmc_command *cmd,
	struct mmc_data *data,
	u32 *l_force_prg,
	u32 *l_bypass_flush,
	int prepare_case)
{
#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	void __iomem *base = host->base;
#endif

	#ifndef MSDC_WQ_ERROR_TUNE
	if ((prepare_case != PREPARE_TUNE) && (host->mmc->bus_dead != 1))
		host->error = 0;
	#else
	if (prepare_case != PREPARE_TUNE)
		host->error = 0;
	#endif

	atomic_set(&host->abort, 0);

#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		pr_err("[SD%d] register abnormal,please check!\n", host->id);
		msdc_reset_hw(host->id);
	}
#endif

	if ((prepare_case == PREPARE_NON_ASYNC) && !data) {

#ifdef MTK_MSDC_USE_CACHE
		if ((host->hw->host_function == MSDC_EMMC) &&
		    check_mmc_cache_flush_cmd(cmd)) {
			if (g_cache_status == CACHE_FLUSHED) {
				N_MSG(CHE, "bypass flush command, g_cache_status=%d",
					g_cache_status);
				*l_bypass_flush = 1;
				return 1;
			}
			*l_bypass_flush = 0;
		}
#endif

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		if (check_mmc_cmd13_sqs(cmd)) {
			if (msdc_do_cmdq_command(host, cmd, 0, CMD_TIMEOUT) != 0)
				return 1;
		} else {
#endif
		if (msdc_do_command(host, cmd, 0, CMD_TIMEOUT) != 0)
			return 1;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		}
#endif

		
		if ((host->hw->host_function == MSDC_EMMC) &&
			(cmd->opcode == MMC_ALL_SEND_CID))
			g_emmc_id = UNSTUFF_BITS(cmd->resp, 120, 8);

		return 1;
	}

	BUG_ON(data->blksz > HOST_MAX_BLKSZ);

	data->error = 0;
	msdc_latest_op[host->id] = (data->flags & MMC_DATA_READ)
		? OPER_TYPE_READ : OPER_TYPE_WRITE;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	
	if (!check_mmc_cmd13_sqs(cmd))
		host->data = data;
#else
	host->data = data;
#endif

	host->xfer_size = data->blocks * data->blksz;
	host->blksz = data->blksz;
	if (prepare_case != PREPARE_NON_ASYNC) {
		host->dma_xfer = 1;
	} else {
		
		if (drv_mode[host->id] == MODE_PIO) {
			host->dma_xfer = 0;
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_PIO;
		} else if (drv_mode[host->id] == MODE_DMA) {
			host->dma_xfer = 1;
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_DMA;
		} else if (drv_mode[host->id] == MODE_SIZE_DEP) {
			host->dma_xfer = (host->xfer_size >= dma_size[host->id])
				? 1 : 0;
			msdc_latest_transfer_mode[host->id] =
				host->dma_xfer ? TRAN_MOD_DMA : TRAN_MOD_PIO;
		}
	}

	if (data->flags & MMC_DATA_READ) {
		if ((host->timeout_ns != data->timeout_ns) ||
		    (host->timeout_clks != data->timeout_clks)) {
			msdc_set_timeout(host, data->timeout_ns,
				data->timeout_clks);
		}
	}

	msdc_set_blknum(host, data->blocks);
	 

#ifdef MTK_MSDC_USE_CACHE
	if (prepare_case != PREPARE_TUNE
	 && check_mmc_cache_ctrl(host->mmc->card)
	 && (cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK))
		*l_force_prg = !msdc_can_apply_cache(cmd->arg, data->blocks);
#endif

	return 0;
}

int msdc_do_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	u32 l_autocmd23_is_set = 0;
#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#endif
#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;
	
	u32 l_bypass_flush = 2;
#endif
	void __iomem *base = host->base;
	
	unsigned int left = 0;
	int read = 1, dir = DMA_FROM_DEVICE;
	u32 map_sg = 0;
	unsigned long pio_tmo;

	if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))
		msdc_sdio_restore_after_resume(host);

#if (MSDC_DATA1_INT == 1)
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		if ((u_sdio_irq_counter > 0) && ((u_sdio_irq_counter%800) == 0))
			ERR_MSG("sdio_irq=%d, msdc_irq=%d  SDC_CFG=%x MSDC_INTEN=%x MSDC_INT=%x ",
				u_sdio_irq_counter, u_msdc_irq_counter,
				MSDC_READ32(SDC_CFG), MSDC_READ32(MSDC_INTEN),
				MSDC_READ32(MSDC_INT));
	}
#endif

	BUG_ON(mmc == NULL || mrq == NULL);

	cmd = mrq->cmd;
	data = mrq->cmd->data;

#ifdef MTK_MSDC_USE_CACHE
	if (msdc_do_request_prepare(host, mrq, cmd, data, &l_force_prg,
		&l_bypass_flush, PREPARE_NON_ASYNC))
#else
	if (msdc_do_request_prepare(host, mrq, cmd, data, NULL,
		NULL, PREPARE_NON_ASYNC))
#endif
		goto done;

#ifdef MTK_MSDC_USE_CMD23
	if (0 == (host->autocmd & MSDC_AUTOCMD23)) {
		if (mrq->sbc) {
			host->autocmd &= ~MSDC_AUTOCMD12;

			if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
				if (!((mrq->sbc->arg >> 31) & 0x1) &&
				    l_force_prg)
					mrq->sbc->arg |= (1 << 24);
#endif
			}

			if (msdc_command_start(host, mrq->sbc, 0,
				CMD_TIMEOUT) != 0)
				goto done;

			
			if (msdc_command_resp_polling(host, mrq->sbc, 0,
				CMD_TIMEOUT) != 0) {
				goto stop;
			}
		} else {
			if (host->hw->host_function != MSDC_SDIO)
				host->autocmd |= MSDC_AUTOCMD12;
		}
	} else {
		
		if (mrq->sbc) {
			host->autocmd &= ~MSDC_AUTOCMD12;
			if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
				if (!((mrq->sbc->arg >> 31) & 0x1) &&
				    l_force_prg)
					mrq->sbc->arg |= (1 << 24);
#endif
			}
		} else {
			if (host->hw->host_function != MSDC_SDIO) {
				host->autocmd &= ~MSDC_AUTOCMD23;
				host->autocmd |= MSDC_AUTOCMD12;
				l_card_no_cmd23 = 1;
			}
		}
	}
#endif 

	read = data->flags & MMC_DATA_READ ? 1 : 0;
	if (host->dma_xfer) {
#ifndef MTK_MSDC_USE_CMD23
		
		if (host->hw->host_function != MSDC_SDIO)
			host->autocmd |= MSDC_AUTOCMD12;
#endif
		map_sg = msdc_rw_cmd_dma(mmc, cmd, data, mrq, 0);
		if (map_sg == -1)
			goto done;
		else if (map_sg == -2)
			goto stop;

	} else {
		
		if (is_card_sdio(host)) {
			msdc_reset_hw(host->id);
			msdc_dma_off();
			data->error = 0;
		}
		
		host->autocmd &= ~MSDC_AUTOCMD12;

		l_autocmd23_is_set = 0;
		if (host->autocmd & MSDC_AUTOCMD23) {
			l_autocmd23_is_set = 1;
			host->autocmd &= ~MSDC_AUTOCMD23;
		}

		host->dma_xfer = 0;
		if (msdc_do_command(host, cmd, 0, CMD_TIMEOUT) != 0)
			goto stop;

		
		if (read) {
#ifdef MTK_MSDC_DUMP_FIFO
			pr_debug("[%s]: start pio read\n", __func__);
#endif
			if (msdc_pio_read(host, data)) {
				msdc_gate_clock(host, 0);
				msdc_ungate_clock(host);
				goto stop;      
			}
		} else {
#ifdef MTK_MSDC_DUMP_FIFO
			pr_debug("[%s]: start pio write\n", __func__);
#endif
			if (msdc_pio_write(host, data)) {
				msdc_gate_clock(host, 0);
				msdc_ungate_clock(host);
				goto stop;
			}


			pio_tmo = jiffies + DAT_TIMEOUT;
			while (1) {
				left = msdc_txfifocnt();
				if (left == 0)
					break;

				if (msdc_pio_abort(host, data, pio_tmo))
					break;
			}
		}
	}

stop:
	
	if (l_autocmd23_is_set == 1) {
		l_autocmd23_is_set = 0;
		host->autocmd |= MSDC_AUTOCMD23;
	}

#ifndef MTK_MSDC_USE_CMD23
	
	if (msdc_if_send_stop(host, cmd, data))
		goto done;

#else

	if (host->hw->host_function == MSDC_EMMC) {
		if (!check_mmc_cmd2425(cmd->opcode))
			goto done;
		if (((mrq->sbc == NULL) &&
		     !(host->autocmd & MSDC_AUTOCMD12))
		 || (!host->dma_xfer && mrq->sbc &&
		     (host->autocmd & MSDC_AUTOCMD23))) {
			if (msdc_do_command(host, data->stop, 0,
				CMD_TIMEOUT) != 0)
				goto done;
		}
	} else {
		if (msdc_if_send_stop(host, cmd, data))
			goto done;
	}
#endif
done:

#ifdef MTK_MSDC_USE_CMD23
	if (1 == l_card_no_cmd23) {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD23;
			host->autocmd &= ~MSDC_AUTOCMD12;
			l_card_no_cmd23 = 0;
		}
	}
#endif

	if (data != NULL) {
		host->data = NULL;

		if (host->dma_xfer != 0) {
			host->dma_xfer = 0;
			msdc_dma_off();
			host->dma.used_bd = 0;
			host->dma.used_gpd = 0;
			if (map_sg == 1) {
				dma_unmap_sg(mmc_dev(mmc), data->sg,
					data->sg_len, dir);
			}
		}

		if ((cmd->opcode == MMC_SEND_EXT_CSD) &&
			(host->hw->host_function == MSDC_EMMC))
				msdc_cache_onoff(data);

		host->blksz = 0;

		msdc_log_cmd(host, cmd, data);
	}

	if (mrq->cmd->error == (unsigned int)-EILSEQ) {
		if (((cmd->opcode == MMC_SELECT_CARD) ||
		     (cmd->opcode == MMC_SLEEP_AWAKE))
		 && ((host->hw->host_function == MSDC_EMMC) ||
		     (host->hw->host_function == MSDC_SD))) {
			mrq->cmd->error = 0x0;
		} else {
			host->error |= REQ_CMD_EIO;

			if (mrq->cmd->opcode == SD_IO_RW_EXTENDED)
				sdio_tune_flag |= 0x1;
		}
	}

	if (mrq->cmd->error == (unsigned int)-ETIMEDOUT) {
		if (mrq->cmd->opcode == MMC_SLEEP_AWAKE) {
			if (mrq->cmd->arg & 0x8000) {
				pr_err("Sleep_Awake CMD timeout, MSDC_PS %0x\n",
					MSDC_READ32(MSDC_PS));
				emmc_sleep_failed = 1;
				mrq->cmd->error = 0x0;
				pr_err("eMMC sleep CMD5 TMO will reinit\n");
			} else {
				host->error |= REQ_CMD_TMO;
			}
		} else {
			host->error |= REQ_CMD_TMO;
		}
	}

	if (mrq->data && mrq->data->error) {
		host->error |= REQ_DAT_ERR;
		sdio_tune_flag |= 0x10;

		if (mrq->data->flags & MMC_DATA_READ)
			sdio_tune_flag |= 0x80;
		else
			sdio_tune_flag |= 0x40;
	}

#ifdef MTK_MSDC_USE_CMD23
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-EILSEQ))
		host->error |= REQ_CMD_EIO;
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_NE_JBT_TRACES|DB_OPT_DISPLAY_HANG_DUMP,
			"\n@eMMC FATAL ERROR@\n", "eMMC fatal error ");
#endif
		host->error |= REQ_CMD_TMO;
	}
#endif

	if (mrq->stop && (mrq->stop->error == (unsigned int)-EILSEQ))
		host->error |= REQ_STOP_EIO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-ETIMEDOUT))
		host->error |= REQ_STOP_TMO;

#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host) && !host->error)
		host->sdio_error = 0;
#endif

#ifdef MTK_MSDC_USE_CACHE
	msdc_update_cache_flush_status(host, mrq, data, l_bypass_flush);
#endif

	return host->error;
}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static int msdc_do_discard_task_cq(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 task_id;

	task_id = (mrq->sbc->arg >> 16) & 0x1f;
	memset(&mmc->deq_cmd, 0, sizeof(struct mmc_command));
	mmc->deq_cmd.opcode = MMC_CMDQ_TASK_MGMT;
	mmc->deq_cmd.arg = 2 | (task_id << 16);
	mmc->deq_cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1B | MMC_CMD_AC;
	mmc->deq_cmd.data = NULL;
	msdc_do_command(host, &mmc->deq_cmd, 0, CMD_TIMEOUT);

	pr_err("[%s]: msdc%d, discard task id %d, CMD<%d> arg<0x%08x> rsp<0x%08x>",
		__func__, host->id, task_id, mmc->deq_cmd.opcode, mmc->deq_cmd.arg, mmc->deq_cmd.resp[0]);

	return mmc->deq_cmd.error;
}

static int msdc_do_request_cq(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;
#endif

	BUG_ON(mmc == NULL);
	BUG_ON(mrq == NULL);
	BUG_ON(mrq->data);

	host->error = 0;
	atomic_set(&host->abort, 0);

	cmd  = mrq->sbc;
	data = mrq->data;

	mrq->sbc->error = 0;
	mrq->cmd->error = 0;

#ifdef MTK_MSDC_USE_CACHE
	
	if (check_mmc_cache_ctrl(host->mmc->card) &&
		!((cmd->arg >> 30) & 0x1)) {
		l_force_prg = !msdc_can_apply_cache(mrq->cmd->arg, cmd->arg & 0xffff);
		
		if (!((cmd->arg >> 31) & 0x1) &&
			l_force_prg)
			cmd->arg |= (1 << 24);
	}
#endif

	if (msdc_do_cmdq_command(host, cmd, 0, CMD_TIMEOUT) != 0)
		goto done1;

done1:
	if (cmd->error == (unsigned int)-EILSEQ)
		host->error |= REQ_CMD_EIO;
	else if (cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;

	cmd  = mrq->cmd;
	data = mrq->cmd->data;

	if (msdc_do_cmdq_command(host, cmd, 0, CMD_TIMEOUT) != 0)
		goto done2;

done2:
	if (cmd->error == (unsigned int)-EILSEQ)
		host->error |= REQ_CMD_EIO;
	else if (cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;

	return host->error;
}

static int tune_cmdq_cmdrsp(struct mmc_host *mmc,
	struct mmc_request *mrq, int *retry)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	unsigned long polling_tmo = 0, polling_status_tmo;
	u32 state = 0;
	u32 err = 0, status = 0;

	do {
		err = msdc_get_card_status(mmc, host, &status);
		if (err) {
			
			polling_tmo = jiffies + 10 * HZ;
			pr_err("msdc%d waiting data transfer done\n",
				host->id);
			while (mmc->is_data_dma) {
				if (time_after(jiffies, polling_tmo))
					goto error;
			}

			ERR_MSG("get card status, err = %d", err);
#ifdef MSDC_AUTOK_ON_ERROR
			if (msdc_execute_tuning(mmc, MMC_SEND_STATUS)) {
				ERR_MSG("failed to updata cmd para");
				return 1;
			}
#else
			if (msdc_tune_cmdrsp(host)) {
				ERR_MSG("failed to updata cmd para");
				return 1;
			}
#endif
			continue;
		}

		if (status & (1 << 22)) {
			
			 (*retry)--;
			ERR_MSG("status = %x, illegal command, retry = %d",
				status, *retry);
			if ((mrq->cmd->error || mrq->sbc->error) && *retry)
				return 0;
			else
				return 1;
		} else {
			state = R1_CURRENT_STATE(status);
			if (state != R1_STATE_TRAN) {
				
				if (mmc->is_data_dma == 0) {
					if (state == R1_STATE_DATA || state == R1_STATE_RCV) {
						ERR_MSG("state<%d> need cmd12 to stop", state);
						msdc_send_stop(host);
					} else if (state == R1_STATE_PRG) {
						ERR_MSG("state<%d> card is busy", state);
						msleep(100);
					}
				}

				if (time_after(jiffies, polling_status_tmo))
					ERR_MSG("wait transfer state timeout\n");
				else {
					err = 1;
					continue;
				}
			}
			ERR_MSG("status = %x, discard task, re-send command",
				status);
			err = msdc_do_discard_task_cq(mmc, mrq);
			if (err == (unsigned int)-EIO)
				continue;
			else
				break;
		}
	} while (err);

	
	polling_tmo = jiffies + 10 * HZ;
	pr_err("msdc%d waiting data transfer done\n", host->id);
	while (mmc->is_data_dma) {
		if (time_after(jiffies, polling_tmo))
			goto error;
	}
	if (msdc_execute_tuning(mmc, MMC_SEND_STATUS)) {
		pr_err("msdc%d autok failed\n", host->id);
		return 1;
	}

	return 0;

error:
	ERR_MSG("waiting data transfer done TMO");
	msdc_dump_info(host->id);
	msdc_dma_stop(host);
	msdc_dma_clear(host);
	msdc_reset_hw(host->id);

	return -1;
}

static int tune_cmdq_data(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret;

	if (mrq->cmd && (mrq->cmd->error == (unsigned int)-EILSEQ)) {
		ret = msdc_tune_cmdrsp(host);
	} else if (mrq->data && (mrq->data->error == (unsigned int)-EILSEQ)) {
		if (host->timing == MMC_TIMING_MMC_HS400) {
			ret = emmc_hs400_tune_rw(host);
		} else if (host->timing == MMC_TIMING_MMC_HS200) {
			if (mrq->data->flags & MMC_DATA_READ)
				ret = msdc_tune_read(host);
			else
				ret = msdc_tune_write(host);
		}
	}

	return ret;
}
#endif

static int msdc_tune_rw_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	int ret;

#ifdef MTK_MSDC_USE_CMD23
	u32 l_autocmd23_is_set = 0;
#endif

	BUG_ON(mmc == NULL || mrq == NULL);

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	msdc_do_request_prepare(host, mrq, cmd, data, NULL,
		NULL, PREPARE_TUNE);

	if (host->hw->host_function != MSDC_SDIO) {
		host->autocmd |= MSDC_AUTOCMD12;

#ifdef MTK_MSDC_USE_CMD23
		
		l_autocmd23_is_set = 0;
		if (host->autocmd & MSDC_AUTOCMD23) {
			l_autocmd23_is_set = 1;
			host->autocmd &= ~MSDC_AUTOCMD23;
		}
#endif
	}

	ret = msdc_rw_cmd_dma(mmc, cmd, data, mrq, 1);
	if (ret == -1)
		goto done;
	else if (ret == -2)
		goto stop;

stop:
	
	if (msdc_if_send_stop(host, cmd, data))
		goto done;

done:
	msdc_dma_clear(host);
	host->mrq = mrq; 

	msdc_log_cmd(host, cmd, data);

	host->error = 0;

	msdc_if_set_err(host, mrq, cmd);

#ifdef MTK_MSDC_USE_CMD23
	if (l_autocmd23_is_set == 1) {
		
		host->autocmd |= MSDC_AUTOCMD23;
	}
#endif
	return host->error;
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq,
	bool is_first_req)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;
	struct mmc_command *cmd = mrq->cmd;
	int read = 1, dir = DMA_FROM_DEVICE;

	BUG_ON(!cmd);
	data = mrq->data;

	if (!data)
		return;

	data->host_cookie = MSDC_COOKIE_ASYNC;
	if (check_mmc_cmd1718(cmd->opcode) ||
	    check_mmc_cmd2425(cmd->opcode)) {
		host->xfer_size = data->blocks * data->blksz;
		read = data->flags & MMC_DATA_READ ? 1 : 0;
		if (drv_mode[host->id] == MODE_PIO) {
			data->host_cookie |= MSDC_COOKIE_PIO;
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_PIO;
		} else if (drv_mode[host->id] == MODE_DMA) {
			msdc_latest_transfer_mode[host->id] = TRAN_MOD_DMA;
		} else if (drv_mode[host->id] == MODE_SIZE_DEP) {
			if (host->xfer_size < dma_size[host->id]) {
				data->host_cookie |= MSDC_COOKIE_PIO;
				msdc_latest_transfer_mode[host->id] =
					TRAN_MOD_PIO;
			} else {
				msdc_latest_transfer_mode[host->id] =
					TRAN_MOD_DMA;
			}
		}
		if (msdc_use_async_dma(data->host_cookie)) {
			dir = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
			(void)dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len,
				dir);
		}
		N_MSG(OPS, "CMD<%d> ARG<0x%x> data<%s %s> blksz<%d> block<%d> error<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			(data->host_cookie ? "dma" : "pio"),
			(read ? "read " : "write"), data->blksz,
			data->blocks, data->error);
	}
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	else if (data && check_mmc_cmd4647(cmd->opcode)) {
		read = data->flags & MMC_DATA_READ ? 1 : 0;
		dir = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
		(void)dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
	}
#endif
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
	int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;
	
	int dir = DMA_FROM_DEVICE;

	data = mrq->data;
	if (data && (msdc_use_async_dma(data->host_cookie))) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		if (!mmc->card->ext_csd.cmdq_mode_en)
#endif
			host->xfer_size = data->blocks * data->blksz;
		dir = data->flags & MMC_DATA_READ ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE;
		dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
		data->host_cookie = 0;
		N_MSG(OPS, "CMD<%d> ARG<0x%x> blksz<%d> block<%d> error<%d>",
			mrq->cmd->opcode, mrq->cmd->arg, data->blksz,
			data->blocks, data->error);
	}
	data->host_cookie = 0;

}

static int msdc_do_request_async(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	void __iomem *base = host->base;

#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#endif

#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;
#endif
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	u32 task_id;
#endif

	MVG_EMMC_DECLARE_INT32(delay_ns);
	MVG_EMMC_DECLARE_INT32(delay_us);
	MVG_EMMC_DECLARE_INT32(delay_ms);

	BUG_ON(mmc == NULL || mrq == NULL);

	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;
		if (mrq->done)
			mrq->done(mrq); 
		return 0;
	}
	msdc_ungate_clock(host);
	
	host->async_tuning_in_progress = false;
	

	spin_lock(&host->lock);

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	host->mrq = mrq;

#ifdef MTK_MSDC_USE_CACHE
	if (msdc_do_request_prepare(host, mrq, cmd, data, &l_force_prg,
		NULL, PREPARE_ASYNC))
		goto done;
#else
	if (msdc_do_request_prepare(host, mrq, cmd, data, NULL,
		NULL, PREPARE_ASYNC))
		goto done;
#endif

#ifdef MTK_MSDC_USE_CMD23
	
	if (mrq->sbc) {
		host->autocmd &= ~MSDC_AUTOCMD12;

		if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CACHE
			if (l_force_prg && !((mrq->sbc->arg >> 31) & 0x1))
				mrq->sbc->arg |= (1 << 24);
#endif
		}

		if (0 == (host->autocmd & MSDC_AUTOCMD23)) {
			if (msdc_command_start(host, mrq->sbc, 0,
				CMD_TIMEOUT) != 0)
				goto done;

			
			if (msdc_command_resp_polling(host, mrq->sbc, 0,
				CMD_TIMEOUT) != 0) {
				goto stop;
			}
		}
	} else {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD12;
			if (0 != (host->autocmd & MSDC_AUTOCMD23)) {
				host->autocmd &= ~MSDC_AUTOCMD23;
				l_card_no_cmd23 = 1;
			}
		}
	}

#else
	
	if (host->hw->host_function != MSDC_SDIO)
		host->autocmd |= MSDC_AUTOCMD12;
#endif 

	msdc_dma_on();          
	

	if (msdc_command_start(host, cmd, 0, CMD_TIMEOUT) != 0)
		goto done;

	
	if (msdc_command_resp_polling(host, cmd, 0, CMD_TIMEOUT) != 0)
		goto stop;

	msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);

	msdc_dma_start(host);
	

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (check_mmc_cmd4647(cmd->opcode)) {
		task_id = (cmd->arg >> 16) & 0x1f;
		MVG_EMMC_WRITE_MATCH(host,
			(u64)mmc->areq_que[task_id]->mrq_que->cmd->arg,
			delay_ms, delay_us, delay_ns,
			cmd->opcode, host->xfer_size);
	} else
#endif
	MVG_EMMC_WRITE_MATCH(host, (u64)cmd->arg, delay_ms, delay_us, delay_ns,
		cmd->opcode, host->xfer_size);

	spin_unlock(&host->lock);

#ifdef MTK_MSDC_USE_CMD23
	if (1 == l_card_no_cmd23) {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD23;
			host->autocmd &= ~MSDC_AUTOCMD12;
			l_card_no_cmd23 = 0;
		}
	}
#endif

#ifdef MTK_MSDC_USE_CACHE
	msdc_update_cache_flush_status(host, mrq, data, 1);
#endif

	return 0;


stop:
#ifndef MTK_MSDC_USE_CMD23
	
	if (msdc_if_send_stop(host, cmd, data))
		goto done;
#else

	if (host->hw->host_function == MSDC_EMMC) {
		
	} else {
		if (msdc_if_send_stop(host, cmd, data))
			goto done;
	}
#endif

done:
#ifdef MTK_MSDC_USE_CMD23
	if (1 == l_card_no_cmd23) {
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD23;
			host->autocmd &= ~MSDC_AUTOCMD12;
			l_card_no_cmd23 = 0;
		}
	}
#endif

	msdc_dma_clear(host);

	msdc_log_cmd(host, cmd, data);

#ifdef MTK_MSDC_USE_CMD23
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-EILSEQ))
		host->error |= REQ_CMD_EIO;
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning_api(__FILE__, __LINE__,
			DB_OPT_NE_JBT_TRACES|DB_OPT_DISPLAY_HANG_DUMP,
			"\n@eMMC FATAL ERROR@\n", "eMMC fatal error ");
#endif
		host->error |= REQ_CMD_TMO;
	}
#endif

	msdc_if_set_err(host, mrq, cmd);

#ifdef MTK_MSDC_USE_CACHE
	msdc_update_cache_flush_status(host, mrq, data, 1);
#endif

	if (!host->async_tuning_in_progress
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	 && !host->mmc->card->ext_csd.cmdq_mode_en
#endif
	) {
		if (data && data->error) {
			#ifndef MSDC_WQ_ERROR_TUNE
			if (data->flags & MMC_DATA_WRITE)
				host->err_mrq_dir |= MMC_DATA_WRITE;
			else if (data->flags & MMC_DATA_READ)
				host->err_mrq_dir |= MMC_DATA_READ;
			#endif
			host->async_tuning_done = false;
		}
		if (cmd && (cmd->error == (unsigned int)-EILSEQ))
			host->async_tuning_done = false;
		#ifndef MSDC_WQ_ERROR_TUNE
		if (!host->async_tuning_done &&
		    (host->hw->host_function == MSDC_SD)) {
			host->mmc->bus_dead = 1;
		}
		#endif
	}

	if (mrq->done)
		mrq->done(mrq);

	msdc_gate_clock(host, 1);
	spin_unlock(&host->lock);
	return host->error;
}

#ifdef TUNE_FLOW_TEST
static void msdc_reset_para(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 dsmpl, rsmpl, clkmode;
	int hs400 = 0;

	

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, dsmpl);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, rsmpl);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;

	if (dsmpl == 0) {
		msdc_set_smpl(host, clkmode, 1, TYPE_READ_DATA_EDGE, NULL);
		ERR_MSG("set dspl<0>");
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, 0);
	}

	if (rsmpl == 0) {
		msdc_set_smpl(host, clkmode, 1, TYPE_CMD_RESP_EDGE, NULL);
		ERR_MSG("set rspl<0>");
		MSDC_WRITE32(MSDC_DAT_RDDLY0, 0);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY, 0);
	}
}
#endif

static void msdc_dump_trans_error(struct msdc_host   *host,
	struct mmc_command *cmd,
	struct mmc_data    *data,
	struct mmc_command *stop,
	struct mmc_command *sbc)
{
	if ((cmd->opcode == 52) && (cmd->arg == 0xc00))
		return;
	if ((cmd->opcode == 52) && (cmd->arg == 0x80000c08))
		return;

	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		
		if ((host->hw->host_function == MSDC_SD) &&
		    (cmd->opcode == 5))
			return;
	} else {
		if (cmd->opcode == 8)
			return;
	}

	ERR_MSG("XXX CMD<%d><0x%x> Error<%d> Resp<0x%x>", cmd->opcode, cmd->arg,
		cmd->error, cmd->resp[0]);

	if (data) {
		ERR_MSG("XXX DAT block<%d> Error<%d>", data->blocks,
			data->error);
	}
	if (stop) {
		ERR_MSG("XXX STOP<%d><0x%x> Error<%d> Resp<0x%x>",
			stop->opcode, stop->arg, stop->error, stop->resp[0]);
	}

	if (sbc) {
		ERR_MSG("XXX SBC<%d><0x%x> Error<%d> Resp<0x%x>",
			sbc->opcode, sbc->arg, sbc->error, sbc->resp[0]);
	}

	if ((host->hw->host_function == MSDC_SD)
	 && (host->sclk > 100000000)
	 && (data)
	 && (data->error != (unsigned int)-ETIMEDOUT)) {
		if ((data->flags & MMC_DATA_WRITE) &&
		    (host->write_timeout_uhs104))
			host->write_timeout_uhs104 = 0;
		if ((data->flags & MMC_DATA_READ) &&
		    (host->read_timeout_uhs104))
			host->read_timeout_uhs104 = 0;
	}

	if ((host->hw->host_function == MSDC_EMMC) &&
	    (data) &&
	    (data->error != (unsigned int)-ETIMEDOUT)) {
		if ((data->flags & MMC_DATA_WRITE) &&
		    (host->write_timeout_emmc))
			host->write_timeout_emmc = 0;
		if ((data->flags & MMC_DATA_READ) &&
		    (host->read_timeout_emmc))
			host->read_timeout_emmc = 0;
	}
#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host) &&
	    (host->sdio_error != -EILSEQ) &&
	    (cmd->opcode == 53) &&
	    (msdc_sg_len(data->sg, host->dma_xfer) > 4)) {
		host->sdio_error = -EILSEQ;
		ERR_MSG("XXX SDIO Error ByPass");
	}
#endif
}

static void msdc_do_request_with_retry(struct msdc_host *host,
	struct mmc_request *mrq,
	struct mmc_command *cmd,
	struct mmc_data *data,
	struct mmc_command *stop,
	struct mmc_command *sbc,
	int async)
{
	struct mmc_host *mmc = host->mmc;

	do {
		if (!async) {
			if (!msdc_do_request(host->mmc, mrq))
				break;
		}
		
		if (cmd->opcode != 19)
			msdc_dump_trans_error(host, cmd, data, stop, sbc);

		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			
			return;
		}

		if (host->legacy_tuning_in_progress)
			return;

		#ifdef MSDC_AUTOK_ON_ERROR 
		if (mmc->ios.timing != MMC_TIMING_LEGACY
		 && mmc->ios.timing != MMC_TIMING_SD_HS
		 && mmc->ios.timing != MMC_TIMING_UHS_DDR50) {
			if (!host->legacy_tuning_in_progress) {
				if ((cmd->error == (unsigned int)-EILSEQ)
				 || (sbc && (sbc->error == (unsigned int)-EILSEQ))
				 || (stop && (stop->error == (unsigned int)-EILSEQ))
				 || (data && data->error)) {
					host->legacy_tuning_done = false;
				}

				return;
			}
		}
		#endif

#ifdef MTK_MSDC_USE_CMD23
		if ((sbc != NULL) &&
		    (sbc->error == (unsigned int)-ETIMEDOUT)) {
			if (check_mmc_cmd1718(cmd->opcode)) {
				
				pr_err("===[%s:%d]==cmd23 timeout==\n",
					__func__, __LINE__);
				return;
			}
		}
#endif

		if (msdc_crc_tune(host, cmd, data, stop, sbc))
			return;

		
		if (!async) {
			if (cmd->error == (unsigned int)-ETIMEDOUT &&
			    !check_mmc_cmd2425(cmd->opcode) &&
			    !check_mmc_cmd1718(cmd->opcode))
				return;
		}

		if (cmd->error == (unsigned int)-ENOMEDIUM)
			return;

		if (msdc_data_timeout_tune(host, data))
			return;

		
		cmd->error = 0;
		if (data)
			data->error = 0;
		if (stop)
			stop->error = 0;

#ifdef MTK_MSDC_USE_CMD23
		if (sbc)
			sbc->error = 0;
#endif

		
		if (!async && host->app_cmd) {
			while (msdc_app_cmd(host->mmc, host)) {
				if (msdc_tune_cmdrsp(host)) {
					ERR_MSG("failed to updata cmd para for app");
					return;
				}
			}
		}

		if (async)
			host->sw_timeout = 0;

		if (!is_card_present(host))
			return;

		if (async) {
			if  (!msdc_tune_rw_request(host->mmc, mrq))
				break;
		}
	} while (1);

	if (async) {
		if ((host->rwcmd_time_tune)
		 && (check_mmc_cmd1718(cmd->opcode) ||
		     check_mmc_cmd2425(cmd->opcode))) {
			host->rwcmd_time_tune = 0;
			ERR_MSG("RW cmd recover");
			msdc_dump_trans_error(host, cmd, data, stop, sbc);
		}
	}
	if ((host->read_time_tune)
	 && check_mmc_cmd1718(cmd->opcode)) {
		host->read_time_tune = 0;
		ERR_MSG("Read recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}
	if ((host->write_time_tune)
	 && check_mmc_cmd2425(cmd->opcode)) {
		host->write_time_tune = 0;
		ERR_MSG("Write recover");
		msdc_dump_trans_error(host, cmd, data, stop, sbc);
	}

	if (async)
		host->power_cycle_enable = 1;

	host->sw_timeout = 0;

}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static int msdc_do_cmdq_request_with_retry(struct msdc_host *host,
	struct mmc_request *mrq)
{
	struct mmc_host *mmc;
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
	int ret = 0, retry;

	mmc = host->mmc;
	cmd = mrq->cmd;
	data = mrq->cmd->data;
	if (data)
		stop = data->stop;

	retry = 5;
	while (msdc_do_request_cq(mmc, mrq)) {
		msdc_dump_trans_error(host, cmd, data, stop, mrq->sbc);
		if ((cmd->error == (unsigned int)-EILSEQ) ||
			(cmd->error == (unsigned int)-ETIMEDOUT) ||
			(mrq->sbc->error == (unsigned int)-EILSEQ) ||
			(mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
			ret = tune_cmdq_cmdrsp(mmc, mrq, &retry);
			if (ret)
				return ret;
		} else {
			ERR_MSG("CMD44 and CMD45 error - error %d %d",
				mrq->sbc->error, cmd->error);
			break;
		}
	}

	return ret;
}
#endif

static void msdc_ops_request_legacy(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
	struct mmc_command *sbc = NULL;
	
	u32 old_H32 = 0, old_L32 = 0, new_H32 = 0, new_L32 = 0;
	u32 ticks = 0, opcode = 0, sizes = 0, bRx = 0;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int ret;
#endif

	msdc_reset_crc_tune_counter(host, all_counter);
#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (host->mrq) {
		ERR_MSG("XXX host->mrq<0x%p> cmd<%d>arg<0x%x>", host->mrq,
			host->mrq->cmd->opcode, host->mrq->cmd->arg);
		BUG();
	}
#endif

	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;

		if (mrq->done)
			mrq->done(mrq); 

		return;
	}

	
	spin_lock(&host->lock);
	host->power_cycle_enable = 1;

	cmd = mrq->cmd;
	data = mrq->cmd->data;
	if (data)
		stop = data->stop;

#ifdef MTK_MSDC_USE_CMD23
	if (data)
		sbc = mrq->sbc;
#endif

	msdc_ungate_clock(host);  

	if (sdio_pro_enable) {
		
		if (mrq->cmd->opcode == 52 || mrq->cmd->opcode == 53)
			; 
	}

#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	host->mrq = mrq;
#endif

#ifndef MSDC_WQ_ERROR_TUNE
	
	if (msdc_do_request(host->mmc, mrq)) {
		if (!host->legacy_tuning_in_progress)
			msdc_dump_trans_error(host, cmd, data, stop, sbc);
		if (data && data->error) {
			if (data->flags & MMC_DATA_WRITE)
				host->err_mrq_dir |= MMC_DATA_WRITE;
			else if (data->flags & MMC_DATA_READ)
				host->err_mrq_dir |= MMC_DATA_READ;
			host->legacy_tuning_done = false;
		}
		if (cmd && (cmd->error == (unsigned int)-EILSEQ))
			host->legacy_tuning_done = false;
		if (!host->legacy_tuning_done &&
		    (host->hw->host_function == MSDC_SD)) {
			host->mmc->bus_dead = 1;
		}
	}
#else

	#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (check_mmc_cmd44(mrq->sbc)) {
		ret = msdc_do_cmdq_request_with_retry(host, mrq);
	} else {
		if (mmc->card && mmc->card->ext_csd.cmdq_mode_en
			&& atomic_read(&mmc->areq_cnt)
			&& !check_mmc_cmd01213(cmd->opcode)
			&& !check_mmc_cmd48(cmd->opcode)) {
			ERR_MSG("[%s][WARNING] CMDQ on, sending CMD%d\n",
				__func__, cmd->opcode);
		}
		if (!check_mmc_cmd13_sqs(mrq->cmd))
			host->mrq = mrq;
	#endif
		msdc_do_request_with_retry(host, mrq, cmd, data, stop, sbc, 0);

	#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	}
	#endif
	msdc_reset_crc_tune_counter(host, all_counter);
#endif

#ifdef MTK_MSDC_USE_CACHE
	msdc_check_cache_flush_error(host, cmd);
#endif

#ifdef TUNE_FLOW_TEST
	if (!is_card_sdio(host))
		msdc_reset_para(host);
#endif

	
	if (mrq->cmd->opcode == MMC_APP_CMD) {
		host->app_cmd = 1;
		host->app_cmd_arg = mrq->cmd->arg;      
	} else {
		host->app_cmd = 0;
		
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (!(check_mmc_cmd13_sqs(mrq->cmd)
		|| check_mmc_cmd44(mrq->sbc))) {
		host->mrq = NULL;
	}
#else
	host->mrq = NULL;
#endif

	
	if (sdio_pro_enable) {
		if (mrq->cmd->opcode == 52 || mrq->cmd->opcode == 53) {
			
			ticks = msdc_time_calc(old_L32, old_H32,
				new_L32, new_H32);

			opcode = mrq->cmd->opcode;
			if (mrq->cmd->data) {
				sizes = mrq->cmd->data->blocks *
					mrq->cmd->data->blksz;
				bRx = mrq->cmd->data->flags & MMC_DATA_READ ?
					1 : 0;
			} else {
				bRx = mrq->cmd->arg & 0x80000000 ? 1 : 0;
			}

			if (!mrq->cmd->error)
				msdc_performance(opcode, sizes, bRx, ticks);
		}
	}

	msdc_gate_clock(host, 1);       
	spin_unlock(&host->lock);

	mmc_request_done(mmc, mrq);

	return;
}

#ifdef MSDC_WQ_ERROR_TUNE
static void msdc_tune_async_request(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
	struct mmc_command *sbc = NULL;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc->card->ext_csd.cmdq_mode_en == 1
		&& (atomic_read(&mmc->cq_tuning_now) == 1)) {
		tune_cmdq_data(mmc, mrq);
		return;
	}
#endif

	
	if (host->mrq) {
#ifdef CONFIG_MTK_AEE_FEATURE
		aee_kernel_warning("MSDC",
				   "MSDC request not clear.\n host attached<0x%p> current<0x%p>.\n",
				   host->mrq, mrq);
#else
		WARN_ON(host->mrq);
#endif
		ERR_MSG("XXX host->mrq<0x%p> cmd<%d>arg<0x%x>", host->mrq,
			host->mrq->cmd->opcode, host->mrq->cmd->arg);
		if (host->mrq->data) {
			ERR_MSG("XXX request data size<%d>",
				host->mrq->data->blocks *
				host->mrq->data->blksz);
			ERR_MSG("XXX request attach to host force data timeout and retry");
			host->mrq->data->error = (unsigned int)-ETIMEDOUT;
		} else {
			ERR_MSG("XXX request attach to host force cmd timeout and retry");
			host->mrq->cmd->error = (unsigned int)-ETIMEDOUT;
		}
		ERR_MSG("XXX current request <0x%p> cmd<%d>arg<0x%x>",
			mrq, mrq->cmd->opcode, mrq->cmd->arg);
		if (mrq->data)
			ERR_MSG("XXX current request data size<%d>",
				mrq->data->blocks * mrq->data->blksz);
	}

	if (!is_card_present(host) || host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;
		
		goto done;
	}

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	if (data)
		stop = data->stop;
#ifdef MTK_MSDC_USE_CMD23
	if (data)
		sbc = mrq->sbc;
#endif

	
	spin_lock(&host->lock);

	msdc_ungate_clock(host);        
	
	host->async_tuning_in_progress = true;
	
	host->mrq = mrq;

	msdc_do_request_with_retry(host, mrq, cmd, data, stop, sbc, 1);

	if (host->sclk <= 50000000
	 && (host->timing != MMC_TIMING_MMC_DDR52)
	 && (host->timing != MMC_TIMING_UHS_DDR50))
		host->sd_30_busy = 0;
	msdc_reset_crc_tune_counter(host, all_counter);
	host->mrq = NULL;
	msdc_gate_clock(host, 1);       
	
	host->async_tuning_in_progress = false;
	
	spin_unlock(&host->lock);

done:
	host->mrq_tune = NULL;
	mmc_request_done(mmc, mrq);
}

static void msdc_async_tune(struct work_struct *work)
{
	struct msdc_host *host = NULL;
	struct mmc_host *mmc = NULL;

	host = container_of(work, struct msdc_host, work_tune);
	BUG_ON(!host || !host->mmc);
	mmc = host->mmc;

	msdc_tune_async_request(mmc, host->mrq_tune);
}
#endif

int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	ktime_t start, diff;

	pr_err("%s: %s: Tuning Start, timing:%u, op:%u\n"
		, mmc_hostname(host->mmc), __func__, mmc->ios.timing, opcode);
	start = ktime_get();
	host->legacy_tuning_in_progress = true;
	

	msdc_init_tune_path(host, mmc->ios.timing);

	msdc_ungate_clock(host);

	#if !defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	pmic_force_vcore_pwm(true);
	#endif

	if (host->hw->host_function == MSDC_SD) {
		if (mmc->ios.timing == MMC_TIMING_UHS_SDR104 ||
		    mmc->ios.timing == MMC_TIMING_UHS_SDR50) {
			if (host->is_autok_done == 0) {
				pr_err("[AUTOK]SD Autok\n");
				autok_execute_tuning(host, sd_autok_res[AUTOK_VCORE_HIGH]);
				host->is_autok_done = 1;
			} else {
				autok_init_sdr104(host);
				autok_tuning_parameter_init(host, sd_autok_res[AUTOK_VCORE_HIGH]);
			}
		}
	} else if (host->hw->host_function == MSDC_EMMC) {
		#ifdef MSDC_HQA
		msdc_HQA_set_vcore(host);
		#endif

		if (mmc->ios.timing == MMC_TIMING_MMC_HS200) {
			if (opcode == MMC_SEND_STATUS) {
				pr_err("[AUTOK]eMMC HS200 Tune CMD only\n");
				hs200_execute_tuning_cmd(host, NULL);
			} else {
				pr_err("[AUTOK]eMMC HS200 Tune\n");
				hs200_execute_tuning(host, NULL);
			}
		} else if (mmc->ios.timing == MMC_TIMING_MMC_HS400) {
			if (opcode == MMC_SEND_STATUS) {
				pr_err("[AUTOK]eMMC HS400 Tune CMD only\n");
				hs400_execute_tuning_cmd(host, NULL);
			} else {
				pr_err("[AUTOK]eMMC HS400 Tune\n");
				hs400_execute_tuning(host, NULL);
			}
		}
	} else if (host->hw->host_function == MSDC_SDIO) {
		
		if (sdio_autok_res_apply(host, AUTOK_VCORE_HIGH) != 0) {
			pr_err("sdio autok result not exist!, excute tuning\n");
			if (host->is_autok_done == 0) {
				pr_err("[AUTOK]SDIO SDR104 Tune\n");
				
				sdio_autok_wait_dvfs_ready();

				
				if (vcorefs_request_dvfs_opp(KIR_AUTOK_SDIO, OPPI_PERF) != 0)
					pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");
				autok_execute_tuning(host, sdio_autok_res[AUTOK_VCORE_HIGH]);

			#ifdef SDIO_FIX_VCORE_CONDITIONAL
				
				if (vcorefs_request_dvfs_opp(KIR_AUTOK_SDIO, OPPI_LOW_PWR) != 0)
					pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");
				autok_execute_tuning(host, sdio_autok_res[AUTOK_VCORE_LOW]);

				if (autok_res_check(sdio_autok_res[AUTOK_VCORE_HIGH], sdio_autok_res[AUTOK_VCORE_LOW])
					== 0) {
					pr_err("[AUTOK] No need change para when dvfs\n");
				} else {
					pr_err("[AUTOK] Need change para when dvfs or lock dvfs\n");
					sdio_lock_dvfs = 1;
				}
			#else
				
				if (sdio_version(host) == 2) {
					sdio_set_vcorefs_sram(AUTOK_VCORE_HIGH, 0, host);
					
					if (vcorefs_request_dvfs_opp(KIR_AUTOK_SDIO, OPPI_LOW_PWR) != 0)
						pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");
					autok_execute_tuning(host, sdio_autok_res[AUTOK_VCORE_LOW]);
					sdio_set_vcorefs_sram(AUTOK_VCORE_LOW, 1, host);
				}
			#endif

				
				if (vcorefs_request_dvfs_opp(KIR_AUTOK_SDIO, OPPI_UNREQ) != 0)
					pr_err("vcorefs_request_dvfs_opp@OPPI_UNREQ fail!\n");

				host->is_autok_done = 1;
				complete(&host->autok_done);
			} else {
				autok_init_sdr104(host);
				autok_tuning_parameter_init(host, sdio_autok_res[AUTOK_VCORE_HIGH]);
			}
		} else {
			autok_init_sdr104(host);

		#ifdef SDIO_FIX_VCORE_CONDITIONAL
			if (autok_res_check(sdio_autok_res[AUTOK_VCORE_HIGH], sdio_autok_res[AUTOK_VCORE_LOW]) == 0) {
				pr_err("[AUTOK] No need change para when dvfs\n");
			} else {
				pr_err("[AUTOK] Need change para when dvfs or lock dvfs\n");
				sdio_lock_dvfs = 1;
			}
		#else
			
			if (sdio_version(host) == 2) {
				sdio_set_vcorefs_sram(AUTOK_VCORE_HIGH, 0, host);
				if (sdio_autok_res_apply(host, AUTOK_VCORE_LOW) == 0)
					sdio_set_vcorefs_sram(AUTOK_VCORE_LOW, 1, host);
			}
		#endif

			if (host->is_autok_done == 0) {
				host->is_autok_done = 1;
				complete(&host->autok_done);
			}
		}
	}
	host->legacy_tuning_in_progress = false;
	host->legacy_tuning_done = true;
	host->first_tune_done = 1;

	#if !defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	pmic_force_vcore_pwm(false);
	#endif

	msdc_gate_clock(host, 1);
	diff = ktime_sub(ktime_get(), start);
	pr_err("%s: %s: Tuning End, cost time: %lld ms\n"
		, mmc_hostname(host->mmc), __func__, ktime_to_ms(diff));

	return 0;
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_data *data;
	int host_cookie = 0;
	struct msdc_host *host = mmc_priv(mmc);
	BUG_ON(mmc == NULL || mrq == NULL);

	if ((host->hw->host_function == MSDC_SDIO) &&
	    !(host->trans_lock.active))
		__pm_stay_awake(&host->trans_lock);

	
	if ((host->id == 2) && (sdio_lock_dvfs == 1))
		sdio_set_vcore_performance(host, 1);

	data = mrq->data;
	if (data)
		host_cookie = data->host_cookie;

	
	if (msdc_use_async_dma(host_cookie)) {
		#if defined(MSDC_AUTOK_ON_ERROR)
		if (!host->async_tuning_in_progress &&
		    !host->async_tuning_done) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			if (host->mmc->card->ext_csd.cmdq_mode_en)
				ERR_MSG("[%s][ERROR] CMDQ on, not support async tuning",
					__func__);
#endif
			if (mmc->ios.timing == MMC_TIMING_UHS_SDR104 ||
			    mmc->ios.timing == MMC_TIMING_UHS_SDR50) {
				msdc_execute_tuning(mmc,
					MMC_SEND_TUNING_BLOCK);
			} else if (mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
				   mmc->ios.timing == MMC_TIMING_MMC_HS400) {
				msdc_execute_tuning(mmc,
					MMC_SEND_TUNING_BLOCK_HS200);
			#ifndef MSDC_WQ_ERROR_TUNE
			} else {
				if (msdc_tuning_wo_autok(host))
					pr_err("msdc%d tuning smpl failed\n",
						host->id);
				if (host->mmc->bus_dead)
					host->mmc->bus_dead = 0;
			#endif
			}
			host->async_tuning_in_progress = false;
			host->async_tuning_done = true;
		}
		#endif
		msdc_do_request_async(mmc, mrq);
	} else {
		if (!host->legacy_tuning_in_progress
		 && !host->legacy_tuning_done) {
			if (mmc->ios.timing == MMC_TIMING_UHS_SDR104 ||
			    mmc->ios.timing == MMC_TIMING_UHS_SDR50) {
				msdc_execute_tuning(mmc,
					MMC_SEND_TUNING_BLOCK);
			} else if (mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
				   mmc->ios.timing == MMC_TIMING_MMC_HS400) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
				if (host->error == REQ_CMD_EIO) {
					msdc_execute_tuning(mmc, MMC_SEND_STATUS);
					host->error &= ~REQ_CMD_EIO;
				} else
#endif
				{
					msdc_execute_tuning(mmc,
						MMC_SEND_TUNING_BLOCK_HS200);
				}
			#ifndef MSDC_WQ_ERROR_TUNE
			} else {
				if (msdc_tuning_wo_autok(host))
					pr_err("msdc%d tuning smpl failed\n",
						host->id);
				if (host->mmc->bus_dead)
					host->mmc->bus_dead = 0;
			#endif
			}
		}
		msdc_ops_request_legacy(mmc, mrq);
	}

	
	if ((host->id == 2) && (sdio_lock_dvfs == 1))
		sdio_set_vcore_performance(host, 0);

	if ((host->hw->host_function == MSDC_SDIO) && (host->trans_lock.active))
		__pm_relax(&host->trans_lock);

	return;
}


static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	void __iomem *base = host->base;
	u32 val = MSDC_READ32(SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	MSDC_WRITE32(SDC_CFG, val);
}

static void msdc_init_hw(struct msdc_host *host)
{
	void __iomem *base = host->base;
	struct msdc_hw *hw = host->hw;

	
	

	msdc_ungate_clock(host);

	
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

	
	msdc_reset_hw(host->id);

	
	MSDC_CLR_BIT32(MSDC_PS, MSDC_PS_CDEN);

	
	MSDC_CLR_BIT32(MSDC_INTEN, MSDC_READ32(MSDC_INTEN));
	MSDC_WRITE32(MSDC_INT, MSDC_READ32(MSDC_INT));

	
	msdc_init_tune_setting(host);

	
	MSDC_SET_BIT32(SDC_CFG, SDC_CFG_SDIO);

	
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		ghost = host;
		
		MSDC_SET_BIT32(SDC_CFG, SDC_CFG_SDIOIDE);
	} else {
		MSDC_CLR_BIT32(SDC_CFG, SDC_CFG_SDIOIDE);
	}

	msdc_set_smt(host, 1);
	msdc_set_driving(host, hw, 0);
	
	

	
	MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_DETWR_CRCTMO, 1);

	
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, DEFAULT_DTOC);

	msdc_set_buswidth(host, MMC_BUS_WIDTH_1);

	msdc_gate_clock(host, 1);

	N_MSG(FUC, "init hardware done!");
}

static void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;

	spin_lock(&host->lock);

	msdc_ungate_clock(host);

	if (host->power_mode != ios->power_mode) {
		switch (ios->power_mode) {
		case MMC_POWER_OFF:
		case MMC_POWER_UP:
			spin_unlock(&host->lock);
			msdc_init_hw(host);
			msdc_set_power_mode(host, ios->power_mode);
			spin_lock(&host->lock);
			break;
		case MMC_POWER_ON:
		default:
			break;
		}
		host->power_mode = ios->power_mode;
	}

	if (host->bus_width != ios->bus_width) {
		msdc_set_buswidth(host, ios->bus_width);
		host->bus_width = ios->bus_width;
	}

	if (msdc_clock_src[host->id] != hw->clk_src) {
		hw->clk_src = msdc_clock_src[host->id];
		msdc_select_clksrc(host, hw->clk_src);
	}

	if (host->mclk != ios->clock || host->timing != ios->timing) {
		if (ios->clock > 100000000)
			msdc_set_driving(host, hw, 1);

		
		

		msdc_ios_tune_setting(mmc, ios);
		host->timing = ios->timing;
	}

	msdc_gate_clock(host, 1);
	spin_unlock(&host->lock);
}

static int msdc_ops_get_ro(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	unsigned long flags;
	int ro = 0;

	spin_lock_irqsave(&host->lock, flags);
	msdc_ungate_clock(host);
	if (host->hw->flags & MSDC_WP_PIN_EN)
		ro = (MSDC_READ32(MSDC_PS) >> 31);
	msdc_gate_clock(host, 1);
	spin_unlock_irqrestore(&host->lock, flags);
	return ro;
}

static int msdc_ops_get_cd(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	
	int level = 0;

	

	
	if (is_card_sdio(host) && !(host->hw->flags & MSDC_SDIO_IRQ)) {
		host->card_inserted =
			(host->pm_state.event == PM_EVENT_USER_RESUME) ? 1 : 0;
		goto end;
	}

	
	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		host->card_inserted = 1;
		goto end;
	} else {
		level = __gpio_get_value(cd_gpio);
		
		host->card_inserted = (host->hw->cd_level == level) ? 1 : 0;
	}

	if (host->block_bad_card)
		host->card_inserted = 0;
 end:
	
	sd_register_zone[host->id] = 1;
	INIT_MSG("Card insert<%d> Block bad card<%d>",
		host->card_inserted, host->block_bad_card);
	
	return host->card_inserted;
}

static void msdc_ops_card_event(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->block_bad_card = 0;
	host->is_autok_done = 0;
	host->async_tuning_done = true;
	host->legacy_tuning_done = true;
	msdc_reset_pwr_cycle_counter(host);
	msdc_reset_crc_tune_counter(host, all_counter);
	msdc_reset_tmo_tune_counter(host, all_counter);

	msdc_ops_get_cd(mmc);
	
	sd_register_zone[host->id] = 0;
}

static void msdc_ops_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;
	unsigned long flags;

	if (hw->flags & MSDC_EXT_SDIO_IRQ) {    
		if (enable)
			hw->enable_sdio_eirq(); 
		else
			hw->disable_sdio_eirq(); 
	} else if (hw->flags & MSDC_SDIO_IRQ) {

#if (MSDC_DATA1_INT == 1)
		spin_lock_irqsave(&host->sdio_irq_lock, flags);

		if (enable) {
			while (1) {
				MSDC_SET_BIT32(MSDC_INTEN, MSDC_INT_SDIOIRQ);
				pr_debug("@#0x%08x @e >%d<\n",
					(MSDC_READ32(MSDC_INTEN)),
					host->mmc->sdio_irq_pending);
				if ((MSDC_READ32(MSDC_INTEN) & MSDC_INT_SDIOIRQ)
					== 0) {
					pr_debug("Should never ever get into this >%d<\n",
						host->mmc->sdio_irq_pending);
				} else {
					break;
				}
			}
		} else {
			MSDC_CLR_BIT32(MSDC_INTEN, MSDC_INT_SDIOIRQ);
			pr_debug("@#0x%08x @d\n", (MSDC_READ32(MSDC_INTEN)));
		}

		spin_unlock_irqrestore(&host->sdio_irq_lock, flags);
#endif
	}
}

static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	int err = 0;
	u32 timeout = 100;
	u32 retry = 10;
	u32 status;

	if (host->hw->host_function == MSDC_EMMC)
		return 0;

	if (ios->signal_voltage != MMC_SIGNAL_VOLTAGE_330) {
		
		
		err = (unsigned int)-EILSEQ;
		msdc_retry(sdc_is_busy(), retry, timeout, host->id);
		if (retry == 0) {
			err = (unsigned int)-ETIMEDOUT;
			goto out;
		}

		
		if ((MSDC_READ32(MSDC_PS) & ((1 << 24) | (0xF << 16))) == 0) {
			
			msdc_pin_config(host, MSDC_PIN_PULL_NONE);

			if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {

				if (host->power_switch)
					host->power_switch(host, 1);

			}
			
			mdelay(10);


			
			msdc_set_mclk(host, MMC_TIMING_LEGACY, 260000);

			
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
			mdelay(105);

			MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_BV18SDT);

			
			mdelay(1);
			

			while ((status =
				MSDC_READ32(MSDC_CFG)) & MSDC_CFG_BV18SDT)
				;

			if (status & MSDC_CFG_BV18PSS)
				err = 0;
		}
	}
 out:

	return err;
}

static int msdc_card_busy(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	u32 status = MSDC_READ32(MSDC_PS);

	
	if (((status >> 16) & 0xf) != 0xf)
		return 1;

	return 0;
}

static void msdc_check_write_timeout(struct work_struct *work)
{
	struct msdc_host *host =
		container_of(work, struct msdc_host, write_timeout.work);
	void __iomem *base = host->base;
	struct mmc_data  *data = host->data;
	struct mmc_request *mrq = host->mrq;
	struct mmc_host *mmc = host->mmc;

	u32 status = 0;
	u32 state = 0;
	u32 err = 0;
	unsigned long tmo;

	if (!data || !mrq || !mmc)
		return;

	pr_err("[%s]: XXX DMA Data Write Busy Timeout: %u ms, CMD<%d>",
		__func__, host->write_timeout_ms, mrq->cmd->opcode);

	if (msdc_use_async_dma(data->host_cookie) && (host->tune == 0)) {
		msdc_dump_info(host->id);

		msdc_dma_stop(host);
		msdc_dma_clear(host);
		msdc_reset_hw(host->id);

		tmo = jiffies + POLLING_BUSY;

		
		spin_lock(&host->lock);
		do {
			err = msdc_get_card_status(mmc, host, &status);
			if (err) {
				ERR_MSG("CMD13 ERR<%d>", err);
				break;
			}

			state = R1_CURRENT_STATE(status);
			ERR_MSG("check card state<%d>", state);
			if (state == R1_STATE_DATA || state == R1_STATE_RCV) {
				ERR_MSG("state<%d> need cmd12 to stop", state);
				msdc_send_stop(host);
			} else if (state == R1_STATE_PRG) {
				ERR_MSG("state<%d> card is busy", state);
				spin_unlock(&host->lock);
				msleep(100);
				spin_lock(&host->lock);
			}

			if (time_after(jiffies, tmo)) {
				if (mmc->removed_cnt < MMC_DETECT_RETRIES) {
					pr_err("%s: CMD%d timeout, remove card (retry=%d)\n",
						mmc_hostname(mmc), mrq->cmd->opcode, mmc->removed_cnt);
					mmc_blk_remove_card(mmc);
					break;
				}

				ERR_MSG("abort timeout and stuck in %d state, remove such bad card!",
					state);
				spin_unlock(&host->lock);
				msdc_set_bad_card_and_remove(host);
				spin_lock(&host->lock);
				break;
			}
		} while (state != R1_STATE_TRAN);
		spin_unlock(&host->lock);

		data->error = (unsigned int)-ETIMEDOUT;
		host->sw_timeout++;

		if (mrq->done)
			mrq->done(mrq);

		msdc_gate_clock(host, 1);
		host->error |= REQ_DAT_ERR;
	} else {
		
	}
}

static void msdc_underclocking(struct msdc_host *host)
{
	if (host->mmc->crc_count++ < HTC_MAX_CRCERR_COUNT) {
		pr_err("%s: %s: error count : %d (timing:%d)\n", mmc_hostname(host->mmc),
			__func__, host->mmc->crc_count, host->mmc->ios.timing);
		return;
	}
	switch (host->mmc->ios.timing) {
	case MMC_TIMING_UHS_SDR12:
		host->mmc->caps &= ~MMC_CAP_UHS_SDR12;
	case MMC_TIMING_UHS_SDR25:
		host->mmc->caps &= ~MMC_CAP_UHS_SDR25;
	case MMC_TIMING_UHS_SDR50:
		host->mmc->caps &= ~MMC_CAP_UHS_SDR50;
	case MMC_TIMING_UHS_DDR50:
		host->mmc->caps &= ~MMC_CAP_UHS_DDR50;
	case MMC_TIMING_UHS_SDR104:
		host->mmc->caps &= ~MMC_CAP_UHS_SDR104;
		break;
	default:
		pr_err("%s: %s: unknow timing : %d\n", mmc_hostname(host->mmc),
			__func__, host->mmc->ios.timing);
		break;
	}
	host->mmc->crc_count = 0;
	pr_err("%s: %s: disable clock : %d\n", mmc_hostname(host->mmc),
		__func__, host->mmc->ios.timing);
}

static struct mmc_host_ops mt_msdc_ops = {
	.post_req                      = msdc_post_req,
	.pre_req                       = msdc_pre_req,
	.request                       = msdc_ops_request,
	.set_ios                       = msdc_ops_set_ios,
	.get_ro                        = msdc_ops_get_ro,
	.get_cd                        = msdc_ops_get_cd,
	.card_event                    = msdc_ops_card_event,
	.enable_sdio_irq               = msdc_ops_enable_sdio_irq,
	.start_signal_voltage_switch   = msdc_ops_switch_volt,
	.execute_tuning                = msdc_execute_tuning,
	.hw_reset                      = msdc_card_reset,
	.card_busy                     = msdc_card_busy,
};

static void msdc_irq_data_complete(struct msdc_host *host,
	struct mmc_data *data, int error)
{
#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	void __iomem *base = host->base;
#endif
	struct mmc_request *mrq;
	#ifdef MSDC_WQ_ERROR_TUNE
	struct mmc_host *mmc = host->mmc;
	int done_to_mmc_core = 1;
	#endif

	if ((msdc_use_async_dma(data->host_cookie)) &&
	    (!host->async_tuning_in_progress)) {
		msdc_dma_stop(host);
		if (error) {
			msdc_clr_fifo(host->id);
#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
			msdc_clr_int();
#endif
		}
		mrq = host->mrq;
		msdc_dma_clear(host);
		#ifdef MSDC_WQ_ERROR_TUNE
		if (error) {
			if (((host->hw->host_function == MSDC_SD) &&
			     (mmc->ios.timing != MMC_TIMING_UHS_SDR104) &&
			     (mmc->ios.timing != MMC_TIMING_UHS_SDR50))
			 || ((host->hw->host_function == MSDC_EMMC) &&
			     (mmc->ios.timing != MMC_TIMING_MMC_HS200) &&
			     (mmc->ios.timing != MMC_TIMING_MMC_HS400))) {
				done_to_mmc_core = 0;
			}
		} else {
			host->prev_cmd_cause_dump = 0;
		}
		if (done_to_mmc_core) {
			if (mrq->done)
				mrq->done(mrq);
		}
		#else
		if (mrq->done)
			mrq->done(mrq);
		#endif

		msdc_gate_clock(host, 1);
		if (!error) {
			host->error &= ~REQ_DAT_ERR;
		} else {
			host->error |= REQ_DAT_ERR;
			#ifdef MSDC_WQ_ERROR_TUNE
			host->mrq_tune = mrq;
			if (!done_to_mmc_core) {
				if (!queue_work(wq_tune, &host->work_tune)) {
					pr_err("msdc%d queue work failed BUG_ON,[%s]L:%d\n",
						host->id, __func__, __LINE__);
					BUG();
				}
			}
			#endif
		}
	} else {
		complete(&host->xfer_done);
	}

}

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *)dev_id;
	struct mmc_data *data = host->data;
	struct mmc_command *cmd = host->cmd;
	struct mmc_command *stop = NULL;
	void __iomem *base = host->base;

	u32 cmdsts = MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO | MSDC_INT_CMDRDY |
		     MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO | MSDC_INT_ACMDRDY |
		     MSDC_INT_ACMD19_DONE;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	u32 cmdqsts = MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO | MSDC_INT_CMDRDY;
#endif
	u32 datsts = MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	u32 intsts, inten;

	if (host->hw->flags & MSDC_SDIO_IRQ)
		spin_lock(&host->sdio_irq_lock);

	if (host->core_clkon == 0) {
		msdc_clk_enable(host);
		host->core_clkon = 1;
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);
	}
	intsts = MSDC_READ32(MSDC_INT);

	latest_int_status[host->id] = intsts;
	inten = MSDC_READ32(MSDC_INTEN);
#if (MSDC_DATA1_INT == 1)
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		intsts &= inten;
	} else
#endif
	{
		inten &= intsts;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	intsts &= ~cmdqsts;
#endif

	MSDC_WRITE32(MSDC_INT, intsts); 

	
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		spin_unlock(&host->sdio_irq_lock);

		#if (MSDC_DATA1_INT == 1)
		if (intsts & MSDC_INT_SDIOIRQ)
			mmc_signal_sdio_irq(host->mmc);
		#endif
	}

	
	if (data == NULL)
		goto skip_data_interrupts;

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	msdc_error_tune_debug2(host, stop, &intsts);
#endif

	stop = data->stop;
#if (MSDC_DATA1_INT == 1)
	if ((host->hw->flags & MSDC_SDIO_IRQ) &&
	    (intsts & MSDC_INT_XFER_COMPL)) {
		goto done;
	} else
#endif
	{
		if (inten & MSDC_INT_XFER_COMPL)
			goto done;
	}

	if (intsts & datsts) {
		
		if (intsts & MSDC_INT_DATTMO)
			msdc_dump_info(host->id);

		if (host->dma_xfer)
			msdc_reset(host->id);
		else
			msdc_reset_hw(host->id);

		atomic_set(&host->abort, 1);    

		if (intsts & MSDC_INT_DATTMO) {
			data->error = (unsigned int)-ETIMEDOUT;
			ERR_MSG("XXX CMD<%d> Arg<0x%.8x> MSDC_INT_DATTMO",
				host->mrq->cmd->opcode, host->mrq->cmd->arg);
		} else if (intsts & MSDC_INT_DATCRCERR) {
			data->error = (unsigned int)-EILSEQ;
			ERR_MSG("XXX CMD<%d> Arg<0x%.8x> MSDC_INT_DATCRCERR, SDC_DCRC_STS<0x%x>",
				host->mrq->cmd->opcode, host->mrq->cmd->arg,
				MSDC_READ32(SDC_DCRC_STS));
			if (host->hw->host_function == MSDC_SD)
				msdc_underclocking(host);
		}

		goto tune;

	}
	if ((stop != NULL) &&
	    (host->autocmd & MSDC_AUTOCMD12) &&
	    (intsts & cmdsts)) {
		if (intsts & MSDC_INT_ACMDRDY) {
			u32 *arsp = &stop->resp[0];
			*arsp = MSDC_READ32(SDC_ACMD_RESP);
		} else if (intsts & MSDC_INT_ACMDCRCERR) {
			stop->error = (unsigned int)-EILSEQ;
			host->error |= REQ_STOP_EIO;
			if (host->dma_xfer)
				msdc_reset(host->id);
			else
				msdc_reset_hw(host->id);
		} else if (intsts & MSDC_INT_ACMDTMO) {
			stop->error = (unsigned int)-ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
			if (host->dma_xfer)
				msdc_reset(host->id);
			else
				msdc_reset_hw(host->id);
		}
		if ((intsts & MSDC_INT_ACMDCRCERR) ||
		    (intsts & MSDC_INT_ACMDTMO)) {
			goto tune;
		}
	}

skip_data_interrupts:

	
	if ((cmd == NULL) || !(intsts & cmdsts))
		goto skip_cmd_interrupts;

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	msdc_error_tune_debug3(host, cmd, &intsts);
#endif

#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (intsts & MSDC_INT_CMDRDY) {
		u32 *rsp = NULL;

		rsp = &cmd->resp[0];
		switch (host->cmd_rsp) {
		case RESP_NONE:
			break;
		case RESP_R2:
			*rsp++ = MSDC_READ32(SDC_RESP3);
			*rsp++ = MSDC_READ32(SDC_RESP2);
			*rsp++ = MSDC_READ32(SDC_RESP1);
			*rsp++ = MSDC_READ32(SDC_RESP0);
			break;
		default: 
			*rsp = MSDC_READ32(SDC_RESP0);
			break;
		}
	} else if (intsts & MSDC_INT_RSPCRCERR) {
		cmd->error = (unsigned int)-EILSEQ;
		ERR_MSG("XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>",
			cmd->opcode, cmd->arg);
		msdc_reset_hw(host->id);
		if (host->hw->host_function == MSDC_SD)
			msdc_underclocking(host);
	} else if (intsts & MSDC_INT_CMDTMO) {
		cmd->error = (unsigned int)-ETIMEDOUT;
		ERR_MSG("XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>",
			cmd->opcode, cmd->arg);
		msdc_reset_hw(host->id);
	}
	if (intsts & (MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO))
		complete(&host->cmd_done);
#endif

skip_cmd_interrupts:
	
	if (intsts & MSDC_INT_MMCIRQ) {
	}

	if (!host->async_tuning_in_progress
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		&& (!host->mmc->card ||
			!host->mmc->card->ext_csd.cmdq_mode_en)
#endif
	) {
		if (cmd && (cmd->error == (unsigned int)-EILSEQ))
			host->async_tuning_done = false;
	}

	if (host->dma_xfer)
		msdc_irq_data_complete(host, data, 1);
	latest_int_status[host->id] = 0;
	return IRQ_HANDLED;

done:   
	data->bytes_xfered = host->dma.xfersz;
	msdc_irq_data_complete(host, data, 0);
	return IRQ_HANDLED;

tune:   
	

	if (!host->mmc->card) {
		ERR_MSG("%s: card become null pointer, drop!\n", __func__);
		host->async_tuning_done = false;
	} else if (!host->async_tuning_in_progress
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		&& (!host->mmc->card ||
			!host->mmc->card->ext_csd.cmdq_mode_en)
#endif
	) {
		if ((data && data->error)
		 || (cmd && (cmd->error == (unsigned int)-EILSEQ))) {
			#ifndef MSDC_WQ_ERROR_TUNE
			if (data->flags & MMC_DATA_WRITE)
				host->err_mrq_dir |= MMC_DATA_WRITE;
			else if (data->flags & MMC_DATA_READ)
				host->err_mrq_dir |= MMC_DATA_READ;
			if (host->hw->host_function == MSDC_SD) {
				host->mmc->bus_dead = 1;
			}
			#endif
			host->async_tuning_done = false;
		}
	}

	if (host->dma_xfer)
		msdc_irq_data_complete(host, data, 1);

	if (data && !data->error)
		host->prev_cmd_cause_dump = 0;

	return IRQ_HANDLED;
}

static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct gpd_t *gpd = dma->gpd;
	struct bd_t *bd = dma->bd;
	struct bd_t *ptr, *prev;

	
	int bdlen = MAX_BD_PER_GPD;

	
	memset(gpd, 0, sizeof(struct gpd_t) * 2);
	gpd->next = (u32)dma->gpd_addr + sizeof(struct gpd_t);

	
	gpd->bdp = 1;           
	
	gpd->ptr = (u32)dma->bd_addr; 

	memset(bd, 0, sizeof(struct bd_t) * bdlen);
	ptr = bd + bdlen - 1;
	while (ptr != bd) {
		prev = ptr - 1;
		prev->next = (u32)dma->bd_addr
			+ sizeof(struct bd_t) * (ptr - bd);
		ptr = prev;
	}
}

#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
static void msdc_tasklet_flush_cache(unsigned long arg)
{
	struct msdc_host *host = (struct msdc_host *)arg;

	if (host->mmc->card) {
		mmc_claim_host(host->mmc);
		mmc_flush_cache(host->mmc->card);
		mmc_release_host(host->mmc);
	}
}
#endif

static void msdc_timer_pm(unsigned long data)
{
	struct msdc_host *host = (struct msdc_host *)data;
	unsigned long flags;

	spin_lock_irqsave(&host->clk_gate_lock, flags);
	if (host->clk_gate_count == 0) {
		msdc_clksrc_onoff(host, 0);
		N_MSG(CLK, "time out, dsiable clock, clk_gate_count=%d",
			host->clk_gate_count);
	}
#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
	if (check_mmc_cache_ctrl(mmc->card))
		tasklet_hi_schedule(&host->flush_cache_tasklet);
#endif

	spin_unlock_irqrestore(&host->clk_gate_lock, flags);
}

static void msdc_set_host_power_control(struct msdc_host *host)
{
	if (MSDC_EMMC == host->hw->host_function) {
		host->power_control = msdc_emmc_power;
	} else if (MSDC_SD == host->hw->host_function) {
		host->power_control = msdc_sd_power;
		host->power_switch = msdc_sd_power_switch;
	} else if (MSDC_SDIO == host->hw->host_function) {
		host->power_control = msdc_sdio_power;
	}

	if (host->power_control != NULL) {
		msdc_power_calibration_init(host);
	} else {
		ERR_MSG("Host function defination error for msdc%d", host->id);
		BUG();
	}
}

void SRC_trigger_signal(int i_on)
{
	if ((ghost != NULL) && (ghost->hw->flags & MSDC_SDIO_IRQ)) {
		pr_debug("msdc2 SRC_trigger_signal %d\n", i_on);
		src_clk_control = i_on;
		if (src_clk_control) {
			msdc_clksrc_onoff(ghost, 1);
			
			if (ghost->mmc->sdio_irq_thread &&
			    (atomic_read(&ghost->mmc->sdio_irq_thread_abort)
				== 0)) {
				mmc_signal_sdio_irq(ghost->mmc);
				if (u_msdc_irq_counter < 3)
					pr_debug("msdc2 SRC_trigger_signal mmc_signal_sdio_irq\n");
			}
		}
	}

}
EXPORT_SYMBOL(SRC_trigger_signal);

#ifdef CONFIG_MTK_HIBERNATION
int msdc_drv_pm_restore_noirq(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct mmc_host *mmc = NULL;
	struct msdc_host *host = NULL;
	BUG_ON(pdev == NULL);
	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);
	if (host->hw->host_function == MSDC_SD) {
		if ((host->id == 1) && !(mmc->caps & MMC_CAP_NONREMOVABLE)) {
			if ((host->hw->cd_level == __gpio_get_value(cd_gpio))
			 && host->mmc->card) {
				mmc_card_set_removed(host->mmc->card);
				host->card_inserted = 0;
			}
		} else if ((host->id == 2) &&
			   !(mmc->caps & MMC_CAP_NONREMOVABLE)) {
			
		}
		host->block_bad_card = 0;
	}
	return 0;
}
#endif

static void msdc_deinit_hw(struct msdc_host *host)
{
	void __iomem *base = host->base;

	
	MSDC_CLR_BIT32(MSDC_INTEN, MSDC_READ32(MSDC_INTEN));
	MSDC_WRITE32(MSDC_INT, MSDC_READ32(MSDC_INT));

	
	msdc_set_power_mode(host, MMC_POWER_OFF);
}

static void msdc_add_host(struct work_struct *work)
{
	int ret;
	struct msdc_host *host = NULL;
	struct mmc_host *mmc = NULL;

	host = container_of(work, struct msdc_host, work_init.work);
	BUG_ON(!host);
	mmc = host->mmc;
	BUG_ON(!mmc);

	
	mmc->caps_uhs = mmc->caps;

	ret = mmc_add_host(mmc);

	if (ret) {
		free_irq(host->irq, host);
		pr_err("[%s]: msdc%d init fail free irq!\n", __func__, host->id);
		platform_set_drvdata(host->pdev, NULL);
		msdc_deinit_hw(host);
		pr_err("[%s]: msdc%d init fail release!\n", __func__, host->id);
		kfree(host->hw);
		mmc_free_host(mmc);
	}
}

void msdc1_set_bad_block(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	host->block_bad_card = 1;
}
EXPORT_SYMBOL(msdc1_set_bad_block);

static int msdc1_read_tray_status(struct seq_file *m, void *v)
{
	struct msdc_host *host = (struct msdc_host*) m->private;
	int level = __gpio_get_value(cd_gpio);
	return seq_printf(m, "%d", (host->hw->cd_level == level) ? 1 : 0);
}

static int msdc1_proc_sd_tray_open(struct inode *inode, struct file *file)
{
	return single_open(file, msdc1_read_tray_status, PDE_DATA(file_inode(file)));
}

static const struct file_operations msdc1_proc_sd_tray_fops = {
	.open		= msdc1_proc_sd_tray_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int msdc1_read_speed_class(struct seq_file *m, void *v)
{
	struct mmc_host *host = (struct mmc_host*) m->private;
	return seq_printf(m, "%d", host->card ? host->card->speed_class : -1);
}

static int msdc1_proc_sd_speed_class_open(struct inode *inode, struct file *file)
{
	return single_open(file, msdc1_read_speed_class, PDE_DATA(file_inode(file)));
}

static const struct file_operations msdc1_proc_sd_speed_class_fops = {
	.open		= msdc1_proc_sd_speed_class_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc = NULL;
	struct msdc_host *host = NULL;
	struct msdc_hw *hw = NULL;
	void __iomem *base = NULL;
	u32 *hclks = NULL;
	int ret = 0;

	
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	ret = msdc_dt_init(pdev, mmc);
	if (ret) {
		mmc_free_host(mmc);
		return ret;
	}

	host = mmc_priv(mmc);
	base = host->base;
	hw = host->hw;

	
	mmc->ops        = &mt_msdc_ops;
	mmc->f_min      = HOST_MIN_MCLK;
	mmc->f_max      = HOST_MAX_MCLK;
	mmc->ocr_avail  = MSDC_OCR_AVAIL;

	if ((hw->flags & MSDC_SDIO_IRQ) || (hw->flags & MSDC_EXT_SDIO_IRQ))
		mmc->caps |= MMC_CAP_SDIO_IRQ;  

#ifdef MTK_MSDC_USE_CMD23
	if (host->hw->host_function == MSDC_EMMC)
		mmc->caps |= MMC_CAP_ERASE | MMC_CAP_CMD23;
	else
		mmc->caps |= MMC_CAP_ERASE;
#else
	mmc->caps |= MMC_CAP_ERASE;
#endif

	mmc->max_busy_timeout = 0;

	
	mmc->max_segs = MAX_HW_SGMTS;
	
	if (hw->host_function == MSDC_SDIO)
		mmc->max_seg_size  = MAX_SGMT_SZ_SDIO;
	else
		mmc->max_seg_size  = MAX_SGMT_SZ;
	mmc->max_blk_size  = HOST_MAX_BLKSZ;
	mmc->max_req_size  = MAX_REQ_SZ;
	mmc->max_blk_count = MAX_REQ_SZ / 512; 

	hclks = msdc_get_hclks(pdev->id);

	host->error             = 0;
	host->mclk              = 0;    
	host->hclk              = hclks[hw->clk_src];
					
	host->sclk              = 0;    
	host->pm_state          = PMSG_RESUME;
	host->suspend           = 0;

	
	INIT_DELAYED_WORK(&(host->set_vcore_workq), sdio_unreq_vcore);

	init_completion(&host->autok_done);
	host->is_autok_done     = 0;

	host->core_clkon        = 0;
	host->clk_gate_count    = 0;
	host->power_mode        = MMC_POWER_OFF;
	host->power_control     = NULL;
	host->power_switch      = NULL;

	host->dma_mask          = DMA_BIT_MASK(33);
	mmc_dev(mmc)->dma_mask  = &host->dma_mask;

#ifndef FPGA_PLATFORM
	if (msdc_get_ccf_clk_pointer(pdev, host))
		return 1;
#endif

	msdc_set_host_power_control(host);
	if ((host->hw->host_function == MSDC_SD) &&
	    !(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
		msdc_sd_power(host, 1); 
		msdc_sd_power(host, 0);
	}

	

	host->card_inserted = host->mmc->caps & MMC_CAP_NONREMOVABLE ? 1 : 0;
	host->timeout_ns = 0;
	host->timeout_clks = DEFAULT_DTOC * 1048576;

#ifndef MTK_MSDC_USE_CMD23
	if (host->hw->host_function != MSDC_SDIO)
		host->autocmd |= MSDC_AUTOCMD12;
	else
		host->autocmd &= ~MSDC_AUTOCMD12;
#else
	if (host->hw->host_function == MSDC_EMMC) {
		host->autocmd &= ~MSDC_AUTOCMD12;

#if (1 == MSDC_USE_AUTO_CMD23)
		host->autocmd |= MSDC_AUTOCMD23;
#endif

	} else if (host->hw->host_function == MSDC_SD) {
		host->autocmd |= MSDC_AUTOCMD12;
	} else {
		host->autocmd &= ~MSDC_AUTOCMD12;
	}
#endif  

	host->mrq = NULL;

#ifdef MTK_MSDC_USE_CACHE
	if (host->hw->host_function == MSDC_EMMC)
		msdc_set_cache_quirk(host);
#endif

	host->dma.used_gpd = 0;
	host->dma.used_bd = 0;

	
	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
			MAX_GPD_NUM * sizeof(struct gpd_t),
			&host->dma.gpd_addr, GFP_KERNEL);
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
			MAX_BD_NUM * sizeof(struct bd_t),
			&host->dma.bd_addr, GFP_KERNEL);
	BUG_ON((!host->dma.gpd) || (!host->dma.bd));
	msdc_init_gpd_bd(host, &host->dma);
	msdc_clock_src[host->id] = hw->clk_src;
	msdc_host_mode[host->id] = mmc->caps;
	msdc_host_mode2[host->id] = mmc->caps2;
	
	mtk_msdc_host[host->id] = host;
	host->write_timeout_uhs104 = 0;
	host->write_timeout_emmc = 0;
	host->read_timeout_uhs104 = 0;
	host->read_timeout_emmc = 0;
	host->sw_timeout = 0;
	host->async_tuning_in_progress = false;
	host->async_tuning_done = true;
	host->legacy_tuning_in_progress = false;
	host->legacy_tuning_done = true;
	#ifndef MSDC_WQ_ERROR_TUNE
	host->err_mrq_dir = 0;
	#endif
	host->timing = 0;
	host->block_bad_card = 0;
	host->sd_30_busy = 0;
	msdc_reset_tmo_tune_counter(host, all_counter);
	msdc_reset_pwr_cycle_counter(host);
	host->error_tune_enable = 1;

	if (host->hw->host_function == MSDC_SDIO)
		wakeup_source_init(&host->trans_lock, "MSDC Transfer Lock");

#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
	if (host->mmc->caps2 & MMC_CAP2_CACHE_CTRL)
		tasklet_init(&host->flush_cache_tasklet,
			msdc_tasklet_flush_cache, (ulong) host);
#endif
	INIT_DELAYED_WORK(&host->write_timeout, msdc_check_write_timeout);
	INIT_DELAYED_WORK(&host->work_init, msdc_add_host);

	
	spin_lock_init(&host->lock);
	spin_lock_init(&host->clk_gate_lock);
	spin_lock_init(&host->remove_bad_card);
	spin_lock_init(&host->sdio_irq_lock);
	
	init_timer(&host->timer);
	
	host->timer.function = msdc_timer_pm;
	host->timer.data = (unsigned long)host;

	ret = request_irq((unsigned int)host->irq, msdc_irq, IRQF_TRIGGER_NONE,
		DRV_NAME, host);
	if (ret)
		goto release;

	MVG_EMMC_SETUP(host);

	if (hw->request_sdio_eirq)
		
		
		hw->request_sdio_eirq(msdc_eirq_sdio, (void *)host);

#ifdef CONFIG_PM
	if (hw->register_pm) {
		
		hw->register_pm(msdc_pm, (void *)host);
		if (hw->flags & MSDC_SYS_SUSPEND) {
			
			ERR_MSG("MSDC_SYS_SUSPEND and register_pm both set");
		}
		
		mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;
	}
#endif

	if (host->hw->host_function == MSDC_EMMC)
		mmc->pm_flags |= MMC_PM_KEEP_POWER;

	platform_set_drvdata(pdev, mmc);

#ifdef CONFIG_MTK_HIBERNATION
	if (pdev->id == 1)
		register_swsusp_restore_noirq_func(ID_M_MSDC,
			msdc_drv_pm_restore_noirq, &(pdev->dev));
#endif

	
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
		MSDC_CLR_BIT32(MSDC_PS, MSDC_PS_CDEN);
		MSDC_CLR_BIT32(MSDC_INTEN, MSDC_INTEN_CDSC);
		MSDC_CLR_BIT32(SDC_CFG, SDC_CFG_INSWKUP);
	}

	#ifdef MSDC_WQ_ERROR_TUNE
	
	INIT_WORK(&host->work_tune, msdc_async_tune);
	host->mrq_tune = NULL;
	#endif

	
	if (!queue_delayed_work(wq_init, &host->work_init, 0)) {
		pr_err("msdc%d queue delay work failed BUG_ON,[%s]L:%d\n",
			host->id, __func__, __LINE__);
		BUG();
	}

#ifdef MTK_MSDC_BRINGUP_DEBUG
	pr_debug("[%s]: msdc%d, mmc->caps=0x%x, mmc->caps2=0x%x\n",
		__func__, host->id, mmc->caps, mmc->caps2);
	msdc_dump_clock_sts();
#endif

	if (host->id == 1) {
		
		host->sd_tray_state = proc_create_data("sd_tray_state", 0444, NULL,
						&msdc1_proc_sd_tray_fops, host);
		if (!host->sd_tray_state)
			pr_warning("msdc%d: Failed to create sd_tray_state entry\n", host->id);

		
		host->speed_class = proc_create_data("sd_speed_class", 0444, NULL,
						&msdc1_proc_sd_speed_class_fops, host->mmc);
		if (!host->speed_class)
			pr_warning("msdc%d: Failed to create sd_speed_class entry\n", host->id);
	}

	return 0;

release:
	platform_set_drvdata(pdev, NULL);
	msdc_deinit_hw(host);
	pr_err("[%s]: msdc%d init fail release!\n", __func__, host->id);

#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
	if (host->mmc->caps2 & MMC_CAP2_CACHE_CTRL)
		tasklet_kill(&host->flush_cache_tasklet);
#endif

	kfree(host->hw);
	mmc_free_host(mmc);

	return ret;
}

static int msdc_drv_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct msdc_host *host;
	struct resource *mem;

	mmc = platform_get_drvdata(pdev);
	BUG_ON(!mmc);

	host = mmc_priv(mmc);
	BUG_ON(!host);

	ERR_MSG("msdc_drv_remove");
#ifndef FPGA_PLATFORM
	
	if (host->clock_control)
		clk_unprepare(host->clock_control);

#endif
	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(host->mmc);
	msdc_deinit_hw(host);

#ifdef MTK_MSDC_FLUSH_BY_CLK_GATE
	if ((host->hw->host_function == MSDC_EMMC) &&
	    (host->mmc->caps2 & MMC_CAP2_CACHE_CTRL))
		tasklet_kill(&host->flush_cache_tasklet);
#endif
	free_irq(host->irq, host);

	dma_free_coherent(NULL, MAX_GPD_NUM * sizeof(struct gpd_t),
		host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(NULL, MAX_BD_NUM * sizeof(struct bd_t),
		host->dma.bd, host->dma.bd_addr);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (mem)
		release_mem_region(mem->start, mem->end - mem->start + 1);

	kfree(host->hw);

	if (host->sd_tray_state)
		remove_proc_entry("sd_tray_state", NULL);

	if (host->speed_class)
		remove_proc_entry("sd_speed_class", NULL);

	mmc_free_host(host->mmc);

	return 0;
}

#ifdef CONFIG_PM
static int msdc_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct msdc_host *host;
	void __iomem *base;

	if (mmc == NULL)
		return 0;

	host = mmc_priv(mmc);
	base = host->base;

	if (state.event == PM_EVENT_SUSPEND) {
		if  (host->hw->flags & MSDC_SYS_SUSPEND) {
			
			msdc_pm(state, (void *)host);
		} else {
			
			msdc_gate_clock(host, -1);
			if (host->error == -EBUSY) {
				ret = host->error;
				host->error = 0;
			}
		}
	}

	if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
		if (host->clk_gate_count > 0) {
			host->error = 0;
			return -EBUSY;
		}
		if (host->saved_para.suspend_flag == 0) {
			host->saved_para.hz = host->mclk;
			if (host->saved_para.hz) {
				host->saved_para.suspend_flag = 1;
				
				msdc_ungate_clock(host);
				msdc_save_timing_setting(host, 2);
				msdc_gate_clock(host, 0);
				if (host->error == -EBUSY) {
					ret = host->error;
					host->error = 0;
				}
			}
			ERR_MSG("msdc suspend cur_cfg=%x, save_cfg=%x, cur_hz=%d",
				MSDC_READ32(MSDC_CFG),
				host->saved_para.msdc_cfg, host->mclk);
		}
	}
	return ret;
}

static int msdc_drv_resume(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct msdc_host *host = mmc_priv(mmc);
	struct pm_message state;

	if (host->hw->flags & MSDC_SDIO_IRQ)
		pr_err("msdc msdc_drv_resume\n");
	state.event = PM_EVENT_RESUME;
	if (mmc && (host->hw->flags & MSDC_SYS_SUSPEND)) {
		msdc_pm(state, (void *)host);
	}

	
	if (host->hw->host_function == MSDC_SDIO) {
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		host->mmc->rescan_entered = 0;
	}

	return 0;
}
#endif

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
#ifdef CONFIG_PM
	.suspend = msdc_drv_suspend,
	.resume = msdc_drv_resume,
#endif
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msdc_of_ids,
	},
};

static int __init mt_msdc_init(void)
{
	int ret;

	
	wq_init = alloc_ordered_workqueue("msdc-init", 0);
	if (!wq_init) {
		pr_err("msdc create work_queue failed.[%s]:%d", __func__, __LINE__);
		BUG();
	}

	#ifdef MSDC_WQ_ERROR_TUNE
	
	wq_tune = create_workqueue("msdc-tune");
	if (!wq_tune) {
		pr_err("msdc create work_queue failed.[%s]:%d",
			__func__, __LINE__);
		BUG();
	}
	#endif

	ret = platform_driver_register(&mt_msdc_driver);
	if (ret) {
		pr_err(DRV_NAME ": Can't register driver");
		return ret;
	}

	msdc_proc_emmc_create();
	msdc_debug_proc_init();

	pr_debug(DRV_NAME ": MediaTek MSDC Driver\n");

	return 0;
}

static void __exit mt_msdc_exit(void)
{
	#ifdef MSDC_WQ_ERROR_TUNE
	if (wq_tune) {
		destroy_workqueue(wq_tune);
		wq_tune = NULL;
	}
	#endif

	platform_driver_unregister(&mt_msdc_driver);

	if (wq_init) {
		destroy_workqueue(wq_init);
		wq_init = NULL;
	}

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_MSDC);
#endif
}

module_init(mt_msdc_init);
module_exit(mt_msdc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
