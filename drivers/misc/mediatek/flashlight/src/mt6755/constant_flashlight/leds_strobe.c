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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_flashlight_type.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>

#include <mt_gpio.h>
#include <gpio_const.h>

#include "../../../../leds/mt6755/leds_sw.h"

#define TAG_NAME "[leds_strobe.c]"
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(TAG_NAME "%s: " fmt, __func__ , ##arg)

#ifdef DEBUG_LEDS_STROBE
#define PK_DBG PK_DBG_FUNC
#else
#define PK_DBG(a, ...)
#endif

#define SY7806_NAME "flashlight"

static DEFINE_SPINLOCK(g_strobeSMPLock);	


static u32 strobe_Res;
static u32 strobe_Timeus;
static BOOL g_strobe_On;

static int g_timeOutTimeMs;

static DEFINE_MUTEX(g_strobeSem);
#define STROBE_DEVICE_ID 0x63


static struct work_struct workTimeOut;


static void work_timeOutFunc(struct work_struct *data);

static struct i2c_client *SY7806_i2c_client;

#if defined (CONFIG_SY7806_FLASHLIGHT)
int (*htc_flash_main)(int led1, int led2);
int (*htc_torch_main)(int led1, int led2);
int sy7806_flashlight_mode(int ,int);
int sy7806_torch_mode(int ,int);
#endif

extern void lcm_screen_on(void);
extern void lcm_screen_off(void);

extern int mt65xx_leds_brightness_set(enum mt65xx_led_type type, enum led_brightness level);
extern unsigned char get_camera_blk(void);

enum flashlight_mode_flags {
	FL_MODE_OFF = 0,
	FL_MODE_TORCH = 1,
	FL_MODE_FLASH = 2,
	FL_MODE_PRE_FLASH = 3,
	FL_MODE_TORCH_LEVEL_1 = 6,
	FL_MODE_TORCH_LEVEL_2 = 7,
};

struct SY7806_platform_data {
	u8 torch_pin_enable;	
	u8 pam_sync_pin_enable;	
	u8 thermal_comp_mode_enable;	
	u8 strobe_pin_disable;	
	u8 vout_mode_enable;	
};

struct SY7806_chip_data {
	struct i2c_client *client;

	struct led_classdev cdev_flash;
	
	
	enum flashlight_mode_flags 	mode_status;
	struct SY7806_platform_data *pdata;
	struct mutex lock;

	u8 last_flag;
	u8 no_pdata;
};

int SY7806_flashlight_control(int);

#if defined (CONFIG_SY7806_FLASHLIGHT)
static int sy7806_flt_flash_adapter(int mA1, int mA2)
{

	return sy7806_flashlight_mode(mA1,mA2);

}

static int sy7806_flt_torch_adapter(int mA1, int mA2)
{

	return sy7806_torch_mode(mA1,mA2);

}
#endif

static void flt_brightness_set(struct led_classdev *led_cdev,
						enum led_brightness brightness)
{
	enum flashlight_mode_flags mode;
	int ret = -1;

	if (brightness > 0 && brightness <= LED_HALF) {
		if (brightness == (LED_HALF - 2))
			mode = FL_MODE_TORCH_LEVEL_1;
		else if (brightness == (LED_HALF - 1))
			mode = FL_MODE_TORCH_LEVEL_2;
		else
			mode = FL_MODE_TORCH;
	} else if (brightness > LED_HALF && brightness <= LED_FULL) {
		if (brightness == (LED_HALF + 1))
			mode = FL_MODE_PRE_FLASH; 
		else
			mode = FL_MODE_FLASH; 
	} else
		
		mode = FL_MODE_OFF;

	ret = SY7806_flashlight_control(mode);
	if (ret) {
		printk("%s: control failure rc:%d\n", __func__, ret);
		return;
	}
}

static int i2c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret = 0;
	struct SY7806_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		printk("failed writing at 0x%02x\n", reg);
	return ret;
}

static int i2c_read_reg(struct i2c_client *client, u8 reg)
{
	int val=0;
	struct SY7806_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);


	return val;
}

static int SY7806_chip_init(struct SY7806_chip_data *chip)
{

#if defined (CONFIG_SY7806_FLASHLIGHT)
	return 0;
#else
	return -1;
#endif
}

static int SY7806_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct SY7806_chip_data *chip;
	struct SY7806_platform_data *pdata = client->dev.platform_data;
	int ret;
	int err = -1;

	printk("SY7806_probe start--->.\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		printk("SY7806 i2c functionality check fail.\n");
		return err;
	}

	chip = kzalloc(sizeof(struct SY7806_chip_data), GFP_KERNEL);
	chip->client = client;

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);
#if 0
	if (pdata == NULL) {	

		printk("SY7806 Platform data does not exist\n");
		printk("SY7806 Platform data does not exist\n");
		pdata = kzalloc(sizeof(struct SY7806_platform_data), GFP_KERNEL);
		chip->pdata = pdata;
		chip->no_pdata = 1;
	}
#endif
	chip->pdata = pdata;
	if (SY7806_chip_init(chip) < 0)
		goto err_chip_init;

	SY7806_i2c_client = client;
	flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
	ret = i2c_read_reg(SY7806_i2c_client, 0x0c);
	if(ret < 0)
		printk("%s: failed on I2C read reg \n",__func__);
	flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_LOW);
	chip->cdev_flash.name = SY7806_NAME;
	chip->cdev_flash.brightness_set = flt_brightness_set;
	ret= led_classdev_register(&client->dev,&chip->cdev_flash);
	if(ret < 0)
		printk("%s: failed on led_classdev_register\n", __func__);

#if defined (CONFIG_SY7806_FLASHLIGHT)
	htc_flash_main          = &sy7806_flt_flash_adapter;
	htc_torch_main          = &sy7806_flt_torch_adapter;
#endif

	printk("SY7806 Initializing is done[%x]\n", ret);

	return 0;

err_chip_init:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
#if defined (CONFIG_SY7806_FLASHLIGHT)
	htc_flash_main          = NULL;
	htc_torch_main          = NULL;
#endif
	printk("SY7806 probe is failed\n");
	return -ENODEV;

return 0;
}

static int SY7806_remove(struct i2c_client *client)
{
	struct SY7806_chip_data *chip = i2c_get_clientdata(client);
#if defined (CONFIG_SY7806_FLASHLIGHT)

	htc_flash_main          = NULL;
	htc_torch_main          = NULL;

#endif
	if (chip->no_pdata)
		kfree(chip->pdata);
	kfree(chip);
	return 0;
}


static const struct i2c_device_id SY7806_id[] = {
	{SY7806_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id SY7806_of_match[] = {
	{.compatible = "mediatek,strobe_main"},
	{},
};
#endif

static struct i2c_driver SY7806_i2c_driver = {
	.driver = {
		   .name = SY7806_NAME,
#ifdef CONFIG_OF
		   .of_match_table = SY7806_of_match,
#endif
		   },
	.probe = SY7806_probe,
	.remove = SY7806_remove,
	.id_table = SY7806_id,
};
static int __init SY7806_init(void)
{
	printk("SY7806_init\n");
	return i2c_add_driver(&SY7806_i2c_driver);
}

static void __exit SY7806_exit(void)
{
	i2c_del_driver(&SY7806_i2c_driver);
}


module_init(SY7806_init);
module_exit(SY7806_exit);

MODULE_DESCRIPTION("Flash driver for SY7806");
MODULE_AUTHOR("pw <pengwei@mediatek.com>");
MODULE_LICENSE("GPL v2");




enum
{
	e_DutyNum = 21,
};

static bool g_IsTorch[21] = {1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static char g_TorchDutyCode[21] ={35,71,106,127,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; 
static char g_TorchDutyCode_2[21] ={8,16,25,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; 
static int g_FlashDutyCode[21] = {3,8,12,14,16,20,25,29,33,37,42,46,50,55,59,63,67,72,76,80,84};
static int g_FlashDutyCode_2[21] = {2,5,9,10,12,15,18,21,25,28,31,34,37,41,44,47,50,54,57,60,63};
static char g_TorchDutyCode_2_nt[21] ={8,16,25,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; 
static int g_FlashDutyCode_2_nt[21] = {3,8,12,14,16,20,25,29,33,37,42,46,50,55,59,63,67,72,76,80,84};
static char g_EnableReg;
static int g_duty1 = -1;
static int g_duty2 = -1;
int nt_50572;
static int device_id;

static int SY7806_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	return i2c_write_reg(client, reg, val);
}
int flashEnable_sy7806_1(void)
{
	int ret;
	char buf[2];
	
	printk("tqq flashEnable_sy7806_1\n");
	buf[0] = 0x01; 
	if (g_IsTorch[g_duty1] == 1) 
		g_EnableReg = (0x0B);
	else{
		g_EnableReg = (0x0F);
	}
	buf[1] = g_EnableReg;
	#if 0
       printk("\n tff: start main flash \n");
       mt65xx_leds_brightness_set(6,0);
       msleep(5);
       #endif

	printk("\n tff: %s g_EnableReg = 0x%x \n", __func__, g_EnableReg);
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	return ret;
}

int flashEnable_sy7806_2(void)
{
	int ret;
	char buf[2];
	printk("tqq flashEnable_sy7806_2\n");
	buf[0] = 0x01; 
	if (g_IsTorch[g_duty2] == 1) 
		g_EnableReg |= (0x0A);
	else
		g_EnableReg |= (0x0E);
	buf[1] = g_EnableReg;
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
	return ret;
}

int flashDisable_sy7806_1(void)
{
	int ret;
	char buf[2];
	printk("tqq  flashDisable_sy7806_1\n");
	buf[0] = 0x01; 
	
		
	
	
	
	buf[1] = 0;
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
	g_EnableReg = 0;

       
	return ret;
}

int flashDisable_sy7806_2(void)
{
	int ret;
	char buf[2];
	printk("tqq flashDisable_sy7806_2\n");
	buf[0] = 0x01; 
	if ((g_EnableReg&0x01) == 0x01) 
		g_EnableReg &= (~0x02);
	else
		g_EnableReg &= (~0x0E);
	buf[1] = g_EnableReg;
	printk("\n tff: restore display back light \n");
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
	return ret;
}

int SY7806_flashlight_control(int mode)
{
	int ret = 0;

		switch (mode) {
			case FL_MODE_OFF:
				flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
				ret = SY7806_write_reg(SY7806_i2c_client,0x01, 0x00);
			break;
			case FL_MODE_FLASH:
				flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
				SY7806_write_reg(SY7806_i2c_client,0x03, g_FlashDutyCode[20]);
				SY7806_write_reg(SY7806_i2c_client,0x04, g_FlashDutyCode[20]);
				ret = SY7806_write_reg(SY7806_i2c_client,0x01, 0x0F);
			break;
			case FL_MODE_PRE_FLASH:
				flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
				SY7806_write_reg(SY7806_i2c_client,0x03, g_FlashDutyCode[11]);
				SY7806_write_reg(SY7806_i2c_client,0x04, g_FlashDutyCode[11]);
				ret = SY7806_write_reg(SY7806_i2c_client,0x01, 0x0F);
			break;
			case FL_MODE_TORCH:
				flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
				SY7806_write_reg(SY7806_i2c_client,0x05, g_TorchDutyCode[3]);
				SY7806_write_reg(SY7806_i2c_client,0x06, g_TorchDutyCode[3]);
				ret = SY7806_write_reg(SY7806_i2c_client,0x01, 0x0B);
			break;
			case FL_MODE_TORCH_LEVEL_1:
				flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
				SY7806_write_reg(SY7806_i2c_client,0x05, g_TorchDutyCode[1]);
				SY7806_write_reg(SY7806_i2c_client,0x06, g_TorchDutyCode[1]);
				ret = SY7806_write_reg(SY7806_i2c_client,0x01, 0x0B);
			break;
			case FL_MODE_TORCH_LEVEL_2:
				flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
				SY7806_write_reg(SY7806_i2c_client,0x05, g_TorchDutyCode[2]);
				SY7806_write_reg(SY7806_i2c_client,0x06, g_TorchDutyCode[2]);
				ret = SY7806_write_reg(SY7806_i2c_client,0x01, 0x0B);
			break;

			default:
			printk("%s: unknown flash_light flags: %d\n",__func__, mode);
				ret = -EINVAL;
			break;
		}

		printk("%s: mode: %d\n", __func__, mode);
		

		return ret;
}

int setDuty_sy7806_1(int duty)
{
	int ret;
	char buf[2];
	
	if (duty < 0)
		duty = 0;
	else if (duty >= e_DutyNum)
		duty = e_DutyNum-1;

	g_duty1 = duty;
	if(duty < 4)
	{
			buf[0] = 0x05; 
			buf[1] = g_TorchDutyCode[duty];
			ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

			buf[0] = 0x06; 
			buf[1] = g_TorchDutyCode[duty];
			ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
			printk("arg duty = %d, g_TorchDutyCode_sy7806_1 duty = %d\n",duty,buf[1]);
	}
	else
	{
			buf[0] = 0x03; 
			buf[1] = g_FlashDutyCode[duty];
			ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

			buf[0] = 0x04; 
			buf[1] = g_FlashDutyCode[duty];
			ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
			printk("arg duty = %d, g_FlashDutyCode_sy7806_1 duty = %d\n",duty,buf[1]);

	}

	return ret;
}

int setDuty_sy7806_2(int duty)
{
	int ret;
	char buf[2];

	if (duty < 0)
		duty = 0;
	else if (duty >= e_DutyNum)
		duty = e_DutyNum-1;

	g_duty2 = duty;
	buf[0] = 0x06; 
if (nt_50572 == 1)
{
	buf[1] = g_TorchDutyCode_2_nt[duty];
}else{
	buf[1] = g_TorchDutyCode_2[duty];
}

	printk("tqq g_TorchDutyCode_2 sy7806_2 duty%d\n",buf[1]);
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	buf[0] = 0x04; 
if (nt_50572 == 1)
{
	buf[1] = g_FlashDutyCode_2_nt[duty];
}else{

	buf[1] = g_FlashDutyCode_2[duty];
}
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
	return ret;
}

#if 0
void leds_add_dev(void)
{
struct devinfo_struct *dev_led;
dev_led = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
    if (device_id == 0x08)
	{
		nt_50572 = 0;

   	dev_led->device_vendor = "KTD";
    	dev_led->device_ic = "KTD2684";
	}
	else
	{
		nt_50572 = 1;
		
    	dev_led->device_vendor = "NT";
    	dev_led->device_ic = "NT50572";
	}
    dev_led->device_type = "Flash_LED_DRIVER";
    dev_led->device_version = DEVINFO_NULL;
    dev_led->device_module = DEVINFO_NULL;
    dev_led->device_info = DEVINFO_NULL;
	dev_led->device_used = DEVINFO_USED;
	devinfo_check_add_device(dev_led);
	mdelay(200);
	
}
#endif
int init_sy7806(void)
{

	int ret;
	char buf[2];
	
	printk("tqq init_sy7806 start!\n");
	flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_HIGH);
	

 
	buf[0] = 0x01; 
	buf[1] = 0x00;
	g_EnableReg = buf[1];
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	buf[0] = 0x03; 
	buf[1] = 0x3F;
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	buf[0] = 0x04; 
	buf[1] = 0x3F;
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	buf[0] = 0x08; 
	buf[1] = 0x1F;
	ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);
	device_id =  i2c_read_reg(SY7806_i2c_client,0x0c);

if (device_id == 0)
{
printk("tqq sy7806 device_id = %d\n", device_id);
	nt_50572 = 1;	
}
printk("tqq sy7806 nt_50572 = %d\n", nt_50572);
	return ret;
}

int FL_Enable(void)
{
	printk("tqq sy7806 FL_Enable line=%d\n", __LINE__);
	return flashEnable_sy7806_1();

}


int FL_Disable(void)
{
	printk("tqq sy7806 FL_Disable line=%d\n", __LINE__);
	return flashDisable_sy7806_1();

}

int FL_dim_duty(kal_uint32 duty)
{

	printk("tqq sy7806 FL_dim_duty line=%d\n", __LINE__);
	return setDuty_sy7806_1(duty);
}

int FL_Init(void)
{

	printk("tqq sy7806_FL_Init line=%d\n", __LINE__);
	return init_sy7806();
}

int FL_Uninit(void)
{
	printk("tqq sy7806 FL_Uninit\n");
	FL_Disable();
	flashlight_gpio_set(FLASHLIGHT_PIN_HWEN, STATE_LOW);
	return 0;	
}


static void work_timeOutFunc(struct work_struct *data)
{
	FL_Disable();
	printk("tqq sy7806 ledTimeOut_callback\n");
}



enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
	schedule_work(&workTimeOut);
	return HRTIMER_NORESTART;
}
static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
	static int init_flag;

	if (init_flag == 0) {
		init_flag = 1;
		INIT_WORK(&workTimeOut, work_timeOutFunc);
		g_timeOutTimeMs = 1000;
		hrtimer_init(&g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		g_timeOutTimer.function = ledTimeOutCallback;
	}
}
static int constant_flashlight_ioctl(unsigned int cmd, unsigned long arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;

	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC, 0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC, 0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC, 0, int));
	switch (cmd) {

	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		printk("tqq FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n", (int)arg);
		g_timeOutTimeMs = arg;
		break;


	case FLASH_IOC_SET_DUTY:
		printk("tqq FLASHLIGHT_DUTY: %d\n", (int)arg);
		FL_dim_duty(arg);
		break;


	case FLASH_IOC_SET_STEP:
		printk("tqq FLASH_IOC_SET_STEP: %d\n", (int)arg);

		break;

	case FLASH_IOC_SET_ONOFF:
		printk("tqq  FLASHLIGHT_ONOFF: %d\n", (int)arg);
		if (arg == 1) {

			int s;
			int ms;

			if (g_timeOutTimeMs > 1000) {
				s = g_timeOutTimeMs / 1000;
				ms = g_timeOutTimeMs - s * 1000;
			} else {
				s = 0;
				ms = g_timeOutTimeMs;
			}

			if (g_timeOutTimeMs != 0) {
				ktime_t ktime;

				ktime = ktime_set(s, ms * 1000000);
				hrtimer_start(&g_timeOutTimer, ktime, HRTIMER_MODE_REL);
			}
			FL_Enable();
		} else {
			FL_Disable();
			hrtimer_cancel(&g_timeOutTimer);
		}
		break;

      case FLASH_IOC_SET_DISPLAY_BACKLIGHT:
           printk("\n tff: close display back light \n");
           lcm_screen_off();
           
           msleep(5);
           break;

      case FLASH_IOC_RESUME_DISPLAY_BACKLIGHT:
           printk("\n tff: resume display back light \n");
           lcm_screen_on();
           
           msleep(5);
           break;

	default:
		printk("tqq  No such command\n");
		i4RetValue = -EPERM;
		break;
	}
	return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
	int i4RetValue = 0;

	printk("tqq  sy7806 constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res) {
		FL_Init();
		timerInit();
		
	}
	printk("tqq  sy7806 constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


	if (strobe_Res) {
		printk("tqq  sy7806  busy!\n");
		i4RetValue = -EBUSY;
	} else {
		strobe_Res += 1;
	}


	spin_unlock_irq(&g_strobeSMPLock);
	printk("tqq  sy7806 constant_flashlight_open line=%d\n", __LINE__);

	return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
	printk("tqq sy7806 constant_flashlight_release\n");

	if (strobe_Res) {
		spin_lock_irq(&g_strobeSMPLock);

		strobe_Res = 0;
		strobe_Timeus = 0;

		
		g_strobe_On = FALSE;

		spin_unlock_irq(&g_strobeSMPLock);

		FL_Uninit();
	}

	printk("sy7806 Done\n");

	return 0;

}


FLASHLIGHT_FUNCTION_STRUCT constantFlashlightFunc = {
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &constantFlashlightFunc;
	return 0;
}



ssize_t strobe_VDIrq(void)
{

	return 0;
}
EXPORT_SYMBOL(strobe_VDIrq);

int sy7806_flashlight_mode(int led_ma1,int led_ma2)
{
	int ret = 0;
    char buf[2];

	printk("<%s:%d>led_ma1[%x]\n", __func__, __LINE__, led_ma1);
	printk("<%s:%d>led_ma2[%x]\n", __func__, __LINE__, led_ma2);

 	if (led_ma1 < 0)
		led_ma1 = 0;
	else if (led_ma1 >= e_DutyNum)
		led_ma1 = e_DutyNum-1;

	g_duty1 = led_ma1;

	if (led_ma2 < 0)
		led_ma2 = 0;
	else if (led_ma2 >= e_DutyNum)
		led_ma2 = e_DutyNum-1;

	g_duty2 = led_ma2;
	
	if(led_ma1 >= 0)
	{

		buf[0] = 0x03; 
		buf[1] = g_FlashDutyCode[led_ma1];

		printk("g_FlashDutyCode_sy7806_1 duty%d\n",buf[1]);		
		ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	}
	if(led_ma2 >= 0)
	{

		buf[0] = 0x04; 
		buf[1] = g_FlashDutyCode[led_ma2];

		printk("g_FlashDutyCode_sy7806_1 duty%d\n",buf[1]);
		ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);

	}

		ret = flashEnable_sy7806_1();

		return ret; 
}


int sy7806_torch_mode(int led_ma1,int led_ma2)
{
	int ret = 0;
    char buf[2];

	if (led_ma1 < 0)
		led_ma1 = 0;
	else if (led_ma1 >= e_DutyNum)
		led_ma1 = e_DutyNum-1;

	g_duty1 = led_ma1;  

	if (led_ma2 < 0)
		led_ma2 = 0;
	else if (led_ma2 >= e_DutyNum)
		led_ma2 = e_DutyNum-1;

	g_duty2 = led_ma2;

	if(led_ma1 >= 0)
	{

		buf[0] = 0x05; 

		buf[1] = g_TorchDutyCode[led_ma1];

		printk("g_TorchDutyCode_sy7806_1 duty%d\n",buf[1]);
		ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);  
     } 
     if(led_ma2 >= 0)
     {

		buf[0] = 0x06; 

		buf[1] = g_TorchDutyCode[led_ma2];

		printk("g_TorchDutyCode_sy7806_1 duty%d\n",buf[1]);
		ret = SY7806_write_reg(SY7806_i2c_client, buf[0], buf[1]);  
     }

		ret = flashEnable_sy7806_1();

		return ret;       
}

