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

#ifndef __MODEM_CCIF_H__
#define __MODEM_CCIF_H__

#include <linux/wakelock.h>
#include <linux/dmapool.h>
#include <linux/atomic.h>
#include <mt-plat/mt_ccci_common.h>
#include "ccci_config.h"
#include "ccci_ringbuf.h"
#include "ccci_core.h"

#define QUEUE_NUM   8

struct ccif_sram_layout {
	struct ccci_header dl_header;
	struct md_query_ap_feature md_rt_data;
	struct ccci_header up_header;
	struct ap_query_md_feature ap_rt_data;
};

struct md_ccif_queue {
	DIRECTION dir;
	unsigned char index;
	atomic_t rx_on_going;
	unsigned char debug_id;
	int budget;
	unsigned int ccif_ch;
	struct ccci_modem *modem;
	struct ccci_port *napi_port;
	struct ccci_ringbuf *ringbuf;
	spinlock_t rx_lock;	
	spinlock_t tx_lock;	
	wait_queue_head_t req_wq;	
	struct work_struct qwork;
	struct workqueue_struct *worker;
};

struct md_ccif_ctrl {
	struct md_ccif_queue txq[QUEUE_NUM];
	struct md_ccif_queue rxq[QUEUE_NUM];
	unsigned int total_smem_size;
	atomic_t reset_on_going;

	volatile unsigned long channel_id;	
	unsigned int ccif_irq_id;
	unsigned int md_wdt_irq_id;
	unsigned int sram_size;
	struct ccif_sram_layout *ccif_sram_layout;
	struct wake_lock trm_wake_lock;
	char wakelock_name[32];
	struct work_struct ccif_sram_work;
	struct tasklet_struct ccif_irq_task;
	struct timer_list bus_timeout_timer;
	void __iomem *ccif_ap_base;
	void __iomem *ccif_md_base;
	void __iomem *md_rgu_base;
	void __iomem *md_boot_slave_Vector;
	void __iomem *md_boot_slave_Key;
	void __iomem *md_boot_slave_En;

	struct work_struct wdt_work;
	struct md_hw_info *hw_info;
#ifdef TRAFFIC_MONITOR_INTERVAL_C2K
	unsigned tx_traffic_monitor[QUEUE_NUM];
	unsigned rx_traffic_monitor[QUEUE_NUM];

	struct timer_list traffic_monitor;
	unsigned long traffic_stamp;
#endif

};

static inline void memset_io_raw(void __iomem *dst, int c, size_t count)
{
    
#if defined(__raw_write_logged) && defined(CONFIG_MSM_RTB)
    while (count) {
        count--;
        __raw_writeb_no_log(c, dst);
        dst++;
    }
#else
    memset_io(dst, c, count);
#endif
}

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
extern unsigned long ccci_modem_boot_count[];
#endif				
