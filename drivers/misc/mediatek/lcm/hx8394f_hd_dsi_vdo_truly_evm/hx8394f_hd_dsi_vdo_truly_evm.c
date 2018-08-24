#ifndef BUILD_LK
#include <linux/string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#endif

#include "lcm_drv.h"
#include "tps65132_i2c.h"

#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (720)
#define FRAME_HEIGHT (1280)

#define REGFLAG_DELAY 0xAB
#define REGFLAG_END_OF_TABLE 0xAA 

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define SET_LCD_BIAS_ENP_PIN(v) (lcm_util.set_gpio_lcd_enp_bias((v)))
#define SET_LCD_BIAS_ENN_PIN(v) (lcm_util.set_gpio_lcd_enn_bias((v)))
#define SET_LCD_IO_18_EN_PIN(v) (lcm_util.set_gpio_lcd_io_18_en((v)))
#define SET_LCDBL_EN_PIN(v) (lcm_util.set_gpio_lcdbl_en((v)))
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
	
	{0xB9, 3, {0xFF, 0x83, 0x94}},
	
	{0xBA, 6, {0x62, 0x03, 0x68, 0x6B, 0xB2, 0xC0}}, 
	
	{0xB1, 10, {0x50, 0x12, 0x72, 0x09, 0x31, 0x54, 0x71,
				0x31, 0x50, 0x34}},
	
	{0xB2, 6, {0x00, 0x80, 0x64, 0x06, 0x08, 0x2F}},
	
	{0xB4, 21, {0x74, 0x76, 0x0D, 0x5C, 0x5A, 0x5B, 0x01,
				0x05, 0x7E, 0x35, 0x00, 0x3F, 0x74, 0x76,
				0x0D, 0x5C, 0x5A, 0x5B, 0x01, 0x05, 0x7E}},
	
	{0xE0, 58, {0x00, 0x05, 0x10, 0x17, 0x19, 0x1D, 0x21,
				0x20, 0x43, 0x55, 0x69, 0x6B, 0x78, 0x8C,
				0x96, 0x9C, 0xAB, 0xAF, 0xAB, 0xBA, 0xC8,
				0x63, 0x61, 0x66, 0x6C, 0x6F, 0x74, 0x7F,
				0x7F, 0x00, 0x05, 0x0F, 0x16, 0x19, 0x1D,
				0x21, 0x20, 0x43, 0x55, 0x69, 0x6B, 0x78,
				0x8C, 0x96, 0x9C, 0xAB, 0xAF, 0xAB, 0xBA,
				0xC8, 0x63, 0x61, 0x66, 0x6C, 0x6F, 0x74,
				0x7F, 0x7F}},
	
	{0xD3, 33, {0x00, 0x00, 0x06, 0x06, 0x40, 0x07, 0x0A,
				0x0A, 0x32, 0x10, 0x01, 0x00, 0x01, 0x52,
				0x15, 0x07, 0x05, 0x07, 0x32, 0x10, 0x00,
				0x00, 0x00, 0x67, 0x44, 0x05, 0x05, 0x37,
				0x0E, 0x0E, 0x27, 0x06, 0x40}},
	
	{0xD5, 44, {0x20, 0x21, 0x00, 0x01, 0x02, 0x03, 0x04,
				0x05, 0x06, 0x07, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18,
				0x24, 0x25}},
	
	{0xD6, 44, {0x24, 0x25, 0x07, 0x06, 0x05, 0x04, 0x03,
				0x02, 0x01, 0x00, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x58, 0x58, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
				0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19,
				0x20, 0x21}},
	
	{0xCC, 1, {0x0B}},
	
	{0xC0, 2, {0x1F, 0x73}},
	
	{0xD4, 1, {0x02}},
	
	{0xBD, 1, {0x01}},
	
	{0xB1, 1, {0x60}},
	
	{0xBD, 1, {0x00}},
	
	{0x53, 1, {0x24}},
	
	{0xC9, 8, {0x13, 0x00, 0x00, 0x7F, 0x11, 0x10, 0x00,
				0x01}},
	
	{0x55, 1, {0x11}},
	{REGFLAG_DELAY, 5, {}},
	
	{0x5E, 1, {0x00}},
	
	{0xCA, 9, {0x2D, 0x26, 0x25, 0x22, 0x21, 0x21, 0x20,
				0x20, 0x20}},
	
	{0xE5, 32, {0x00, 0x00, 0x08, 0x06, 0x0C, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00,
				0x00, 0x02, 0xC1, 0x0A, 0x00, 0x00, 0x02,
				0x04, 0x04, 0x08, 0x08, 0x07, 0x06, 0x06,
				0x05, 0x03, 0x02, 0x01}},
	
	{0xE6, 19, {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x20, 0x20, 0x00, 0x00, 0x00, 0x20,
				0x00, 0x00, 0x00, 0x00, 0x00}},
	
	{0xE4, 2, {0x01, 0xC3}},
	{REGFLAG_DELAY, 10, {}},
	
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	
	{0x29, 0, {}},
	{REGFLAG_DELAY, 10, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{REGFLAG_DELAY, 10, {}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 40, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_bl_setting[] = {
	{0x51, 1, {0xFF}},
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
	params->physical_width = 62;	
	params->physical_height = 110;	
	params->lcm_if = LCM_INTERFACE_DSI0;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
#endif

	
	
	params->dsi.LANE_NUM = LCM_THREE_LANE;
	
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	
	params->dsi.packet_size = 256;
	params->dsi.ssc_disable = 1;
	params->dsi.ssc_range = 4;

	
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 12;
	params->dsi.vertical_frontporch	= 16;
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 48;
	params->dsi.horizontal_backporch = 166;
	params->dsi.horizontal_frontporch = 100;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	
	params->dsi.PLL_CLOCK = 342;

	
	params->pwm_min = 6;
	params->pwm_default = 76;
	params->pwm_max = 255;
	params->camera_blk = 194;
}

static void lcm_power_on(void) {
	
	SET_LCD_BIAS_ENP_PIN(1);
	MDELAY(1);
	tps65132_write_bytes(TPS65132_VPOS_REG, 0x0F);
	MDELAY(1);

	
	SET_LCD_IO_18_EN_PIN(1);
	MDELAY(1);

	
	SET_LCD_BIAS_ENN_PIN(1);
	MDELAY(1);
	tps65132_write_bytes(TPS65132_VNEG_REG, 0x0F);
	MDELAY(1);

	
	SET_RESET_PIN(1);
	MDELAY(50);

	
	SET_LCDBL_EN_PIN(1);
}

static void lcm_power_off(void) {
	
	SET_LCDBL_EN_PIN(0);

	
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(1);

	
	SET_LCD_BIAS_ENN_PIN(0);
	MDELAY(1);

	
	SET_RESET_PIN(0);
	MDELAY(1);

	
	SET_LCD_BIAS_ENP_PIN(0);
	MDELAY(5);

	
	SET_LCD_IO_18_EN_PIN(0);
	MDELAY(5);
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
	pr_info("[DISP] %s: truly hx8394f evm\n", __FUNCTION__);
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void) {
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void) {
	lcm_init();
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
	
	unsigned int retval = (which_lcd_module_triple() == 8) ? 1 : 0;

	
	return retval;
}

static void lcm_setbacklight_cmdq(void* handle, unsigned int level) {
	lcm_bl_setting[0].para_list[0] = level;
	push_table(lcm_bl_setting, sizeof(lcm_bl_setting) / sizeof(struct LCM_setting_table), 1);
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

LCM_DRIVER hx8394f_hd_dsi_vdo_truly_evm_lcm_drv = {
	.name = "hx8394f_hd_dsi_vdo_truly_evm",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume  = lcm_resume,
	.compare_id = lcm_compare_id,
	.check_id = lcm_check_id,
	.init_power	= lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq  = lcm_setbacklight_cmdq,
	.set_lcm_cmd = lcm_set_lcm_cmd,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
};
