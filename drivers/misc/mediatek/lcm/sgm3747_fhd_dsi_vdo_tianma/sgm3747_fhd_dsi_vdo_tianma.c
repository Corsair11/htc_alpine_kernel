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

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/upmu_hw.h>
#include <linux/gpio.h>

#define TPS_I2C_BUSNUM   0
#define TPS_ADDR 0x2C
#endif

#define LCM_DSI_CMD_MODE 0
#define FRAME_WIDTH (1080)
#define FRAME_HEIGHT (1920)

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
	{0x00,1,{0x00}},
	{0xff,3,{0x87,0x16,0x01}},
	{0x00,1,{0x80}},
	{0xff,2,{0x87,0x16}},
	
	{0x51, 1, {0xFF}},
	{0x53, 1, {0x2C}},
	{0x55, 1, {0x00}},
	{0x00,1,{0x00}},
    {0xE1,24,{0x00,0x06,0x16,0x2e,0x38,0x44,0x56,0x65,0x69,0x72,0x79,0x80,0x79,0x74,0x73,0x6c,0x5e,0x53,0x44,0x3a,0x32,0x21,0x0f,0x03}},
    {0x00,1,{0x00}},
    {0xE2,24,{0x00,0x06,0x16,0x2e,0x38,0x44,0x56,0x65,0x69,0x72,0x79,0x80,0x79,0x74,0x73,0x6c,0x5e,0x53,0x44,0x3a,0x32,0x21,0x0f,0x03}},

    {0x00,1,{0x00}},
    {0xec,15,{0x00,0x03,0x07,0x0b,0x6c,0x0e,0x12,0x16,0x1a,0x1b,0x1d,0x25,0x2c,0x34,0x23}},
    {0x00,1,{0x10}},
    {0xec,15,{0x3b,0x43,0x4a,0x52,0x22,0x59,0x60,0x68,0x6f,0xdd,0x77,0x7e,0x86,0x8d,0x89}},
    {0x00,1,{0x20}},
    {0xec,15,{0x94,0x9c,0xa3,0xab,0x37,0xb2,0xba,0xc1,0xc8,0xe2,0xd0,0xd7,0xdf,0xe6,0x4d}},
    {0x00,1,{0x30}},
    {0xec,4,{0xea,0xeb,0xec,0x2c}},

    {0x00,1,{0x00}},
    {0xed,15,{0x00,0x04,0x08,0x0c,0x00,0x10,0x14,0x18,0x1c,0x00,0x20,0x28,0x30,0x38,0x40}},
    {0x00,1,{0x10}},
    {0xed,15,{0x40,0x48,0x50,0x58,0x55,0x60,0x68,0x70,0x78,0x55,0x80,0x88,0x90,0x98,0x55}},
    {0x00,1,{0x20}},
    {0xed,15,{0xa0,0xa8,0xb0,0xb8,0x55,0xc0,0xc8,0xd0,0xd8,0x05,0xe0,0xe8,0xf0,0xf8,0x00}},
    {0x00,1,{0x30}},
    {0xed,4,{0xfc,0xfe,0xff,0x00}},

    {0x00,1,{0x00}},
    {0xee,15,{0x00,0x03,0x07,0x0b,0x6c,0x0e,0x12,0x16,0x1a,0x1b,0x1d,0x25,0x2b,0x31,0xe3}},
    {0x00,1,{0x10}},
    {0xee,15,{0x39,0x40,0x47,0x4f,0x79,0x56,0x5d,0x65,0x6c,0x9e,0x74,0x7b,0x82,0x8a,0x78}},
    {0x00,1,{0x20}},
    {0xee,15,{0x91,0x99,0xa0,0xa8,0x22,0xaf,0xb6,0xbe,0xc5,0xdd,0xcd,0xd5,0xdc,0xe4,0xf2}},
    {0x00,1,{0x30}},
    {0xee,4,{0xe9,0xeb,0xec,0x24}},

     
    {0x00,1,{0xA0}},
    {0xD6,12,{0x16,0x15,0x0b,0x09,0x0c,0x0b,0x08,0x07,0x12,0x18,0x18,0x17}},
     
     {0x00,1,{0xB0}},
    {0xD6,12,{0xcb,0xce,0xbf,0xc2,0x9f,0x85,0x7e,0x69,0x54,0x64,0x8a,0xa9}},
     
    {0x00,1,{0xC0}},
    {0xD6,12,{0xa9,0xa8,0xa5,0xa3,0xd0,0xc8,0xb3,0x90,0x9e,0xb9,0xb6,0xb9}},
     
    {0x00,1,{0xD0}},
    {0xD6,12,{0x91,0x89,0x88,0x84,0x89,0x8b,0x86,0x86,0xaa,0xaa,0xa5,0x8d}},

    {0x00,1,{0x00}},
	{0xff,3,{0x00,0x00,0x00}},

	{0x00,1,{0x80}},
	{0xff,2,{0x00,0x00}},
    {0x91, 1, {0x80}},

	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{REGFLAG_DELAY, 50, {}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 10, {}},
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
	params->dsi.ssc_range = 4;

	
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	
	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch	= 16;
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 32;
	params->dsi.horizontal_frontporch = 32;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	
	params->dsi.PLL_CLOCK = 425;
	params->pwm_min = 5;
	params->pwm_default = 72;
	params->pwm_max = 255;
	params->camera_blk = 177;
}

void disp_dts_gpio_request(struct platform_device *pdev)
{
      int ret = 0;
     struct device_node *node1 = pdev->dev.of_node;
     printk("disp_dts_gpio_request \n");

     if(of_property_read_u32(node1, "te_gpio", &ret) == 0) {
	    if(gpio_request(ret, "mode_te_gpio") < 0)
		printk("gpio %d request failed\n", ret);
      }

    if(of_property_read_u32(node1, "io_18_gpio", &ret) == 0) {
           if(gpio_request(ret, "lcd_io_18_gpio") < 0)
               printk("gpio %d request failed\n", ret);
    }

    if(of_property_read_u32(node1, "bias_enp_gpio", &ret) == 0) {
           if(gpio_request(ret, "lcd_bias_enp_gpio") < 0)
               printk("gpio %d request failed\n", ret);
    }

    if(of_property_read_u32(node1, "bias_enn_gpio", &ret) == 0) {
           if(gpio_request(ret, "lcd_bias_enn_gpio") < 0)
               printk("gpio %d request failed\n", ret);
    }

    if(of_property_read_u32(node1, "id0_gpio", &ret) == 0) {
           if(gpio_request(ret, "lcd_id0_gpio") < 0)
               printk("gpio %d request failed\n", ret);
    }

    if(of_property_read_u32(node1, "id1_gpio", &ret) == 0) {
           if(gpio_request(ret, "lcd_id1_gpio") < 0)
               printk("gpio %d request failed\n", ret);
    }

    if(of_property_read_u32(node1, "rst_gpio", &ret) == 0) {
           if(gpio_request(ret, "lcm_rst_gpio") < 0)
               printk("gpio %d request failed\n", ret);
    }

}

static void lcm_power_on(void) {

        printk("lcm_power_on begin\n");
        
        SET_RESET_PIN(1);

        
	  SET_LCD_IO_18_EN_PIN(1);
	  MDELAY(10);

        SET_RESET_PIN(0);
        MDELAY(4);
        SET_RESET_PIN(1);
        MDELAY(3);

	
	SET_LCD_BIAS_ENP_PIN(1);
	MDELAY(1);
	tps65132_write_bytes(TPS65132_VPOS_REG, 0x0f);
	MDELAY(10);

	
	SET_LCD_BIAS_ENN_PIN(1);
	MDELAY(1);
	tps65132_write_bytes(TPS65132_VNEG_REG, 0x0f);
	MDELAY(1);

        
  #if 0
         lp8557_write_bytes(0x13,0x03); 
         MDELAY(1);
	lp8557_write_bytes(0x10,0x06);
         MDELAY(1);
	lp8557_write_bytes(0x04,0x44);
         MDELAY(1);
	lp8557_write_bytes(0x00,0x01);  
	MDELAY(1);
#endif
	
	SET_RESET_PIN(0);
	MDELAY(2);
	SET_RESET_PIN(1);
	MDELAY(15);

}

static void lcm_power_off(void) {

	
	SET_RESET_PIN(0);
	MDELAY(2);

	
	SET_LCD_BIAS_ENN_PIN(0);
	MDELAY(1);

	
	SET_LCD_BIAS_ENP_PIN(0);
	MDELAY(4);

	
	SET_LCD_IO_18_EN_PIN(0);
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
	pr_info("[DISP] %s: Tianma sgm3747 lcm init start\n", __FUNCTION__);
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void) {
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	pr_info("[DISP] %s: lcm_suspend is over\n", __FUNCTION__);
}

static void lcm_resume(void) {
	lcm_init();
	printk("lcm_resume is over");
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
	
	unsigned int retval = (which_lcd_module_triple() == 0) ? 1 : 0;

	
	return retval;
}

static void lcm_setbacklight_cmdq(void* handle, unsigned int level) {
	lcm_bl_setting[0].para_list[0] = level;
	push_table(lcm_bl_setting, sizeof(lcm_bl_setting) / sizeof(struct LCM_setting_table), 1);

	pr_info("[DISP] %s: backlight level = %d\n", __func__, level);
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

LCM_DRIVER sgm3747_fhd_dsi_vdo_tianma_lcm_drv = {
	.name = "sgm3747_fhd_dsi_vdo_tianma",
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
