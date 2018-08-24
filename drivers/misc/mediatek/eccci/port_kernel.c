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

#include <linux/device.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtc.h>
#include <linux/kernel.h>
#include <mt-plat/mt_boot_common.h>

#include <linux/fs.h>
#include <linux/random.h>
#include <linux/timer.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
#include <linux/irq.h>
#endif
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
#include <sec_export.h>
#endif
#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#else
#define DB_OPT_DEFAULT    (0)	
#define DB_OPT_FTRACE   (0)	
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

#include <mt-plat/mt_gpio.h>
#include <mt-plat/mt_gpio_core.h>
#ifdef FEATURE_RF_CLK_BUF
#include <linux/gpio.h>
#include <mt_clkbuf_ctl.h>
#endif
#ifdef FEATURE_GET_MD_GPIO_VAL
#include <linux/gpio.h>
#endif
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_platform.h"
#include "port_kernel.h"

#if defined(ENABLE_32K_CLK_LESS)
#include <mt-plat/mtk_rtc.h>
#endif

#ifdef CONFIG_HTC_FEATURES_RIL_PCN0006_DRDI
#include <linux/htc_devices_dtb.h>
#endif

#include "modem_cldma.h"

static void ccci_ee_info_dump(struct ccci_modem *md);
static void ccci_aed(struct ccci_modem *md, unsigned int dump_flag, char *aed_str, int db_opt);
static void status_msg_handler(struct ccci_port *port, struct ccci_request *req);

#define MAX_QUEUE_LENGTH 16
#define EX_TIMER_MD_HANG 5

#if defined(FEATURE_GET_MD_GPIO_NUM)
static struct gpio_item gpio_mapping_table[] = {
	{"GPIO_FDD_Band_Support_Detection_1", "GPIO_FDD_BAND_SUPPORT_DETECT_1ST_PIN",},
	{"GPIO_FDD_Band_Support_Detection_2", "GPIO_FDD_BAND_SUPPORT_DETECT_2ND_PIN",},
	{"GPIO_FDD_Band_Support_Detection_3", "GPIO_FDD_BAND_SUPPORT_DETECT_3RD_PIN",},
	{"GPIO_FDD_Band_Support_Detection_4", "GPIO_FDD_BAND_SUPPORT_DETECT_4TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_5", "GPIO_FDD_BAND_SUPPORT_DETECT_5TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_6", "GPIO_FDD_BAND_SUPPORT_DETECT_6TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_7", "GPIO_FDD_BAND_SUPPORT_DETECT_7TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_8", "GPIO_FDD_BAND_SUPPORT_DETECT_8TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_9", "GPIO_FDD_BAND_SUPPORT_DETECT_9TH_PIN",},
	{"GPIO_FDD_Band_Support_Detection_A", "GPIO_FDD_BAND_SUPPORT_DETECT_ATH_PIN",},
};
#endif

#ifdef CONFIG_MTK_ECCCI_C2K
int modem_dtr_set(int on, int low_latency)
{
	struct ccci_modem *md = NULL;
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int ret = 0;

	
	md = ccci_get_modem_by_id(MD_SYS3);

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_IND_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_IND_MSG & 0xFF;
	c2k_ctl_msg.option = 0;
	if (on)
		c2k_ctl_msg.option |= 0x04;
	else
		c2k_ctl_msg.option &= 0xFB;

	CCCI_NORMAL_LOG(md->index, KERN, "usb bypass dtr set(%d)(0x%x)\n", on, (u32) (*((u32 *)&c2k_ctl_msg)));
	ccci_send_msg_to_md(md, CCCI_CONTROL_TX, C2K_STATUS_IND_MSG, (u32) (*((u32 *)&c2k_ctl_msg)), 1);

	return ret;
}

int modem_dcd_state(void)
{
	struct ccci_modem *md = NULL;
	struct c2k_ctrl_port_msg c2k_ctl_msg;
	int dcd_state = 0;
	int ret = 0;

	
	md = ccci_get_modem_by_id(MD_SYS3);

	c2k_ctl_msg.chan_num = DATA_PPP_CH_C2K;
	c2k_ctl_msg.id_hi = (C2K_STATUS_QUERY_MSG & 0xFF00) >> 8;
	c2k_ctl_msg.id_low = C2K_STATUS_QUERY_MSG & 0xFF;
	c2k_ctl_msg.option = 0;

	CCCI_NORMAL_LOG(md->index, KERN, "usb bypass query state(0x%x)\n", (u32) (*((u32 *)&c2k_ctl_msg)));
	ret = ccci_send_msg_to_md(md, CCCI_CONTROL_TX, C2K_STATUS_QUERY_MSG, (u32) (*((u32 *)&c2k_ctl_msg)), 1);
	if (ret == -CCCI_ERR_MD_NOT_READY)
		dcd_state = 0;
	else {
		msleep(20);
		dcd_state = md->dtr_state;
	}
	return dcd_state;
}
#endif

static inline void append_runtime_feature(char **p_rt_data, struct ccci_runtime_feature *rt_feature, void *data)
{
	CCCI_DEBUG_LOG(-1, KERN, "append rt_data %p, feature %u len %u\n",
		     *p_rt_data, rt_feature->feature_id, rt_feature->data_len);
	memcpy_toio(*p_rt_data, rt_feature, sizeof(struct ccci_runtime_feature));
	*p_rt_data += sizeof(struct ccci_runtime_feature);
	if (data != NULL) {
		memcpy_toio(*p_rt_data, data, rt_feature->data_len);
		*p_rt_data += rt_feature->data_len;
	}
}

static unsigned int get_booting_start_id(struct ccci_modem *md)
{
	LOGGING_MODE mdlog_flag = MODE_IDLE;
	u32 booting_start_id;

	mdlog_flag = md->mdlg_mode;

	if (is_meta_mode() || is_advanced_meta_mode())
		booting_start_id = ((char)mdlog_flag << 8 | META_BOOT_ID);
	else if ((get_boot_mode() == FACTORY_BOOT || get_boot_mode() == ATE_FACTORY_BOOT))
		booting_start_id = ((char)mdlog_flag << 8 | FACTORY_BOOT_ID);
	else
		booting_start_id = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);

	CCCI_BOOTUP_LOG(md->index, KERN, "get_booting_start_id 0x%x\n", booting_start_id);
	return booting_start_id;
}

static void config_ap_side_feature(struct ccci_modem *md, struct md_query_ap_feature *ap_side_md_feature)
{

	md->runtime_version = AP_MD_HS_V2;
	ap_side_md_feature->feature_set[BOOT_INFO].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[EXCEPTION_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	if (md->index == MD_SYS1)
		ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	else
		ap_side_md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;

#ifdef FEATURE_SCP_CCCI_SUPPORT
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCISM_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef FEATURE_DHL_CCB_RAW_SUPPORT
	
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[CCB_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef FEATURE_DIRECT_TETHERING_LOGGING
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[DT_NETD_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	ap_side_md_feature->feature_set[DT_USB_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

#ifdef SMART_LOGGING_SUPPORT
	ap_side_md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask = CCCI_FEATURE_OPTIONAL_SUPPORT;
#else
#ifdef FEATURE_SMART_LOGGING
	ap_side_md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#else
	ap_side_md_feature->feature_set[SMART_LOGGING_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
#endif

#ifdef FEATURE_MD1MD3_SHARE_MEM
	ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MD1MD3_SHARE_MEMORY].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif

	ap_side_md_feature->feature_set[MISC_INFO_HIF_DMA_REMAP].support_mask = CCCI_FEATURE_MUST_SUPPORT;

#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
	ap_side_md_feature->feature_set[MULTI_MD_MPU].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#endif

#if defined(ENABLE_32K_CLK_LESS)
	if (crystal_exist_status()) {
		CCCI_DEBUG_LOG(md->index, KERN, "MISC_32K_LESS no support, crystal_exist_status 1\n");
		ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask = CCCI_FEATURE_NOT_SUPPORT;
	} else {
		CCCI_DEBUG_LOG(md->index, KERN, "MISC_32K_LESS support\n");
		ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	}
#else
	CCCI_DEBUG_LOG(md->index, KERN, "ENABLE_32K_CLK_LESS disabled\n");
	ap_side_md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[MISC_INFO_RANDOM_SEED_NUM].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_GPS_COCLOCK].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_SBP_ID].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	ap_side_md_feature->feature_set[MISC_INFO_CCCI].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#ifdef FEATURE_MD_GET_CLIB_TIME
	ap_side_md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
#ifdef FEATURE_C2K_ALWAYS_ON
	ap_side_md_feature->feature_set[MISC_INFO_C2K].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#else
	ap_side_md_feature->feature_set[MISC_INFO_C2K].support_mask = CCCI_FEATURE_NOT_SUPPORT;
#endif
	ap_side_md_feature->feature_set[MD_IMAGE_START_MEMORY].support_mask = CCCI_FEATURE_OPTIONAL_SUPPORT;

	ap_side_md_feature->feature_set[CCMNI_MTU].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
#ifdef FEATURE_LWA
	ap_side_md_feature->feature_set[LWA_SHARE_MEMORY].support_mask = CCCI_FEATURE_MUST_SUPPORT;
#endif
#endif
}

#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
unsigned int align_to_2_power(unsigned int n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;

	return n;
}
#endif

static int prepare_runtime_data(struct ccci_modem *md, struct ccci_request *req)
{
	u8 i = 0;
	u32 total_len;
	
	char *rt_data = (char *)md->smem_layout.ccci_rt_smem_base_vir;

	struct ccci_runtime_feature rt_feature;
	
	struct ccci_runtime_share_memory rt_shm;
	struct ccci_misc_info_element rt_f_element;

	struct md_query_ap_feature *md_feature;
	struct md_query_ap_feature md_feature_ap;
	struct ccci_runtime_boot_info boot_info;
	unsigned int random_seed = 0;
	struct timeval t;

	CCCI_BOOTUP_LOG(md->index, KERN, "prepare_runtime_data  AP total %u features\n", RUNTIME_FEATURE_ID_MAX);

	memset(&md_feature_ap, 0, sizeof(struct md_query_ap_feature));
	config_ap_side_feature(md, &md_feature_ap);

	md_feature = (struct md_query_ap_feature *)skb_pull(req->skb, sizeof(struct ccci_header));

	if (md_feature->head_pattern != MD_FEATURE_QUERY_PATTERN ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_PATTERN) {
		CCCI_BOOTUP_LOG(md->index, KERN, "md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			     md_feature->head_pattern, md_feature->tail_pattern);
		if (md->index == MD_SYS3)
			md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		return -1;
	}

	for (i = BOOT_INFO; i < FEATURE_COUNT; i++) {
		memset(&rt_feature, 0, sizeof(struct ccci_runtime_feature));
		memset(&rt_shm, 0, sizeof(struct ccci_runtime_share_memory));
		memset(&rt_f_element, 0, sizeof(struct ccci_misc_info_element));
		rt_feature.feature_id = i;
		if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_MUST_SUPPORT &&
		    md_feature_ap.feature_set[i].support_mask < CCCI_FEATURE_MUST_SUPPORT) {
			CCCI_BOOTUP_LOG(md->index, KERN, "feature %u not support for AP\n", rt_feature.feature_id);
			return -1;
		}

		CCCI_BOOTUP_DUMP_LOG(md->index, KERN, "ftr %u mask %u, ver %u\n",
				rt_feature.feature_id, md_feature->feature_set[i].support_mask,
				md_feature->feature_set[i].version);

		if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_NOT_EXIST) {
			rt_feature.support_info = md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.support_info = md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_OPTIONAL_SUPPORT) {
			if (md_feature->feature_set[i].version == md_feature_ap.feature_set[i].version &&
			    md_feature_ap.feature_set[i].support_mask >= CCCI_FEATURE_MUST_SUPPORT) {
				rt_feature.support_info.support_mask = CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask = CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			}
		} else if (md_feature->feature_set[i].support_mask == CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (md_feature->feature_set[i].version >= md_feature_ap.feature_set[i].version) {
				rt_feature.support_info.support_mask = CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask = CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version = md_feature_ap.feature_set[i].version;
			}
		}

		if (rt_feature.support_info.support_mask == CCCI_FEATURE_MUST_SUPPORT) {
			switch (rt_feature.feature_id) {
			case BOOT_INFO:
				memset(&boot_info, 0, sizeof(boot_info));
				rt_feature.data_len = sizeof(boot_info);
				boot_info.boot_channel = CCCI_CONTROL_RX;
				boot_info.booting_start_id = get_booting_start_id(md);
				append_runtime_feature(&rt_data, &rt_feature, &boot_info);
				break;

			case EXCEPTION_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_exp_smem_base_phy -
				    md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_exp_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCIF_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_ccif_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_ccif_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_ccism_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_ccism_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCB_SHARE_MEMORY:
				
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_ccb_dhl_base_phy -
					md->mem_layout.smem_offset_AP_to_MD + 4; 
				rt_shm.size = md->smem_layout.ccci_ccb_dhl_size - 4;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
			case MULTI_MD_MPU:
				CCCI_BOOTUP_LOG(md->index, KERN, "new version md use multi-MPU.\n");
				md->multi_md_mpu_support = 1;
				rt_feature.data_len = 0;
				append_runtime_feature(&rt_data, &rt_feature, NULL);
				break;
#endif
			case DHL_RAW_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_raw_dhl_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_raw_dhl_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case DT_NETD_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_dt_netd_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_dt_netd_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case DT_USB_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_dt_usb_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_dt_usb_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case SMART_LOGGING_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_smart_logging_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_smart_logging_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case MD1MD3_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->mem_layout.md1_md3_smem_phy - md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->mem_layout.md1_md3_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;

			case MISC_INFO_HIF_DMA_REMAP:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RTC_32K_LESS:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RANDOM_SEED_NUM:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				get_random_bytes(&random_seed, sizeof(int));
				rt_f_element.feature[0] = random_seed;
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_GPS_COCLOCK:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_SBP_ID:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				rt_f_element.feature[0] = md->sbp_code;
				if (md->config.load_type < modem_ultg)
					rt_f_element.feature[1] = 0;
				else
					rt_f_element.feature[1] = get_md_wm_id_map(md->config.load_type);
				CCCI_BOOTUP_LOG(md->index, KERN, "wmid[%d]\n", rt_f_element.feature[1]);
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CCCI:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
#ifdef FEATURE_SEQ_CHECK_EN
				rt_f_element.feature[0] |= (1 << 0);
#endif
#ifdef FEATURE_POLL_MD_EN
				rt_f_element.feature[0] |= (1 << 1);
#endif
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CLIB_TIME:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
				do_gettimeofday(&t);

				
				rt_f_element.feature[0] = ((unsigned int *)&t.tv_sec)[0];
				rt_f_element.feature[1] = ((unsigned int *)&t.tv_sec)[1];
				
				rt_f_element.feature[2] = current_time_zone;
				
				rt_f_element.feature[3] = sys_tz.tz_dsttime;
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MISC_INFO_C2K:
				rt_feature.data_len = sizeof(struct ccci_misc_info_element);
#ifdef FEATURE_C2K_ALWAYS_ON
				rt_f_element.feature[0] = (0
#ifdef CONFIG_MTK_C2K_SUPPORT
							   | (1 << 0)
#endif
#ifdef CONFIG_MTK_SVLTE_SUPPORT
							   | (1 << 1)
#endif
#ifdef CONFIG_MTK_SRLTE_SUPPORT
							   | (1 << 2)
#endif
#ifdef CONFIG_MTK_C2K_OM_SOLUTION1
							   | (1 << 3)
#endif
#ifdef CONFIG_CT6M_SUPPORT
							   | (1 << 4)
#endif
				    );
#endif
				append_runtime_feature(&rt_data, &rt_feature, &rt_f_element);
				break;
			case MD_IMAGE_START_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->img_info[IMG_MD].address;
				rt_shm.size = md->img_info[IMG_MD].size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
			case CCMNI_MTU:
				rt_feature.data_len = sizeof(unsigned int);
				random_seed = NET_RX_BUF - sizeof(struct ccci_header);
				append_runtime_feature(&rt_data, &rt_feature, &random_seed);
				break;
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
			case LWA_SHARE_MEMORY:
				rt_feature.data_len = sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = md->smem_layout.ccci_lwa_smem_base_phy -
					md->mem_layout.smem_offset_AP_to_MD;
				rt_shm.size = md->smem_layout.ccci_lwa_smem_size;
				append_runtime_feature(&rt_data, &rt_feature, &rt_shm);
				break;
#endif
			default:
				break;
			};
		} else {
			rt_feature.data_len = 0;
			append_runtime_feature(&rt_data, &rt_feature, NULL);
		}

	}

	total_len = rt_data - (char *)md->smem_layout.ccci_rt_smem_base_vir;
	ccci_util_mem_dump(md->index, CCCI_DUMP_BOOTUP, md->smem_layout.ccci_rt_smem_base_vir, total_len);

	return 0;
}

static void control_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_modem *md = port->modem;
	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
	unsigned long flags;
	struct c2k_ctrl_port_msg *c2k_ctl_msg = NULL;
	char need_update_state = 0;

	CCCI_NORMAL_LOG(md->index, KERN, "control message 0x%X,0x%X\n", ccci_h->data[1], ccci_h->reserved);
	ccci_event_log("md%d: control message 0x%X,0x%X\n", md->index, ccci_h->data[1], ccci_h->reserved);
	if (ccci_h->data[1] == MD_INIT_START_BOOT
	    && ccci_h->reserved == MD_INIT_CHK_ID && md->boot_stage == MD_BOOT_STAGE_0) {
		del_timer(&md->bootup_timer);
		ccci_update_md_boot_stage(md, MD_BOOT_STAGE_1);
		md->flight_mode = MD_FIGHT_MODE_NONE; 
		if (req->skb->len == sizeof(struct md_query_ap_feature) + sizeof(struct ccci_header))
			prepare_runtime_data(md, req);
		else if (req->skb->len == sizeof(struct ccci_header))
			CCCI_NORMAL_LOG(md->index, KERN, "get old handshake message\n");
		else
			CCCI_ERROR_LOG(md->index, KERN, "get invalid MD_QUERY_MSG, skb->len =%u\n", req->skb->len);

#ifdef SET_EMI_STEP_BY_STAGE
		ccci_set_mem_access_protection_second_stage(md);
#endif
		ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_BOOT_UP, 0);
		md->ops->dump_info(md, DUMP_MD_BOOTUP_STATUS, NULL, 0);
	} else if (ccci_h->data[1] == MD_NORMAL_BOOT && md->boot_stage == MD_BOOT_STAGE_1) {
		del_timer(&md->bootup_timer);
		wake_lock_timeout(&md->md_wake_lock, 10 * HZ); 
		ccci_update_md_boot_stage(md, MD_BOOT_STAGE_2);
		md->ops->broadcast_state(md, READY);
		ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_BOOT_READY, 0);
	} else if (ccci_h->data[1] == MD_EX) {
		if (unlikely(ccci_h->reserved != MD_EX_CHK_ID)) {
			CCCI_ERROR_LOG(md->index, KERN, "receive invalid MD_EX\n");
		} else {
			spin_lock_irqsave(&md->ctrl_lock, flags);
			md->ee_info_flag |= ((1 << MD_EE_FLOW_START) | (1 << MD_EE_MSG_GET) | (1 << MD_STATE_UPDATE) |
					     (1 << MD_EE_TIME_OUT_SET));
			md->config.setting |= MD_SETTING_RELOAD;
			spin_unlock_irqrestore(&md->ctrl_lock, flags);
			del_timer(&md->bootup_timer);
			if (!MD_IN_DEBUG(md))
				mod_timer(&md->ex_monitor, jiffies + EX_TIMER_MD_EX * HZ);
			md->ops->broadcast_state(md, EXCEPTION);
			ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_EXCEPTION, 0);
			ccci_send_msg_to_md(md, CCCI_CONTROL_TX, MD_EX, MD_EX_CHK_ID, 1);
		}
	} else if (ccci_h->data[1] == MD_EX_REC_OK) {
		if (unlikely
		    (ccci_h->reserved != MD_EX_REC_OK_CHK_ID
		     || req->skb->len < (sizeof(struct ccci_header) + sizeof(EX_LOG_T)))) {
			CCCI_ERROR_LOG(md->index, KERN, "receive invalid MD_EX_REC_OK, resv=%x, len=%d\n",
				     ccci_h->reserved, req->skb->len);
		} else {
			spin_lock_irqsave(&md->ctrl_lock, flags);
			md->ee_info_flag |= ((1 << MD_EE_FLOW_START) | (1 << MD_EE_OK_MSG_GET));
			if ((md->ee_info_flag & (1 << MD_STATE_UPDATE)) == 0) {
				md->ee_info_flag |= (1 << MD_STATE_UPDATE);
				md->ee_info_flag &= ~(1 << MD_EE_TIME_OUT_SET);
				md->config.setting |= MD_SETTING_RELOAD;
				need_update_state = 1;
			}
			spin_unlock_irqrestore(&md->ctrl_lock, flags);
			if (need_update_state) {
				CCCI_ERROR_LOG(md->index, KERN, "get MD_EX_REC_OK without exception MD_EX\n");
				del_timer(&md->bootup_timer);
				md->ops->broadcast_state(md, EXCEPTION);
			}
			
#ifdef MD_UMOLY_EE_SUPPORT
			if (md->index == MD_SYS1)
				memcpy(md->ex_pl_info, skb_pull(req->skb, sizeof(struct ccci_header)),
							MD_HS1_FAIL_DUMP_SIZE);
			else
#endif
				memcpy(&md->ex_info, skb_pull(req->skb, sizeof(struct ccci_header)), sizeof(EX_LOG_T));

			mod_timer(&md->ex_monitor, jiffies);
		}
	} else if (ccci_h->data[1] == MD_EX_PASS) {
		spin_lock_irqsave(&md->ctrl_lock, flags);
		md->ee_info_flag |= (1 << MD_EE_PASS_MSG_GET);
		
		mod_timer(&md->ex_monitor2, jiffies);
		spin_unlock_irqrestore(&md->ctrl_lock, flags);
	} else if (ccci_h->data[1] == MD_INIT_START_BOOT
		   && ccci_h->reserved == MD_INIT_CHK_ID && !(md->config.setting & MD_SETTING_FIRST_BOOT)) {
		ccci_update_md_boot_stage(md, MD_BOOT_STAGE_0);
		CCCI_ERROR_LOG(md->index, KERN, "MD second bootup detected!\n");
		ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET, 0);
	} else if (ccci_h->data[1] == MD_EX_RESUME) {
		memset(&md->debug_info, 0, sizeof(md->debug_info));
		md->debug_info.type = MD_EX_TYPE_EMI_CHECK;
		md->debug_info.name = "EMI_CHK";
		md->debug_info.data = *(ccci_msg_t *) ccci_h;
		md->ex_type = MD_EX_TYPE_EMI_CHECK;
		ccci_ee_info_dump(md);
	} else if (ccci_h->data[1] == CCCI_DRV_VER_ERROR) {
		CCCI_ERROR_LOG(md->index, KERN, "AP CCCI driver version mis-match to MD!!\n");
		md->config.setting |= MD_SETTING_STOP_RETRY_BOOT;
		ccci_aed(md, 0, "AP/MD driver version mis-match\n", DB_OPT_DEFAULT);
	} else if (md->index == MD_SYS3 && ccci_h->data[1] == C2K_HB_MSG) {
		status_msg_handler(port, req);
		CCCI_NORMAL_LOG(md->index, KERN, "heart beat msg received\n");
		return;
	} else if (ccci_h->data[1] == C2K_STATUS_IND_MSG && md->index == MD_SYS3) {
		c2k_ctl_msg = (struct c2k_ctrl_port_msg *)&ccci_h->reserved;
		CCCI_NORMAL_LOG(md->index, KERN, "c2k status ind 0x%02x\n", c2k_ctl_msg->option);
		if (c2k_ctl_msg->option & 0x80)	
			md->dtr_state = 1;
		else		
			md->dtr_state = 0;
	} else if (ccci_h->data[1] == C2K_STATUS_QUERY_MSG && md->index == MD_SYS3) {
		c2k_ctl_msg = (struct c2k_ctrl_port_msg *)&ccci_h->reserved;
		CCCI_NORMAL_LOG(md->index, KERN, "c2k status query 0x%02x\n", c2k_ctl_msg->option);
		if (c2k_ctl_msg->option & 0x80)	
			md->dtr_state = 1;
		else		
			md->dtr_state = 0;
	} else if (ccci_h->data[1] == C2K_CCISM_SHM_INIT_ACK && md->index == MD_SYS3) {
		ccci_update_md_boot_stage(md, MD_ACK_SCP_INIT);
	} else {
		CCCI_ERROR_LOG(md->index, KERN, "receive unknown data from CCCI_CONTROL_RX = %d\n", ccci_h->data[1]);
	}
	req->policy = RECYCLE;
	ccci_free_req(req);
}

#ifdef TEST_MESSAGE_FOR_BRINGUP
int ccci_notify_md_by_sys_msg(int md_id, unsigned int msg, unsigned int data)
{
	int ret = 0;
	struct ccci_modem *md;
	int idx = 0;		

	md = ccci_get_modem_by_id(idx);

	
	if (md == NULL)
		return 0;

	return ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, msg, data, 1);
}

int ccci_sysmsg_echo_test(int md_id, int data)
{
	CCCI_DEBUG_LOG(md_id, KERN, "system message: Enter ccci_sysmsg_echo_test data= %08x", data);
	ccci_notify_md_by_sys_msg(md_id, TEST_MSG_ID_AP2MD, data);
	return 0;
}
EXPORT_SYMBOL(ccci_sysmsg_echo_test);

int ccci_sysmsg_echo_test_l1core(int md_id, int data)
{
	CCCI_DEBUG_LOG(md_id, KERN, "system message: Enter ccci_sysmsg_echo_test_l1core data= %08x", data);
	ccci_notify_md_by_sys_msg(md_id, TEST_MSG_ID_L1CORE_AP2MD, data);
	return 0;
}
EXPORT_SYMBOL(ccci_sysmsg_echo_test_l1core);

#endif
ccci_sys_cb_func_info_t ccci_sys_cb_table_100[MAX_MD_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t ccci_sys_cb_table_1000[MAX_MD_NUM][MAX_KERN_API];
int register_ccci_sys_call_back(int md_id, unsigned int id, ccci_sys_cb_func_t func)
{
	int ret = 0;
	ccci_sys_cb_func_info_t *info_ptr;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, KERN, "register_sys_call_back fail: invalid md id\n");
		return -EINVAL;
	}

	if ((id >= 0x100) && ((id - 0x100) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_100[md_id][id - 0x100]);
	} else if ((id >= 0x1000) && ((id - 0x1000) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_1000[md_id][id - 0x1000]);
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "register_sys_call_back fail: invalid func id(0x%x)\n", id);
		return -EINVAL;
	}

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "register_sys_call_back fail: func(0x%x) registered!\n", id);
	}

	return ret;
}

void exec_ccci_sys_call_back(int md_id, int cb_id, int data)
{
	ccci_sys_cb_func_t func;
	int id;
	ccci_sys_cb_func_info_t *curr_table;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: invalid md id\n");
		return;
	}

	id = cb_id & 0xFF;
	if (id >= MAX_KERN_API) {
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	if ((cb_id & (0x1000 | 0x100)) == 0x1000) {
		curr_table = ccci_sys_cb_table_1000[md_id];
	} else if ((cb_id & (0x1000 | 0x100)) == 0x100) {
		curr_table = ccci_sys_cb_table_100[md_id];
	} else {
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	func = curr_table[id].func;
	if (func != NULL)
		func(md_id, data);
	else
		CCCI_ERROR_LOG(md_id, KERN, "exec_sys_cb fail: func id(0x%x) not register!\n", cb_id);
}

static void system_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_modem *md = port->modem;
	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;

	CCCI_NORMAL_LOG(md->index, KERN, "system message (%x %x %x %x)\n", ccci_h->data[0], ccci_h->data[1],
		     ccci_h->channel, ccci_h->reserved);
	switch (ccci_h->data[1]) {
	case MD_GET_BATTERY_INFO:
		ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_SEND_BATTERY_INFO, 0);
		break;
	case MD_WDT_MONITOR:
		
		break;
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
	case MD_RESET_AP:
		ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, 0xFAF51234, 0);
		break;
#endif
	case MD_SIM_TYPE:
		md->sim_type = ccci_h->reserved;
		CCCI_NORMAL_LOG(md->index, KERN, "MD send sys msg sim type(0x%x)\n", md->sim_type);
		break;
	case MD_TX_POWER:
	case MD_RF_TEMPERATURE:
	case MD_RF_TEMPERATURE_3G:
#if defined(CONFIG_MTK_SWITCH_TX_POWER)
	case MD_SW_MD1_TX_POWER_REQ:
	case MD_SW_MD2_TX_POWER_REQ:
#endif
#ifdef TEST_MESSAGE_FOR_BRINGUP
	
	case TEST_MSG_ID_MD2AP:
	
	case TEST_MSG_ID_AP2MD:
	
	case TEST_MSG_ID_L1CORE_MD2AP:
	
	case TEST_MSG_ID_L1CORE_AP2MD:
#endif
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
	case LWA_CONTROL_MSG:
#endif
		exec_ccci_sys_call_back(md->index, ccci_h->data[1], ccci_h->reserved);
		break;
	case CCISM_SHM_INIT_ACK:
		ccci_update_md_boot_stage(md, MD_ACK_SCP_INIT);
		break;
	};
	req->policy = RECYCLE;
	ccci_free_req(req);
}

static int get_md_gpio_val(unsigned int num)
{
#if defined(FEATURE_GET_MD_GPIO_VAL)
#if defined(CONFIG_MTK_LEGACY)
	return mt_get_gpio_in(num);
#else
	return __gpio_get_value(num);
#endif
#else
	return -1;
#endif
}

static int get_md_adc_val(unsigned int num)
{
#if defined(FEATURE_GET_MD_ADC_VAL)
	int data[4] = { 0, 0, 0, 0 };
	int val = 0;
	int ret = 0;

	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_ADC_VAL\n");
	ret = IMM_GetOneChannelValue(num, data, &val);
	if (ret == 0)
		return val;
	else
		return ret;
#elif defined(FEATURE_GET_MD_PMIC_ADC_VAL)
	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_PMIC_ADC_VAL\n");
	return PMIC_IMM_GetOneChannelValue(num, 1, 0);
#else
	return -1;
#endif
}

static int get_td_eint_info(char *eint_name, unsigned int len)
{
#if defined(FEATURE_GET_TD_EINT_NUM)
	return get_td_eint_num(eint_name, len);
#else
	return -1;
#endif
}

static int get_md_adc_info(char *adc_name, unsigned int len)
{
#if defined(FEATURE_GET_MD_ADC_NUM)
	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_ADC_NUM\n");
	return IMM_get_adc_channel_num(adc_name, len);
#elif defined(FEATURE_GET_MD_PMIC_ADC_NUM)
	CCCI_DEBUG_LOG(0, RPC, "FEATURE_GET_MD_PMIC_ADC_NUM\n");
	return PMIC_IMM_get_adc_channel_num(adc_name, len);
#else
	return -1;
#endif
}

static int get_md_gpio_info(char *gpio_name, unsigned int len)
{
#if defined(FEATURE_GET_MD_GPIO_NUM)
	int i = 0;
	struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,gpio_usage_mapping");
	int gpio_id = -1;

	if (!node) {
		CCCI_NORMAL_LOG(0, RPC, "MD_USE_GPIO is not set in device tree,need to check?\n");
		return gpio_id;
	}

	CCCI_BOOTUP_LOG(0, RPC, "looking for %s id, len %d\n", gpio_name, len);
	for (i = 0; i < ARRAY_SIZE(gpio_mapping_table); i++) {
		CCCI_DEBUG_LOG(0, RPC, "compare with %s\n", gpio_mapping_table[i].gpio_name_from_md);
		if (!strncmp(gpio_name, gpio_mapping_table[i].gpio_name_from_md, len)) {
			CCCI_BOOTUP_LOG(0, RPC, "searching %s in device tree\n",
							gpio_mapping_table[i].gpio_name_from_dts);
			of_property_read_u32(node, gpio_mapping_table[i].gpio_name_from_dts, &gpio_id);
			break;
		}
	}

	if (gpio_id < 0) {
		CCCI_BOOTUP_LOG(0, RPC, "try directly get id from device tree\n");
		of_property_read_u32(node, gpio_name, &gpio_id);
	}

	CCCI_BOOTUP_LOG(0, RPC, "%s id %d\n", gpio_name, gpio_id);
	return gpio_id;

#else
	return -1;
#endif
}

static int get_dram_type_clk(int *clk, int *type)
{
#if defined(FEATURE_GET_DRAM_TYPE_CLK)
	return get_dram_info(clk, type);
#else
	return -1;
#endif
}

#ifdef FEATURE_GET_MD_EINT_ATTR_DTS

static struct eint_struct md_eint_struct[] = {
	
	{SIM_HOT_PLUG_EINT_NUMBER, "interrupts", 0,},
	{SIM_HOT_PLUG_EINT_DEBOUNCETIME, "debounce", 1,},
	{SIM_HOT_PLUG_EINT_POLARITY, "interrupts", 1,},
	{SIM_HOT_PLUG_EINT_SENSITIVITY, "interrupts", 1,},
	{SIM_HOT_PLUG_EINT_SOCKETTYPE, "sockettype", 1,},
	{SIM_HOT_PLUG_EINT_DEDICATEDEN, "dedicated", 1,},
	{SIM_HOT_PLUG_EINT_SRCPIN, "src_pin", 1,},
	{SIM_HOT_PLUG_EINT_MAX, "invalid_type", 0xFF,},
};

static struct eint_node_name md_eint_node[] = {
	{"MD1_SIM1_HOT_PLUG_EINT", 1, 1,},
	{"MD1_SIM2_HOT_PLUG_EINT", 1, 2,},
	{"MD1_SIM3_HOT_PLUG_EINT", 1, 3,},
	{"MD1_SIM4_HOT_PLUG_EINT", 1, 4,},
	
	
	
	
	
	
	
	
	
	
	
	
	{NULL,},
};

struct eint_node_struct eint_node_prop = {
	0,
	md_eint_node,
	md_eint_struct,
};

static int get_eint_attr_val(struct device_node *node, int index)
{
	int value;
	int ret = 0, type;
	int covert_AP_to_MD_unit = 1000; 

	for (type = 0; type < SIM_HOT_PLUG_EINT_MAX; type++) {
		ret = of_property_read_u32_index(node, md_eint_struct[type].property,
			md_eint_struct[type].index, &value);
		if (!ret) {
			
			if (type == SIM_HOT_PLUG_EINT_POLARITY) {
				switch (value) {
				case IRQ_TYPE_EDGE_RISING:
				case IRQ_TYPE_EDGE_FALLING:
				case IRQ_TYPE_LEVEL_HIGH:
				case IRQ_TYPE_LEVEL_LOW:
					md_eint_struct[SIM_HOT_PLUG_EINT_POLARITY].value_sim[index] =
					    (value & 0x5) ? 1 : 0;
					
					md_eint_struct[SIM_HOT_PLUG_EINT_SENSITIVITY].value_sim[index] =
					    (value & 0x3) ? 1 : 0;
					
					break;
				default:	
					md_eint_struct[SIM_HOT_PLUG_EINT_POLARITY].value_sim[index] = -1;
					md_eint_struct[SIM_HOT_PLUG_EINT_SENSITIVITY].value_sim[index] = -1;
					CCCI_ERROR_LOG(-1, RPC, "invalid value, please check dtsi!\n");
					break;
				}
				type++;
			} else if (type == SIM_HOT_PLUG_EINT_DEBOUNCETIME) {
				
				md_eint_struct[type].value_sim[index] = value/covert_AP_to_MD_unit;
			} else {
				md_eint_struct[type].value_sim[index] = value;
			}
		} else {
			md_eint_struct[type].value_sim[index] = ERR_SIM_HOT_PLUG_QUERY_TYPE;
			CCCI_NORMAL_LOG(-1, RPC, "%s:  not found\n", md_eint_struct[type].property);
			return ERR_SIM_HOT_PLUG_QUERY_TYPE;
		}
	}
	return 0;
}

void get_dtsi_eint_node(void)
{
	int i;
	struct device_node *node;

	for (i = 0; i < MD_SIM_MAX; i++) {
		if (eint_node_prop.name[i].node_name != NULL) {
			if (strlen(eint_node_prop.name[i].node_name) > 0) {
				node = of_find_node_by_name(NULL, eint_node_prop.name[i].node_name);
				if (node != NULL) {
					eint_node_prop.ExistFlag |= (1 << i);
					get_eint_attr_val(node, i);
				} else {
					CCCI_INIT_LOG(-1, RPC, "%s: node %d no found\n",
						     eint_node_prop.name[i].node_name, i);
				}
			}
		} else {
			CCCI_INIT_LOG(-1, RPC, "node %d is NULL\n", i);
			break;
		}
	}
}

int get_eint_attr_DTSVal(char *name, unsigned int name_len, unsigned int type, char *result, unsigned int *len)
{
	int i, sim_value;
	int *sim_info = (int *)result;

	if ((name == NULL) || (result == NULL) || (len == NULL))
		return ERR_SIM_HOT_PLUG_NULL_POINTER;
	
	if (type >= SIM_HOT_PLUG_EINT_MAX)
		return ERR_SIM_HOT_PLUG_QUERY_TYPE;

	
	for (i = 0; i < MD_SIM_MAX; i++) {
		if (eint_node_prop.ExistFlag & (1 << i)) {
			if (!(strncmp(name, eint_node_prop.name[i].node_name, name_len))) {
				sim_value = eint_node_prop.eint_value[type].value_sim[i];
				*len = sizeof(sim_value);
				memcpy(sim_info, &sim_value, *len);
				CCCI_NORMAL_LOG(-1, RPC, "md_eint:%s, sizeof: %d, sim_info: %d, %d\n",
						eint_node_prop.eint_value[type].property,
						*len, *sim_info, eint_node_prop.eint_value[type].value_sim[i]);
				return 0;
			}
		}
	}
	return ERR_SIM_HOT_PLUG_QUERY_STRING;
}
#endif

static int get_eint_attr(char *name, unsigned int name_len, unsigned int type, char *result, unsigned int *len)
{
#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
	return get_eint_attr_DTSVal(name, name_len, type, result, len);
#else
#if defined(FEATURE_GET_MD_EINT_ATTR)
	return get_eint_attribute(name, name_len, type, result, len);
#else
	return -1;
#endif
#endif
}

static void ccci_rpc_get_gpio_adc(struct ccci_rpc_gpio_adc_intput *input, struct ccci_rpc_gpio_adc_output *output)
{
	int num;
	unsigned int val, i;

	if ((input->reqMask & (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) == (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) {
		for (i = 0; i < GPIO_MAX_COUNT; i++) {
			if (input->gpioValidPinMask & (1 << i)) {
				num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
				if (num >= 0) {
					output->gpioPinNum[i] = num;
					val = get_md_gpio_val(num);
					output->gpioPinValue[i] = val;
				}
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_GPIO_PIN) {
			for (i = 0; i < GPIO_MAX_COUNT; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
					if (num >= 0)
						output->gpioPinNum[i] = num;
				}
			}
		}
		if (input->reqMask & RPC_REQ_GPIO_VALUE) {
			for (i = 0; i < GPIO_MAX_COUNT; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					val = get_md_gpio_val(input->gpioPinNum[i]);
					output->gpioPinValue[i] = val;
				}
			}
		}
	}
	if ((input->reqMask & (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) == (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) {
		num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
		if (num >= 0) {
			output->adcChNum = num;
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(num);
				output->adcChMeasSum += val;
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_ADC_PIN) {
			num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
			if (num >= 0)
				output->adcChNum = num;
		}
		if (input->reqMask & RPC_REQ_ADC_VALUE) {
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(input->adcChNum);
				output->adcChMeasSum += val;
			}
		}
	}
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0006_DRDI
	if(input->reqMask & RPC_REQ_RF_DRDI) {
		val = get_sku_data(1);
		CCCI_INF_MSG(0, KERN, "RPC_REQ_RF_DRDI request=0x%x,val=0x%x\n", input->reqMask, val);
		output->rfDRDI = val;
	}
#endif

}

static void ccci_rpc_get_gpio_adc_v2(struct ccci_rpc_gpio_adc_intput_v2 *input,
				     struct ccci_rpc_gpio_adc_output_v2 *output)
{
	int num;
	unsigned int val, i;

	if ((input->reqMask & (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) == (RPC_REQ_GPIO_PIN | RPC_REQ_GPIO_VALUE)) {
		for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
			if (input->gpioValidPinMask & (1 << i)) {
				num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
				if (num >= 0) {
					output->gpioPinNum[i] = num;
					val = get_md_gpio_val(num);
					output->gpioPinValue[i] = val;
				}
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_GPIO_PIN) {
			for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					num = get_md_gpio_info(input->gpioPinName[i], strlen(input->gpioPinName[i]));
					if (num >= 0)
						output->gpioPinNum[i] = num;
				}
			}
		}
		if (input->reqMask & RPC_REQ_GPIO_VALUE) {
			for (i = 0; i < GPIO_MAX_COUNT_V2; i++) {
				if (input->gpioValidPinMask & (1 << i)) {
					val = get_md_gpio_val(input->gpioPinNum[i]);
					output->gpioPinValue[i] = val;
				}
			}
		}
	}
	if ((input->reqMask & (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) == (RPC_REQ_ADC_PIN | RPC_REQ_ADC_VALUE)) {
		num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
		if (num >= 0) {
			output->adcChNum = num;
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(num);
				output->adcChMeasSum += val;
			}
		}
	} else {
		if (input->reqMask & RPC_REQ_ADC_PIN) {
			num = get_md_adc_info(input->adcChName, strlen(input->adcChName));
			if (num >= 0)
				output->adcChNum = num;
		}
		if (input->reqMask & RPC_REQ_ADC_VALUE) {
			output->adcChMeasSum = 0;
			for (i = 0; i < input->adcChMeasCount; i++) {
				val = get_md_adc_val(input->adcChNum);
				output->adcChMeasSum += val;
			}
		}
	}
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0006_DRDI
	if(input->reqMask & RPC_REQ_RF_DRDI) {
		val = get_sku_data(1);
		CCCI_INF_MSG(0, KERN, "RPC_REQ_RF_DRDI request=0x%x,val=0x%x\n", input->reqMask, val);
		output->rfDRDI = val;
	}
#endif
}

static void ccci_rpc_work_helper(struct ccci_modem *md, struct rpc_pkt *pkt,
				 struct rpc_buffer *p_rpc_buf, unsigned int tmp_data[])
{
	int pkt_num = p_rpc_buf->para_num;

	CCCI_DEBUG_LOG(md->index, RPC, "ccci_rpc_work_helper++ %d\n", p_rpc_buf->para_num);
	tmp_data[0] = 0;
	switch (p_rpc_buf->op_id) {
	case IPC_RPC_CPSVC_SECURE_ALGO_OP:
		{
			unsigned char Direction = 0;
			unsigned long ContentAddr = 0;
			unsigned int ContentLen = 0;
			sed_t CustomSeed = SED_INITIALIZER;

			unsigned char *ResText __always_unused;
			unsigned char *RawText __always_unused;
			unsigned int i __always_unused;

			if (pkt_num < 4 || pkt_num >= RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid pkt_num %d for RPC_SECURE_ALGO_OP!\n", pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}

			Direction = *(unsigned char *)pkt[0].buf;
			ContentAddr = (unsigned long)pkt[1].buf;
			CCCI_DEBUG_LOG(md->index, RPC,
				     "RPC_SECURE_ALGO_OP: Content_Addr = 0x%p, RPC_Base = 0x%p, RPC_Len = 0x%zu\n",
				     (void *)ContentAddr, p_rpc_buf, sizeof(unsigned int) + RPC_MAX_BUF_SIZE);
			if (ContentAddr < (unsigned long)p_rpc_buf
			    || ContentAddr > ((unsigned long)p_rpc_buf + sizeof(unsigned int) + RPC_MAX_BUF_SIZE)) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid ContentAdddr[0x%p] for RPC_SECURE_ALGO_OP!\n",
					     (void *)ContentAddr);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			ContentLen = *(unsigned int *)pkt[2].buf;
			
			WARN_ON(sizeof(CustomSeed.sed) < pkt[3].len);
			memcpy(CustomSeed.sed, pkt[3].buf, pkt[3].len);

#ifdef ENCRYPT_DEBUG
			unsigned char log_buf[128];
			int curr;

			if (Direction == TRUE)
				CCCI_DEBUG_LOG(md->index, RPC, "HACC_S: EnCrypt_src:\n");
			else
				CCCI_DEBUG_LOG(md->index, RPC, "HACC_S: DeCrypt_src:\n");
			for (i = 0; i < ContentLen; i++) {
				if (i % 16 == 0) {
					if (i != 0)
						CCCI_NORMAL_LOG(md->index, RPC, "%s\n", log_buf);
					curr = 0;
					curr += snprintf(log_buf, sizeof(log_buf) - curr, "HACC_S: ");
				}
				
				curr +=
				    snprintf(&log_buf[curr], sizeof(log_buf) - curr, "0x%02X ",
					     *(unsigned char *)(ContentAddr + i));
				
			}
			CCCI_NORMAL_LOG(md->index, RPC, "%s\n", log_buf);

			RawText = kmalloc(ContentLen, GFP_KERNEL);
			if (RawText == NULL)
				CCCI_ERROR_LOG(md->index, RPC, "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
			else
				memcpy(RawText, (unsigned char *)ContentAddr, ContentLen);
#endif

			ResText = kmalloc(ContentLen, GFP_KERNEL);
			if (ResText == NULL) {
				CCCI_ERROR_LOG(md->index, RPC, "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
			if (!masp_secure_algo_init()) {
				CCCI_ERROR_LOG(md->index, RPC, "masp_secure_algo_init fail!\n");
				BUG_ON(1);
			}

			CCCI_DEBUG_LOG(md->index, RPC,
				     "RPC_SECURE_ALGO_OP: Dir=0x%08X, Addr=0x%08lX, Len=0x%08X, Seed=0x%016llX\n",
				     Direction, ContentAddr, ContentLen, *(long long *)CustomSeed.sed);
			masp_secure_algo(Direction, (unsigned char *)ContentAddr, ContentLen, CustomSeed.sed, ResText);

			if (!masp_secure_algo_deinit())
				CCCI_ERROR_LOG(md->index, RPC, "masp_secure_algo_deinit fail!\n");
#endif

			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = ContentLen;

#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
			memcpy(pkt[pkt_num++].buf, ResText, ContentLen);
			CCCI_DEBUG_LOG(md->index, RPC, "RPC_Secure memory copy OK: %d!", ContentLen);
#else
			memcpy(pkt[pkt_num++].buf, (void *)ContentAddr, ContentLen);
			CCCI_DEBUG_LOG(md->index, RPC, "RPC_NORMAL memory copy OK: %d!", ContentLen);
#endif

#ifdef ENCRYPT_DEBUG
			if (Direction == TRUE)
				CCCI_DEBUG_LOG(md->index, RPC, "HACC_D: EnCrypt_dst:\n");
			else
				CCCI_DEBUG_LOG(md->index, RPC, "HACC_D: DeCrypt_dst:\n");
			for (i = 0; i < ContentLen; i++) {
				if (i % 16 == 0) {
					if (i != 0)
						CCCI_DEBUG_LOG(md->index, RPC, "%s\n", log_buf);
					curr = 0;
					curr += snprintf(&log_buf[curr], sizeof(log_buf) - curr, "HACC_D: ");
				}
				
				curr += snprintf(&log_buf[curr], sizeof(log_buf) - curr, "0x%02X ", *(ResText + i));
				
			}

			CCCI_DEBUG_LOG(md->index, RPC, "%s\n", log_buf);
			kfree(RawText);
#endif

			kfree(ResText);
			break;
		}

#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	case IPC_RPC_GET_SECRO_OP:
		{
			unsigned char *addr = NULL;
			unsigned int img_len = 0;
			unsigned int img_len_bak = 0;
			unsigned int blk_sz = 0;
			unsigned int tmp = 1;
			unsigned int cnt = 0;
			unsigned int req_len = 0;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md->index, RPC, "RPC_GET_SECRO_OP: invalid parameter: pkt_num=%d\n",
					     pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				tmp_data[1] = img_len;
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				break;
			}

			req_len = *(unsigned int *)(pkt[0].buf);
			if (masp_secro_en()) {
				img_len = masp_secro_md_len(md->post_fix);

				if ((img_len > RPC_MAX_BUF_SIZE) || (req_len > RPC_MAX_BUF_SIZE)) {
					pkt_num = 0;
					tmp_data[0] = FS_MEM_OVERFLOW;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void *)&tmp_data[0];
					
					pkt[pkt_num].len = img_len;
					
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
					CCCI_ERROR_LOG(md->index, RPC,
						     "RPC_GET_SECRO_OP: md request length is larger than rpc memory: (%d, %d)\n",
						     req_len, img_len);
					break;
				}

				if (img_len > req_len) {
					pkt_num = 0;
					tmp_data[0] = FS_NO_MATCH;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void *)&tmp_data[0];
					
					pkt[pkt_num].len = img_len;
					
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
					CCCI_ERROR_LOG(md->index, RPC,
						     "RPC_GET_SECRO_OP: AP mis-match MD request length: (%d, %d)\n",
						     req_len, img_len);
					break;
				}

				
				
				CCCI_DEBUG_LOG(md->index, RPC, "<rpc>RPC_GET_SECRO_OP: save MD SECRO length: (%d)\n",
					     img_len);
				img_len_bak = img_len;

				blk_sz = masp_secro_blk_sz();
				for (cnt = 0; cnt < blk_sz; cnt++) {
					tmp = tmp * 2;
					if (tmp >= blk_sz)
						break;
				}
				++cnt;
				img_len = ((img_len + (blk_sz - 1)) >> cnt) << cnt;

				addr = (unsigned char *)&(p_rpc_buf->para_num) + 4 * sizeof(unsigned int);
				tmp_data[0] = masp_secro_md_get_data(md->post_fix, addr, 0, img_len);

				
				
				img_len = img_len_bak;

				CCCI_DEBUG_LOG(md->index, RPC, "<rpc>RPC_GET_SECRO_OP: restore MD SECRO length: (%d)\n",
					     img_len);

				if (tmp_data[0] != 0) {
					CCCI_ERROR_LOG(md->index, RPC, "RPC_GET_SECRO_OP: get data fail:%d\n",
						     tmp_data[0]);
					pkt_num = 0;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void *)&tmp_data[0];
					pkt[pkt_num].len = sizeof(unsigned int);
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
				} else {
					CCCI_DEBUG_LOG(md->index, RPC, "RPC_GET_SECRO_OP: get data OK: %d,%d\n",
									img_len, tmp_data[0]);
					pkt_num = 0;
					pkt[pkt_num].len = sizeof(unsigned int);
					
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void *)&tmp_data[1];
					pkt[pkt_num].len = img_len;
					pkt[pkt_num++].buf = (void *)addr;
					
					
				}
			} else {
				CCCI_DEBUG_LOG(md->index, RPC, "RPC_GET_SECRO_OP: secro disable\n");
				tmp_data[0] = FS_NO_FEATURE;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				tmp_data[1] = img_len;
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
			}

			break;
		}
#endif

		
	case IPC_RPC_GET_TDD_EINT_NUM_OP:
	case IPC_RPC_GET_GPIO_NUM_OP:
	case IPC_RPC_GET_ADC_NUM_OP:
		{
			int get_num = 0;
			unsigned char *name = NULL;
			unsigned int length = 0;

			if (pkt_num < 2 || pkt_num > RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err1;
			}
			length = pkt[0].len;
			if (length < 1) {
				CCCI_ERROR_LOG(md->index, RPC,
								"invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n",
								p_rpc_buf->op_id, pkt_num, length);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err1;
			}

			name = kmalloc(length, GFP_KERNEL);
			if (name == NULL) {
				CCCI_ERROR_LOG(md->index, RPC, "Fail alloc Mem for [0x%X]!\n", p_rpc_buf->op_id);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err1;
			} else {
				memcpy(name, (unsigned char *)(pkt[0].buf), length);

				if (p_rpc_buf->op_id == IPC_RPC_GET_TDD_EINT_NUM_OP) {
					get_num = get_td_eint_info(name, length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				} else if (p_rpc_buf->op_id == IPC_RPC_GET_GPIO_NUM_OP) {
					get_num = get_md_gpio_info(name, length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				} else if (p_rpc_buf->op_id == IPC_RPC_GET_ADC_NUM_OP) {
					get_num = get_md_adc_info(name, length);
					if (get_num < 0)
						get_num = FS_FUNC_FAIL;
				}

				CCCI_NORMAL_LOG(md->index, RPC, "[0x%08X]: name:%s, len=%d, get_num:%d\n",
					     p_rpc_buf->op_id, name, length, get_num);
				pkt_num = 0;

				
				tmp_data[1] = (unsigned int)get_num;
				
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)(&tmp_data[1]);
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)(&tmp_data[1]);
				kfree(name);
			}
			break;

 err1:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}

	case IPC_RPC_GET_EMI_CLK_TYPE_OP:
		{
			int dram_type = 0;
			int dram_clk = 0;

			if (pkt_num != 0) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err2;
			}

			if (get_dram_type_clk(&dram_clk, &dram_type)) {
				tmp_data[0] = FS_FUNC_FAIL;
				goto err2;
			} else {
				tmp_data[0] = 0;
				CCCI_NORMAL_LOG(md->index, RPC, "[0x%08X]: dram_clk: %d, dram_type:%d\n",
					     p_rpc_buf->op_id, dram_clk, dram_type);
			}

			tmp_data[1] = (unsigned int)dram_type;
			tmp_data[2] = (unsigned int)dram_clk;

			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)(&tmp_data[0]);
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)(&tmp_data[1]);
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)(&tmp_data[2]);
			break;

 err2:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}

	case IPC_RPC_GET_EINT_ATTR_OP:
		{
			char *eint_name = NULL;
			unsigned int name_len = 0;
			unsigned int type = 0;
			char *res = NULL;
			unsigned int res_len = 0;
			int ret = 0;

			if (pkt_num < 3 || pkt_num > RPC_MAX_ARG_NUM) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err3;
			}
			name_len = pkt[0].len;
			if (name_len < 1) {
				CCCI_ERROR_LOG(md->index, RPC,
								"invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n",
								p_rpc_buf->op_id, pkt_num, name_len);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err3;
			}

			eint_name = kmalloc(name_len, GFP_KERNEL);
			if (eint_name == NULL) {
				CCCI_ERROR_LOG(md->index, RPC, "Fail alloc Mem for [0x%X]!\n", p_rpc_buf->op_id);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err3;
			} else {
				memcpy(eint_name, (unsigned char *)(pkt[0].buf), name_len);
			}

			type = *(unsigned int *)(pkt[2].buf);
			res = (unsigned char *)&(p_rpc_buf->para_num) + 4 * sizeof(unsigned int);
			ret = get_eint_attr(eint_name, name_len, type, res, &res_len);
			if (ret == 0) {
				tmp_data[0] = ret;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = res_len;
				pkt[pkt_num++].buf = (void *)res;
				CCCI_DEBUG_LOG(md->index, RPC,
					     "[0x%08X] OK: name:%s, len:%d, type:%d, res:%d, res_len:%d\n",
					     p_rpc_buf->op_id, eint_name, name_len, type, *res, res_len);
				kfree(eint_name);
			} else {
				tmp_data[0] = ret;
				CCCI_DEBUG_LOG(md->index, RPC, "[0x%08X] fail: name:%s, len:%d, type:%d, ret:%d\n",
					     p_rpc_buf->op_id, eint_name, name_len, type, ret);
				kfree(eint_name);
				goto err3;
			}
			break;

 err3:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}
#ifdef FEATURE_RF_CLK_BUF
	case IPC_RPC_GET_RF_CLK_BUF_OP:
		{
			u16 count = 0;
			struct ccci_rpc_clkbuf_result *clkbuf;
			CLK_BUF_SWCTRL_STATUS_T swctrl_status[CLKBUF_MAX_COUNT];
#ifdef MD_UMOLY_EE_SUPPORT
			struct ccci_rpc_clkbuf_input *clkinput;
			u32 AfcDac;
#endif

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
#ifdef MD_UMOLY_EE_SUPPORT
			clkinput = (struct ccci_rpc_clkbuf_input *)pkt[0].buf;
			AfcDac = clkinput->AfcCwData;
			count = clkinput->CLKBuf_Num;
#else
			count = *(u16 *) (pkt[0].buf);
#endif
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(struct ccci_rpc_clkbuf_result);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			clkbuf = (struct ccci_rpc_clkbuf_result *)&tmp_data[1];
			if (count != CLKBUF_MAX_COUNT) {
				CCCI_ERROR_LOG(md->index, RPC, "IPC_RPC_GET_RF_CLK_BUF, wrong count %d/%d\n", count,
					     CLKBUF_MAX_COUNT);
				clkbuf->CLKBuf_Count = 0xFF;
				memset(&clkbuf->CLKBuf_Status, 0, sizeof(clkbuf->CLKBuf_Status));
			} else if (is_clk_buf_from_pmic()) {
				clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
				memset(&clkbuf->CLKBuf_Status, 0, sizeof(clkbuf->CLKBuf_Status));
				memset(&clkbuf->CLKBuf_SWCtrl_Status, 0, sizeof(clkbuf->CLKBuf_SWCtrl_Status));
#ifdef MD_UMOLY_EE_SUPPORT
				memset(&clkbuf->ClkBuf_Driving, 0, sizeof(clkbuf->ClkBuf_Driving));
#endif
			} else {
#ifdef MD_UMOLY_EE_SUPPORT
				unsigned int vals_drv[CLKBUF_MAX_COUNT] = {2, 2, 2, 2};
#endif
#if !defined(CONFIG_MTK_LEGACY)
				u32 vals[CLKBUF_MAX_COUNT] = {0, 0, 0, 0};
				struct device_node *node;

				node = of_find_compatible_node(NULL, NULL, "mediatek,rf_clock_buffer");
				if (node) {
					of_property_read_u32_array(node, "mediatek,clkbuf-config", vals,
						CLKBUF_MAX_COUNT);
				} else {
					CCCI_ERROR_LOG(md->index, RPC, "%s can't find compatible node\n", __func__);
				}
#else
				u32 vals[4] = {CLK_BUF1_STATUS, CLK_BUF2_STATUS, CLK_BUF3_STATUS, CLK_BUF4_STATUS};
#endif
				clkbuf->CLKBuf_Count = CLKBUF_MAX_COUNT;
				clkbuf->CLKBuf_Status[0] = vals[0];
				clkbuf->CLKBuf_Status[1] = vals[1];
				clkbuf->CLKBuf_Status[2] = vals[2];
				clkbuf->CLKBuf_Status[3] = vals[3];
				clk_buf_get_swctrl_status(swctrl_status);
#ifdef MD_UMOLY_EE_SUPPORT
				clk_buf_get_rf_drv_curr(vals_drv);
				clk_buf_save_afc_val(AfcDac);
#endif
				clkbuf->CLKBuf_SWCtrl_Status[0] = swctrl_status[0];
				clkbuf->CLKBuf_SWCtrl_Status[1] = swctrl_status[1];
				clkbuf->CLKBuf_SWCtrl_Status[2] = swctrl_status[2];
				clkbuf->CLKBuf_SWCtrl_Status[3] = swctrl_status[3];
#ifdef MD_UMOLY_EE_SUPPORT
				clkbuf->ClkBuf_Driving[0] = vals_drv[0];
				clkbuf->ClkBuf_Driving[1] = vals_drv[1];
				clkbuf->ClkBuf_Driving[2] = vals_drv[2];
				clkbuf->ClkBuf_Driving[3] = vals_drv[3];
				CCCI_INF_MSG(md->index, RPC, "RF_CLK_BUF*_DRIVING_CURR %d, %d, %d, %d, AfcDac: %d\n",
					vals_drv[0], vals_drv[1], vals_drv[2], vals_drv[3], AfcDac);
#endif
			}
			CCCI_DEBUG_LOG(md->index, RPC, "IPC_RPC_GET_RF_CLK_BUF count=%x\n", clkbuf->CLKBuf_Count);
			break;
		}
#endif
	case IPC_RPC_GET_GPIO_VAL_OP:
	case IPC_RPC_GET_ADC_VAL_OP:
		{
			unsigned int num = 0;
			int val = 0;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err4;
			}

			num = *(unsigned int *)(pkt[0].buf);
			if (p_rpc_buf->op_id == IPC_RPC_GET_GPIO_VAL_OP)
				val = get_md_gpio_val(num);
			else if (p_rpc_buf->op_id == IPC_RPC_GET_ADC_VAL_OP)
				val = get_md_adc_val(num);
			tmp_data[0] = val;
			CCCI_DEBUG_LOG(md->index, RPC, "[0x%X]: num=%d, val=%d!\n", p_rpc_buf->op_id, num, val);

 err4:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			break;
		}

	case IPC_RPC_GET_GPIO_ADC_OP:
		{
			struct ccci_rpc_gpio_adc_intput *input;
			struct ccci_rpc_gpio_adc_output *output;
			struct ccci_rpc_gpio_adc_intput_v2 *input_v2;
			struct ccci_rpc_gpio_adc_output_v2 *output_v2;
			unsigned int pkt_size;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			pkt_size = pkt[0].len;
			if (pkt_size == sizeof(struct ccci_rpc_gpio_adc_intput)) {
				input = (struct ccci_rpc_gpio_adc_intput *)(pkt[0].buf);
				pkt_num = 0;
				tmp_data[0] = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(struct ccci_rpc_gpio_adc_output);
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				output = (struct ccci_rpc_gpio_adc_output *)&tmp_data[1];
				memset(output, 0xF, sizeof(struct ccci_rpc_gpio_adc_output));	
				CCCI_BOOTUP_LOG(md->index, KERN, "IPC_RPC_GET_GPIO_ADC_OP request=%x\n",
								input->reqMask);
				ccci_rpc_get_gpio_adc(input, output);
			} else if (pkt_size == sizeof(struct ccci_rpc_gpio_adc_intput_v2)) {
				input_v2 = (struct ccci_rpc_gpio_adc_intput_v2 *)(pkt[0].buf);
				pkt_num = 0;
				tmp_data[0] = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(struct ccci_rpc_gpio_adc_output_v2);
				pkt[pkt_num++].buf = (void *)&tmp_data[1];
				output_v2 = (struct ccci_rpc_gpio_adc_output_v2 *)&tmp_data[1];
				
				memset(output_v2, 0xF, sizeof(struct ccci_rpc_gpio_adc_output_v2));
				CCCI_BOOTUP_LOG(md->index, KERN, "IPC_RPC_GET_GPIO_ADC_OP request=%x\n",
					     input_v2->reqMask);
				ccci_rpc_get_gpio_adc_v2(input_v2, output_v2);
			} else {
				CCCI_ERROR_LOG(md->index, RPC, "can't recognize pkt size%d!\n", pkt_size);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
			}
			break;
		}

	case IPC_RPC_DSP_EMI_MPU_SETTING:
		{
			struct ccci_rpc_dsp_emi_mpu_input *input, *output;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			input = (struct ccci_rpc_dsp_emi_mpu_input *)(pkt[0].buf);
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(struct ccci_rpc_dsp_emi_mpu_input);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			output = (struct ccci_rpc_dsp_emi_mpu_input *)&tmp_data[1];
			output->request = 0;
			CCCI_NORMAL_LOG(md->index, KERN, "IPC_RPC_DSP_EMI_MPU_SETTING request=%x\n", input->request);
			if (md->mem_layout.dsp_region_phy != 0)
				ccci_set_dsp_region_protection(md, 1);
			break;
		}

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	case IPC_RPC_USIM2NFC_OP:
		{
			struct ccci_rpc_usim2nfs *input, *output;

			if (pkt_num != 1) {
				CCCI_ERROR_LOG(md->index, RPC, "invalid parameter for [0x%X]: pkt_num=%d!\n",
					     p_rpc_buf->op_id, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void *)&tmp_data[0];
				break;
			}
			input = (struct ccci_rpc_usim2nfs *)(pkt[0].buf);
			pkt_num = 0;
			tmp_data[0] = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			pkt[pkt_num].len = sizeof(struct ccci_rpc_usim2nfs);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			output = (struct ccci_rpc_usim2nfs *)&tmp_data[1];
			output->lock_vsim1 = input->lock_vsim1;
			CCCI_DEBUG_LOG(md->index, KERN, "IPC_RPC_USIM2NFC_OP request=%x\n", input->lock_vsim1);
			
			inform_nfc_vsim_change(md->index, 1, input->lock_vsim1);
			break;
		}
#endif

	case IPC_RPC_IT_OP:
		{
			int i;

			CCCI_NORMAL_LOG(md->index, RPC, "[RPCIT] enter IT operation in ccci_rpc_work\n");
			
			for (i = 0; i < pkt_num; i++) {
				CCCI_NORMAL_LOG(md->index, RPC, "len=%d val=%X\n", pkt[i].len,
					     *((unsigned int *)pkt[i].buf));
			}
			tmp_data[0] = 1;
			tmp_data[1] = 0xA5A5;
			pkt_num = 0;
			CCCI_NORMAL_LOG(md->index, RPC, "[RPCIT] prepare output parameters\n");
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[0];
			CCCI_NORMAL_LOG(md->index, RPC, "[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n", 0, pkt[0].len,
				     *((unsigned int *)pkt[0].buf));
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void *)&tmp_data[1];
			CCCI_NORMAL_LOG(md->index, RPC, "[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X\n", 1, pkt[1].len,
				     *((unsigned int *)pkt[1].buf));
			break;
		}

	default:
		CCCI_NORMAL_LOG(md->index, RPC, "[Error]Unknown Operation ID (0x%08X)\n", p_rpc_buf->op_id);
		tmp_data[0] = FS_NO_OP;
		pkt_num = 0;
		pkt[pkt_num].len = sizeof(int);
		pkt[pkt_num++].buf = (void *)&tmp_data[0];
		break;
	}

	p_rpc_buf->para_num = pkt_num;
	CCCI_DEBUG_LOG(md->index, RPC, "ccci_rpc_work_helper-- %d\n", p_rpc_buf->para_num);
}

static void rpc_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_modem *md = port->modem;
	struct rpc_buffer *rpc_buf = (struct rpc_buffer *)req->skb->data;
	int i, data_len = 0, AlignLength, ret;
	struct rpc_pkt pkt[RPC_MAX_ARG_NUM];
	char *ptr, *ptr_base;
		
	unsigned int *tmp_data = kmalloc(128*sizeof(unsigned int), GFP_ATOMIC);

	if (tmp_data == NULL) {
		CCCI_ERROR_LOG(md->index, RPC, "RPC request buffer fail 128*sizeof(unsigned int)\n");
		goto err_out;
	}
	
	if (rpc_buf->header.reserved < 0 || rpc_buf->header.reserved > RPC_REQ_BUFFER_NUM ||
	    rpc_buf->para_num < 0 || rpc_buf->para_num > RPC_MAX_ARG_NUM) {
		CCCI_ERROR_LOG(md->index, RPC, "invalid RPC index %d/%d\n", rpc_buf->header.reserved,
						rpc_buf->para_num);
		goto err_out;
	}
	
	ptr_base = ptr = rpc_buf->buffer;
	for (i = 0; i < rpc_buf->para_num; i++) {
		pkt[i].len = *((unsigned int *)ptr);
		ptr += sizeof(pkt[i].len);
		pkt[i].buf = ptr;
		ptr += ((pkt[i].len + 3) >> 2) << 2;	
	}
	if ((ptr - ptr_base) > RPC_MAX_BUF_SIZE) {
		CCCI_ERROR_LOG(md->index, RPC, "RPC overflow in parse 0x%p\n", (void *)(ptr - ptr_base));
		goto err_out;
	}
	
	ccci_rpc_work_helper(md, pkt, rpc_buf, tmp_data);
	
	
	rpc_buf->op_id |= RPC_API_RESP_ID;
	data_len += (sizeof(rpc_buf->op_id) + sizeof(rpc_buf->para_num));
	ptr = rpc_buf->buffer;
	for (i = 0; i < rpc_buf->para_num; i++) {
		if ((data_len + sizeof(pkt[i].len) + pkt[i].len) > RPC_MAX_BUF_SIZE) {
			CCCI_ERROR_LOG(md->index, RPC, "RPC overflow in write %zu\n",
				     data_len + sizeof(pkt[i].len) + pkt[i].len);
			goto err_out;
		}

		*((unsigned int *)ptr) = pkt[i].len;
		ptr += sizeof(pkt[i].len);
		data_len += sizeof(pkt[i].len);

		AlignLength = ((pkt[i].len + 3) >> 2) << 2;	
		data_len += AlignLength;

		if (ptr != pkt[i].buf)
			memcpy(ptr, pkt[i].buf, pkt[i].len);
		else
			CCCI_DEBUG_LOG(md->index, RPC, "same addr, no copy, op_id=0x%x\n", rpc_buf->op_id);

		ptr += AlignLength;
	}
	
	data_len += sizeof(struct ccci_header);
	if (data_len > req->skb->len)
		skb_put(req->skb, data_len - req->skb->len);
	else if (data_len < req->skb->len)
		skb_trim(req->skb, data_len);
	
	rpc_buf->header.channel = CCCI_RPC_TX;
	rpc_buf->header.data[1] = data_len;
	CCCI_DEBUG_LOG(md->index, RPC, "Write %d/%d, %08X, %08X, %08X, %08X, op_id=0x%x\n", req->skb->len, data_len,
		     rpc_buf->header.data[0], rpc_buf->header.data[1], rpc_buf->header.channel,
		     rpc_buf->header.reserved, rpc_buf->op_id);
	
	req->policy = RECYCLE;
	req->blocking = 1;
	ret = ccci_port_send_request(port, req);
	if (ret)
		goto err_out;
	kfree(tmp_data);
	return;

 err_out:
	kfree(tmp_data);
	req->policy = RECYCLE;
	ccci_free_req(req);
}

static void status_msg_handler(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_modem *md = port->modem;

	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
	u64 ts_nsec1 = last_cldma_isr;
	u64 ts_nsec2 = last_q0_rx_isr;
	u64 ts_nsec3 = last_rx_done;

	unsigned long rem_nsec1, rem_nsec2, rem_nsec3;

	if (ts_nsec1 == 0)
		rem_nsec1 = 0;
	else
		rem_nsec1 = do_div(ts_nsec1, 1000000000);

	if (ts_nsec2 == 0)
		rem_nsec2 = 0;
	else
		rem_nsec2 = do_div(ts_nsec2, 1000000000);

	if (ts_nsec3 == 0)
		rem_nsec3 = 0;
	else
		rem_nsec3 = do_div(ts_nsec3, 1000000000);


	del_timer(&port->modem->md_status_timeout);
	CCCI_REPEAT_LOG(port->modem->index, KERN,
			"modem status info seq=0x%X, cldma_isr=%5lu.%06lu, q0_rx=%5lu.%06lu, rx_done=%5lu.%06lu\n",
			*(((u32 *) ccci_h) + 2),
			(unsigned long)ts_nsec1, rem_nsec1 / 1000,
			(unsigned long)ts_nsec2, rem_nsec2 / 1000,
			(unsigned long)ts_nsec3, rem_nsec3 / 1000);
	ccci_util_cmpt_mem_dump(md->index, CCCI_DUMP_REPEAT, req->skb->data, req->skb->len);
	req->policy = RECYCLE;
	ccci_free_req(req);
	
	if (port->modem->md_state == READY)
		mod_timer(&port->modem->md_status_poller, jiffies + 20 * HZ);
}

void md_status_poller_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	int ret;

	
	mod_timer(&md->md_status_timeout, jiffies + 15 * HZ);
	ret = ccci_send_msg_to_md(md, CCCI_STATUS_TX, 0, 0, 0);
	CCCI_REPEAT_LOG(md->index, KERN, "poll modem status %d seq=0x%X\n", ret, md->seq_nums[OUT][CCCI_STATUS_TX]);

	if (ret) {
		CCCI_ERROR_LOG(md->index, KERN, "fail to send modem status polling msg ret=%d\n", ret);
		del_timer(&md->md_status_timeout);
		if ((ret == -EBUSY || (ret == -CCCI_ERR_ALLOCATE_MEMORY_FAIL)) && md->md_state == READY) {

			if (md->md_status_poller_flag & MD_STATUS_POLL_BUSY) {
				md->ops->dump_info(md, DUMP_FLAG_QUEUE_0, NULL, 0);
				ccci_md_exception_notify(md, MD_NO_RESPONSE);
			} else if (ret == -CCCI_ERR_ALLOCATE_MEMORY_FAIL) {
				mod_timer(&md->md_status_poller, jiffies + 10 * HZ);
			} else {
				md->md_status_poller_flag |= MD_STATUS_POLL_BUSY;
				mod_timer(&md->md_status_poller, jiffies + 10 * HZ);
			}
		}
	} else {
		md->md_status_poller_flag &= ~MD_STATUS_POLL_BUSY;
	}
}

void md_status_timeout_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	u64 ts_nsec1 = last_cldma_isr;
	u64 ts_nsec2 = last_q0_rx_isr;
	u64 ts_nsec3 = last_rx_done;

	unsigned long rem_nsec1, rem_nsec2, rem_nsec3;

	if (ts_nsec1 == 0)
		rem_nsec1 = 0;
	else
		rem_nsec1 = do_div(ts_nsec1, 1000000000);

	if (ts_nsec2 == 0)
		rem_nsec2 = 0;
	else
		rem_nsec2 = do_div(ts_nsec2, 1000000000);


	if (ts_nsec3 == 0)
		rem_nsec3 = 0;
	else
		rem_nsec3 = do_div(ts_nsec3, 1000000000);

	mt_irq_dump_status(md_ctrl->cldma_irq_id);

	if (md->md_status_poller_flag & MD_STATUS_ASSERTED) {
		CCCI_ERROR_LOG(md->index, KERN, "modem status polling timeout, assert fail,");
		CCCI_ERROR_LOG(md->index, KERN, "cldma_isr=%5lu.%06lu, q0_rx=%5lu.%06lu, rx_done=%5lu.%06lu\n",
				(unsigned long)ts_nsec1, rem_nsec1 / 1000,
				(unsigned long)ts_nsec2, rem_nsec2 / 1000,
				(unsigned long)ts_nsec3, rem_nsec3 / 1000);
		ccci_md_exception_notify(md, MD_NO_RESPONSE);
	} else {
		CCCI_ERROR_LOG(md->index, KERN, "modem status polling timeout, force assert,");
		CCCI_ERROR_LOG(md->index, KERN, "cldma_isr=%5lu.%06lu, q0_rx=%5lu.%06lu, rx_done=%5lu.%06lu\n",
				(unsigned long)ts_nsec1, rem_nsec1 / 1000,
				(unsigned long)ts_nsec2, rem_nsec2 / 1000,
				(unsigned long)ts_nsec3, rem_nsec3 / 1000);
		md->md_status_poller_flag |= MD_STATUS_ASSERTED;
		md->ops->dump_info(md, DUMP_FLAG_QUEUE_0, NULL, 0);
		mod_timer(&md->md_status_timeout, jiffies + 5 * HZ);
		md->ops->force_assert(md, CCIF_INTR_SEQ);
	}
}

static int port_kernel_thread(void *arg)
{
	struct ccci_port *port = arg;
	
	struct ccci_request *req;
	struct ccci_header *ccci_h;
	unsigned long flags;
	int ret;

	CCCI_DEBUG_LOG(port->modem->index, KERN, "port %s's thread running\n", port->name);
	

	while (1) {
		if (list_empty(&port->rx_req_list)) {
			ret = wait_event_interruptible(port->rx_wq, !list_empty(&port->rx_req_list));
			if (ret == -ERESTARTSYS)
				continue;	
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(port->modem->index, KERN, "read on %s\n", port->name);
		
		spin_lock_irqsave(&port->rx_req_lock, flags);
		req = list_first_entry(&port->rx_req_list, struct ccci_request, entry);
		list_del(&req->entry);
		if (--(port->rx_length) == 0)
			ccci_port_ask_more_request(port);
		spin_unlock_irqrestore(&port->rx_req_lock, flags);
		
		ccci_h = (struct ccci_header *)req->skb->data;
		switch (ccci_h->channel) {	
		case CCCI_CONTROL_RX:
			control_msg_handler(port, req);
			break;
		case CCCI_SYSTEM_RX:
			system_msg_handler(port, req);
			break;
		case CCCI_RPC_RX:
			rpc_msg_handler(port, req);
			CCCI_DEBUG_LOG(port->modem->index, KERN, "rpc done %s\n", port->name);
			break;
		case CCCI_STATUS_RX:
			status_msg_handler(port, req);
			break;
		};
		
	}
	return 0;
}

#ifdef CONFIG_HTC_MODEM_EXCEPTION_INFO_EXPORT
#define CCCI_MODEM_EX_INFO_MAX_SIZE 512
enum {
	SLOT_MD1 = 0,
	SLOT_MD3 = 1,
	SLOT_MAX
};
#define EX_SLOT(c) (c == MD_SYS3 ? SLOT_MD3:SLOT_MD1)
#define EX_PROC_NAME(c) (c == MD_SYS3 ? "c2k_ex_info":"ccci_ex_info")
char ccci_modem_ex_info_msg[SLOT_MAX][CCCI_MODEM_EX_INFO_MAX_SIZE];

static int ccci_modem_proc_show(struct seq_file *m, void *v)
{
	char err_string[CCCI_MODEM_EX_INFO_MAX_SIZE];
	struct ccci_modem *md = (struct ccci_modem *)m->private;
	int slot_id = EX_SLOT(md->index);
	CCCI_INF_MSG(0, KERN, "%s:%d\n", __func__, slot_id);

	memset(err_string, 0, sizeof(err_string));

	if (strlen(ccci_modem_ex_info_msg[slot_id]) > 0)
		snprintf(err_string, CCCI_MODEM_EX_INFO_MAX_SIZE, "modem fatal:%s\n", ccci_modem_ex_info_msg[slot_id]);
	memset(ccci_modem_ex_info_msg[slot_id], 0, CCCI_MODEM_EX_INFO_MAX_SIZE);

	seq_printf(m, "%s\n", err_string);
	return 0;
}

static int ccci_modem_ex_open(struct inode *inode, struct file *file)
{
       return single_open(file, ccci_modem_proc_show, PDE_DATA(inode));
}

static const struct file_operations ccci_modem_ex_fops = {
       .open           = ccci_modem_ex_open,
       .read           = seq_read,
       .release        = single_release,
};

void ccci_modem_ex_proc_init(struct ccci_port *port)
{
	struct proc_dir_entry *ccci_modem_ex_proc;
	int modem_id = port->modem->index;

	ccci_modem_ex_proc = proc_create_data(EX_PROC_NAME(modem_id), 0664, NULL, &ccci_modem_ex_fops,port->modem);
	if(ccci_modem_ex_proc == NULL) {
		CCCI_DEBUG_LOG(port->modem->index, KERN, "fail to create proc entry\n");
	}

}
#endif

int port_kernel_init(struct ccci_port *port)
{
	CCCI_DEBUG_LOG(port->modem->index, KERN, "kernel port %s is initializing\n", port->name);
	port->private_data = kthread_run(port_kernel_thread, port, "%s", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
#ifdef CONFIG_HTC_MODEM_EXCEPTION_INFO_EXPORT
	if ((port != NULL) &&
		((!strcmp(port->name, "ccci_ctrl")) || (!strcmp(port->name, "ccci3_ctrl")))) {
		ccci_modem_ex_proc_init(port);
	}
#endif
	return 0;
}

int port_kernel_recv_req(struct ccci_port *port, struct ccci_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&port->rx_req_lock, flags);
	CCCI_DEBUG_LOG(port->modem->index, IPC, "recv on %s, len=%d\n", port->name, port->rx_length);
	if (port->rx_length < port->rx_length_th) {
		port->flags &= ~PORT_F_RX_FULLED;
		port->rx_length++;
		list_del(&req->entry);	
		list_add_tail(&req->entry, &port->rx_req_list);
		spin_unlock_irqrestore(&port->rx_req_lock, flags);
		wake_lock_timeout(&port->rx_wakelock, HZ);
		wake_up_all(&port->rx_wq);
		return 0;
	}

	port->flags |= PORT_F_RX_FULLED;
	spin_unlock_irqrestore(&port->rx_req_lock, flags);
	if (port->flags & PORT_F_ALLOW_DROP) {
		CCCI_NORMAL_LOG(port->modem->index, KERN, "port %s Rx full, drop packet\n", port->name);
		goto drop;
	} else {
		return -CCCI_ERR_PORT_RX_FULL;
	}

 drop:
	
	CCCI_NORMAL_LOG(port->modem->index, KERN, "drop on %s, len=%d\n", port->name, port->rx_length);
	list_del(&req->entry);
	req->policy = RECYCLE;
	ccci_free_req(req);
	return -CCCI_ERR_DROP_PACKET;
}

int port_kernel_req_match(struct ccci_port *port, struct ccci_request *req)
{
	struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
	struct rpc_buffer *rpc_buf;

	if (ccci_h->channel == CCCI_RPC_RX) {
		rpc_buf = (struct rpc_buffer *)req->skb->data;
		switch (rpc_buf->op_id) {
#ifdef CONFIG_MTK_TC1_FEATURE
		
		case RPC_CCCI_LGE_FAC_READ_SIM_LOCK_TYPE:
		case RPC_CCCI_LGE_FAC_READ_FUSG_FLAG:
		case RPC_CCCI_LGE_FAC_CHECK_UNLOCK_CODE_VALIDNESS:
		case RPC_CCCI_LGE_FAC_CHECK_NETWORK_CODE_VALIDNESS:
		case RPC_CCCI_LGE_FAC_WRITE_SIM_LOCK_TYPE:
		case RPC_CCCI_LGE_FAC_READ_IMEI:
		case RPC_CCCI_LGE_FAC_WRITE_IMEI:
		case RPC_CCCI_LGE_FAC_READ_NETWORK_CODE_LIST_NUM:
		case RPC_CCCI_LGE_FAC_READ_NETWORK_CODE:
		case RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE_LIST_NUM:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE_VERIFY_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_READ_UNLOCK_CODE_VERIFY_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_READ_UNLOCK_FAIL_COUNT:
		case RPC_CCCI_LGE_FAC_WRITE_UNLOCK_CODE:
		case RPC_CCCI_LGE_FAC_VERIFY_UNLOCK_CODE:
		case RPC_CCCI_LGE_FAC_WRITE_NETWORK_CODE:
		case RPC_CCCI_LGE_FAC_INIT_SIM_LOCK_DATA:
			CCCI_DEBUG_LOG(port->modem->index, KERN, "userspace rpc msg 0x%x on %s\n",
						rpc_buf->op_id, port->name);
			return 0;
#endif
		default:
			CCCI_DEBUG_LOG(port->modem->index, KERN, "kernelspace rpc msg 0x%x on %s\n",
						rpc_buf->op_id, port->name);
			return 1;
		}
	}
	
	return 1;
}

static void port_kernel_md_state_notice(struct ccci_port *port, MD_STATE state)
{
	if (port->rx_ch != CCCI_CONTROL_RX)
		return;

	
	switch (state) {
	case RESET:
		del_timer(&port->modem->md_status_poller);
		del_timer(&port->modem->md_status_timeout);

		del_timer(&port->modem->ex_monitor);
		del_timer(&port->modem->ex_monitor2);

		port->modem->md_status_poller_flag = 0;
		break;
	default:
		break;
	};
}

struct ccci_port_ops kernel_port_ops = {
	.init = &port_kernel_init,
	.req_match = &port_kernel_req_match,
	.recv_request = &port_kernel_recv_req,
	.md_state_notice = &port_kernel_md_state_notice,
};

void ccci_md_exception_notify(struct ccci_modem *md, MD_EX_STAGE stage)
{
	unsigned long flags;

	spin_lock_irqsave(&md->ctrl_lock, flags);
	CCCI_NORMAL_LOG(md->index, KERN, "MD exception logical %d->%d\n", md->ex_stage, stage);
	md->ex_stage = stage;
	switch (md->ex_stage) {
	case EX_INIT:
		del_timer(&md->ex_monitor2);
		del_timer(&md->bootup_timer);
		del_timer(&md->md_status_poller);
		del_timer(&md->md_status_timeout);
		md->ee_info_flag |= ((1 << MD_EE_FLOW_START) | (1 << MD_EE_SWINT_GET));
		if (!MD_IN_DEBUG(md))
			mod_timer(&md->ex_monitor, jiffies + EX_TIMER_SWINT * HZ);
		md->ops->broadcast_state(md, EXCEPTION);
		break;
	case EX_DHL_DL_RDY:
		break;
	case EX_INIT_DONE:
		ccci_reset_seq_num(md);
		break;
	case MD_NO_RESPONSE:
		
		del_timer(&md->md_status_timeout);
		CCCI_ERROR_LOG(md->index, KERN, "MD long time no response, flag=%x\n", md->md_status_poller_flag);
		md->ee_info_flag |= ((1 << MD_EE_FLOW_START) | (1 << MD_EE_PENDING_TOO_LONG) | (1 << MD_STATE_UPDATE));
		mod_timer(&md->ex_monitor, jiffies);
		break;
	case MD_WDT:
		del_timer(&md->md_status_poller);
		del_timer(&md->md_status_timeout);

		md->ee_info_flag |= ((1 << MD_EE_FLOW_START) | (1 << MD_EE_WDT_GET) | (1 << MD_STATE_UPDATE));
		md->ops->broadcast_state(md, EXCEPTION);
		mod_timer(&md->ex_monitor, jiffies);
		break;
	default:
		break;
	};
	spin_unlock_irqrestore(&md->ctrl_lock, flags);
}
EXPORT_SYMBOL(ccci_md_exception_notify);

static void ccci_aed(struct ccci_modem *md, unsigned int dump_flag, char *aed_str, int db_opt)
{
	void *ex_log_addr = NULL;
	int ex_log_len = 0;
	void *md_img_addr = NULL;
	int md_img_len = 0;
	int info_str_len = 0;
	char *buff;		
	char *img_inf;

	buff = kmalloc(AED_STR_LEN, GFP_ATOMIC);
	if (buff == NULL) {
		CCCI_ERROR_LOG(md->index, KERN, "Fail alloc Mem for buff!\n");
		goto err_exit1;
	}
	img_inf = ccci_get_md_info_str(md->index);
	if (img_inf == NULL)
		img_inf = "";
	info_str_len = strlen(aed_str);
	info_str_len += strlen(img_inf);

	if (info_str_len > AED_STR_LEN)
		buff[AED_STR_LEN - 1] = '\0';	

	snprintf(buff, AED_STR_LEN, "md%d:%s%s", md->index + 1, aed_str, img_inf);
	
 err_exit1:
	if (dump_flag & CCCI_AED_DUMP_CCIF_REG) {	
		ex_log_addr = md->smem_layout.ccci_exp_smem_mdss_debug_vir;
		ex_log_len = md->smem_layout.ccci_exp_smem_mdss_debug_size;
		md->ops->dump_info(md, DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG,
				   md->smem_layout.ccci_exp_smem_base_vir + CCCI_SMEM_OFFSET_CCIF_SRAM,
				   CCCC_SMEM_CCIF_DUMP_SIZE);
	}
	if (dump_flag & CCCI_AED_DUMP_EX_MEM) {
		ex_log_addr = md->smem_layout.ccci_exp_smem_mdss_debug_vir;
		ex_log_len = md->smem_layout.ccci_exp_smem_mdss_debug_size;
	}
	if (dump_flag & CCCI_AED_DUMP_EX_PKT) {
#ifdef MD_UMOLY_EE_SUPPORT
		if (md->index == MD_SYS1) {
			ex_log_addr = (void *)md->ex_pl_info;
			ex_log_len = MD_HS1_FAIL_DUMP_SIZE;
		} else {
#endif
			ex_log_addr = (void *)&md->ex_info;
			ex_log_len = sizeof(EX_LOG_T);
#ifdef MD_UMOLY_EE_SUPPORT
		}
#endif
	}
	if (dump_flag & CCCI_AED_DUMP_MD_IMG_MEM) {
		md_img_addr = (void *)md->mem_layout.md_region_vir;
		md_img_len = MD_IMG_DUMP_SIZE;
	}
#if defined(CONFIG_MTK_AEE_FEATURE)
	aed_md_exception_api(ex_log_addr, ex_log_len, md_img_addr, md_img_len, buff, db_opt);
#endif
	kfree(buff);
}

#ifdef MD_UMOLY_EE_SUPPORT
static void ccci_md_ee_info_dump(struct ccci_modem *md)
{
	char *ex_info;		
	char *i_bit_ex_info = NULL;
	char buf_fail[] = "Fail alloc mem for exception\n";
	int db_opt = (DB_OPT_DEFAULT | DB_OPT_FTRACE);
	int dump_flag = 0;
	int core_id;
	char *ex_info_temp = NULL;
	DEBUG_INFO_T *debug_info = &md->debug_info;
	unsigned char c;
	EX_PL_LOG_T *ex_pl_info = (EX_PL_LOG_T *)md->ex_pl_info;

	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct timeval tv_android = { 0 };
	struct rtc_time tm_android;

	ex_info = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
	if (ex_info == NULL) {
		CCCI_ERROR_LOG(md->index, KERN, "Fail alloc Mem for ex_info!\n");
		goto err_exit;
	}
	ex_info_temp = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
	if (ex_info_temp == NULL) {
		CCCI_ERROR_LOG(md->index, KERN, "Fail alloc Mem for ex_info_temp!\n");
		goto err_exit;
	}

	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);
	CCCI_ERROR_LOG(md->index, KERN, "Sync:%d%02d%02d %02d:%02d:%02d.%u(%02d:%02d:%02d.%03d(TZone))\n",
		     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     (unsigned int)tv.tv_usec,
		     tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec, (unsigned int)tv_android.tv_usec);
	for (core_id = 0; core_id < md->ex_core_num; core_id++) {
		if (core_id == 1) {
			snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%s", ex_info);
			debug_info = &md->debug_info1[core_id - 1];
		} else if (core_id > 1) {
			snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%smd%d:%s", ex_info_temp, md->index + 1, ex_info);
			debug_info = &md->debug_info1[core_id - 1];
		}
		CCCI_ERROR_LOG(md->index, KERN, "exception type(%d):%s\n", debug_info->type,
			     debug_info->name ? : "Unknown");

		switch (debug_info->type) {
		case MD_EX_DUMP_ASSERT:
			CCCI_ERROR_LOG(md->index, KERN, "filename = %s\n", debug_info->assert.file_name);
			CCCI_ERROR_LOG(md->index, KERN, "line = %d\n", debug_info->assert.line_num);
			CCCI_ERROR_LOG(md->index, KERN, "para0 = %d, para1 = %d, para2 = %d\n",
				     debug_info->assert.parameters[0],
				     debug_info->assert.parameters[1], debug_info->assert.parameters[2]);
			snprintf(ex_info, EE_BUF_LEN_UMOLY,
					"%s\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n\n",
					debug_info->core_name,
					debug_info->name,
					debug_info->assert.file_name,
					debug_info->assert.line_num,
					debug_info->assert.parameters[0],
					debug_info->assert.parameters[1], debug_info->assert.parameters[2]);
#ifdef CONFIG_HTC_MODEM_EXCEPTION_INFO_EXPORT
			memset(ccci_modem_ex_info_msg[SLOT_MD1], 0, CCCI_MODEM_EX_INFO_MAX_SIZE);
			snprintf(ccci_modem_ex_info_msg[SLOT_MD1], CCCI_MODEM_EX_INFO_MAX_SIZE, "%d%02d%02d %02d:%02d:%02d.%u, exception type(%d):%s, %s, %d, %d, %d, %d\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
					tm.tm_hour, tm.tm_min, tm.tm_sec,
					(unsigned int) tv.tv_usec,
					debug_info->type,debug_info->name?:"Unknown",
					debug_info->assert.file_name,
					debug_info->assert.line_num,
					debug_info->assert.parameters[0],
					debug_info->assert.parameters[1],
					debug_info->assert.parameters[2]);
#endif
			break;
		case MD_EX_DUMP_3P_EX:
		case MD_EX_CC_C2K_EXCEPTION:
			CCCI_ERROR_LOG(md->index, KERN, "fatal error code 1 = 0x%08X\n",
				     debug_info->fatal_error.err_code1);
			CCCI_ERROR_LOG(md->index, KERN, "fatal error code 2 = 0x%08X\n",
				     debug_info->fatal_error.err_code2);
			CCCI_ERROR_LOG(md->index, KERN, "fatal error code 3 = 0x%08X\n",
				     debug_info->fatal_error.err_code3);
			CCCI_ERROR_LOG(md->index, KERN, "fatal error offender %s\n", debug_info->fatal_error.offender);
			if (debug_info->fatal_error.offender[0] != '\0') {
				snprintf(ex_info, EE_BUF_LEN_UMOLY,
				"%s\n[%s] err_code1:0x%08X err_code2:0x%08X erro_code3:0x%08X\nMD Offender:%s\n%s",
					 debug_info->core_name, debug_info->name, debug_info->fatal_error.err_code1,
					 debug_info->fatal_error.err_code2, debug_info->fatal_error.err_code3,
					 debug_info->fatal_error.offender, debug_info->fatal_error.ExStr);
			} else {
				snprintf(ex_info, EE_BUF_LEN_UMOLY,
					 "%s\n[%s] err_code1:0x%08X err_code2:0x%08X err_code3:0x%08X\n%s\n",
					 debug_info->core_name, debug_info->name, debug_info->fatal_error.err_code1,
					 debug_info->fatal_error.err_code2, debug_info->fatal_error.err_code3,
					 debug_info->fatal_error.ExStr);
			}
			if (debug_info->fatal_error.err_code1 == 0x3104) {
				snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s, MD base = 0x%08X\n\n", ex_info,
					md->ex_mpu_string, (unsigned int)md->mem_layout.md_region_phy);
				memset(md->ex_mpu_string, 0x0, sizeof(md->ex_mpu_string));
			}
			break;
		case MD_EX_DUMP_2P_EX:
			CCCI_ERROR_LOG(md->index, KERN, "fatal error code 1 = 0x%08X\n\n",
				     debug_info->fatal_error.err_code1);
			CCCI_ERROR_LOG(md->index, KERN, "fatal error code 2 = 0x%08X\n\n",
				     debug_info->fatal_error.err_code2);

			snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[%s] err_code1:0x%08X err_code2:0x%08X\n\n",
				 debug_info->core_name, debug_info->name, debug_info->fatal_error.err_code1,
				 debug_info->fatal_error.err_code2);
			break;
		case MD_EX_DUMP_EMI_CHECK:
			CCCI_ERROR_LOG(md->index, KERN, "md_emi_check: 0x%08X, 0x%08X, %02d, 0x%08X\n\n",
				     debug_info->data.data0, debug_info->data.data1,
				     debug_info->data.channel, debug_info->data.reserved);
			snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[emi_chk] 0x%08X, 0x%08X, %02d, 0x%08X\n\n",
				 debug_info->core_name, debug_info->data.data0, debug_info->data.data1,
				 debug_info->data.channel, debug_info->data.reserved);
			break;
		case MD_EX_DUMP_UNKNOWN:
		default:	
			snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[%s]\n", debug_info->core_name, debug_info->name);
			break;
		}
		ccci_event_log("md%d %s\n", md->index+1, ex_info);
	}
	if (md->ex_core_num > 1) {
		CCCI_NORMAL_LOG(md->index, KERN, "%s+++++++%s", ex_info_temp, ex_info);
		snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%smd%d:%s", ex_info_temp, md->index + 1, ex_info);
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s", ex_info_temp);

		debug_info = &md->debug_info;
	} else if (md->ex_core_num == 0)
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "\n");
	
	switch (debug_info->more_info) {
	case MD_EE_CASE_ONLY_SWINT:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nOnly SWINT case\n");
		break;
	case MD_EE_CASE_SWINT_MISSING:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nSWINT missing case\n");
		break;
	case MD_EE_CASE_ONLY_EX:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nOnly EX case\n");
		break;
	case MD_EE_CASE_ONLY_EX_OK:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nOnly EX_OK case\n");
		break;
	case MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG:
		i_bit_ex_info = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
		if (i_bit_ex_info == NULL) {
			CCCI_ERROR_LOG(md->index, KERN, "Fail alloc Mem for i_bit_ex_info!\n");
			break;
		}
		snprintf(i_bit_ex_info, EE_BUF_LEN_UMOLY, "\n[Others] May I-Bit dis too long\n%s", ex_info);
		strcpy(ex_info, i_bit_ex_info);
		break;
	case MD_EE_CASE_TX_TRG:
	case MD_EE_CASE_ISR_TRG:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\n[Others] May I-Bit dis too long\n");
		break;
	case MD_EE_CASE_NO_RESPONSE:
		
		strcpy(ex_info, "\n[Others] MD long time no response\n");
		db_opt |= DB_OPT_FTRACE;
		break;
	case MD_EE_CASE_WDT:
		strcpy(ex_info, "\n[Others] MD watchdog timeout interrupt\n");
		break;
	default:
		break;
	}

	
	c = ex_pl_info->envinfo.ELM_status;
	CCCI_NORMAL_LOG(md->index, KERN, "ELM_status: %x\n", c);
	switch (c) {
	case 0xFF:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nno ELM info\n");
		break;
	case 0xAE:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nELM rlat:FAIL\n");
		break;
	case 0xBE:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nELM wlat:FAIL\n");
		break;
	case 0xDE:
		snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info, "\nELM r/wlat:PASS\n");
		break;
	default:
		break;
	}

	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD EX log, 0x%x, 0x%x\n", debug_info->more_info,
			(unsigned int)md->boot_stage);
	if (debug_info->more_info == MD_EE_CASE_NORMAL && md->boot_stage == MD_BOOT_STAGE_0 &&
		md->flight_mode == MD_FIGHT_MODE_NONE) {
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, md->ex_pl_info, MD_HS1_FAIL_DUMP_SIZE);
		
		dump_flag = CCCI_AED_DUMP_EX_PKT;
	} else {
#if 1
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
					md->smem_layout.ccci_exp_smem_base_vir, (2048 + 512));
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
					(md->smem_layout.ccci_exp_smem_mdss_debug_vir + 6 * 1024), 2048);
#else
		ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_dump_size);
#endif
		dump_flag = CCCI_AED_DUMP_EX_MEM;
		if (debug_info->more_info == MD_EE_CASE_NO_RESPONSE)
			dump_flag |= CCCI_AED_DUMP_CCIF_REG;
	}
	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD image memory\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, (void *)md->mem_layout.md_region_vir, MD_IMG_DUMP_SIZE);
	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD layout struct\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, &md->mem_layout, sizeof(struct ccci_mem_layout));
	
	if (debug_info->more_info == MD_EE_CASE_NO_RESPONSE ||
		debug_info->more_info == MD_EE_CASE_WDT) 
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
err_exit:
	ccci_update_md_boot_stage(md, MD_BOOT_STAGE_EXCEPTION);
	
	if (debug_info->type == MD_EX_TYPE_C2K_ERROR)
		CCCI_NORMAL_LOG(md->index, KERN, "C2K EE, No need trigger DB\n");
	else if ((debug_info->type == MD_EX_DUMP_EMI_CHECK) && (Is_MD_EMI_voilation() == 0))
		CCCI_NORMAL_LOG(md->index, KERN, "Not MD EMI violation, No need trigger DB\n");
	else if (ex_info == NULL)
		ccci_aed(md, dump_flag, buf_fail, db_opt);
	else
		ccci_aed(md, dump_flag, ex_info, db_opt);
	if (debug_info->more_info == MD_EE_CASE_ONLY_SWINT)
		md->ops->dump_info(md, DUMP_FLAG_QUEUE_0 | DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG, NULL, 0);

	kfree(ex_info);
	kfree(ex_info_temp);
	kfree(i_bit_ex_info);
}

static char md_ee_plstr[MD_EX_PL_FATALE_TOTAL + MD_EX_OTHER_CORE_EXCEPTIN - MD_EX_CC_INVALID_EXCEPTION][32] = {
	"INVALID",
	"Fatal error (undefine)",
	"Fatal error (swi)",
	"Fatal error (prefetch abort)",
	"Fatal error (data abort)",
	"Fatal error (stack)",
	"Fatal error (task)",
	"Fatal error (buff)",
	"Fatal error (CC invalid)",
	"Fatal error (CC PCore)",
	"Fatal error (CC L1Core)",
	"Fatal error (CC CS)",
	"Fatal error (CC MD32)",
	"Fatal error (CC C2K)",
	"Fatal error (CC spc)"
};
void strmncopy(char *src, char *dst, int src_len, int dst_len)
{
	int temp_m, temp_n, temp_i;

	temp_m = src_len - 1;
	temp_n = dst_len - 1;
	temp_n = (temp_m > temp_n) ? temp_n : temp_m;
	for (temp_i = 0; temp_i < temp_n; temp_i++) {
		dst[temp_i] = src[temp_i];
		if (dst[temp_i] == 0x00)
			break;
	}
	CCCI_DEBUG_LOG(-1, KERN, "copy str(%d) %s\n", temp_i, dst);
}


static void ccci_md_exp_change(struct ccci_modem *md)
{
	EX_OVERVIEW_T *ex_overview;
	int ee_type, ee_case;

	DEBUG_INFO_T *debug_info = &md->debug_info;
	int core_id;
	unsigned char off_core_num; 
	unsigned int temp_i;
	EX_PL_LOG_T *ex_pl_info = (EX_PL_LOG_T *)md->ex_pl_info;
	EX_PL_LOG_T *ex_PLloginfo;
	EX_CS_LOG_T *ex_csLogInfo;
	EX_MD32_LOG_T *ex_md32LogInfo;

	if (debug_info == NULL)
		return;
	CCCI_NORMAL_LOG(md->index, KERN, "ccci_md_exp_change, ee_case(0x%x)\n", debug_info->more_info);

	ee_case = debug_info->more_info;
	memset(debug_info, 0, sizeof(DEBUG_INFO_T));
	debug_info->more_info = ee_case;
	off_core_num = 0;
	ex_overview = (EX_OVERVIEW_T *) md->smem_layout.ccci_exp_smem_mdss_debug_vir;

	if ((debug_info->more_info == MD_EE_CASE_NORMAL) && (md->boot_stage == MD_BOOT_STAGE_0) &&
		(md->flight_mode != MD_FIGHT_MODE_ENTER)) {
		ex_PLloginfo = ex_pl_info;
		core_id = MD_CORE_NUM;
		off_core_num++;
		snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "(MCU_PCORE)");
		goto PL_CORE_PROC;
	}

	for (core_id = 0; core_id < MD_CORE_NUM; core_id++) {
		CCCI_DEBUG_LOG(md->index, KERN, "core_id(%x/%x): offset=%x, if_offender=%d, %s\n", (core_id + 1),
			     ex_overview->core_num, ex_overview->main_reson[core_id].core_offset,
			     ex_overview->main_reson[core_id].is_offender, ex_overview->main_reson[core_id].core_name);
		if (ex_overview->main_reson[core_id].is_offender) {
			off_core_num++;

			
			debug_info->core_name[0] = '(';
			for (temp_i = 1; temp_i < MD_CORE_NAME_LEN; temp_i++) {
				debug_info->core_name[temp_i] = ex_overview->main_reson[core_id].core_name[temp_i - 1];
				if (debug_info->core_name[temp_i] == '\0')
					break;
			}
			debug_info->core_name[temp_i++] = ')';
			debug_info->core_name[temp_i] = '\0';
			CCCI_NORMAL_LOG(md->index, KERN, "core_id(0x%x/%d), %s\n", core_id, off_core_num,
				     debug_info->core_name);
			ex_pl_info->envinfo.ELM_status = 0;
			switch (core_id) {
			case MD_PCORE:
			case MD_L1CORE:
				ex_PLloginfo =
				    (EX_PL_LOG_T *) ((char *)ex_overview +
						     ex_overview->main_reson[core_id].core_offset);
				ex_pl_info->envinfo.ELM_status = ex_PLloginfo->envinfo.ELM_status;
PL_CORE_PROC:
				ee_type = ex_PLloginfo->header.ex_type;
				debug_info->type = ee_type;
				ee_case = ee_type;
				CCCI_NORMAL_LOG(md->index, KERN, "PL ex type(0x%x)\n", ee_type);
				switch (ee_type) {
				case MD_EX_PL_INVALID:
					debug_info->name = "INVALID";
					break;
				case MD_EX_CC_INVALID_EXCEPTION:
					
				case MD_EX_CC_PCORE_EXCEPTION:
					
				case MD_EX_CC_L1CORE_EXCEPTION:
					
				case MD_EX_CC_CS_EXCEPTION:
					
				case MD_EX_CC_MD32_EXCEPTION:
					
				case MD_EX_CC_C2K_EXCEPTION:
					
				case MD_EX_CC_ARM7_EXCEPTION:
					ee_type = ee_type - MD_EX_CC_INVALID_EXCEPTION + MD_EX_PL_FATALE_TOTAL;
					
				case MD_EX_PL_UNDEF:
					
				case MD_EX_PL_SWI:
					
				case MD_EX_PL_PREF_ABT:
					
				case MD_EX_PL_DATA_ABT:
					
				case MD_EX_PL_STACKACCESS:
					
				case MD_EX_PL_FATALERR_TASK:
					
				case MD_EX_PL_FATALERR_BUF:
					
					
					if ((core_id == MD_CORE_NUM) && (ee_type ==
							(MD_EX_CC_C2K_EXCEPTION  - MD_EX_CC_INVALID_EXCEPTION +
							MD_EX_PL_FATALE_TOTAL)))
						debug_info->type = MD_EX_CC_C2K_EXCEPTION;
					else
						debug_info->type = MD_EX_DUMP_3P_EX;
					debug_info->name = md_ee_plstr[ee_type];
					if (ex_PLloginfo->content.fatalerr.ex_analy.owner[0] != 0xCC) {
						strmncopy(ex_PLloginfo->content.fatalerr.ex_analy.owner,
							debug_info->fatal_error.offender,
							sizeof(ex_PLloginfo->content.fatalerr.ex_analy.owner),
							sizeof(debug_info->fatal_error.offender));
						CCCI_NORMAL_LOG(md->index, KERN, "offender: %s\n",
							     debug_info->fatal_error.offender);
					}
					debug_info->fatal_error.err_code1 =
					    ex_PLloginfo->content.fatalerr.error_code.code1;
					debug_info->fatal_error.err_code2 =
					    ex_PLloginfo->content.fatalerr.error_code.code2;
					debug_info->fatal_error.err_code3 =
					    ex_PLloginfo->content.fatalerr.error_code.code3;
					if (ex_PLloginfo->content.fatalerr.ex_analy.is_cadefa_sup == 0x01)
						debug_info->fatal_error.ExStr = "CaDeFa Supported\n";
					else
						debug_info->fatal_error.ExStr = "";
					break;
				case MD_EX_PL_ASSERT_FAIL:
					
				case MD_EX_PL_ASSERT_DUMP:
					
				case MD_EX_PL_ASSERT_NATIVE:
					debug_info->type = MD_EX_DUMP_ASSERT;
					debug_info->name = "ASSERT";
					CCCI_DEBUG_LOG(md->index, KERN, "p filename1(%s)\n",
						ex_PLloginfo->content.assert.filepath);
					strmncopy(ex_PLloginfo->content.assert.filepath,
						debug_info->assert.file_name,
						sizeof(ex_PLloginfo->content.assert.filepath),
						sizeof(debug_info->assert.file_name));
					CCCI_DEBUG_LOG(md->index, KERN,
						"p filename2:(%s)\n", debug_info->assert.file_name);
					debug_info->assert.line_num = ex_PLloginfo->content.assert.linenumber;
					debug_info->assert.parameters[0] = ex_PLloginfo->content.assert.para[0];
					debug_info->assert.parameters[1] = ex_PLloginfo->content.assert.para[1];
					debug_info->assert.parameters[2] = ex_PLloginfo->content.assert.para[2];
					break;

				case EMI_MPU_VIOLATION:
					debug_info->type = MD_EX_DUMP_EMI_CHECK;
					ee_case = MD_EX_TYPE_EMI_CHECK;
					debug_info->name = "Fatal error (rmpu violation)";
					debug_info->fatal_error.err_code1 =
					    ex_PLloginfo->content.fatalerr.error_code.code1;
					debug_info->fatal_error.err_code2 =
					    ex_PLloginfo->content.fatalerr.error_code.code2;
					debug_info->fatal_error.err_code3 =
					    ex_PLloginfo->content.fatalerr.error_code.code3;
					debug_info->fatal_error.ExStr = "EMI MPU VIOLATION\n";
					break;
				default:
					debug_info->name = "UNKNOWN Exception";
					break;
				}
				debug_info->ext_mem = ex_PLloginfo;
				debug_info->ext_size = sizeof(EX_PL_LOG_T);
				break;
			case MD_CS_ICC:
				
			case MD_CS_IMC:
				
			case MD_CS_MPC:
				ex_csLogInfo =
				    (EX_CS_LOG_T *) ((char *)ex_overview +
						     ex_overview->main_reson[core_id].core_offset);
				ee_type = ex_csLogInfo->except_type;
				CCCI_NORMAL_LOG(md->index, KERN, "cs ex type(0x%x)\n", ee_type);
				switch (ee_type) {
				case CS_EXCEPTION_ASSERTION:
					debug_info->type = MD_EX_DUMP_ASSERT;
					ee_case = MD_EX_TYPE_ASSERT;

					debug_info->name = "ASSERT";
					strmncopy(ex_csLogInfo->except_content.assert.file_name,
						debug_info->assert.file_name,
						sizeof(ex_csLogInfo->except_content.assert.file_name),
						sizeof(debug_info->assert.file_name));
					debug_info->assert.line_num = ex_csLogInfo->except_content.assert.line_num;
					debug_info->assert.parameters[0] = ex_csLogInfo->except_content.assert.para1;
					debug_info->assert.parameters[1] = ex_csLogInfo->except_content.assert.para2;
					debug_info->assert.parameters[2] = ex_csLogInfo->except_content.assert.para3;
					break;
				case CS_EXCEPTION_FATAL_ERROR:
					debug_info->type = MD_EX_DUMP_2P_EX;
					ee_case = MD_EX_TYPE_FATALERR_TASK;

					debug_info->name = "Fatal error";
					debug_info->fatal_error.err_code1 =
					    ex_csLogInfo->except_content.fatalerr.error_code1;
					debug_info->fatal_error.err_code2 =
					    ex_csLogInfo->except_content.fatalerr.error_code2;
					break;
				case CS_EXCEPTION_CTI_EVENT:
					debug_info->name = "CC CTI Exception";
					break;
				case CS_EXCEPTION_UNKNOWN:
				default:
					debug_info->name = "UNKNOWN Exception";
					break;
				}
				debug_info->ext_mem = ex_csLogInfo;
				debug_info->ext_size = sizeof(EX_CS_LOG_T);
				break;
			case MD_MD32_DFE:
				
			case MD_MD32_BRP:
				
			case MD_MD32_RAKE:
				ex_md32LogInfo = (EX_MD32_LOG_T *) ((char *)ex_overview +
						       ex_overview->main_reson[core_id].core_offset);
				ee_type = ex_md32LogInfo->except_type;
				CCCI_NORMAL_LOG(md->index, KERN, "md32 ex type(0x%x), name: %s\n", ee_type,
					     ex_md32LogInfo->except_content.assert.file_name);
				switch (ex_md32LogInfo->md32_active_mode) {
				case 1:
					snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "%s%s",
						debug_info->core_name, MD32_FDD_ROCODE);
					break;
				case 2:
					snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "%s%s",
						debug_info->core_name, MD32_TDD_ROCODE);
					break;
				default:
					break;
				}
				switch (ee_type) {
				case CMIF_MD32_EX_ASSERT_LINE:
					
				case CMIF_MD32_EX_ASSERT_EXT:
					debug_info->type = MD_EX_DUMP_ASSERT;
					ee_case = MD_EX_TYPE_ASSERT;
					debug_info->name = "ASSERT";
					strmncopy(ex_md32LogInfo->except_content.assert.file_name,
						debug_info->assert.file_name,
						sizeof(ex_md32LogInfo->except_content.assert.file_name),
						sizeof(debug_info->assert.file_name));
					debug_info->assert.line_num = ex_md32LogInfo->except_content.assert.line_num;
					debug_info->assert.parameters[0] =
					    ex_md32LogInfo->except_content.assert.ex_code[0];
					debug_info->assert.parameters[1] =
					    ex_md32LogInfo->except_content.assert.ex_code[1];
					debug_info->assert.parameters[2] =
					    ex_md32LogInfo->except_content.assert.ex_code[2];
					break;
				case CMIF_MD32_EX_FATAL_ERROR:
					
				case CMIF_MD32_EX_FATAL_ERROR_EXT:
					debug_info->type = MD_EX_DUMP_2P_EX;
					ee_case = MD_EX_TYPE_FATALERR_TASK;

					debug_info->name = "Fatal error";
					debug_info->fatal_error.err_code1 =
					    ex_md32LogInfo->except_content.fatalerr.ex_code[0];
					debug_info->fatal_error.err_code2 =
					    ex_md32LogInfo->except_content.fatalerr.ex_code[1];
					break;
				case CS_EXCEPTION_CTI_EVENT:
					
				case CS_EXCEPTION_UNKNOWN:
				default:
					debug_info->name = "UNKNOWN Exception";
					break;
				}
				debug_info->ext_mem = ex_md32LogInfo;
				debug_info->ext_size = sizeof(EX_MD32_LOG_T);
				break;
			}
			if (off_core_num < MD_CORE_NUM) {
				debug_info->md_image = (void *)md->mem_layout.md_region_vir;
				debug_info->md_size = MD_IMG_DUMP_SIZE;

				debug_info = &md->debug_info1[off_core_num - 1];
				memset(debug_info, 0, sizeof(DEBUG_INFO_T));
				if (off_core_num == 1)
					md->ex_type = ee_case;
			}

		}
	}

	if (off_core_num == 0) {
		ex_PLloginfo = (EX_PL_LOG_T *) ((char *)ex_overview +
				ex_overview->main_reson[MD_PCORE].core_offset);
		ex_pl_info->envinfo.ELM_status = ex_PLloginfo->envinfo.ELM_status;
		off_core_num++;
		core_id = MD_CORE_NUM;
		snprintf(debug_info->core_name, MD_CORE_NAME_DEBUG, "(MCU_PCORE)");
		goto PL_CORE_PROC;
	}
	md->ex_core_num = off_core_num;
	CCCI_NORMAL_LOG(md->index, KERN, "core_ex_num(%d/%d)\n", off_core_num, md->ex_core_num);
}
#endif

static void ccci_ee_info_dump(struct ccci_modem *md)
{
	char *ex_info;
	char *i_bit_ex_info = NULL;
	char buf_fail[] = "Fail alloc mem for exception\n";
	int db_opt = (DB_OPT_DEFAULT | DB_OPT_FTRACE);
	int dump_flag = 0;
	DEBUG_INFO_T *debug_info = &md->debug_info;
	unsigned char c;

	struct rtc_time tm;
	struct timeval tv = { 0 };
	struct timeval tv_android = { 0 };
	struct rtc_time tm_android;

	do_gettimeofday(&tv);
	tv_android = tv;
	rtc_time_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tv_android.tv_sec, &tm_android);
	CCCI_NORMAL_LOG(md->index, KERN, "Sync:%d%02d%02d %02d:%02d:%02d.%u(%02d:%02d:%02d.%03d(TZone))\n",
		     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     (unsigned int)tv.tv_usec,
		     tm_android.tm_hour, tm_android.tm_min, tm_android.tm_sec, (unsigned int)tv_android.tv_usec);

	ex_info = kmalloc(EE_BUF_LEN, GFP_ATOMIC);
	if (ex_info == NULL) {
		CCCI_ERROR_LOG(md->index, KERN, "Fail alloc Mem for ex_info!\n");
		goto err_exit;
	}
	CCCI_NORMAL_LOG(md->index, KERN, "exception type(%d):%s\n", debug_info->type, debug_info->name ? : "Unknown");

	switch (debug_info->type) {
	case MD_EX_TYPE_ASSERT_DUMP:
		
	case MD_EX_TYPE_ASSERT:
		CCCI_NORMAL_LOG(md->index, KERN, "filename = %s\n", debug_info->assert.file_name);
		CCCI_NORMAL_LOG(md->index, KERN, "line = %d\n", debug_info->assert.line_num);
		CCCI_NORMAL_LOG(md->index, KERN, "para0 = %d, para1 = %d, para2 = %d\n",
			     debug_info->assert.parameters[0],
			     debug_info->assert.parameters[1], debug_info->assert.parameters[2]);
		snprintf(ex_info, EE_BUF_LEN, "\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n",
			 debug_info->name,
			 debug_info->assert.file_name,
			 debug_info->assert.line_num,
			 debug_info->assert.parameters[0],
			 debug_info->assert.parameters[1], debug_info->assert.parameters[2]);
#ifdef CONFIG_HTC_MODEM_EXCEPTION_INFO_EXPORT
		memset(ccci_modem_ex_info_msg[SLOT_MD3], 0, CCCI_MODEM_EX_INFO_MAX_SIZE);
		snprintf(ccci_modem_ex_info_msg[SLOT_MD3], CCCI_MODEM_EX_INFO_MAX_SIZE, "%d%02d%02d %02d:%02d:%02d.%u, exception type(%d):%s, %s, %d, %d, %d, %d\n",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			 tm.tm_hour, tm.tm_min, tm.tm_sec,
			 (unsigned int) tv.tv_usec,
			 debug_info->type,debug_info->name?:"Unknown",
			 debug_info->assert.file_name,
			 debug_info->assert.line_num,
			 debug_info->assert.parameters[0],
			 debug_info->assert.parameters[1],
			 debug_info->assert.parameters[2]);
#endif
		break;
	case MD_EX_TYPE_UNDEF:
		
	case MD_EX_TYPE_SWI:
		
	case MD_EX_TYPE_PREF_ABT:
		
	case MD_EX_TYPE_DATA_ABT:
		
	case MD_EX_TYPE_FATALERR_BUF:
		
	case MD_EX_TYPE_FATALERR_TASK:
		
	case MD_EX_TYPE_C2K_ERROR:
		CCCI_NORMAL_LOG(md->index, KERN, "fatal error code 1 = %d\n", debug_info->fatal_error.err_code1);
		CCCI_NORMAL_LOG(md->index, KERN, "fatal error code 2 = %d\n", debug_info->fatal_error.err_code2);
		CCCI_NORMAL_LOG(md->index, KERN, "fatal error offender %s\n", debug_info->fatal_error.offender);
		if (debug_info->fatal_error.offender[0] != '\0') {
			snprintf(ex_info, EE_BUF_LEN, "\n[%s] err_code1:%d err_code2:%d\nMD Offender:%s\n",
				 debug_info->name, debug_info->fatal_error.err_code1, debug_info->fatal_error.err_code2,
				 debug_info->fatal_error.offender);
		} else {
			snprintf(ex_info, EE_BUF_LEN, "\n[%s] err_code1:%d err_code2:%d\n", debug_info->name,
				 debug_info->fatal_error.err_code1, debug_info->fatal_error.err_code2);
		}
		break;
	case CC_MD1_EXCEPTION:
		CCCI_NORMAL_LOG(md->index, KERN, "fatal error code 1 = %d\n", debug_info->fatal_error.err_code1);
		CCCI_NORMAL_LOG(md->index, KERN, "fatal error code 2 = %d\n", debug_info->fatal_error.err_code2);
		snprintf(ex_info, EE_BUF_LEN, "\n[%s] err_code1:%d err_code2:%d\n", debug_info->name,
				debug_info->fatal_error.err_code1, debug_info->fatal_error.err_code2);
		break;
	case MD_EX_TYPE_EMI_CHECK:
		CCCI_NORMAL_LOG(md->index, KERN, "md_emi_check: %08X, %08X, %02d, %08X\n",
			     debug_info->data.data0, debug_info->data.data1,
			     debug_info->data.channel, debug_info->data.reserved);
		snprintf(ex_info, EE_BUF_LEN, "\n[emi_chk] %08X, %08X, %02d, %08X\n",
			 debug_info->data.data0, debug_info->data.data1,
			 debug_info->data.channel, debug_info->data.reserved);
		break;
	case DSP_EX_TYPE_ASSERT:
		CCCI_NORMAL_LOG(md->index, KERN, "filename = %s\n", debug_info->dsp_assert.file_name);
		CCCI_NORMAL_LOG(md->index, KERN, "line = %d\n", debug_info->dsp_assert.line_num);
		CCCI_NORMAL_LOG(md->index, KERN, "exec unit = %s\n", debug_info->dsp_assert.execution_unit);
		CCCI_NORMAL_LOG(md->index, KERN, "para0 = %d, para1 = %d, para2 = %d\n",
			     debug_info->dsp_assert.parameters[0],
			     debug_info->dsp_assert.parameters[1], debug_info->dsp_assert.parameters[2]);
		snprintf(ex_info, EE_BUF_LEN, "\n[%s] file:%s line:%d\nexec:%s\np1:%d\np2:%d\np3:%d\n",
			 debug_info->name, debug_info->assert.file_name, debug_info->assert.line_num,
			 debug_info->dsp_assert.execution_unit,
			 debug_info->dsp_assert.parameters[0],
			 debug_info->dsp_assert.parameters[1], debug_info->dsp_assert.parameters[2]);
		break;
	case DSP_EX_TYPE_EXCEPTION:
		CCCI_NORMAL_LOG(md->index, KERN, "exec unit = %s, code1:0x%08x\n",
			     debug_info->dsp_exception.execution_unit, debug_info->dsp_exception.code1);
		snprintf(ex_info, EE_BUF_LEN, "\n[%s] exec:%s code1:0x%08x\n", debug_info->name,
			 debug_info->dsp_exception.execution_unit, debug_info->dsp_exception.code1);
		break;
	case DSP_EX_FATAL_ERROR:
		CCCI_NORMAL_LOG(md->index, KERN, "exec unit = %s\n", debug_info->dsp_fatal_err.execution_unit);
		CCCI_NORMAL_LOG(md->index, KERN, "err_code0 = 0x%08x, err_code1 = 0x%08x\n",
			     debug_info->dsp_fatal_err.err_code[0], debug_info->dsp_fatal_err.err_code[1]);

		snprintf(ex_info, EE_BUF_LEN, "\n[%s] exec:%s err_code1:0x%08x err_code2:0x%08x\n",
			 debug_info->name, debug_info->dsp_fatal_err.execution_unit,
			 debug_info->dsp_fatal_err.err_code[0], debug_info->dsp_fatal_err.err_code[1]);
		break;

	default:		
		snprintf(ex_info, EE_BUF_LEN, "\n[%s]\n", debug_info->name);
		break;
	}

	
	switch (debug_info->more_info) {
	case MD_EE_CASE_ONLY_SWINT:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nOnly SWINT case\n");
		break;
	case MD_EE_CASE_SWINT_MISSING:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nSWINT missing case\n");
		break;
	case MD_EE_CASE_ONLY_EX:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nOnly EX case\n");
		break;
	case MD_EE_CASE_ONLY_EX_OK:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nOnly EX_OK case\n");
		break;
	case MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG:
		i_bit_ex_info = kmalloc(EE_BUF_LEN, GFP_ATOMIC);
		if (i_bit_ex_info == NULL) {
			CCCI_ERROR_LOG(md->index, KERN, "Fail alloc Mem for i_bit_ex_info!\n");
			break;
		}
		snprintf(i_bit_ex_info, EE_BUF_LEN, "\n[Others] May I-Bit dis too long\n%s", ex_info);
		strcpy(ex_info, i_bit_ex_info);
		break;
	case MD_EE_CASE_TX_TRG:
		
	case MD_EE_CASE_ISR_TRG:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\n[Others] May I-Bit dis too long\n");
		break;
	case MD_EE_CASE_NO_RESPONSE:
		
		strcpy(ex_info, "\n[Others] MD long time no response\n");
		db_opt |= DB_OPT_FTRACE;
		break;
	case MD_EE_CASE_WDT:
		strcpy(ex_info, "\n[Others] MD watchdog timeout interrupt\n");
		break;
	default:
		break;
	}

	
	c = md->ex_info.envinfo.ELM_status;
	CCCI_NORMAL_LOG(md->index, KERN, "ELM_status: %x\n", c);
	switch (c) {
	case 0xFF:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nno ELM info\n");
		break;
	case 0xAE:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nELM rlat:FAIL\n");
		break;
	case 0xBE:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nELM wlat:FAIL\n");
		break;
	case 0xDE:
		snprintf(ex_info, EE_BUF_LEN, "%s%s", ex_info, "\nELM r/wlat:PASS\n");
		break;
	default:
		break;
	}
err_exit:
	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD EX log\n");
	if ((md->index == MD_SYS3) || (debug_info->more_info == MD_EE_CASE_NORMAL && md->boot_stage == MD_BOOT_STAGE_0))
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, &md->ex_info, sizeof(EX_LOG_T));
	else
		ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_dump_size);
	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD image memory\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, (void *)md->mem_layout.md_region_vir, MD_IMG_DUMP_SIZE);
	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD layout struct\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, &md->mem_layout, sizeof(struct ccci_mem_layout));
	
	md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);

	if (debug_info->more_info == MD_EE_CASE_NORMAL && md->boot_stage == MD_BOOT_STAGE_0) {
		
		dump_flag = CCCI_AED_DUMP_EX_PKT;
	} else {
		dump_flag = CCCI_AED_DUMP_EX_MEM;
		if (debug_info->more_info == MD_EE_CASE_NO_RESPONSE)
			dump_flag |= CCCI_AED_DUMP_CCIF_REG;
	}
	ccci_update_md_boot_stage(md, MD_BOOT_STAGE_EXCEPTION);
	
	if (debug_info->type == MD_EX_TYPE_C2K_ERROR && debug_info->fatal_error.err_code1 == MD_EX_C2K_FATAL_ERROR)
		CCCI_NORMAL_LOG(md->index, KERN, "C2K EE, No need trigger DB\n");
	else if (debug_info->type == CC_MD1_EXCEPTION)
		CCCI_NORMAL_LOG(md->index, KERN, "MD1 EE, No need trigger DB\n");
	else if (ex_info == NULL)
		ccci_aed(md, dump_flag, buf_fail, db_opt);
	else
		ccci_aed(md, dump_flag, ex_info, db_opt);
	if (debug_info->more_info == MD_EE_CASE_ONLY_SWINT)
		md->ops->dump_info(md, DUMP_FLAG_QUEUE_0 | DUMP_FLAG_CCIF | DUMP_FLAG_CCIF_REG, NULL, 0);

	kfree(ex_info);
	kfree(i_bit_ex_info);
}

static void ccci_md_exception(struct ccci_modem *md)
{
	EX_LOG_T *ex_info;
	int ee_type, ee_case;
	DEBUG_INFO_T *debug_info = &md->debug_info;

	if (debug_info == NULL)
		return;

	if ((md->index == MD_SYS3) ||
	(debug_info->more_info == MD_EE_CASE_NORMAL && md->boot_stage == MD_BOOT_STAGE_0)) {
		ex_info = &md->ex_info;
		CCCI_DEBUG_LOG(md->index, KERN, "Parse ex info from ccci packages\n");
	} else {
		ex_info = (EX_LOG_T *) md->smem_layout.ccci_exp_smem_mdss_debug_vir;
		CCCI_DEBUG_LOG(md->index, KERN, "Parse ex info from shared memory\n");
	}
	ee_case = debug_info->more_info;

	memset(debug_info, 0, sizeof(DEBUG_INFO_T));
	ee_type = ex_info->header.ex_type;
	debug_info->type = ee_type;
	debug_info->more_info = ee_case;
	md->ex_type = ee_type;

	if (*((char *)ex_info + CCCI_EXREC_OFFSET_OFFENDER) != 0xCC) {
		memcpy(debug_info->fatal_error.offender, (char *)ex_info + CCCI_EXREC_OFFSET_OFFENDER,
		       sizeof(debug_info->fatal_error.offender) - 1);
		debug_info->fatal_error.offender[sizeof(debug_info->fatal_error.offender) - 1] = '\0';
	} else {
		debug_info->fatal_error.offender[0] = '\0';
	}

	switch (ee_type) {
	case MD_EX_TYPE_INVALID:
		debug_info->name = "INVALID";
		break;

	case MD_EX_TYPE_UNDEF:
		debug_info->name = "Fatal error (undefine)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_SWI:
		debug_info->name = "Fatal error (swi)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_PREF_ABT:
		debug_info->name = "Fatal error (prefetch abort)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_DATA_ABT:
		debug_info->name = "Fatal error (data abort)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_ASSERT:
		debug_info->name = "ASSERT";
		snprintf(debug_info->assert.file_name, sizeof(debug_info->assert.file_name),
			 ex_info->content.assert.filename);
		debug_info->assert.line_num = ex_info->content.assert.linenumber;
		debug_info->assert.parameters[0] = ex_info->content.assert.parameters[0];
		debug_info->assert.parameters[1] = ex_info->content.assert.parameters[1];
		debug_info->assert.parameters[2] = ex_info->content.assert.parameters[2];
		break;

	case MD_EX_TYPE_FATALERR_TASK:
		debug_info->name = "Fatal error (task)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;
	case MD_EX_TYPE_C2K_ERROR:
		debug_info->name = "Fatal error (C2K_EXP)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;
	case CC_MD1_EXCEPTION:
		debug_info->name = "Fatal error (LTE_EXP)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;
	case MD_EX_TYPE_FATALERR_BUF:
		debug_info->name = "Fatal error (buff)";
		debug_info->fatal_error.err_code1 = ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 = ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_LOCKUP:
		debug_info->name = "Lockup";
		break;

	case MD_EX_TYPE_ASSERT_DUMP:
		debug_info->name = "ASSERT DUMP";
		snprintf(debug_info->assert.file_name, sizeof(debug_info->assert.file_name),
			 ex_info->content.assert.filename);
		debug_info->assert.line_num = ex_info->content.assert.linenumber;
		break;

	case DSP_EX_TYPE_ASSERT:
		debug_info->name = "MD DMD ASSERT";
		snprintf(debug_info->dsp_assert.file_name, sizeof(debug_info->dsp_assert.file_name),
			 ex_info->content.assert.filename);
		debug_info->dsp_assert.line_num = ex_info->content.assert.linenumber;
		snprintf(debug_info->dsp_assert.execution_unit, sizeof(debug_info->dsp_assert.execution_unit),
			 ex_info->envinfo.execution_unit);
		debug_info->dsp_assert.parameters[0] = ex_info->content.assert.parameters[0];
		debug_info->dsp_assert.parameters[1] = ex_info->content.assert.parameters[1];
		debug_info->dsp_assert.parameters[2] = ex_info->content.assert.parameters[2];
		break;

	case DSP_EX_TYPE_EXCEPTION:
		debug_info->name = "MD DMD Exception";
		snprintf(debug_info->dsp_exception.execution_unit, sizeof(debug_info->dsp_exception.execution_unit),
			 ex_info->envinfo.execution_unit);
		debug_info->dsp_exception.code1 = ex_info->content.fatalerr.error_code.code1;
		break;

	case DSP_EX_FATAL_ERROR:
		debug_info->name = "MD DMD FATAL ERROR";
		snprintf(debug_info->dsp_fatal_err.execution_unit, sizeof(debug_info->dsp_fatal_err.execution_unit),
			 ex_info->envinfo.execution_unit);
		debug_info->dsp_fatal_err.err_code[0] = ex_info->content.fatalerr.error_code.code1;
		debug_info->dsp_fatal_err.err_code[1] = ex_info->content.fatalerr.error_code.code2;
		break;

	default:
		debug_info->name = "UNKNOWN Exception";
		break;
	}

	debug_info->ext_mem = ex_info;
	debug_info->ext_size = sizeof(EX_LOG_T);
	debug_info->md_image = (void *)md->mem_layout.md_region_vir;
	debug_info->md_size = MD_IMG_DUMP_SIZE;
}

void md_ex_monitor_func(unsigned long data)
{
	int ee_on_going = 0;
	int ee_case;
	int need_update_state = 0;
	unsigned long flags;
	unsigned int ee_info_flag = 0;
	struct ccci_modem *md = (struct ccci_modem *)data;
#if defined(CONFIG_MTK_AEE_FEATURE)
	CCCI_NORMAL_LOG(md->index, KERN, "MD exception timer 1:disable tracing\n");
	tracing_off();
#endif
	CCCI_MEM_LOG_TAG(md->index, KERN, "MD exception timer 1! ee=%x\n", md->ee_info_flag);
	spin_lock_irqsave(&md->ctrl_lock, flags);
	if ((1 << MD_EE_DUMP_ON_GOING) & md->ee_info_flag) {
		ee_on_going = 1;
	} else {
		ee_info_flag = md->ee_info_flag;
		md->ee_info_flag |= (1 << MD_EE_DUMP_ON_GOING);
	}
	spin_unlock_irqrestore(&md->ctrl_lock, flags);

	if (ee_on_going)
		return;

	if ((ee_info_flag & ((1 << MD_EE_MSG_GET) | (1 << MD_EE_OK_MSG_GET) | (1 << MD_EE_SWINT_GET))) ==
	    ((1 << MD_EE_MSG_GET) | (1 << MD_EE_OK_MSG_GET) | (1 << MD_EE_SWINT_GET))) {
		ee_case = MD_EE_CASE_NORMAL;
		CCCI_DEBUG_LOG(md->index, KERN, "Recv SWINT & MD_EX & MD_EX_REC_OK\n");
		if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG))
			ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
	} else if (!(ee_info_flag & (1 << MD_EE_SWINT_GET))
		   && (ee_info_flag & ((1 << MD_EE_MSG_GET) | (1 << MD_EE_OK_MSG_GET)))) {
		ee_case = MD_EE_CASE_SWINT_MISSING;
		CCCI_NORMAL_LOG(md->index, KERN, "SWINT missing, ee_info_flag=%x\n", ee_info_flag);
	} else if ((ee_info_flag & ((1 << MD_EE_MSG_GET) | (1 << MD_EE_SWINT_GET))) & (1 << MD_EE_MSG_GET)) {
		ee_case = MD_EE_CASE_ONLY_EX;
		CCCI_NORMAL_LOG(md->index, KERN, "Only recv SWINT & MD_EX.\n");
		if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG))
			ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
	} else if ((ee_info_flag & ((1 << MD_EE_OK_MSG_GET) | (1 << MD_EE_SWINT_GET))) & (1 << MD_EE_OK_MSG_GET)) {
		ee_case = MD_EE_CASE_ONLY_EX_OK;
		CCCI_NORMAL_LOG(md->index, KERN, "Only recv SWINT & MD_EX_OK\n");
		if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG))
			ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
	} else if (ee_info_flag & (1 << MD_EE_SWINT_GET)) {
		ee_case = MD_EE_CASE_ONLY_SWINT;
		CCCI_NORMAL_LOG(md->index, KERN, "Only recv SWINT.\n");
		if ((ee_info_flag & (1 << MD_STATE_UPDATE)) == 0)
			need_update_state = 1;
	} else if (ee_info_flag & (1 << MD_EE_AP_MASK_I_BIT_TOO_LONG)) {
		ee_case = MD_EE_CASE_AP_MASK_I_BIT_TOO_LONG;
		if ((ee_info_flag & (1 << MD_STATE_UPDATE)) == 0)
			need_update_state = 1;
	} else if (ee_info_flag & (1 << MD_EE_FOUND_BY_ISR)) {
		ee_case = MD_EE_CASE_ISR_TRG;
		if ((ee_info_flag & (1 << MD_STATE_UPDATE)) == 0)
			need_update_state = 1;
	} else if (ee_info_flag & (1 << MD_EE_FOUND_BY_TX)) {
		ee_case = MD_EE_CASE_TX_TRG;
		if ((ee_info_flag & (1 << MD_STATE_UPDATE)) == 0)
			need_update_state = 1;
	} else if (ee_info_flag & (1 << MD_EE_PENDING_TOO_LONG)) {
		ee_case = MD_EE_CASE_NO_RESPONSE;
		if ((ee_info_flag & (1 << MD_STATE_UPDATE)) == 0)
			need_update_state = 1;
	} else if (ee_info_flag & (1 << MD_EE_WDT_GET)) {
		ee_case = MD_EE_CASE_WDT;
		if ((ee_info_flag & (1 << MD_STATE_UPDATE)) == 0)
			need_update_state = 1;
	} else {
		CCCI_ERROR_LOG(md->index, KERN, "Invalid MD_EX, ee_info=%x\n", ee_info_flag);
		goto _dump_done;
	}

	if (need_update_state) {
		md->config.setting |= MD_SETTING_RELOAD;
		md->ops->broadcast_state(md, EXCEPTION);
	}

	if (md->boot_stage < MD_BOOT_STAGE_2)
		md->ops->broadcast_state(md, BOOT_FAIL);

	
	md->debug_info.more_info = ee_case;
	
	CCCI_MEM_LOG_TAG(md->index, KERN, "Dump MD EX log\n");
#ifdef MD_UMOLY_EE_SUPPORT
	if (md->index == MD_SYS1) {
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
					md->smem_layout.ccci_exp_smem_mdss_debug_vir, (2048 + 512));
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
					(md->smem_layout.ccci_exp_smem_mdss_debug_vir + 6 * 1024), 2048);
	} else
#endif
		ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP,
					md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_dump_size);
	
	md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	spin_lock_irqsave(&md->ctrl_lock, flags);
	if ((1 << MD_EE_PASS_MSG_GET) & md->ee_info_flag)
		CCCI_ERROR_LOG(md->index, KERN, "MD exception timer 2 has been set!\n");
	else if (ee_case == MD_EE_CASE_WDT && md->index == MD_SYS3)
		mod_timer(&md->ex_monitor2, jiffies);
	else
		mod_timer(&md->ex_monitor2, jiffies + EX_TIMER_MD_EX_REC_OK * HZ);
	spin_unlock_irqrestore(&md->ctrl_lock, flags);

 _dump_done:
	return;
}
EXPORT_SYMBOL(md_ex_monitor_func);

void md_ex_monitor2_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	unsigned long flags;
	int md_wdt_ee = 0;
	int ee_on_going = 0;
	struct ccci_modem *another_md;

	CCCI_ERROR_LOG(md->index, KERN, "MD exception timer 2! ee=%x\n", md->ee_info_flag);
	CCCI_MEM_LOG_TAG(md->index, KERN, "MD exception timer 2!\n");

	spin_lock_irqsave(&md->ctrl_lock, flags);
	if ((1 << MD_EE_TIMER2_DUMP_ON_GOING) & md->ee_info_flag)
		ee_on_going = 1;
	else
		md->ee_info_flag |= (1 << MD_EE_TIMER2_DUMP_ON_GOING);
	if ((1 << MD_EE_WDT_GET) & md->ee_info_flag)
		md_wdt_ee = 1;
	spin_unlock_irqrestore(&md->ctrl_lock, flags);

	if (ee_on_going)
		return;

#ifdef MD_UMOLY_EE_SUPPORT
	if ((md->index == MD_SYS1)
) {
		ccci_md_exp_change(md);
		ccci_md_ee_info_dump(md);
	} else {
#endif
		ccci_md_exception(md);
		ccci_ee_info_dump(md);
#ifdef MD_UMOLY_EE_SUPPORT
	}
#endif
	
	another_md = ccci_get_another_modem(md->index);
	if (another_md && another_md->boot_stage == MD_BOOT_STAGE_1)
		another_md->ops->dump_info(another_md, DUMP_FLAG_CCIF, NULL, 0);

	spin_lock_irqsave(&md->ctrl_lock, flags);
	md->ee_info_flag = 0;	
	spin_unlock_irqrestore(&md->ctrl_lock, flags);

	CCCI_MEM_LOG_TAG(md->index, KERN, "Enable WDT at exception exit.\n");
	md->ops->ee_callback(md, EE_FLAG_ENABLE_WDT);
	if (md_wdt_ee && md->index == MD_SYS3) {
		CCCI_ERROR_LOG(md->index, KERN, "trigger force assert after WDT EE.\n");
		md->ops->force_assert(md, CCIF_INTERRUPT);
	}
}
EXPORT_SYMBOL(md_ex_monitor2_func);

void md_bootup_timeout_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	char ex_info[EE_BUF_LEN] = "";

	if (md->boot_stage == MD_BOOT_STAGE_2 ||
		md->boot_stage == MD_BOOT_STAGE_EXCEPTION) {
		CCCI_ERROR_LOG(md->index, KERN, "MD bootup timer false alarm!\n");
		return;
	}

	CCCI_ERROR_LOG(md->index, KERN, "MD_BOOT_HS%d_FAIL!\n", (md->boot_stage + 1));
	md->ops->broadcast_state(md, BOOT_FAIL);
	if (md->config.setting & MD_SETTING_STOP_RETRY_BOOT)
		return;

	
	snprintf(ex_info, EE_BUF_LEN, "\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", (md->boot_stage + 1));
	CCCI_NORMAL_LOG(md->index, KERN, "Dump MD image memory\n");
	ccci_mem_dump(md->index, (void *)md->mem_layout.md_region_vir, MD_IMG_DUMP_SIZE);
	CCCI_NORMAL_LOG(md->index, KERN, "Dump MD layout struct\n");
	ccci_mem_dump(md->index, &md->mem_layout, sizeof(struct ccci_mem_layout));
	md->ops->dump_info(md, DUMP_FLAG_QUEUE_0_1, NULL, 0);

	if (md->boot_stage == MD_BOOT_STAGE_0) {
		md->ops->dump_info(md, DUMP_MD_BOOTUP_STATUS, NULL, 0);
		
#ifndef MD_UMOLY_EE_SUPPORT
		if (md->flight_mode)
			md->flight_mode = MD_FIGHT_MODE_NONE;
#else
		if (md->flight_mode) {
			md->flight_mode = MD_FIGHT_MODE_NONE;
			ccci_aed(md, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_MD_IMG_MEM | CCCI_AED_DUMP_EX_MEM,
				ex_info, DB_OPT_DEFAULT);
		} else
#endif
			ccci_aed(md, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_MD_IMG_MEM, ex_info, DB_OPT_DEFAULT);
	} else if (md->boot_stage == MD_BOOT_STAGE_1) {
		
		CCCI_NORMAL_LOG(md->index, KERN, "Dump MD EX log\n");
		ccci_mem_dump(md->index, md->smem_layout.ccci_exp_smem_base_vir,
						md->smem_layout.ccci_exp_dump_size);

		ccci_aed(md, CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_EX_MEM, ex_info, DB_OPT_FTRACE);
	}
}
EXPORT_SYMBOL(md_bootup_timeout_func);

void ccci_subsys_kernel_init(void)
{
#ifdef FEATURE_GET_MD_EINT_ATTR_DTS
	get_dtsi_eint_node();
#endif
}