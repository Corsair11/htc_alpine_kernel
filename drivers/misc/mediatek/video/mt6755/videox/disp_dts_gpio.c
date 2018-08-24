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

#include "disp_dts_gpio.h"
#include <linux/bug.h>
#include "disp_helper.h"
#include <linux/kernel.h>

static struct pinctrl *this_pctrl; 

static const char *this_state_name[DTS_GPIO_STATE_MAX] = {
	"mode_te_gpio",
	"mode_te_te",
	"lcm_rst_out0_gpio",
	"lcm_rst_out1_gpio",
#ifdef CONFIG_LCD_RST_IN
	"lcm_rst_in0_gpio",
#endif
#ifdef CONFIG_PINCTRL_TCA6418
	"lcd_bias_enp0_ioexp_gpio",
	"lcd_bias_enp1_ioexp_gpio",
	"lcd_bias_enp_in_ioexp_gpio",
	"lcd_bias_enn0_ioexp_gpio",
	"lcd_bias_enn1_ioexp_gpio",
	"lcd_bias_enn_in_ioexp_gpio",
	"lm36274_en0_ioexp_gpio",
	"lm36274_en1_ioexp_gpio",
	"tp_rst_out0_ioexp_gpio",
	"tp_rst_out1_ioexp_gpio",
#else
	"lcd_bias_enp0_gpio",
	"lcd_bias_enp1_gpio",
	"lcd_bias_enn0_gpio",
	"lcd_bias_enn1_gpio",
#endif
	"lcd_io_18_en0_gpio",
	"lcd_io_18_en1_gpio",
	"lcdbl_en0_gpio",
	"lcdbl_en1_gpio"
};

#if defined(CONFIG_MTK_GPIO_REQUEST)
    extern void disp_dts_gpio_request(struct platform_device *pdev);
#endif

static long _set_state(const char *name)
{
	long ret = 0;
	struct pinctrl_state *pState = 0;

	BUG_ON(!this_pctrl);

	pState = pinctrl_lookup_state(this_pctrl, name);
	if (IS_ERR(pState)) {
		pr_err("set state '%s' failed\n", name);
		ret = PTR_ERR(pState);
		goto exit;
	}

	
	pinctrl_select_state(this_pctrl, pState);

exit:
	return ret; 
}


long disp_dts_gpio_init(struct platform_device *pdev)
{
	long ret = 0;
	struct pinctrl *pctrl;

	
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		dev_err(&pdev->dev, "Cannot find disp pinctrl!");
		ret = PTR_ERR(pctrl);
		goto exit;
	}

	this_pctrl = pctrl;

#if defined(CONFIG_MTK_GPIO_REQUEST)
         disp_dts_gpio_request(pdev);
#endif

exit:
	return ret;
}

long disp_dts_gpio_select_state(DTS_GPIO_STATE s)
{
	BUG_ON(!((unsigned int)(s) < (unsigned int)(DTS_GPIO_STATE_MAX)));
	return _set_state(this_state_name[s]);
}

