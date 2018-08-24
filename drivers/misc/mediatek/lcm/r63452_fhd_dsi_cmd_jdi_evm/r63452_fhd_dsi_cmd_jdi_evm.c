#include <linux/string.h>
#include "lcm_drv.h"
#include "lm36274_i2c.h"


extern void htc_cyttsp5_drv_pm_ops(int is_lcm_suspend);


#define LCM_DSI_CMD_MODE 1
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (1920)

#define REGFLAG_DELAY 0xAB
#define REGFLAG_END_OF_TABLE 0xAA 

static LCM_UTIL_FUNCS lcm_util = {0};
static bool mipi_lp11_ready = false;
LCM_DRIVER r63452_fhd_dsi_cmd_jdi_evm_lcm_drv;

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define SET_LCD_BIAS_ENP_IOEXP_PIN(v) (lcm_util.set_ioexp_gpio_lcd_enp_bias((v)))
#define SET_LCD_BIAS_ENN_IOEXP_PIN(v) (lcm_util.set_ioexp_gpio_lcd_enn_bias((v)))
#define SET_LM36274_HWEN_IOEXP_PIN(v) (lcm_util.set_ioexp_gpio_lm36274_hwen((v)))
#define SET_TP_RESET_IOEXP_PIN(v) (lcm_util.set_ioexp_tp_reset_pin((v)))
#define MDELAY(n) (lcm_util.mdelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	
	{0x35, 1, {0x00}},
	
	{0x36, 1, {0x00}},
	
	{0x3A, 1, {0x77}},
	
	{0x2A, 4, {0x00, 0x00, 0x04, 0x37}},
	
	{0x2B, 4, {0x00, 0x00, 0x07, 0x7F}},
	
	{0x44, 2, {0x00, 0x00}},
	
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	
	{0x29, 0, {}},
	{REGFLAG_DELAY, 40,{}},
#if (!LM36274_BRIGHTNESS_CTRL_MODE_I2C_ONLY)
	
	{0x53, 1, {0x24}},
	
	{0x51, 1, {0xFF}},
#endif
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	
	{0x28, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	
	{0x10, 0, {}},
	{REGFLAG_DELAY, 80, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_cmd_setting[] = {
	{0x00, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update) {
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY :
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE :
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util) {
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params) {
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = 68;
	params->physical_height = 121;
	params->lcm_if = LCM_INTERFACE_DSI0;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
#endif

	
	
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	
	params->dsi.packet_size = 256;
	
	params->dsi.ssc_disable = 1;
	params->dsi.ssc_range = 5;

	
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.vertical_sync_active = 1;
	params->dsi.vertical_backporch = 3;
	params->dsi.vertical_frontporch	= 4;
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 2;
	params->dsi.horizontal_backporch = 58;
	params->dsi.horizontal_frontporch = 106;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	
	params->dsi.PLL_CLOCK = 433;

	params->pwm_min = 7;
	params->pwm_default = 104;
	params->pwm_max = 255;
	params->camera_blk = 194;
}

static void lcm_power_on(void) {

	if (r63452_fhd_dsi_cmd_jdi_evm_lcm_drv.mipi_lp11) {
		if (!mipi_lp11_ready) {
			pr_info("[DISP] lcm_power_on (lp11)\n");

#if (MOTION_LAUNCH_SUPPORT)
			SET_TP_RESET_IOEXP_PIN(0);
			SET_RESET_PIN(0);
			SET_LCD_BIAS_ENN_IOEXP_PIN(0);
			SET_LCD_BIAS_ENP_IOEXP_PIN(0);
#endif

			
			lm36274_enable();

			
			SET_RESET_PIN(0);
			MDELAY(1);

			
			

			
			SET_LCD_BIAS_ENP_IOEXP_PIN(1);
			MDELAY(1);
			
			lm36274_write_bytes(LM36274_VPOS_BIAS_REG, 0x1E);
			MDELAY(2);

			
			SET_TP_RESET_IOEXP_PIN(1);
			MDELAY(1);

			
			SET_LCD_BIAS_ENN_IOEXP_PIN(1);
			MDELAY(1);
			
			lm36274_write_bytes(LM36274_VNEG_BIAS_REG, 0x1E);
			MDELAY(2);
			MDELAY(30); 

			mipi_lp11_ready = true;
		} else {
			
			SET_RESET_PIN(0);
			MDELAY(10);
			SET_RESET_PIN(1);
			MDELAY(10);

			pr_info("[DISP] lcm_power_on (lp11) end\n");
		}
	} else {
		pr_info("[DISP] lcm_power_on\n");

#if (MOTION_LAUNCH_SUPPORT)
		SET_TP_RESET_IOEXP_PIN(0);
		SET_RESET_PIN(0);
		SET_LCD_BIAS_ENN_IOEXP_PIN(0);
		SET_LCD_BIAS_ENP_IOEXP_PIN(0);
#endif

		
		lm36274_enable();

		
		SET_RESET_PIN(0);
		MDELAY(1);

		
		

		
		SET_LCD_BIAS_ENP_IOEXP_PIN(1);
		MDELAY(1);
		
		lm36274_write_bytes(LM36274_VPOS_BIAS_REG, 0x1E);
		MDELAY(2);

		
		SET_TP_RESET_IOEXP_PIN(1);
		MDELAY(1);

		
		SET_LCD_BIAS_ENN_IOEXP_PIN(1);
		MDELAY(1);
		
		lm36274_write_bytes(LM36274_VNEG_BIAS_REG, 0x1E);
		MDELAY(2);
		MDELAY(30); 

		
		SET_RESET_PIN(0);
		MDELAY(10);
		SET_RESET_PIN(1);
		MDELAY(10);

		pr_info("[DISP] lcm_power_on end\n");
	}
}

static void lcm_power_off(void) {
	pr_info("[DISP] %s\n", __FUNCTION__);
	

#if (MOTION_LAUNCH_SUPPORT)
	
	SET_RESET_PIN(2);
	MDELAY(5);

	
	SET_LCD_BIAS_ENN_IOEXP_PIN(2);
	MDELAY(10);

	
	SET_LCD_BIAS_ENP_IOEXP_PIN(2);
	MDELAY(10);

	lm36274_disable();
#else
	lm36274_disable();

	
	SET_RESET_PIN(0);
	MDELAY(5);

	
	SET_LCD_BIAS_ENN_IOEXP_PIN(0);
	MDELAY(10);

	
	SET_LCD_BIAS_ENP_IOEXP_PIN(0);
	MDELAY(10);
#endif

	if (r63452_fhd_dsi_cmd_jdi_evm_lcm_drv.mipi_lp11)
		mipi_lp11_ready = false;
}

static void lcm_init_power(void) {
	lcm_power_on();
}

static void lcm_resume_power(void) {
	lcm_power_on();
}

static void lcm_suspend_power(void) {
	lcm_power_off();
}

static void lcm_init(void) {
	pr_info("[DISP] %s: JDI r63452 evm\n", __FUNCTION__);
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	pr_info("[DISP] %s done\n", __FUNCTION__);
}

static void lcm_suspend(void) {
	htc_cyttsp5_drv_pm_ops(1);

	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void) {
	lcm_init();

	htc_cyttsp5_drv_pm_ops(0);

}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
			unsigned int width, unsigned int height) {
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
#endif

static unsigned int lcm_compare_id(void) {
	return 1;
}

static unsigned int lcm_check_id(void) {
	
	unsigned int retval = (which_lcd_module_triple() == 10) ? 1 : 0;

	
	return retval;
}

static void lcm_setbacklight_cmdq(void* handle, unsigned int level) {
	
	lm36274_write_brightness(level);
}

static void lcm_set_lcm_cmd(void* handle, unsigned int *lcm_cmd,
				unsigned int *lcm_count, unsigned int *lcm_value) {
	unsigned int i;

	lcm_cmd_setting[0].cmd = lcm_cmd[0];
	lcm_cmd_setting[0].count = lcm_count[0];
	for (i = 0 ;i < lcm_count[0]; i++)
		lcm_cmd_setting[0].para_list[i] = lcm_value[i];

	push_table(lcm_cmd_setting, sizeof(lcm_cmd_setting) / sizeof(struct LCM_setting_table), 1);
}

LCM_DRIVER r63452_fhd_dsi_cmd_jdi_evm_lcm_drv = {
	.name = "r63452_fhd_dsi_cmd_jdi_evm",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.check_id = lcm_check_id,
	.init_power	= lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_lcm_cmd = lcm_set_lcm_cmd,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
	.mipi_lp11 = 1,
};
