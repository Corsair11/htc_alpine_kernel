/*
 * Copyright (C) 2014 mtk
 *
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __LINUX_XHCI_MTK_H
#define __LINUX_XHCI_MTK_H

#include "xhci.h"

#define MTK_SCH_NEW	1

#define SCH_SUCCESS 1
#define SCH_FAIL	0

#define MAX_EP_NUM			64
#define SS_BW_BOUND			51000
#define HS_BW_BOUND			6144

#define USB_EP_CONTROL	0
#define USB_EP_ISOC		1
#define USB_EP_BULK		2
#define USB_EP_INT		3

#define USB_SPEED_LOW	1
#define USB_SPEED_FULL	2
#define USB_SPEED_HIGH	3
#define USB_SPEED_SUPER	5

#define BPKTS(p)	((p) & 0x3f)
#define BCSCOUNT(p)	(((p) & 0x7) << 8)
#define BBM(p)		((p) << 11)
#define BOFFSET(p)	((p) & 0x3fff)
#define BREPEAT(p)	(((p) & 0x7fff) << 16)


#if 1
typedef unsigned int mtk_u32;
typedef unsigned long long mtk_u64;
#endif

#define NULL ((void *)0)

struct mtk_xhci_ep_ctx {
	mtk_u32 ep_info;
	mtk_u32 ep_info2;
	mtk_u64 deq;
	mtk_u32 tx_info;
	
	mtk_u32 reserved[3];
};

struct sch_ep {
	
	int dev_speed;
	int isTT;
	
	int is_in;
	int ep_type;
	int maxp;
	int interval;
	int burst;
	int mult;
	
	int offset;
	int repeat;
	int pkts;
	int cs_count;
	int burst_mode;
	
	int bw_cost;		
	mtk_u32 *ep;		
};

extern int mtk_xhci_scheduler_init(void);
extern int mtk_xhci_scheduler_add_ep(int dev_speed, int is_in, int isTT, int ep_type, int maxp,
			      int interval, int burst, int mult, mtk_u32 *ep, mtk_u32 *ep_ctx,
			      struct sch_ep *sch_ep);
extern struct sch_ep *mtk_xhci_scheduler_remove_ep(int dev_speed, int is_in, int isTT, int ep_type,
					    mtk_u32 *ep);
extern int mtk_xhci_ip_init(struct usb_hcd *hcd, struct xhci_hcd *xhci);
extern void mtk_xhci_vbus_on(struct platform_device *pdev);
extern void mtk_xhci_vbus_off(struct platform_device *pdev);
#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
extern int xhci_mtk_host_enable(struct xhci_hcd *xhci);
extern int xhci_mtk_host_disable(struct xhci_hcd *xhci);
extern void mtk_xhci_wakelock_lock(void);
extern void mtk_xhci_wakelock_unlock(void);
#endif 


#define XHCI_DRIVER_NAME		"xhci"
#define XHCI_BASE_REGS_ADDR_RES_NAME	"ssusb_base"
#define XHCI_SIF_REGS_ADDR_RES_NAME	"ssusb_sif"
#define XHCI_SIF2_REGS_ADDR_RES_NAME	"ssusb_sif2"

#endif 
