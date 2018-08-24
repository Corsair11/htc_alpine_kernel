/*
 *  linux/drivers/mmc/core/mmc.c
 *
 *  Copyright (C) 2003-2004 Russell King, All Rights Reserved.
 *  Copyright (C) 2005-2007 Pierre Ossman, All Rights Reserved.
 *  MMCv4 support Copyright (C) 2006 Philip Langdale, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/pm_runtime.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>

#include "core.h"
#include "host.h"
#include "bus.h"
#include "mmc_ops.h"
#include "sd_ops.h"
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
#include <linux/kthread.h>
#endif

extern int msdc_get_gpio_setting(struct msdc_host *host, int suspend);

static const unsigned int tran_exp[] = {
	10000,		100000,		1000000,	10000000,
	0,		0,		0,		0
};

static const unsigned char tran_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

static const unsigned int tacc_exp[] = {
	1,	10,	100,	1000,	10000,	100000,	1000000, 10000000,
};

static const unsigned int tacc_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

#ifdef MTK_BKOPS_IDLE_MAYA
#define MMC_UPDATE_BKOPS_STATS_SUSPEND(stats)\
	do {\
		spin_lock(&stats.lock);\
		if (stats.enabled)\
			stats.suspend++;\
		spin_unlock(&stats.lock);\
	} while (0)
#endif
static int mmc_decode_cid(struct mmc_card *card)
{
	u32 *resp = card->raw_cid;

	switch (card->csd.mmca_vsn) {
	case 0: 
	case 1: 
		card->cid.manfid	= UNSTUFF_BITS(resp, 104, 24);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.prod_name[6]	= UNSTUFF_BITS(resp, 48, 8);
		card->cid.hwrev		= UNSTUFF_BITS(resp, 44, 4);
		card->cid.fwrev		= UNSTUFF_BITS(resp, 40, 4);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 24);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	case 2: 
	case 3: 
	case 4: 
		card->cid.manfid	= UNSTUFF_BITS(resp, 120, 8);
		card->cid.oemid		= UNSTUFF_BITS(resp, 104, 16);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.prv		= UNSTUFF_BITS(resp, 48, 8);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 32);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;

		
		if (card->cid.manfid == CID_MANFID_TOSHIBA ||
			card->cid.manfid ==  CID_MANFID_MICRON ||
			card->cid.manfid == CID_MANFID_SAMSUNG ||
			card->cid.manfid == CID_MANFID_HYNIX)
			card->cid.fwrev = UNSTUFF_BITS(resp, 48, 8);
		break;

	default:
		pr_err("%s: card has unknown MMCA version %d\n",
			mmc_hostname(card->host), card->csd.mmca_vsn);
		return -EINVAL;
	}

	return 0;
}

static void mmc_set_erase_size(struct mmc_card *card)
{
	if (card->ext_csd.erase_group_def & 1)
		card->erase_size = card->ext_csd.hc_erase_size;
	else
		card->erase_size = card->csd.erase_size;

	mmc_init_erase(card);
}

static void mmc_set_wp_grp_size(struct mmc_card *card)
{
	if (card->ext_csd.erase_group_def & 1)
		card->wp_grp_size = card->ext_csd.hc_erase_size *
				    card->ext_csd.raw_hc_erase_gap_size;
	else
		card->wp_grp_size = card->csd.erase_size * (card->csd.wp_grp_size + 1);
}

static int mmc_decode_csd(struct mmc_card *card)
{
	struct mmc_csd *csd = &card->csd;
	unsigned int e, m, a, b;
	u32 *resp = card->raw_csd;

	csd->structure = UNSTUFF_BITS(resp, 126, 2);
	if (csd->structure == 0) {
		pr_err("%s: unrecognised CSD structure version %d\n",
			mmc_hostname(card->host), csd->structure);
		return -EINVAL;
	}

	csd->mmca_vsn	 = UNSTUFF_BITS(resp, 122, 4);
	m = UNSTUFF_BITS(resp, 115, 4);
	e = UNSTUFF_BITS(resp, 112, 3);
	csd->tacc_ns	 = (tacc_exp[e] * tacc_mant[m] + 9) / 10;
	csd->tacc_clks	 = UNSTUFF_BITS(resp, 104, 8) * 100;

	m = UNSTUFF_BITS(resp, 99, 4);
	e = UNSTUFF_BITS(resp, 96, 3);
	csd->max_dtr	  = tran_exp[e] * tran_mant[m];
	csd->cmdclass	  = UNSTUFF_BITS(resp, 84, 12);

	e = UNSTUFF_BITS(resp, 47, 3);
	m = UNSTUFF_BITS(resp, 62, 12);
	csd->capacity	  = (1 + m) << (e + 2);

	csd->read_blkbits = UNSTUFF_BITS(resp, 80, 4);
	csd->read_partial = UNSTUFF_BITS(resp, 79, 1);
	csd->write_misalign = UNSTUFF_BITS(resp, 78, 1);
	csd->read_misalign = UNSTUFF_BITS(resp, 77, 1);
	csd->dsr_imp = UNSTUFF_BITS(resp, 76, 1);
	csd->r2w_factor = UNSTUFF_BITS(resp, 26, 3);
	csd->write_blkbits = UNSTUFF_BITS(resp, 22, 4);
	csd->write_partial = UNSTUFF_BITS(resp, 21, 1);

	if (csd->write_blkbits >= 9) {
		a = UNSTUFF_BITS(resp, 42, 5);
		b = UNSTUFF_BITS(resp, 37, 5);
		csd->erase_size = (a + 1) * (b + 1);
		csd->erase_size <<= csd->write_blkbits - 9;
		csd->wp_grp_size = UNSTUFF_BITS(resp, 32, 5);
	}

	return 0;
}

static void mmc_select_card_type(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u8 card_type = card->ext_csd.raw_card_type;
	u32 caps = host->caps, caps2 = host->caps2;
	unsigned int hs_max_dtr = 0, hs200_max_dtr = 0;
	unsigned int avail_type = 0;

	if (caps & MMC_CAP_MMC_HIGHSPEED &&
	    card_type & EXT_CSD_CARD_TYPE_HS_26) {
		hs_max_dtr = MMC_HIGH_26_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS_26;
	}

	if (caps & MMC_CAP_MMC_HIGHSPEED &&
	    card_type & EXT_CSD_CARD_TYPE_HS_52) {
		hs_max_dtr = MMC_HIGH_52_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS_52;
	}

	if (caps & MMC_CAP_1_8V_DDR &&
	    card_type & EXT_CSD_CARD_TYPE_DDR_1_8V) {
		hs_max_dtr = MMC_HIGH_DDR_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_DDR_1_8V;
	}

	if (caps & MMC_CAP_1_2V_DDR &&
	    card_type & EXT_CSD_CARD_TYPE_DDR_1_2V) {
		hs_max_dtr = MMC_HIGH_DDR_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_DDR_1_2V;
	}

	if (caps2 & MMC_CAP2_HS200_1_8V_SDR &&
	    card_type & EXT_CSD_CARD_TYPE_HS200_1_8V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS200_1_8V;
	}

	if (caps2 & MMC_CAP2_HS200_1_2V_SDR &&
	    card_type & EXT_CSD_CARD_TYPE_HS200_1_2V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS200_1_2V;
	}

	if (caps2 & MMC_CAP2_HS400_1_8V &&
	    card_type & EXT_CSD_CARD_TYPE_HS400_1_8V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS400_1_8V;
	}

	if (caps2 & MMC_CAP2_HS400_1_2V &&
	    card_type & EXT_CSD_CARD_TYPE_HS400_1_2V) {
		hs200_max_dtr = MMC_HS200_MAX_DTR;
		avail_type |= EXT_CSD_CARD_TYPE_HS400_1_2V;
	}

	card->ext_csd.hs_max_dtr = hs_max_dtr;
	card->ext_csd.hs200_max_dtr = hs200_max_dtr;
	card->mmc_avail_type = avail_type;
}

static void mmc_manage_enhanced_area(struct mmc_card *card, u8 *ext_csd)
{
	u8 hc_erase_grp_sz, hc_wp_grp_sz;

	card->ext_csd.enhanced_area_offset = -EINVAL;
	card->ext_csd.enhanced_area_size = -EINVAL;

	if ((ext_csd[EXT_CSD_PARTITION_SUPPORT] & 0x2) &&
	    (ext_csd[EXT_CSD_PARTITION_ATTRIBUTE] & 0x1)) {
		if (card->ext_csd.partition_setting_completed) {
			hc_erase_grp_sz =
				ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
			hc_wp_grp_sz =
				ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

			card->ext_csd.enhanced_area_offset =
				(ext_csd[139] << 24) + (ext_csd[138] << 16) +
				(ext_csd[137] << 8) + ext_csd[136];
			if (mmc_card_blockaddr(card))
				card->ext_csd.enhanced_area_offset <<= 9;
			card->ext_csd.enhanced_area_size =
				(ext_csd[142] << 16) + (ext_csd[141] << 8) +
				ext_csd[140];
			card->ext_csd.enhanced_area_size *=
				(size_t)(hc_erase_grp_sz * hc_wp_grp_sz);
			card->ext_csd.enhanced_area_size <<= 9;
		} else {
			pr_warn("%s: defines enhanced area without partition setting complete\n",
				mmc_hostname(card->host));
		}
	}
}

static void mmc_manage_gp_partitions(struct mmc_card *card, u8 *ext_csd)
{
	int idx;
	u8 hc_erase_grp_sz, hc_wp_grp_sz;
	unsigned int part_size;

	if (ext_csd[EXT_CSD_PARTITION_SUPPORT] &
	    EXT_CSD_PART_SUPPORT_PART_EN) {
		hc_erase_grp_sz =
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
		hc_wp_grp_sz =
			ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

		for (idx = 0; idx < MMC_NUM_GP_PARTITION; idx++) {
			if (!ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3] &&
			    !ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 1] &&
			    !ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 2])
				continue;
			if (card->ext_csd.partition_setting_completed == 0) {
				pr_warn("%s: has partition size defined without partition complete\n",
					mmc_hostname(card->host));
				break;
			}
			part_size =
				(ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 2]
				<< 16) +
				(ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3 + 1]
				<< 8) +
				ext_csd[EXT_CSD_GP_SIZE_MULT + idx * 3];
			part_size *= (size_t)(hc_erase_grp_sz *
				hc_wp_grp_sz);
			mmc_part_add(card, part_size << 19,
				EXT_CSD_PART_CONFIG_ACC_GP0 + idx,
				"gp%d", idx, false,
				MMC_BLK_DATA_AREA_GP);
		}
	}
}

static void htc_mmc_show_cid_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	char *buf;
	int i, j;
	ssize_t n = 0;
	char *check_fw;

	if (mmc_card_mmc(card)) {
		pr_err("%s: cid %08x%08x%08x%08x\n",
			mmc_hostname(card->host),
			card->raw_cid[0], card->raw_cid[1],
			card->raw_cid[2], card->raw_cid[3]);
		pr_err("%s: csd %08x%08x%08x%08x\n",
			mmc_hostname(card->host),
			card->raw_csd[0], card->raw_csd[1],
			card->raw_csd[2], card->raw_csd[3]);

		buf = kmalloc(512, GFP_KERNEL);
		if (buf) {
			for (i = 0; i < 32; i++) {
				for (j = 511 - (16 * i); j >= 496 - (16 * i); j--)
					n += sprintf(buf + n, "%02x", ext_csd[j]);
				n += sprintf(buf + n, "\n");
				pr_err("%s: ext_csd_%03d %s",
					mmc_hostname(card->host), 511 - (16 * i), buf);
				n = 0;
			}
		}
		if (buf)
			kfree(buf);

		
		if (card->cid.manfid == CID_MANFID_SAMSUNG) {
			if (card->ext_csd.rev > 6) { 
				card->cid.fwrev =
				ext_csd[EXT_CSD_VENDOR_SPECIFIC_FIELDS_254];

				
				check_fw = kmalloc(16, GFP_KERNEL);
				if (check_fw)
					sprintf(check_fw, "%02x%02x", ext_csd[255], ext_csd[254]);

				pr_info("%s: Samsung: %s\n", mmc_hostname(card->host),
					check_fw);
				if(check_fw)
					kfree(check_fw);
			}
		}
	}
}

static int mmc_decode_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	int err = 0, idx;
	unsigned int part_size;

	
	card->ext_csd.raw_ext_csd_structure = ext_csd[EXT_CSD_STRUCTURE];
	if (card->csd.structure == 3) {
		if (card->ext_csd.raw_ext_csd_structure > 2) {
			pr_err("%s: unrecognised EXT_CSD structure "
				"version %d\n", mmc_hostname(card->host),
					card->ext_csd.raw_ext_csd_structure);
			err = -EINVAL;
			goto out;
		}
	}

	card->ext_csd.rev = ext_csd[EXT_CSD_REV];

	card->ext_csd.raw_sectors[0] = ext_csd[EXT_CSD_SEC_CNT + 0];
	card->ext_csd.raw_sectors[1] = ext_csd[EXT_CSD_SEC_CNT + 1];
	card->ext_csd.raw_sectors[2] = ext_csd[EXT_CSD_SEC_CNT + 2];
	card->ext_csd.raw_sectors[3] = ext_csd[EXT_CSD_SEC_CNT + 3];
	if (card->ext_csd.rev >= 2) {
		card->ext_csd.sectors =
			ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
			ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
			ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
			ext_csd[EXT_CSD_SEC_CNT + 3] << 24;

		
		if (card->ext_csd.sectors > (2u * 1024 * 1024 * 1024) / 512)
			mmc_card_set_blockaddr(card);
	}

	card->ext_csd.raw_card_type = ext_csd[EXT_CSD_CARD_TYPE];
	mmc_select_card_type(card);

	card->ext_csd.raw_s_a_timeout = ext_csd[EXT_CSD_S_A_TIMEOUT];
	card->ext_csd.raw_erase_timeout_mult =
		ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT];
	card->ext_csd.raw_hc_erase_grp_size =
		ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
	if (card->ext_csd.rev >= 3) {
		u8 sa_shift = ext_csd[EXT_CSD_S_A_TIMEOUT];
		u8 sn_shift = ext_csd[EXT_CSD_SLEEP_NOTIFICATION_TIME];
		card->ext_csd.part_config = ext_csd[EXT_CSD_PART_CONFIG];

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		card->ext_csd.part_time = 40 * ext_csd[EXT_CSD_PART_SWITCH_TIME];
#else
		
		card->ext_csd.part_time = 10 * ext_csd[EXT_CSD_PART_SWITCH_TIME];
#endif

		
		if (sa_shift > 0 && sa_shift <= 0x17)
			card->ext_csd.sa_timeout =
					1 << ext_csd[EXT_CSD_S_A_TIMEOUT];

		
		if (sn_shift > 0 && sn_shift <= 0x17)
			card->ext_csd.sleep_notification_time =
				1 << ext_csd[EXT_CSD_SLEEP_NOTIFICATION_TIME];

		card->ext_csd.erase_group_def =
			ext_csd[EXT_CSD_ERASE_GROUP_DEF];
		card->ext_csd.hc_erase_timeout = 300 *
			ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT];
		card->ext_csd.hc_erase_size =
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] << 10;

		card->ext_csd.rel_sectors = ext_csd[EXT_CSD_REL_WR_SEC_C];

		if (ext_csd[EXT_CSD_BOOT_MULT] && mmc_boot_partition_access(card->host)) {
			for (idx = 0; idx < MMC_NUM_BOOT_PARTITION; idx++) {
				part_size = ext_csd[EXT_CSD_BOOT_MULT] << 17;
				mmc_part_add(card, part_size,
					EXT_CSD_PART_CONFIG_ACC_BOOT0 + idx,
					"boot%d", idx, false,
					MMC_BLK_DATA_AREA_BOOT);
			}
		}
	}

	card->ext_csd.raw_hc_erase_gap_size =
		ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
	card->ext_csd.raw_sec_trim_mult =
		ext_csd[EXT_CSD_SEC_TRIM_MULT];
	card->ext_csd.raw_sec_erase_mult =
		ext_csd[EXT_CSD_SEC_ERASE_MULT];
	card->ext_csd.raw_sec_feature_support =
		ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT];
	card->ext_csd.raw_trim_mult =
		ext_csd[EXT_CSD_TRIM_MULT];
	card->ext_csd.raw_partition_support = ext_csd[EXT_CSD_PARTITION_SUPPORT];
	if (card->ext_csd.rev >= 4) {
		if (ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED] &
		    EXT_CSD_PART_SETTING_COMPLETED)
			card->ext_csd.partition_setting_completed = 1;
		else
			card->ext_csd.partition_setting_completed = 0;

		mmc_manage_enhanced_area(card, ext_csd);

		mmc_manage_gp_partitions(card, ext_csd);

		card->ext_csd.sec_trim_mult =
			ext_csd[EXT_CSD_SEC_TRIM_MULT];
		card->ext_csd.sec_erase_mult =
			ext_csd[EXT_CSD_SEC_ERASE_MULT];
		card->ext_csd.sec_feature_support =
			ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT];
		card->ext_csd.trim_timeout = 300 *
			ext_csd[EXT_CSD_TRIM_MULT];

		card->ext_csd.boot_ro_lock = ext_csd[EXT_CSD_BOOT_WP];
		card->ext_csd.boot_ro_lockable = true;

		
		card->ext_csd.raw_pwr_cl_52_195 =
			ext_csd[EXT_CSD_PWR_CL_52_195];
		card->ext_csd.raw_pwr_cl_26_195 =
			ext_csd[EXT_CSD_PWR_CL_26_195];
		card->ext_csd.raw_pwr_cl_52_360 =
			ext_csd[EXT_CSD_PWR_CL_52_360];
		card->ext_csd.raw_pwr_cl_26_360 =
			ext_csd[EXT_CSD_PWR_CL_26_360];
		card->ext_csd.raw_pwr_cl_200_195 =
			ext_csd[EXT_CSD_PWR_CL_200_195];
		card->ext_csd.raw_pwr_cl_200_360 =
			ext_csd[EXT_CSD_PWR_CL_200_360];
		card->ext_csd.raw_pwr_cl_ddr_52_195 =
			ext_csd[EXT_CSD_PWR_CL_DDR_52_195];
		card->ext_csd.raw_pwr_cl_ddr_52_360 =
			ext_csd[EXT_CSD_PWR_CL_DDR_52_360];
		card->ext_csd.raw_pwr_cl_ddr_200_360 =
			ext_csd[EXT_CSD_PWR_CL_DDR_200_360];
	}

	if (card->ext_csd.rev >= 5) {
		
		if (card->cid.year < 2010)
			card->cid.year += 16;

		
		if (ext_csd[EXT_CSD_BKOPS_SUPPORT] & 0x1) {
			card->ext_csd.bkops = 1;
			card->ext_csd.bkops_en =
				(ext_csd[EXT_CSD_BKOPS_EN] & 0x1);
			card->ext_csd.raw_bkops_status =
				ext_csd[EXT_CSD_BKOPS_STATUS];
			if (!card->ext_csd.bkops_en)
				pr_info("%s: BKOPS_EN bit is not set\n",
					mmc_hostname(card->host));
		}

		
		if (ext_csd[EXT_CSD_HPI_FEATURES] & 0x1) {
			card->ext_csd.hpi = 1;
			if (ext_csd[EXT_CSD_HPI_FEATURES] & 0x2)
				card->ext_csd.hpi_cmd =	MMC_STOP_TRANSMISSION;
			else
				card->ext_csd.hpi_cmd = MMC_SEND_STATUS;
			card->ext_csd.out_of_int_time =
				ext_csd[EXT_CSD_OUT_OF_INTERRUPT_TIME] * 10;
		}

		card->ext_csd.rel_param = ext_csd[EXT_CSD_WR_REL_PARAM];
		card->ext_csd.rst_n_function = ext_csd[EXT_CSD_RST_N_FUNCTION];

		card->ext_csd.raw_rpmb_size_mult = ext_csd[EXT_CSD_RPMB_MULT];
		if (ext_csd[EXT_CSD_RPMB_MULT] && mmc_host_cmd23(card->host)) {
			mmc_part_add(card, ext_csd[EXT_CSD_RPMB_MULT] << 17,
				EXT_CSD_PART_CONFIG_ACC_RPMB,
				"rpmb", 0, false,
				MMC_BLK_DATA_AREA_RPMB);
		}
	}

	card->ext_csd.raw_erased_mem_count = ext_csd[EXT_CSD_ERASED_MEM_CONT];
	if (ext_csd[EXT_CSD_ERASED_MEM_CONT])
		card->erased_byte = 0xFF;
	else
		card->erased_byte = 0x0;

	
	if (card->ext_csd.rev >= 6) {
		card->ext_csd.feature_support |= MMC_DISCARD_FEATURE;

		card->ext_csd.generic_cmd6_time = 10 *
			ext_csd[EXT_CSD_GENERIC_CMD6_TIME];
		card->ext_csd.power_off_longtime = 10 *
			ext_csd[EXT_CSD_POWER_OFF_LONG_TIME];

		card->ext_csd.cache_size =
			ext_csd[EXT_CSD_CACHE_SIZE + 0] << 0 |
			ext_csd[EXT_CSD_CACHE_SIZE + 1] << 8 |
			ext_csd[EXT_CSD_CACHE_SIZE + 2] << 16 |
			ext_csd[EXT_CSD_CACHE_SIZE + 3] << 24;

		if (ext_csd[EXT_CSD_DATA_SECTOR_SIZE] == 1)
			card->ext_csd.data_sector_size = 4096;
		else
			card->ext_csd.data_sector_size = 512;

		if ((ext_csd[EXT_CSD_DATA_TAG_SUPPORT] & 1) &&
		    (ext_csd[EXT_CSD_TAG_UNIT_SIZE] <= 8)) {
			card->ext_csd.data_tag_unit_size =
			((unsigned int) 1 << ext_csd[EXT_CSD_TAG_UNIT_SIZE]) *
			(card->ext_csd.data_sector_size);
		} else {
			card->ext_csd.data_tag_unit_size = 0;
		}

		card->ext_csd.max_packed_writes =
			ext_csd[EXT_CSD_MAX_PACKED_WRITES];
		card->ext_csd.max_packed_reads =
			ext_csd[EXT_CSD_MAX_PACKED_READS];
	} else {
		card->ext_csd.data_sector_size = 512;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (card->ext_csd.rev > 7) {
		card->ext_csd.cmdq_support = ext_csd[EXT_CSD_CMDQ_SUPPORT];
		if (card->ext_csd.cmdq_support) {
			pr_err("[CQ] card support CMDQ\n");
			card->ext_csd.cmdq_depth = ext_csd[EXT_CSD_CMDQ_DEPTH] + 1;
			pr_err("[CQ] cmdq depth %d\n", card->ext_csd.cmdq_depth);
		} else {
			pr_err("[CQ] card NOT support CMDQ\n");
			card->ext_csd.cmdq_support = 0;
			card->ext_csd.cmdq_depth = 16;
		}
	} else {
		card->ext_csd.cmdq_support = 0;
		card->ext_csd.cmdq_depth = 16;
	}
#endif

	htc_mmc_show_cid_ext_csd(card, ext_csd);
	pr_info("%s: ext_csd[EXT_CSD_FFU_FEATURES]=%x\n",
			__func__, ext_csd[EXT_CSD_FFU_FEATURES]);
out:
	return err;
}

static int mmc_read_ext_csd(struct mmc_card *card)
{
	u8 *ext_csd;
	int err;

	if (!mmc_can_ext_csd(card))
		return 0;

	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		if ((err != -EINVAL)
		 && (err != -ENOSYS)
		 && (err != -EFAULT))
			return err;

		if (card->csd.capacity == (4096 * 512)) {
			pr_err("%s: unable to read EXT_CSD on a possible high capacity card. Card will be ignored.\n",
				mmc_hostname(card->host));
		} else {
			pr_warn("%s: unable to read EXT_CSD, performance might suffer\n",
				mmc_hostname(card->host));
			err = 0;
		}

		return err;
	}

	err = mmc_decode_ext_csd(card, ext_csd);
	kfree(ext_csd);
	return err;
}

static int mmc_compare_ext_csds(struct mmc_card *card, unsigned bus_width)
{
	u8 *bw_ext_csd;
	int err;

	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	err = mmc_get_ext_csd(card, &bw_ext_csd);
	if (err)
		return err;

	
	err = !((card->ext_csd.raw_partition_support ==
			bw_ext_csd[EXT_CSD_PARTITION_SUPPORT]) &&
		(card->ext_csd.raw_erased_mem_count ==
			bw_ext_csd[EXT_CSD_ERASED_MEM_CONT]) &&
		(card->ext_csd.rev ==
			bw_ext_csd[EXT_CSD_REV]) &&
		(card->ext_csd.raw_ext_csd_structure ==
			bw_ext_csd[EXT_CSD_STRUCTURE]) &&
		(card->ext_csd.raw_card_type ==
			bw_ext_csd[EXT_CSD_CARD_TYPE]) &&
		(card->ext_csd.raw_s_a_timeout ==
			bw_ext_csd[EXT_CSD_S_A_TIMEOUT]) &&
		(card->ext_csd.raw_hc_erase_gap_size ==
			bw_ext_csd[EXT_CSD_HC_WP_GRP_SIZE]) &&
		(card->ext_csd.raw_erase_timeout_mult ==
			bw_ext_csd[EXT_CSD_ERASE_TIMEOUT_MULT]) &&
		(card->ext_csd.raw_hc_erase_grp_size ==
			bw_ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]) &&
		(card->ext_csd.raw_sec_trim_mult ==
			bw_ext_csd[EXT_CSD_SEC_TRIM_MULT]) &&
		(card->ext_csd.raw_sec_erase_mult ==
			bw_ext_csd[EXT_CSD_SEC_ERASE_MULT]) &&
		(card->ext_csd.raw_sec_feature_support ==
			bw_ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT]) &&
		(card->ext_csd.raw_trim_mult ==
			bw_ext_csd[EXT_CSD_TRIM_MULT]) &&
		(card->ext_csd.raw_sectors[0] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 0]) &&
		(card->ext_csd.raw_sectors[1] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 1]) &&
		(card->ext_csd.raw_sectors[2] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 2]) &&
		(card->ext_csd.raw_sectors[3] ==
			bw_ext_csd[EXT_CSD_SEC_CNT + 3]) &&
		(card->ext_csd.raw_pwr_cl_52_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_52_195]) &&
		(card->ext_csd.raw_pwr_cl_26_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_26_195]) &&
		(card->ext_csd.raw_pwr_cl_52_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_52_360]) &&
		(card->ext_csd.raw_pwr_cl_26_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_26_360]) &&
		(card->ext_csd.raw_pwr_cl_200_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_200_195]) &&
		(card->ext_csd.raw_pwr_cl_200_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_200_360]) &&
		(card->ext_csd.raw_pwr_cl_ddr_52_195 ==
			bw_ext_csd[EXT_CSD_PWR_CL_DDR_52_195]) &&
		(card->ext_csd.raw_pwr_cl_ddr_52_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_DDR_52_360]) &&
		(card->ext_csd.raw_pwr_cl_ddr_200_360 ==
			bw_ext_csd[EXT_CSD_PWR_CL_DDR_200_360]));

	if (err)
		err = -EINVAL;

	kfree(bw_ext_csd);
	return err;
}

MMC_DEV_ATTR(cid, "%08x%08x%08x%08x\n", card->raw_cid[0], card->raw_cid[1],
	card->raw_cid[2], card->raw_cid[3]);
MMC_DEV_ATTR(csd, "%08x%08x%08x%08x\n", card->raw_csd[0], card->raw_csd[1],
	card->raw_csd[2], card->raw_csd[3]);
MMC_DEV_ATTR(date, "%02d/%04d\n", card->cid.month, card->cid.year);
MMC_DEV_ATTR(erase_size, "%u\n", card->erase_size << 9);
MMC_DEV_ATTR(preferred_erase_size, "%u\n", card->pref_erase << 9);
MMC_DEV_ATTR(wp_grp_size, "%u\n", card->wp_grp_size << 9);
MMC_DEV_ATTR(fwrev, "0x%x\n", card->cid.fwrev);
MMC_DEV_ATTR(hwrev, "0x%x\n", card->cid.hwrev);
MMC_DEV_ATTR(manfid, "0x%06x\n", card->cid.manfid);
MMC_DEV_ATTR(name, "%s\n", card->cid.prod_name);
MMC_DEV_ATTR(oemid, "0x%04x\n", card->cid.oemid);
MMC_DEV_ATTR(prv, "0x%x\n", card->cid.prv);
MMC_DEV_ATTR(serial, "0x%08x\n", card->cid.serial);
MMC_DEV_ATTR(enhanced_area_offset, "%llu\n",
		card->ext_csd.enhanced_area_offset);
MMC_DEV_ATTR(enhanced_area_size, "%u\n", card->ext_csd.enhanced_area_size);
MMC_DEV_ATTR(raw_rpmb_size_mult, "%#x\n", card->ext_csd.raw_rpmb_size_mult);
MMC_DEV_ATTR(rel_sectors, "%#x\n", card->ext_csd.rel_sectors);

static ssize_t mmc_manf_name_show (struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mmc_card *card = mmc_dev_to_card(dev);
	int count = 0;

	switch (card->cid.manfid) {
	case CID_MANFID_SANDISK:
	case CID_MANFID_SANDISK_NEW:
		count = sprintf(buf, "Sandisk\n");
		break;
	case CID_MANFID_TOSHIBA:
		count = sprintf(buf, "Toshiba\n");
		break;
	case CID_MANFID_MICRON:
		count = sprintf(buf, "Micron\n");
		break;
	case CID_MANFID_SAMSUNG:
		count = sprintf(buf, "Samsung\n");
		break;
	case CID_MANFID_HYNIX:
		count = sprintf(buf, "Hynix\n");
		break;
	default:
		count = sprintf(buf, "Unknown (%d)\n", card->cid.manfid);
	}
	return count;
}

DEVICE_ATTR(manf_name, S_IRUGO, mmc_manf_name_show, NULL);

static struct attribute *mmc_std_attrs[] = {
	&dev_attr_cid.attr,
	&dev_attr_csd.attr,
	&dev_attr_date.attr,
	&dev_attr_erase_size.attr,
	&dev_attr_preferred_erase_size.attr,
	&dev_attr_wp_grp_size.attr,
	&dev_attr_fwrev.attr,
	&dev_attr_hwrev.attr,
	&dev_attr_manfid.attr,
	&dev_attr_name.attr,
	&dev_attr_oemid.attr,
	&dev_attr_prv.attr,
	&dev_attr_serial.attr,
	&dev_attr_enhanced_area_offset.attr,
	&dev_attr_enhanced_area_size.attr,
	&dev_attr_raw_rpmb_size_mult.attr,
	&dev_attr_rel_sectors.attr,
	&dev_attr_manf_name.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mmc_std);

static struct device_type mmc_type = {
	.groups = mmc_std_groups,
};

static int __mmc_select_powerclass(struct mmc_card *card,
				   unsigned int bus_width)
{
	struct mmc_host *host = card->host;
	struct mmc_ext_csd *ext_csd = &card->ext_csd;
	unsigned int pwrclass_val = 0;
	int err = 0;

	switch (1 << host->ios.vdd) {
	case MMC_VDD_165_195:
		if (host->ios.clock <= MMC_HIGH_26_MAX_DTR)
			pwrclass_val = ext_csd->raw_pwr_cl_26_195;
		else if (host->ios.clock <= MMC_HIGH_52_MAX_DTR)
			pwrclass_val = (bus_width <= EXT_CSD_BUS_WIDTH_8) ?
				ext_csd->raw_pwr_cl_52_195 :
				ext_csd->raw_pwr_cl_ddr_52_195;
		else if (host->ios.clock <= MMC_HS200_MAX_DTR)
			pwrclass_val = ext_csd->raw_pwr_cl_200_195;
		break;
	case MMC_VDD_27_28:
	case MMC_VDD_28_29:
	case MMC_VDD_29_30:
	case MMC_VDD_30_31:
	case MMC_VDD_31_32:
	case MMC_VDD_32_33:
	case MMC_VDD_33_34:
	case MMC_VDD_34_35:
	case MMC_VDD_35_36:
		if (host->ios.clock <= MMC_HIGH_26_MAX_DTR)
			pwrclass_val = ext_csd->raw_pwr_cl_26_360;
		else if (host->ios.clock <= MMC_HIGH_52_MAX_DTR)
			pwrclass_val = (bus_width <= EXT_CSD_BUS_WIDTH_8) ?
				ext_csd->raw_pwr_cl_52_360 :
				ext_csd->raw_pwr_cl_ddr_52_360;
		else if (host->ios.clock <= MMC_HS200_MAX_DTR)
			pwrclass_val = (bus_width == EXT_CSD_DDR_BUS_WIDTH_8) ?
				ext_csd->raw_pwr_cl_ddr_200_360 :
				ext_csd->raw_pwr_cl_200_360;
		break;
	default:
		pr_warn("%s: Voltage range not supported for power class\n",
			mmc_hostname(host));
		return -EINVAL;
	}

	if (bus_width & (EXT_CSD_BUS_WIDTH_8 | EXT_CSD_DDR_BUS_WIDTH_8))
		pwrclass_val = (pwrclass_val & EXT_CSD_PWR_CL_8BIT_MASK) >>
				EXT_CSD_PWR_CL_8BIT_SHIFT;
	else
		pwrclass_val = (pwrclass_val & EXT_CSD_PWR_CL_4BIT_MASK) >>
				EXT_CSD_PWR_CL_4BIT_SHIFT;

	
	if (pwrclass_val > 0) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_POWER_CLASS,
				 pwrclass_val,
				 card->ext_csd.generic_cmd6_time);
	}

	return err;
}

static int mmc_select_powerclass(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u32 bus_width, ext_csd_bits;
	int err, ddr;

	
	if (!mmc_can_ext_csd(card))
		return 0;

	bus_width = host->ios.bus_width;
	
	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	ddr = card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_52;
	if (ddr)
		ext_csd_bits = (bus_width == MMC_BUS_WIDTH_8) ?
			EXT_CSD_DDR_BUS_WIDTH_8 : EXT_CSD_DDR_BUS_WIDTH_4;
	else
		ext_csd_bits = (bus_width == MMC_BUS_WIDTH_8) ?
			EXT_CSD_BUS_WIDTH_8 :  EXT_CSD_BUS_WIDTH_4;

	err = __mmc_select_powerclass(card, ext_csd_bits);
	if (err)
		pr_warn("%s: power class selection to bus width %d ddr %d failed\n",
			mmc_hostname(host), 1 << bus_width, ddr);

	return err;
}

static void mmc_set_bus_speed(struct mmc_card *card)
{
	unsigned int max_dtr = (unsigned int)-1;

	if ((mmc_card_hs200(card) || mmc_card_hs400(card)) &&
	     max_dtr > card->ext_csd.hs200_max_dtr)
		max_dtr = card->ext_csd.hs200_max_dtr;
	else if (mmc_card_hs(card) && max_dtr > card->ext_csd.hs_max_dtr)
		max_dtr = card->ext_csd.hs_max_dtr;
	else if (max_dtr > card->csd.max_dtr)
		max_dtr = card->csd.max_dtr;

	mmc_set_clock(card->host, max_dtr);
}

static int mmc_select_bus_width(struct mmc_card *card)
{
	static unsigned ext_csd_bits[] = {
		EXT_CSD_BUS_WIDTH_8,
		EXT_CSD_BUS_WIDTH_4,
	};
	static unsigned bus_widths[] = {
		MMC_BUS_WIDTH_8,
		MMC_BUS_WIDTH_4,
	};
	struct mmc_host *host = card->host;
	unsigned idx, bus_width = 0;
	int err = 0;

	if (!mmc_can_ext_csd(card) &&
	    !(host->caps & (MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA)))
		return 0;

	idx = (host->caps & MMC_CAP_8_BIT_DATA) ? 0 : 1;

	for (; idx < ARRAY_SIZE(bus_widths); idx++) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_BUS_WIDTH,
				 ext_csd_bits[idx],
				 card->ext_csd.generic_cmd6_time);
		if (err)
			continue;

		bus_width = bus_widths[idx];
		mmc_set_bus_width(host, bus_width);

		if (!(host->caps & MMC_CAP_BUS_WIDTH_TEST))
			err = mmc_compare_ext_csds(card, bus_width);
		else
			err = mmc_bus_test(card, bus_width);

		if (!err) {
			err = bus_width;
			break;
		} else {
			pr_warn("%s: switch to bus width %d failed\n",
				mmc_hostname(host), ext_csd_bits[idx]);
		}
	}

	return err;
}

static int mmc_select_hs(struct mmc_card *card)
{
	int err;

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS,
			   card->ext_csd.generic_cmd6_time,
			   true, true, true);
	if (!err)
		mmc_set_timing(card->host, MMC_TIMING_MMC_HS);

	return err;
}

static int mmc_select_hs_ddr(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	u32 bus_width, ext_csd_bits;
	int err = 0;

	if (!(card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_52))
		return 0;

	bus_width = host->ios.bus_width;
	if (bus_width == MMC_BUS_WIDTH_1)
		return 0;

	ext_csd_bits = (bus_width == MMC_BUS_WIDTH_8) ?
		EXT_CSD_DDR_BUS_WIDTH_8 : EXT_CSD_DDR_BUS_WIDTH_4;

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_BUS_WIDTH,
			ext_csd_bits,
			card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("%s: switch to bus width %d ddr failed\n",
			mmc_hostname(host), 1 << bus_width);
		return err;
	}

	err = -EINVAL;
	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_1_2V)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_120);

	if (err && (card->mmc_avail_type & EXT_CSD_CARD_TYPE_DDR_1_8V))
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_180);

	
	if (err)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_330);

	if (!err)
		mmc_set_timing(host, MMC_TIMING_MMC_DDR52);

	return err;
}

static int mmc_select_hs400(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;

	if (!(card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400 &&
	      host->ios.bus_width == MMC_BUS_WIDTH_8))
		return 0;

	mmc_set_timing(card->host, MMC_TIMING_MMC_HS);
	mmc_set_bus_speed(card);

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS,
			   card->ext_csd.generic_cmd6_time,
			   true, true, true);
	if (err) {
		pr_err("%s: switch to high-speed from hs200 failed, err:%d\n",
			mmc_hostname(host), err);
		return err;
	}

	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_BUS_WIDTH,
			 EXT_CSD_DDR_BUS_WIDTH_8,
			 card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("%s: switch to bus width for hs400 failed, err:%d\n",
			mmc_hostname(host), err);
		return err;
	}

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS400,
			   card->ext_csd.generic_cmd6_time,
			   true, true, true);
	if (err) {
		pr_err("%s: switch to hs400 failed, err:%d\n",
			 mmc_hostname(host), err);
		return err;
	}

	mmc_set_timing(host, MMC_TIMING_MMC_HS400);
	mmc_set_bus_speed(card);

	return 0;
}

int mmc_hs200_to_hs400(struct mmc_card *card)
{
	return mmc_select_hs400(card);
}

static int mmc_switch_status(struct mmc_card *card)
{
	u32 status;
	int err;

	err = mmc_send_status(card, &status);
	if (err)
		return err;

	return mmc_switch_status_error(card->host, status);
}

int mmc_hs400_to_hs200(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	bool send_status = true;
	unsigned int max_dtr;
	int err;

	if (host->caps & MMC_CAP_WAIT_WHILE_BUSY)
		send_status = false;

	
	max_dtr = card->ext_csd.hs_max_dtr;
	mmc_set_clock(host, max_dtr);

	
	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING,
			   EXT_CSD_TIMING_HS, card->ext_csd.generic_cmd6_time,
			   true, send_status, true);
	if (err)
		goto out_err;

	mmc_set_timing(host, MMC_TIMING_MMC_DDR52);

	if (!send_status) {
		err = mmc_switch_status(card);
		if (err)
			goto out_err;
	}

	
	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH,
			   EXT_CSD_BUS_WIDTH_8, card->ext_csd.generic_cmd6_time,
			   true, send_status, true);
	if (err)
		goto out_err;

	mmc_set_timing(host, MMC_TIMING_MMC_HS);

	if (!send_status) {
		err = mmc_switch_status(card);
		if (err)
			goto out_err;
	}

	
	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING,
			   EXT_CSD_TIMING_HS200,
			   card->ext_csd.generic_cmd6_time, true, send_status,
			   true);
	if (err)
		goto out_err;

	mmc_set_timing(host, MMC_TIMING_MMC_HS200);

	if (!send_status) {
		err = mmc_switch_status(card);
		if (err)
			goto out_err;
	}

	mmc_set_bus_speed(card);

	return 0;

out_err:
	pr_err("%s: %s failed, error %d\n", mmc_hostname(card->host),
	       __func__, err);
	return err;
}

static int mmc_select_hs200(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = -EINVAL;

	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200_1_2V)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_120);

	if (err && card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200_1_8V)
		err = __mmc_set_signal_voltage(host, MMC_SIGNAL_VOLTAGE_180);

	
	if (err)
		goto err;

	err = mmc_select_bus_width(card);
	if (!IS_ERR_VALUE(err)) {
		err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				   EXT_CSD_HS_TIMING, EXT_CSD_TIMING_HS200,
				   card->ext_csd.generic_cmd6_time,
				   true, true, true);
		if (!err)
			mmc_set_timing(host, MMC_TIMING_MMC_HS200);
	}
err:
	return err;
}

static int mmc_select_timing(struct mmc_card *card)
{
	int err = 0;

	if (!mmc_can_ext_csd(card))
		goto bus_speed;

	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS200)
		err = mmc_select_hs200(card);
	else if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS)
		err = mmc_select_hs(card);

	if (err && err != -EBADMSG)
		return err;

	if (err) {
		pr_warn("%s: switch to %s failed\n",
			mmc_card_hs(card) ? "high-speed" :
			(mmc_card_hs200(card) ? "hs200" : ""),
			mmc_hostname(card->host));
		err = 0;
	}

bus_speed:
	mmc_set_bus_speed(card);
	return err;
}

static int mmc_hs200_tuning(struct mmc_card *card)
{
	struct mmc_host *host = card->host;

	if (card->mmc_avail_type & EXT_CSD_CARD_TYPE_HS400 &&
	    host->ios.bus_width == MMC_BUS_WIDTH_8)
		if (host->ops->prepare_hs400_tuning)
			host->ops->prepare_hs400_tuning(host, &host->ios);

	return mmc_execute_tuning(card);
}

static int mmc_init_card(struct mmc_host *host, u32 ocr,
	struct mmc_card *oldcard)
{
	struct mmc_card *card;
	int err;
	u32 cid[4];
	u32 rocr;

	BUG_ON(!host);
	WARN_ON(!host->claimed);

	
	if (!mmc_host_is_spi(host))
		mmc_set_bus_mode(host, MMC_BUSMODE_OPENDRAIN);

	mmc_go_idle(host);

	
	err = mmc_send_op_cond(host, ocr | (1 << 30), &rocr);
	if (err)
		goto err;

	if (mmc_host_is_spi(host)) {
		err = mmc_spi_set_crc(host, use_spi_crc);
		if (err)
			goto err;
	}

	if (mmc_host_is_spi(host))
		err = mmc_send_cid(host, cid);
	else
		err = mmc_all_send_cid(host, cid);
	if (err)
		goto err;

#ifdef CONFIG_MMC_FFU
	if (oldcard && (oldcard->state & MMC_STATE_FFUED)) {
		memcpy((void *)oldcard->raw_cid, (void *)cid, sizeof(cid));
		err = mmc_decode_cid(oldcard);
		if (err)
			goto free_card;

		card = oldcard;
		card->nr_parts = 0;
		oldcard = NULL;

	} else
#endif
	if (oldcard) {
		if (memcmp(cid, oldcard->raw_cid, sizeof(cid)) != 0) {
			err = -ENOENT;
			goto err;
		}

		card = oldcard;
	} else {
		card = mmc_alloc_card(host, &mmc_type);
		if (IS_ERR(card)) {
			err = PTR_ERR(card);
			goto err;
		}

		card->ocr = ocr;
		card->type = MMC_TYPE_MMC;
		card->rca = 1;
		memcpy(card->raw_cid, cid, sizeof(card->raw_cid));
	}

	if (!mmc_host_is_spi(host)) {
		err = mmc_set_relative_addr(card);
		if (err)
			goto free_card;

		mmc_set_bus_mode(host, MMC_BUSMODE_PUSHPULL);
	}

	if (!oldcard) {
		err = mmc_send_csd(card, card->raw_csd);
		if (err)
			goto free_card;

		err = mmc_decode_csd(card);
		if (err)
			goto free_card;
		err = mmc_decode_cid(card);
		if (err)
			goto free_card;
	}

	if (card->csd.dsr_imp && host->dsr_req)
		mmc_set_dsr(host);

	if (!mmc_host_is_spi(host)) {
		err = mmc_select_card(card);
		if (err)
			goto free_card;
	}

	if (!oldcard) {
		
		err = mmc_read_ext_csd(card);
		if (err)
			goto free_card;

		if (!(mmc_card_blockaddr(card)) && (rocr & (1<<30)))
			mmc_card_set_blockaddr(card);

		
		mmc_set_erase_size(card);
	}

	if (card->ext_csd.partition_setting_completed ||
	    (card->ext_csd.rev >= 3 && (host->caps2 & MMC_CAP2_HC_ERASE_SZ))) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_ERASE_GROUP_DEF, 1,
				 card->ext_csd.generic_cmd6_time);

		if (err && err != -EBADMSG)
			goto free_card;

		if (err) {
			err = 0;
			card->ext_csd.enhanced_area_offset = -EINVAL;
			card->ext_csd.enhanced_area_size = -EINVAL;
		} else {
			card->ext_csd.erase_group_def = 1;
			mmc_set_erase_size(card);
		}
	}
	mmc_set_wp_grp_size(card);

	if (card->ext_csd.part_config & EXT_CSD_PART_CONFIG_ACC_MASK) {
		card->ext_csd.part_config &= ~EXT_CSD_PART_CONFIG_ACC_MASK;
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONFIG,
				 card->ext_csd.part_config,
				 card->ext_csd.part_time);
		if (err && err != -EBADMSG)
			goto free_card;
	}

	if (card->ext_csd.rev >= 6) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_POWER_OFF_NOTIFICATION,
				 EXT_CSD_POWER_ON,
				 card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG)
			goto free_card;

		if (!err)
			card->ext_csd.power_off_notification = EXT_CSD_POWER_ON;
	}

	err = mmc_select_timing(card);
	if (err)
		goto free_card;

	if (mmc_card_hs200(card)) {
		err = mmc_hs200_tuning(card);
		if (err)
			goto free_card;

		err = mmc_select_hs400(card);
		if (err)
			goto free_card;
	} else if (mmc_card_hs(card)) {
		
		err = mmc_select_bus_width(card);
		if (!IS_ERR_VALUE(err)) {
			err = mmc_select_hs_ddr(card);
			if (err)
				goto free_card;
		}
	}

	mmc_select_powerclass(card);
#ifdef MTK_BKOPS_IDLE_MAYA
	if (card->ext_csd.bkops) {
		if (!card->ext_csd.bkops_en) {
			
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_BKOPS_EN, 1, card->ext_csd.generic_cmd6_time);
			if (err && err != -EBADMSG)
				goto free_card;
			if (err) {
				pr_warn("%s: Enabling BKOPS failed\n",
					mmc_hostname(card->host));
				card->ext_csd.bkops_en = 0;
				err = 0;
			} else {
				card->ext_csd.bkops_en = 1;
			}
		}
	}
#endif
	if (card->ext_csd.hpi) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_HPI_MGMT, 1,
				card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG)
			goto free_card;
		if (err) {
			pr_warn("%s: Enabling HPI failed\n",
				mmc_hostname(card->host));
			err = 0;
		} else
			card->ext_csd.hpi_en = 1;
	}

#ifdef CONFIG_MTK_EMMC_CACHE
	if (card->quirks & MMC_QUIRK_DISABLE_CACHE)
		goto skip_cache;
#endif
	if (card->ext_csd.cache_size > 0) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_CACHE_CTRL, 1,
				card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG)
			goto free_card;

		if (err) {
			pr_warn("%s: Cache is supported, but failed to turn on (%d)\n",
				mmc_hostname(card->host), err);
			card->ext_csd.cache_ctrl = 0;
			err = 0;
		} else {
			card->ext_csd.cache_ctrl = 1;
		}
	}
#ifdef CONFIG_MTK_EMMC_CACHE
skip_cache:
#endif

	if (card->ext_csd.max_packed_writes >= 3 &&
	    card->ext_csd.max_packed_reads >= 5 &&
	    host->caps2 & MMC_CAP2_PACKED_CMD) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_EXP_EVENTS_CTRL,
				EXT_CSD_PACKED_EVENT_EN,
				card->ext_csd.generic_cmd6_time);
		if (err && err != -EBADMSG)
			goto free_card;
		if (err) {
			pr_warn("%s: Enabling packed event failed\n",
				mmc_hostname(card->host));
			card->ext_csd.packed_event_en = 0;
			err = 0;
		} else {
			card->ext_csd.packed_event_en = 1;
		}
	}

	if (!oldcard)
		host->card = card;

	return 0;

free_card:
	if (!oldcard)
		mmc_remove_card(card);
err:
	return err;
}

#ifdef CONFIG_MMC_FFU
int mmc_reinit_oldcard(struct mmc_host *host)
{
	return mmc_init_card(host, host->card->ocr, host->card);
}
#endif

static int mmc_cache_ctrl(struct mmc_host *host, u8 enable)
{
	struct mmc_card *card = host->card;
	unsigned int timeout;
	int err = 0;

	if (card && mmc_card_mmc(card) &&
			(card->ext_csd.cache_size > 0)) {
		enable = !!enable;

		if (card->ext_csd.cache_ctrl ^ enable) {
			timeout = enable ? card->ext_csd.generic_cmd6_time : 0;
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CACHE_CTRL, enable, timeout);
			if (err)
				pr_err("%s: cache %s error %d\n",
						mmc_hostname(card->host),
						enable ? "on" : "off",
						err);
			else
				card->ext_csd.cache_ctrl = enable;
		}
	}

	return err;
}

static int mmc_can_sleep(struct mmc_card *card)
{
	return (card && card->ext_csd.rev >= 3);
}

static int mmc_sleep(struct mmc_host *host)
{
	struct mmc_command cmd = {0};
	struct mmc_card *card = host->card;
	unsigned int timeout_ms = DIV_ROUND_UP(card->ext_csd.sa_timeout, 10000);
	unsigned int sn_timeout_ms =
		DIV_ROUND_UP(card->ext_csd.sleep_notification_time, 100);
	int err;

	
	mmc_retune_hold(host);

	if (card->ext_csd.rev >= 7) {
		err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_POWER_OFF_NOTIFICATION,
				EXT_CSD_SLEEP_NOTIFICATION,
				sn_timeout_ms, true, false, false);
		if (err)
			pr_err("%s: Sleep Notification timed out %u\n",
				mmc_hostname(card->host), sn_timeout_ms);
	}

	err = mmc_deselect_cards(host);
	if (err)
		goto out_release;

	cmd.opcode = MMC_SLEEP_AWAKE;
	cmd.arg = card->rca << 16;
	cmd.arg |= 1 << 15;

	if (host->max_busy_timeout && (timeout_ms > host->max_busy_timeout)) {
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
		cmd.busy_timeout = timeout_ms;
	}

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		goto out_release;

	if (!cmd.busy_timeout || !(host->caps & MMC_CAP_WAIT_WHILE_BUSY))
		mmc_delay(timeout_ms);

out_release:
	mmc_retune_release(host);
	return err;
}

static int mmc_awake(struct mmc_host *host)
{
	struct mmc_command cmd = {0};
	struct mmc_card *card = host->card;
	int err;

	cmd.opcode = MMC_SLEEP_AWAKE;
	cmd.arg = card->rca << 16;

	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		return err;

	err = mmc_select_card(host->card);

	return err;

}

static int mmc_can_poweroff_notify(const struct mmc_card *card)
{
	return card &&
		mmc_card_mmc(card) &&
		(card->ext_csd.power_off_notification == EXT_CSD_POWER_ON);
}

static int mmc_poweroff_notify(struct mmc_card *card, unsigned int notify_type)
{
	unsigned int timeout = card->ext_csd.generic_cmd6_time;
	int err;

	
	if (notify_type == EXT_CSD_POWER_OFF_LONG)
		timeout = card->ext_csd.power_off_longtime;

	err = __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_POWER_OFF_NOTIFICATION,
			notify_type, timeout, true, false, false);
	if (err)
		pr_err("%s: Power Off Notification timed out, %u\n",
		       mmc_hostname(card->host), timeout);

	
	card->ext_csd.power_off_notification = EXT_CSD_NO_POWER_NOTIFICATION;

	return err;
}

static void mmc_remove(struct mmc_host *host)
{
	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_remove_card(host->card);
	host->card = NULL;
}

static int mmc_alive(struct mmc_host *host)
{
	return mmc_send_status(host->card, NULL);
}

static void mmc_detect(struct mmc_host *host)
{
	int err;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_get_card(host->card);

	err = _mmc_detect_card_removed(host);

	mmc_put_card(host->card);

	if (err) {
		mmc_remove(host);

		mmc_claim_host(host);
		mmc_detach_bus(host);
		mmc_power_off(host);
		mmc_release_host(host);
	}
}

static int _mmc_suspend(struct mmc_host *host, bool is_suspend)
{
	int err = 0;
	unsigned int notify_type = is_suspend ? EXT_CSD_POWER_OFF_SHORT :
					EXT_CSD_POWER_OFF_LONG;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_claim_host(host);

	if (mmc_card_suspended(host->card))
		goto out;

	if (mmc_card_doing_bkops(host->card)) {
		err = mmc_stop_bkops(host->card);
		if (err)
			goto out;
#ifdef MTK_BKOPS_IDLE_MAYA
		MMC_UPDATE_BKOPS_STATS_SUSPEND(host->card->bkops_info.bkops_stats);
#endif
	}

	if (host->card->ext_csd.rev < 7)
		err = mmc_cache_ctrl(host, 0);
	else
		err = mmc_flush_cache(host->card);
	if (err)
		goto out;

	if (mmc_can_poweroff_notify(host->card) &&
		((host->caps2 & MMC_CAP2_FULL_PWR_CYCLE) || !is_suspend))
		err = mmc_poweroff_notify(host->card, notify_type);
	else if (mmc_can_sleep(host->card)) {
		err = mmc_sleep(host);
		if (!err && mmc_card_keep_power(host))
			mmc_card_set_sleep(host->card);
	} else if (!mmc_host_is_spi(host))
		err = mmc_deselect_cards(host);

	if (!err) {
		if (!mmc_card_keep_power(host))
			mmc_power_off(host);
		mmc_card_set_suspended(host->card);
	}
out:
	mmc_release_host(host);
	return err;
}

static int mmc_suspend(struct mmc_host *host)
{
	int err;

	err = _mmc_suspend(host, true);
	if (!err) {
		pm_runtime_disable(&host->card->dev);
		pm_runtime_set_suspended(&host->card->dev);
	}
	msdc_get_gpio_setting(mmc_priv(host), 1);

	return err;
}

static int _mmc_resume(struct mmc_host *host)
{
	int err = 0;

	BUG_ON(!host);
	BUG_ON(!host->card);

	mmc_claim_host(host);

	if (!mmc_card_suspended(host->card))
		goto out;

	if (!mmc_card_keep_power(host))
		mmc_power_up(host, host->card->ocr);

	if (mmc_card_is_sleep(host->card) && mmc_can_sleep(host->card)) {
		err = mmc_awake(host);
		if (err)
			return err;
		mmc_card_clr_sleep(host->card);
	} else
		err = mmc_init_card(host, host->card->ocr, host->card);
	mmc_card_clr_suspended(host->card);

	if (host->card->ext_csd.rev < 7)
		err = mmc_cache_ctrl(host, 1);

out:
	mmc_release_host(host);
	return err;
}

static int mmc_shutdown(struct mmc_host *host)
{
	int err = 0;

	if (mmc_can_poweroff_notify(host->card) &&
		!(host->caps2 & MMC_CAP2_FULL_PWR_CYCLE))
		err = _mmc_resume(host);

	if (!err)
		err = _mmc_suspend(host, false);

	return err;
}

static int mmc_resume(struct mmc_host *host)
{
	int err = 0;

	if (!(host->caps & MMC_CAP_RUNTIME_RESUME)) {
		err = _mmc_resume(host);
		pm_runtime_set_active(&host->card->dev);
		pm_runtime_mark_last_busy(&host->card->dev);
	}
	pm_runtime_enable(&host->card->dev);
	msdc_get_gpio_setting(mmc_priv(host), 0);

	return err;
}

static int mmc_runtime_suspend(struct mmc_host *host)
{
	int err;

	if (!(host->caps & MMC_CAP_AGGRESSIVE_PM))
		return 0;

	err = _mmc_suspend(host, true);
	if (err)
		pr_err("%s: error %d doing aggessive suspend\n",
			mmc_hostname(host), err);

	return err;
}

static int mmc_runtime_resume(struct mmc_host *host)
{
	int err;

	if (!(host->caps & (MMC_CAP_AGGRESSIVE_PM | MMC_CAP_RUNTIME_RESUME)))
		return 0;

	err = _mmc_resume(host);
	if (err)
		pr_err("%s: error %d doing aggessive resume\n",
			mmc_hostname(host), err);

	return 0;
}

int mmc_can_reset(struct mmc_card *card)
{
	u8 rst_n_function;

	rst_n_function = card->ext_csd.rst_n_function;
	if ((rst_n_function & EXT_CSD_RST_N_EN_MASK) != EXT_CSD_RST_N_ENABLED)
		return 0;
	return 1;
}
EXPORT_SYMBOL(mmc_can_reset);

static int mmc_reset(struct mmc_host *host)
{
	struct mmc_card *card = host->card;

	if (!(host->caps & MMC_CAP_HW_RESET) || !host->ops->hw_reset)
		return -EOPNOTSUPP;

	if (!mmc_can_reset(card))
		return -EOPNOTSUPP;

	mmc_host_clk_hold(host);
	mmc_set_clock(host, host->f_init);

	host->ops->hw_reset(host);

	
	mmc_set_initial_state(host);
	mmc_host_clk_release(host);

	return mmc_init_card(host, card->ocr, card);
}

static const struct mmc_bus_ops mmc_ops = {
	.remove = mmc_remove,
	.detect = mmc_detect,
	.suspend = mmc_suspend,
	.resume = mmc_resume,
	.runtime_suspend = mmc_runtime_suspend,
	.runtime_resume = mmc_runtime_resume,
	.alive = mmc_alive,
	.shutdown = mmc_shutdown,
	.reset = mmc_reset,
};

int mmc_attach_mmc(struct mmc_host *host)
{
	int err;
	u32 ocr, rocr;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int i;
#endif

	BUG_ON(!host);
	WARN_ON(!host->claimed);

	
	if (!mmc_host_is_spi(host))
		mmc_set_bus_mode(host, MMC_BUSMODE_OPENDRAIN);

	err = mmc_send_op_cond(host, 0, &ocr);
	if (err)
		return err;

	mmc_attach_bus(host, &mmc_ops);
	if (host->ocr_avail_mmc)
		host->ocr_avail = host->ocr_avail_mmc;

	if (mmc_host_is_spi(host)) {
		err = mmc_spi_read_ocr(host, 1, &ocr);
		if (err)
			goto err;
	}

	rocr = mmc_select_voltage(host, ocr);

	if (!rocr) {
		err = -EINVAL;
		goto err;
	}

	err = mmc_init_card(host, rocr, NULL);
	if (err)
		goto err;

#ifdef MTK_BKOPS_IDLE_MAYA
	if (host->card->ext_csd.bkops_en) {
		INIT_DELAYED_WORK(&host->card->bkops_info.dw,
			mmc_start_idle_time_bkops);
		host->card->bkops_info.delay_ms = MMC_IDLE_BKOPS_TIME_MS;
		if (host->card->bkops_info.host_delay_ms)
			host->card->bkops_info.delay_ms =
				host->card->bkops_info.host_delay_ms;
	}
#endif
	mmc_release_host(host);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	pr_debug("[MSDC_EMMC_CQ] init polling status threading\n");
	atomic_set(&host->cq_rw, false);
	atomic_set(&host->cq_w, false);
	atomic_set(&host->cq_wait_rdy, 0);
	host->wp_error = 0;
	host->task_id_index = 0;
	host->is_data_dma = 0;
	host->cur_rw_task = 99;
	host->cmdq_support_changed = 1;
	atomic_set(&host->cq_tuning_now, 0);
#ifdef CONFIG_MMC_FFU
	atomic_set(&host->stop_queue, 0);
#endif

	for (i = 0; i < 32; i++)
		host->data_mrq_queued[i] = false;

	host->cmdq_thread = kthread_run(mmc_run_queue_thread, host, "exe_cq");

#endif
	err = mmc_add_card(host->card);
	mmc_claim_host(host);
	if (err)
		goto remove_card;

	return 0;

remove_card:
	mmc_release_host(host);
	mmc_remove_card(host->card);
	mmc_claim_host(host);
	host->card = NULL;
err:
	mmc_detach_bus(host);

	pr_err("%s: error %d whilst initialising MMC card\n",
		mmc_hostname(host), err);

	return err;
}
