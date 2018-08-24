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

#include "xhci.h"
#include "xhci-mtk-power.h"
#include "xhci-mtk-driver.h"
#include <linux/kernel.h>	
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/types.h>
#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
#include <linux/iopoll.h>
#ifdef CONFIG_PROJECT_PHY
#include <mtk-phy-asic.h>
#endif
#endif




#define mtk_power_log(fmt, args...) \
	pr_debug("%s(%d): " fmt, __func__, __LINE__, ##args)


static bool wait_for_value(unsigned long addr, int msk, int value, int ms_intvl, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if ((readl((void __iomem *)addr) & msk) == value)
			return true;
		mdelay(ms_intvl);
	}

	return false;
}

static void mtk_chk_usb_ip_ck_sts(struct xhci_hcd *xhci)
{
	int ret;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));

	ret =
	    wait_for_value(_SSUSB_IP_PW_STS1(xhci->sif_regs), SSUSB_SYS125_RST_B_STS,
			   SSUSB_SYS125_RST_B_STS, 1, 10);
	if (ret == false)
		mtk_xhci_mtk_printk(K_DEBUG, "sys125_ck is still active!!!\n");

	
	if (num_u2_port
	    && !(readl((void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, 0)) & SSUSB_U2_PORT_PDN)) {
		ret =
		    wait_for_value(_SSUSB_IP_PW_STS2(xhci->sif_regs), SSUSB_U2_MAC_SYS_RST_B_STS,
				   SSUSB_U2_MAC_SYS_RST_B_STS, 1, 10);
		if (ret == false)
			mtk_xhci_mtk_printk(K_DEBUG, "mac2_sys_ck is still active!!!\n");
	}

	
	if (num_u3_port
	    && !(readl((void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, 0)) & SSUSB_U3_PORT_PDN)) {
		ret =
		    wait_for_value(_SSUSB_IP_PW_STS1(xhci->sif_regs), SSUSB_U3_MAC_RST_B_STS,
				   SSUSB_U3_MAC_RST_B_STS, 1, 10);
		if (ret == false)
			mtk_xhci_mtk_printk(K_DEBUG, "mac3_mac_ck is still active!!!\n");
	}
}

void enableXhciAllPortPower(struct xhci_hcd *xhci)
{
	int i;
	u32 port_id, temp;
	u32 __iomem *addr;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));

	pr_debug("%s(%d): port number, u3-%d, u2-%d\n",
		__func__, __LINE__, num_u3_port, num_u2_port);

	for (i = 1; i <= num_u3_port; i++) {
		port_id = i;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp |= PORT_POWER;
		writel(temp, addr);
		while (!(readl(addr) & PORT_POWER))
			;
	}

	for (i = 1; i <= num_u2_port; i++) {
		port_id = i + num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp |= PORT_POWER;
		writel(temp, addr);
		while (!(readl(addr) & PORT_POWER))
			;
	}
}

void disableXhciAllPortPower(struct xhci_hcd *xhci)
{
	int i;
	u32 port_id, temp;
	void __iomem *addr;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));

	mtk_xhci_mtk_printk(K_DEBUG, "port number, u3-%d, u2-%d\n", num_u3_port, num_u2_port);

	for (i = 1; i <= num_u3_port; i++) {
		port_id = i;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		 writel(temp, addr);
		while (readl(addr) & PORT_POWER)
			;
	}

	for (i = 1; i <= num_u2_port; i++) {
		port_id = i + num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS * ((port_id - 1) & 0xff);
		temp = readl(addr);
		temp = xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		writel(temp, addr);
		while (readl(addr) & PORT_POWER)
			;
	}

	xhci_print_registers(mtk_xhci);
}

void enableAllClockPower(struct xhci_hcd *xhci, bool is_reset)
{
	int i;
	u32 temp;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP((xhci->sif_regs))));

	mtk_xhci_mtk_printk(K_DEBUG, "%s(%d): u3 port number = %d\n", __func__, __LINE__, num_u3_port);
	mtk_xhci_mtk_printk(K_DEBUG, "%s(%d): u2 port number = %d\n", __func__, __LINE__, num_u2_port);

	
	if (is_reset) {
		#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
		
		writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL_2(xhci->sif_regs)) | (SSUSB_IPDEV_PDN),
			(void __iomem *)_SSUSB_IP_PW_CTRL_2(xhci->sif_regs));
		udelay(50);
		#endif 
		writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs)) | (SSUSB_IP_SW_RST),
		       (void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs));
		writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs)) &
		       (~SSUSB_IP_SW_RST), (void __iomem *)_SSUSB_IP_PW_CTRL(xhci->sif_regs));
	}

	
	writel(readl((void __iomem *)_SSUSB_IP_PW_CTRL_1(xhci->sif_regs)) & (~SSUSB_IP_PDN),
	       (void __iomem *)_SSUSB_IP_PW_CTRL_1(xhci->sif_regs));

	
	for (i = 0; i < num_u3_port; i++) {
		temp = readl((void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
		temp =
		    (temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS)) | (SSUSB_U3_PORT_HOST_SEL);
		writel(temp, (void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
	}

#if 0
	temp = readl(SSUSB_U3_CTRL(0));
	temp = temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS) & (~SSUSB_U3_PORT_HOST_SEL);
	writel(temp, SSUSB_U3_CTRL(0));
#endif

	
	for (i = 0; i < num_u2_port; i++) {
		temp = readl((void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
		temp =
		    (temp & (~SSUSB_U2_PORT_PDN) & (~SSUSB_U2_PORT_DIS)) | (SSUSB_U2_PORT_HOST_SEL);
		writel(temp, (void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
		if(i >= num_u3_port) {
			temp = readl((void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
			temp = temp | ((SSUSB_U3_PORT_PDN) | (SSUSB_U3_PORT_DIS)| (SSUSB_U2_PORT_HOST_SEL));
			writel(temp, (void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
		}
#endif 
	}
	
	mtk_chk_usb_ip_ck_sts(xhci);
}

#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
int xhci_mtk_host_enable(struct xhci_hcd *xhci)
{
	u32 value, check_val;
	int ret;
	int i;
	int num_u3_port;
	int num_u2_port;

#ifdef CONFIG_PROJECT_PHY
	usb_xhci_suspend_setting(false);
#endif

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP((xhci->sif_regs))));
	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN),
	       (void __iomem *)SSUSB_IP_PW_CTRL_1);

	
	for (i = 0; i < num_u3_port; i++) {
		value = readl((void __iomem *)(void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		value |= SSUSB_U3_PORT_HOST_SEL;
		writel(value, (void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
	}

	
	for (i = 0; i < num_u2_port; i++) {
		value = readl((void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
		value &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		value |= SSUSB_U2_PORT_HOST_SEL;
		writel(value, (void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
	}

	check_val = SSUSB_SYSPLL_STABLE | SSUSB_REF_RST_B_STS |
			SSUSB_SYS125_RST_B_STS | SSUSB_XHCI_RST_B_STS;
		ret = readl_poll_timeout_atomic((void __iomem *)_SSUSB_IP_PW_STS1(xhci->sif_regs), value,
				(check_val == (value & check_val)), 100, 20000);
	if (ret) {
		mtk_xhci_mtk_printk(K_ERR, "clocks are not stable (0x%x)\n", value);
		return ret;
	}
	return 0;
}

int xhci_mtk_host_disable(struct xhci_hcd *xhci)
{
	u32 value;
	int ret;
	int i;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP(xhci->sif_regs)));
	num_u2_port = SSUSB_U2_PORT_NUM(readl((void __iomem *)_SSUSB_IP_CAP((xhci->sif_regs))));

	
	for (i = 0; i < num_u3_port; i++) {
		value = readl((void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
		value |= SSUSB_U3_PORT_PDN;
		writel(value, (void __iomem *)_SSUSB_U3_CTRL(xhci->sif_regs, i));
	}

	
	for (i = 0; i < num_u2_port; i++) {
		value = readl((void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
		value |= SSUSB_U2_PORT_PDN;
		writel(value, (void __iomem *)_SSUSB_U2_CTRL(xhci->sif_regs, i));
	}

	
	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL_1) |SSUSB_IP_PDN,
	       (void __iomem *)SSUSB_IP_PW_CTRL_1);

	
	ret = readl_poll_timeout_atomic((void __iomem *)_SSUSB_IP_PW_STS1(xhci->sif_regs), value,
				(value & SSUSB_IP_SLEEP_STS), 100, 100000);
	if (ret) {
		mtk_xhci_mtk_printk(K_ERR, "ip sleep failed!!! =%d\n",
			readl((void __iomem *)_SSUSB_IP_PW_STS1(xhci->sif_regs)));
		return ret;
	}
#ifdef CONFIG_PROJECT_PHY
	usb_xhci_suspend_setting(true);
#endif
	return 0;
}
#endif 

#if 0
void disableAllClockPower(void)
{
	int i;
	u32 temp;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));

	
	for (i = 0; i < num_u3_port; i++) {
		temp = readl((void __iomem *)SSUSB_U3_CTRL(i));
		temp = temp | SSUSB_U3_PORT_PDN & (~SSUSB_U3_PORT_HOST_SEL);
		writel(temp, (void __iomem *)SSUSB_U3_CTRL(i));
	}

	for (i = 0; i < num_u2_port; i++) {
		temp = readl((void __iomem *)SSUSB_U2_CTRL(i));
		temp = temp | SSUSB_U2_PORT_PDN & (~SSUSB_U2_PORT_HOST_SEL);
		writel(temp, (void __iomem *)SSUSB_U2_CTRL(i));
	}

	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL_1) | (SSUSB_IP_PDN),
	       (void __iomem *)SSUSB_IP_PW_CTRL_1);
	
	mtk_chk_usb_ip_ck_sts();
}
#endif

#if 0
void disablePortClockPower(int port_index, int port_rev)
{
	u32 temp;
	int real_index;

	real_index = port_index;

	if (port_rev == 0x3) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
		temp = temp | (SSUSB_U3_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
	} else if (port_rev == 0x2) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
		temp = temp | (SSUSB_U2_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
	}

	writel(readl((void __iomem *)(unsigned long)SSUSB_IP_PW_CTRL_1) | (SSUSB_IP_PDN),
	       (void __iomem *)(unsigned long)SSUSB_IP_PW_CTRL_1);
}

void enablePortClockPower(int port_index, int port_rev)
{
	u32 temp;
	int real_index;

	real_index = port_index;

	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN),
	       (void __iomem *)SSUSB_IP_PW_CTRL_1);

	if (port_rev == 0x3) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
		temp = temp & (~SSUSB_U3_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U3_CTRL(real_index));
	} else if (port_rev == 0x2) {
		temp = readl((void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
		temp = temp & (~SSUSB_U2_PORT_PDN);
		writel(temp, (void __iomem *)(unsigned long)SSUSB_U2_CTRL(real_index));
	}
}
#endif
