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

#ifndef __DISP_DTS_GPIO_H__
#define __DISP_DTS_GPIO_H__


#include <linux/platform_device.h>	

typedef enum tagDTS_GPIO_STATE {
	DTS_GPIO_STATE_TE_MODE_GPIO = 0,  
	DTS_GPIO_STATE_TE_MODE_TE,  
	DTS_GPIO_STATE_LCM_RST_OUT0,
	DTS_GPIO_STATE_LCM_RST_OUT1,
#ifdef CONFIG_LCD_RST_IN
	DTS_GPIO_STATE_LCM_RST_IN,
#endif
#ifdef CONFIG_PINCTRL_TCA6418
	DTS_IOEXP_GPIO_STATE_LCD_BIAS_ENP0,
	DTS_IOEXP_GPIO_STATE_LCD_BIAS_ENP1,
	DTS_IOEXP_GPIO_STATE_LCD_BIAS_ENP_IN,
	DTS_IOEXP_GPIO_STATE_LCD_BIAS_ENN0,
	DTS_IOEXP_GPIO_STATE_LCD_BIAS_ENN1,
	DTS_IOEXP_GPIO_STATE_LCD_BIAS_ENN_IN,
	DTS_IOEXP_GPIO_STATE_LM36274_HWEN0,
	DTS_IOEXP_GPIO_STATE_LM36274_HWEN1,
	DTS_IOEXP_GPIO_STATE_TP_RST_OUT0,
	DTS_IOEXP_GPIO_STATE_TP_RST_OUT1,
#else
	DTS_GPIO_STATE_LCD_BIAS_ENP0,
	DTS_GPIO_STATE_LCD_BIAS_ENP1,
	DTS_GPIO_STATE_LCD_BIAS_ENN0,
	DTS_GPIO_STATE_LCD_BIAS_ENN1,
#endif
	DTS_GPIO_STATE_LCD_IO_18_EN0,
	DTS_GPIO_STATE_LCD_IO_18_EN1,
	DTS_GPIO_STATE_LCDBL_EN0,
	DTS_GPIO_STATE_LCDBL_EN1,
	DTS_GPIO_STATE_MAX,	
} DTS_GPIO_STATE;

long disp_dts_gpio_init(struct platform_device *pdev);

long disp_dts_gpio_select_state(DTS_GPIO_STATE s);

#ifdef CONFIG_MTK_LEGACY
#define disp_dts_gpio_init_repo(x)  (0)
#else
#define disp_dts_gpio_init_repo(x)  (disp_dts_gpio_init(x))
#endif

#endif
