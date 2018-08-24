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

#include "mtk-phy.h"

#ifdef CONFIG_PROJECT_PHY
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#else
#include <mach/mt_clkmgr.h>
#include <linux/clk.h>
#endif
#include <asm/io.h>
#include "mtk-phy-asic.h"
#include "mu3d_hal_osal.h"
#ifdef CONFIG_MTK_UART_USB_SWITCH
#include "mu3d_hal_usb_drv.h"
#endif

#ifdef FOR_BRING_UP
#define enable_clock(x, y)
#define disable_clock(x, y)
#define hwPowerOn(x, y, z)
#define hwPowerDown(x, y)
#define set_ada_ssusb_xtal_ck(x)
#endif

#include <mt-plat/upmu_common.h>

#ifdef CONFIG_OF
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#ifndef CONFIG_MTK_CLKMGR
static struct clk *musb_clk;
#endif

#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
#include <linux/io.h>
#endif

bool sib_mode = false;

#define PHY_DISCTH_MIN (0)
#define PHY_DISCTH_MAX (15)
#define PHY_DISCTH_INVALID (-1)
static PHY_INT32 phy_discth = PHY_DISCTH_INVALID;

#ifdef USB_CLK_DEBUG
void __iomem *usb_debug_clk_infracfg_base;
#define MODULE_SW_CG_2_SET	(usb_debug_clk_infracfg_base + 0xa4)
#define MODULE_SW_CG_2_CLR	(usb_debug_clk_infracfg_base + 0xa8)
#define MODULE_SW_CG_2_STA	(usb_debug_clk_infracfg_base + 0xac)
static bool get_clk_io = true;
#endif
static bool usb_enable_clock(bool enable)
{
	if (enable) {
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_PERI_USB0, "USB30");
#else
		clk_enable(musb_clk);
#endif
	} else {
#ifdef CONFIG_MTK_CLKMGR
		disable_clock(MT_CG_PERI_USB0, "USB30");
#else
		clk_disable(musb_clk);
#endif
	}

#ifdef USB_CLK_DEBUG
	if (get_clk_io) {
		struct device_node *node;

		get_clk_io = false;
		node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-infrasys");
		usb_debug_clk_infracfg_base = of_iomap(node, 0);
		if (!usb_debug_clk_infracfg_base)
			pr_err("[CLK_INFRACFG_AO] base failed\n");
	}
	if (!IS_ERR(musb_clk))
		pr_err("SSUSB musb clock is okay, enabel: %d\n", enable);
	else
		pr_err("SSUSB musb clock is fail, enabel: %d\n", enable);
	
	pr_err("SSUSB MODULE_SW_CG_2_STA  = 0x%08x\n", DRV_Reg32(MODULE_SW_CG_2_STA));
#endif
	return 1;
}


#ifdef NEVER
void enable_ssusb_xtal_clock(bool enable)
{
	if (enable) {
		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000001),
		       (void __iomem *)AP_PLL_CON0);
		
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000002),
		       (void __iomem *)AP_PLL_CON0);

		writel(readl((void __iomem *)AP_PLL_CON2) | (0x00000001),
		       (void __iomem *)AP_PLL_CON2);

		
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON2) | (0x00000002),
		       (void __iomem *)AP_PLL_CON2);

		writel(readl((void __iomem *)AP_PLL_CON2) | (0x00000004),
		       (void __iomem *)AP_PLL_CON2);
	} else {
		
		
	}
}

void enable_ssusb26m_ck(bool enable)
{
	if (enable) {
		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000001),
		       (void __iomem *)AP_PLL_CON0);
		
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON0) | (0x00000002),
		       (void __iomem *)AP_PLL_CON0);

	} else {
		
		
	}
}
#endif				

void usb20_pll_settings(bool host, bool forceOn)
{
	if (host) {
		if (forceOn) {
			os_printk(K_DEBUG, "%s-%d - Set USBPLL_FORCE_ON.\n", __func__, __LINE__);
			
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST,
				RG_SUSPENDM, 1);
			
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST,
				FORCE_SUSPENDM, 1);
#ifndef CONFIG_MTK_TYPEC_SWITCH
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYA_REG6, RG_SSUSB_RESERVE6_OFST,
				RG_SSUSB_RESERVE6, 0x1);
#endif

		} else {
			os_printk(K_DEBUG, "%s-%d - Clear USBPLL_FORCE_ON.\n", __func__, __LINE__);
			
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST,
				RG_SUSPENDM, 0);
			
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST,
				FORCE_SUSPENDM, 0);
#ifndef CONFIG_MTK_TYPEC_SWITCH
			U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYA_REG6, RG_SSUSB_RESERVE6_OFST,
				RG_SSUSB_RESERVE6, 0x0);
#endif
			return;
		}
	}

	os_printk(K_DEBUG, "%s-%d - Set PLL_FORCE_MODE and SIFSLV PLL_FORCE_ON.\n", __func__,
		  __LINE__);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR2_0, RG_SIFSLV_USB20_PLL_FORCE_MODE_OFST,
			  RG_SIFSLV_USB20_PLL_FORCE_MODE, 0x1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDCR0, RG_SIFSLV_USB20_PLL_FORCE_ON_OFST,
			  RG_SIFSLV_USB20_PLL_FORCE_ON, 0x0);
}

#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
void usb_xhci_suspend_setting(int suspend_mode)
{
	os_printk(K_DEBUG, "usb_xhci_suspend_setting = %d\n",suspend_mode);

	if (suspend_mode) {
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDCR0, RG_SIFSLV_USB20_PLL_STABLE_OFST,
			  RG_SIFSLV_USB20_PLL_STABLE, 0x1);
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST,
			RG_SUSPENDM, 0);
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST,
			FORCE_SUSPENDM, 1);
	}
	else {
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST,
			RG_SUSPENDM, 1);
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST,
			FORCE_SUSPENDM, 1);
		udelay(100);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDCR0, RG_SIFSLV_USB20_PLL_STABLE_OFST,
			  RG_SIFSLV_USB20_PLL_STABLE, 0x0);
	}
}
#endif 

#ifdef CONFIG_MTK_UART_USB_SWITCH
bool in_uart_mode = false;
void uart_usb_switch_dump_register(void)
{
	
	

	
	usb_enable_clock(true);
	udelay(50);

# ifdef CONFIG_MTK_FPGA
	pr_debug("[MUSB]addr: 0x6B, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM0 + 0x3));
	pr_debug("[MUSB]addr: 0x6E, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM1 + 0x2));
	pr_debug("[MUSB]addr: 0x22, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYACR4 + 0x2));
	pr_debug("[MUSB]addr: 0x68, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM0));
	pr_debug("[MUSB]addr: 0x6A, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYDTM0 + 0x2));
	pr_debug("[MUSB]addr: 0x1A, value: %x\n",
		USB_PHY_Read_Register8((phys_addr_t) (uintptr_t)U3D_U2PHYACR6 + 0x2));
#else
#if 0
	os_printk(K_INFO, "[MUSB]addr: 0x6B, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM0 + 0x3));
	os_printk(K_INFO, "[MUSB]addr: 0x6E, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM1 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x22, value: %x\n", U3PhyReadReg8(U3D_U2PHYACR4 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x68, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM0));
	os_printk(K_INFO, "[MUSB]addr: 0x6A, value: %x\n", U3PhyReadReg8(U3D_U2PHYDTM0 + 0x2));
	os_printk(K_INFO, "[MUSB]addr: 0x1A, value: %x\n", U3PhyReadReg8(U3D_USBPHYACR6 + 0x2));
#else
	os_printk(K_INFO, "[MUSB]addr: 0x18, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6));
	os_printk(K_INFO, "[MUSB]addr: 0x20, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4));
	os_printk(K_INFO, "[MUSB]addr: 0x68, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0));
	os_printk(K_INFO, "[MUSB]addr: 0x6C, value: 0x%x\n",
		U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1));
#endif
#endif

	
	

	
	usb_enable_clock(false);

}

bool usb_phy_check_in_uart_mode(void)
{
	PHY_INT32 usb_port_mode;

	
	

	
	usb_enable_clock(true);

	udelay(50);
	usb_port_mode = U3PhyReadReg32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0) >> RG_UART_MODE_OFST;

	
	

	
	usb_enable_clock(false);
	os_printk(K_INFO, "%s+ usb_port_mode = %d\n", __func__, usb_port_mode);

	if (usb_port_mode == 0x1)
		return true;
	else
		return false;
}

void usb_phy_switch_to_uart(void)
{
	PHY_INT32 ret;

	if (usb_phy_check_in_uart_mode()) {
		os_printk(K_INFO, "%s+ UART_MODE\n", __func__);
		return;
	}

	os_printk(K_INFO, "%s+ USB_MODE\n", __func__);

	
	
	
	ret = pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	if (ret)
		pr_debug("VUSB33 enable FAIL!!!\n");

	
	
	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	if (ret)
		pr_debug("VA10 enable FAIL!!!\n");

	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x02);
	if (ret)
		pr_debug("VA10 output selection to 1.0v FAIL!!!\n");

	
	

	
	usb_enable_clock(true);
	udelay(50);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 1);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_UART_MODE_OFST, RG_UART_MODE, 1);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_TX_OE_OFST, FORCE_UART_TX_OE, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_BIAS_EN_OFST, FORCE_UART_BIAS_EN,
			  1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_TX_OE_OFST, RG_UART_TX_OE, 1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_BIAS_EN_OFST, RG_UART_BIAS_EN, 1);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DM_100K_EN_OFST,
			  RG_USB20_DM_100K_EN, 1);

	
	

	
	usb_enable_clock(false);

	
	DRV_WriteReg32(ap_uart0_base + 0xB0, 0x1);

	in_uart_mode = true;
}


void usb_phy_switch_to_usb(void)
{
	in_uart_mode = false;
	
	DRV_WriteReg32(ap_uart0_base + 0xB0, 0x0);	

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);

	phy_init_soc(u3phy);

	
	
	

	
	usb_enable_clock(false);
}
#endif

#define RG_SSUSB_VUSB10_ON (1<<5)
#define RG_SSUSB_VUSB10_ON_OFST (5)

#ifdef CONFIG_MTK_SIB_USB_SWITCH
void usb_phy_sib_enable_switch(bool enable)
{
	pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x02);

	usb_enable_clock(true);
	udelay(50);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			  RG_SSUSB_VUSB10_ON, 1);
	
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x700), 0x00031000);
	
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x704), 0x00000000);
	
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x708), 0x00000000);
	
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x70C), 0x00000000);
	
	U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif_base + 0x730), 0x0000000C);

	if (enable) {
		U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif2_base+0x300), 0x62910008);
		sib_mode = true;
	} else {
		U3PhyWriteReg32((phys_addr_t) (uintptr_t) (u3_sif2_base+0x300), 0x62910002);
		sib_mode = false;
	}
}

bool usb_phy_sib_enable_switch_status(void)
{
	int reg;
	bool ret;

	pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x02);

	usb_enable_clock(true);
	udelay(50);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			  RG_SSUSB_VUSB10_ON, 1);

	reg = U3PhyReadReg32((phys_addr_t) (uintptr_t) (u3_sif2_base+0x300));
	if (reg == 0x62910008)
		ret = true;
	else
		ret = false;

	return ret;
}
#endif


PHY_INT32 phy_init_soc(struct u3phy_info *info)
{
	PHY_INT32 ret;
	PHY_INT32 val1 = 0, val2 = 0;

	os_printk(K_INFO, "%s+\n", __func__);

	

	
	
	
	ret = pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
	if (ret)
		pr_debug("VUSB33 enable FAIL!!!\n");

	
	
	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
	if (ret)
		pr_debug("VA10 enable FAIL!!!\n");

	ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x02);
	if (ret)
		pr_debug("VA10 output selection to 1.0v FAIL!!!\n");

	
	
	

	
	
	

	
	usb_enable_clock(true);

	
	

	
	udelay(50);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
			  RG_SSUSB_VUSB10_ON, 1);

	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_ISO_EN_OFST, RG_USB20_ISO_EN, 0);

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode) {
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN,
				  0);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST,
				  RG_USB20_GPIO_CTL, 0);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST,
				  USB20_GPIO_MODE, 0);
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_UART_MODE_OFST, RG_UART_MODE, 0);
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DM_100K_EN_OFST,
				  RG_USB20_DM_100K_EN, 0);
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM,
				  0);
	}
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DP_100K_MODE_OFST,
			  RG_USB20_DP_100K_MODE, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_DP_100K_EN_OFST, USB20_DP_100K_EN, 0);
#if defined(CONFIG_MTK_HDMI_SUPPORT) || defined(MTK_USB_MODE1)
	os_printk(K_INFO, "%s- USB PHY Driving Tuning Mode 1 Settings(7,7).\n", __func__);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR1, RG_USB20_VRT_VREF_SEL_OFST,
			  RG_USB20_VRT_VREF_SEL, 7);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR1, RG_USB20_TERM_VREF_SEL_OFST,
			  RG_USB20_TERM_VREF_SEL, 7);
#else
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 1);
#endif
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 1);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x2);
#else
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DP_100K_MODE_OFST,
			  RG_USB20_DP_100K_MODE, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_DP_100K_EN_OFST, USB20_DP_100K_EN, 0);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_DM_100K_EN_OFST,
			  RG_USB20_DM_100K_EN, 0);
#if !defined(CONFIG_MTK_HDMI_SUPPORT) && !defined(MTK_USB_MODE1)
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 1);
#endif
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 1);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x2);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 0);
#endif

	
	udelay(800);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, FORCE_VBUSVALID_OFST, FORCE_VBUSVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, FORCE_AVALID_OFST, FORCE_AVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, FORCE_SESSEND_OFST, FORCE_SESSEND, 1);

	
	usb20_pll_settings(false, false);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x1);
	if (phy_discth != PHY_DISCTH_INVALID)
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_DISCTH_OFST, RG_USB20_DISCTH, phy_discth);

	val1 = U3PhyReadField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_DISCTH_OFST, RG_USB20_DISCTH);
	val2 = U3PhyReadField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH);
	printk("[USB][PHY] %s: DISCTH Threshold=0x%x, rx-sensitivity=0x%x\n", __func__, val1, val2);

	os_printk(K_DEBUG, "%s-\n", __func__);

	return PHY_TRUE;
}

PHY_INT32 u2_slew_rate_calibration(struct u3phy_info *info)
{
	PHY_INT32 i = 0;
	PHY_INT32 fgRet = 0;
	PHY_INT32 u4FmOut = 0;
	PHY_INT32 u4Tmp = 0;

	os_printk(K_DEBUG, "%s\n", __func__);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCAL_EN_OFST,
			  RG_USB20_HSTX_SRCAL_EN, 1);

	
	udelay(1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x110)
			  , RG_FRCK_EN_OFST, RG_FRCK_EN, 0x1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x100)
			  , RG_CYCLECNT_OFST, RG_CYCLECNT, 0x400);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x100)
			  , RG_FREQDET_EN_OFST, RG_FREQDET_EN, 0x1);

	os_printk(K_DEBUG, "Freq_Valid=(0x%08X)\n",
		  U3PhyReadReg32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x110)));

	mdelay(1);

	
	for (i = 0; i < 10; i++) {
		
		
		u4FmOut = U3PhyReadReg32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x10C));
		os_printk(K_DEBUG, "FM_OUT value: u4FmOut = %d(0x%08X)\n", u4FmOut, u4FmOut);

		
		if (u4FmOut != 0) {
			fgRet = 0;
			os_printk(K_DEBUG, "FM detection done! loop = %d\n", i);
			break;
		}
		fgRet = 1;
		mdelay(1);
	}
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x100)
			  , RG_FREQDET_EN_OFST, RG_FREQDET_EN, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) (u3_sif2_base + 0x110)
			  , RG_FRCK_EN_OFST, RG_FRCK_EN, 0);

	
	if (u4FmOut == 0) {
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCTRL_OFST,
				  RG_USB20_HSTX_SRCTRL, 0x4);
		fgRet = 1;
	} else {
		
		
		u4Tmp = (1024 * REF_CK * U2_SR_COEF_E60802) / (u4FmOut * 1000);
		os_printk(K_DEBUG, "SR calibration value u1SrCalVal = %d\n", (PHY_UINT8) u4Tmp);
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCTRL_OFST,
				  RG_USB20_HSTX_SRCTRL, u4Tmp);
	}

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HSTX_SRCAL_EN_OFST,
			  RG_USB20_HSTX_SRCAL_EN, 0);

	return fgRet;
}

void usb_phy_savecurrent(unsigned int clk_on)
{
	PHY_INT32 ret;

	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (sib_mode) {
		pr_err("%s sib_mode can't savecurrent\n", __func__);
		return;
	}

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode) {
		
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN,
				  0);
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);

		
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

		
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM,
				  1);
	}
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
#else
	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 1);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 1);
#endif
	
	
	udelay(2000);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DPPULLDOWN_OFST, RG_DPPULLDOWN, 1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DMPULLDOWN_OFST, RG_DMPULLDOWN, 1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_XCVRSEL_OFST, RG_XCVRSEL, 0x1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_TERMSEL_OFST, RG_TERMSEL, 1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DATAIN_OFST, RG_DATAIN, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN,
			  1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN,
			  1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_TERMSEL_OFST, FORCE_TERMSEL, 1);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DATAIN_OFST, FORCE_DATAIN, 1);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 0);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 0);

	
	udelay(800);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_SUSPENDM_OFST, RG_SUSPENDM, 0);

	
	udelay(1);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_VBUSVALID_OFST, RG_VBUSVALID, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_AVALID_OFST, RG_AVALID, 0);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_SESSEND_OFST, RG_SESSEND, 1);

	
	usb20_pll_settings(false, false);

	if (clk_on) {
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
				  RG_SSUSB_VUSB10_ON, 0);

		
		udelay(10);

		
		usb_enable_clock(false);

		
		

		
		
		
		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x00);
	}

	os_printk(K_INFO, "%s-\n", __func__);
}

void usb_phy_recover(unsigned int clk_on)
{
	PHY_INT32 ret;
	PHY_INT32 val1 = 0, val2 = 0;

	os_printk(K_DEBUG, "%s clk_on=%d+\n", __func__, clk_on);

	if (!clk_on) {
		
		
		
		ret = pmic_set_register_value(MT6351_PMIC_RG_VUSB33_EN, 0x01);
		if (ret)
			pr_debug("VUSB33 enable FAIL!!!\n");

		
		
		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x01);
		if (ret)
			pr_debug("VA10 enable FAIL!!!\n");

		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_VOSEL, 0x02);
		if (ret)
			pr_debug("VA10 output selection to 1.0v FAIL!!!\n");

		
		
		

		
		
		

		
		usb_enable_clock(true);

		
		

		
		udelay(50);

		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST,
				  RG_SSUSB_VUSB10_ON, 1);
	}

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_ISO_EN_OFST, RG_USB20_ISO_EN, 0);

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (!in_uart_mode) {
		
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN,
				  0);
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
		
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM,
				  0);
	}
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);
#else
	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_UART_EN_OFST, FORCE_UART_EN, 0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_UART_EN_OFST, RG_UART_EN, 0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, RG_USB20_GPIO_CTL_OFST, RG_USB20_GPIO_CTL,
			  0);
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYACR4, USB20_GPIO_MODE_OFST, USB20_GPIO_MODE, 0);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_SUSPENDM_OFST, FORCE_SUSPENDM, 0);
#endif

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DPPULLDOWN_OFST, RG_DPPULLDOWN, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DMPULLDOWN_OFST, RG_DMPULLDOWN, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_XCVRSEL_OFST, RG_XCVRSEL, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_TERMSEL_OFST, RG_TERMSEL, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, RG_DATAIN_OFST, RG_DATAIN, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DP_PULLDOWN_OFST, FORCE_DP_PULLDOWN,
			  0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DM_PULLDOWN_OFST, FORCE_DM_PULLDOWN,
			  0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_XCVRSEL_OFST, FORCE_XCVRSEL, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_TERMSEL_OFST, FORCE_TERMSEL, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM0, FORCE_DATAIN_OFST, FORCE_DATAIN, 0);

	
	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
			  RG_USB20_BC11_SW_EN, 0);

	
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_OTG_VBUSCMP_EN_OFST,
			  RG_USB20_OTG_VBUSCMP_EN, 1);
	
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH, 0x1);
	if (phy_discth != PHY_DISCTH_INVALID)
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_DISCTH_OFST, RG_USB20_DISCTH, phy_discth);
	val1 = U3PhyReadField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_DISCTH_OFST, RG_USB20_DISCTH);
	val2 = U3PhyReadField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_SQTH_OFST, RG_USB20_SQTH);
	printk("[USB][PHY] %s: DISCTH Threshold=0x%x, rx-sensitivity=0x%x\n", __func__, val1, val2);

#if defined(CONFIG_MTK_HDMI_SUPPORT) || defined(MTK_USB_MODE1)
	os_printk(K_INFO, "%s- USB PHY Driving Tuning Mode 1 Settings.\n", __func__);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR5, RG_USB20_HS_100U_U3_EN_OFST,
			  RG_USB20_HS_100U_U3_EN, 0);
#else
	
	
#endif

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USB30_PHYA_REG6, RG_SSUSB_TX_EIDLE_CM_OFST,
			  RG_SSUSB_TX_EIDLE_CM, 0xE);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_CDR1, RG_SSUSB_CDR_BIR_LTD0_OFST,
			  RG_SSUSB_CDR_BIR_LTD0, 0xC);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_PHYD_CDR1, RG_SSUSB_CDR_BIR_LTD1_OFST,
			  RG_SSUSB_CDR_BIR_LTD1, 0x3);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U3PHYA_DA_REG0, RG_SSUSB_XTAL_EXT_EN_U3_OFST,
			  RG_SSUSB_XTAL_EXT_EN_U3, 2);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_SPLLC_XTALCTL3, RG_SSUSB_XTAL_RX_PWD_OFST,
			  RG_SSUSB_XTAL_RX_PWD, 1);

	
	udelay(800);

	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_VBUSVALID_OFST, RG_VBUSVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_AVALID_OFST, RG_AVALID, 1);
	U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_U2PHYDTM1, RG_SESSEND_OFST, RG_SESSEND, 0);

#ifdef CONFIG_MTK_UART_USB_SWITCH
	if (in_uart_mode) {
		os_printk(K_INFO,
			  "%s- Switch to UART mode when UART cable in inserted before boot.\n",
			  __func__);
		usb_phy_switch_to_uart();
	}
#endif
	if (get_devinfo_with_index(9) & 0x1F) {
		os_printk(K_INFO, "USB HW reg: index9=0x%x\n", get_devinfo_with_index(9));
		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR1, RG_USB20_INTR_CAL_OFST,  RG_USB20_INTR_CAL,
			get_devinfo_with_index(9) & (0x1F));
	}

	
	usb20_pll_settings(false, false);

	os_printk(K_DEBUG, "%s-\n", __func__);
}

void usb_fake_powerdown(unsigned int clk_on)
{
	PHY_INT32 ret;

	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (clk_on) {
		
		
		usb_enable_clock(false);

		
		
		
		ret = pmic_set_register_value(MT6351_PMIC_RG_VA10_EN, 0x00);
	}

	os_printk(K_INFO, "%s-\n", __func__);
}

#ifdef CONFIG_USBIF_COMPLIANCE
static bool charger_det_en = true;

void Charger_Detect_En(bool enable)
{
	charger_det_en = enable;
}
#endif


void Charger_Detect_Init(void)
{
	os_printk(K_DEBUG, "%s+\n", __func__);

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		
		usb_enable_clock(true);

		
		udelay(50);

		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
				  RG_USB20_BC11_SW_EN, 1);

		udelay(1);

		
		usb_enable_clock(false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_INFO, "%s do not init detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_DEBUG, "%s-\n", __func__);
}

void Charger_Detect_Release(void)
{
	os_printk(K_DEBUG, "%s+\n", __func__);

#ifdef CONFIG_USBIF_COMPLIANCE
	if (charger_det_en == true) {
#endif
		
		usb_enable_clock(true);

		
		udelay(50);

		
		U3PhyWriteField32((phys_addr_t) (uintptr_t) U3D_USBPHYACR6, RG_USB20_BC11_SW_EN_OFST,
				  RG_USB20_BC11_SW_EN, 0);

		udelay(1);

		
		usb_enable_clock(false);

#ifdef CONFIG_USBIF_COMPLIANCE
	} else {
		os_printk(K_DEBUG, "%s do not release detection as charger_det_en is false\n",
			  __func__);
	}
#endif

	os_printk(K_INFO, "%s-\n", __func__);
}



#ifdef CONFIG_OF
static int mt_usb_dts_probe(struct platform_device *pdev)
{
	int retval = 0;
	
	struct device *dev = pdev ? &pdev->dev : NULL;
	struct device_node *np = dev ? dev->of_node : NULL;

	if (np && of_property_read_u32(np, "phy_discth", &phy_discth) >= 0) {
		if ((phy_discth < PHY_DISCTH_MIN) || (phy_discth > PHY_DISCTH_MAX))
			phy_discth = PHY_DISCTH_INVALID;
		pr_info("%s phy_discth=%d\n", __func__, phy_discth);
	}
	
#ifndef CONFIG_MTK_CLKMGR
	musb_clk = devm_clk_get(&pdev->dev, "sssub_clk");
	if (IS_ERR(musb_clk)) {
		os_printk(K_ERR, "SSUSB cannot get musb clock\n");
		return PTR_ERR(musb_clk);
	}

	os_printk(K_DEBUG, "SSUSB get musb clock ok, prepare it\n");
	retval = clk_prepare(musb_clk);
	if (retval == 0)
		os_printk(K_DEBUG, "musb clock prepare done\n");
	else
		os_printk(K_ERR, "musb clock prepare fail\n");
#endif
	return retval;
}

static int mt_usb_dts_remove(struct platform_device *pdev)
{
#ifndef CONFIG_MTK_CLKMGR
	clk_unprepare(musb_clk);
#endif
	return 0;
}


static const struct of_device_id apusb_of_ids[] = {
	{.compatible = "mediatek,usb3_phy",},
	{},
};

MODULE_DEVICE_TABLE(of, apusb_of_ids);

static struct platform_driver mt_usb_dts_driver = {
	.remove = mt_usb_dts_remove,
	.probe = mt_usb_dts_probe,
	.driver = {
		   .name = "mt_dts_mu3phy",
		   .of_match_table = apusb_of_ids,
		   },
};
MODULE_DESCRIPTION("mtu3phy MUSB PHY Layer");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mt_usb_dts_driver);

#ifdef CONFIG_USB_XHCI_MTK_OTG_POWER_SAVING
void __iomem *pericfg_base;

#define PERI_U3_WAKE_CTRL0 0x420
#define SSUSB0_CDEN 6
#define SSUSB_IP_SLEEP_SPM_ENABLE 1
#define SSUSB4_CDDEBOUNCE 12


void enable_ip_sleep_wakeup(void)
{
	if (pericfg_base) {
		writel(readl(pericfg_base + PERI_U3_WAKE_CTRL0) | (1 << SSUSB0_CDEN), pericfg_base + PERI_U3_WAKE_CTRL0);
		pr_info("%s reg=%x\n", __func__, readl(pericfg_base + PERI_U3_WAKE_CTRL0));
	}
}

void disable_ip_sleep_wakeup(void)
{
	if (pericfg_base) {
		writel(readl(pericfg_base + PERI_U3_WAKE_CTRL0) & ~(1 << SSUSB0_CDEN), pericfg_base + PERI_U3_WAKE_CTRL0);
		pr_info("%s reg=%x\n",__func__, readl(pericfg_base + PERI_U3_WAKE_CTRL0));
	}
}


static s32 usb_sleep_init(void)
{
	struct device_node *pericfg_node;

	pericfg_node = of_find_compatible_node(NULL, NULL, "mediatek,mt6755-perisys");
	if (!pericfg_node) {
		pr_err("Cannot find pericfg node\n");
		return -ENODEV;
	}

	pericfg_base = of_iomap(pericfg_node, 0);
	if (!pericfg_base) {
		pr_err("pericfg iomap failed\n");
		return -ENOMEM;
	}

	writel(readl(pericfg_base + PERI_U3_WAKE_CTRL0) & ~(1 << SSUSB0_CDEN),
		pericfg_base + PERI_U3_WAKE_CTRL0);
	writel((readl(pericfg_base + PERI_U3_WAKE_CTRL0) | (1 << SSUSB_IP_SLEEP_SPM_ENABLE)) & ~(1<< SSUSB4_CDDEBOUNCE),
		pericfg_base + PERI_U3_WAKE_CTRL0);
	pr_info("%s iomem = %p\n", __func__, pericfg_base);

	return 0;
}



static int mt_usb_ipsleep_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = usb_sleep_init();

	if (ret)
		return ret;

	return 0;
}

static int mt_usb_ipsleep_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id usb_ipsleep_of_match[] = {
	{.compatible = "mediatek,usb_ipsleep"},
	{},
};


MODULE_DEVICE_TABLE(of, usb_ipsleep_of_match);

static struct platform_driver mt_usb_ipsleep_driver = {
	.remove = mt_usb_ipsleep_remove,
	.probe = mt_usb_ipsleep_probe,
	.driver = {
		   .name = "usb_ipsleep",
		   .of_match_table = usb_ipsleep_of_match,
		   },
};
MODULE_DESCRIPTION("musb ipsleep module");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");
module_platform_driver(mt_usb_ipsleep_driver);
#endif 

#endif

#endif
