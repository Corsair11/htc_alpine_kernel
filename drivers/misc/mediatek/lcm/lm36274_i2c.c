#include "lm36274_i2c.h"
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

#define I2C_ID_NAME "i2c_lcd_bias_and_bl"

static struct i2c_client *lm36274_i2c_client = NULL;

struct lm36274_data {
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	unsigned short addr;
	struct mutex lock;
	struct work_struct work;
	u8 backlight_cfg_1;
	u8 brightness_lsb;
	u8 brightness_msb;
	u8 backlight_en;
	u8 disp_bias_cfg_1;
	u8 backlight_opt_2;
	u8 vpos_bias;
	u8 vneg_bias;
	int brightness;
	bool enable;
};

static struct lm36274_data *lm36274_drvdata;

static int lm36274_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lm36274_remove(struct i2c_client *client);

struct lm36274_dev {
	struct i2c_client *client;
};

static const struct i2c_device_id lm36274_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

static struct of_device_id lm36274_match_table[] = {
	{.compatible = "htc,i2c_lcd_bias_and_bl",}
};

static struct i2c_driver lm36274_i2c_driver = {
	.id_table	= lm36274_id,
	.probe		= lm36274_probe,
	.remove		= lm36274_remove,
	.driver		= {
		.owner = THIS_MODULE,
		.name = I2C_ID_NAME,
		.of_match_table = lm36274_match_table,
	},
};


int lm36274_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	char write_data[2] = {0};
	struct i2c_client *client = lm36274_i2c_client;

	if (client == NULL) {
		pr_err("[DISP][LM36274] %s: client = NULL!\n", __FUNCTION__);
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;

	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		printk("[DISP][LM36274] i2c write error! addr=0x%x, ret=%d\n", addr, ret);

	return ret ;
}

int lm36274_read_bytes(unsigned char addr, unsigned char *data)
{
	int ret = 0;
	uint8_t read_data[2] = {0, 0};
	struct i2c_msg msgs[2] = {
		{
			.addr = lm36274_i2c_client->addr,
			.flags = 0,
			.len = 1,
			.buf = read_data,
			.timing = 400,
		},
		{
			.addr = lm36274_i2c_client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = read_data,
			.timing = 400,
		}
	};

	read_data[0] = addr;
	ret = i2c_transfer(lm36274_i2c_client->adapter, msgs, 2);
	if (ret < 0) {
		printk("[DISP][LM36274] i2c read error! addr=0x%x, ret=%d\n", addr, ret);
		read_data[0] = 0;
	}
	*data = read_data[0];

	return ret;
}

void lm36274_enable(void)
{
	if (!lm36274_drvdata->enable) {
		lm36274_write_bytes(LM36274_BACKLIGHT_EN_REG, lm36274_drvdata->backlight_en);
		lm36274_write_bytes(LM36274_BACKLIGHT_CONFIG_1_REG, lm36274_drvdata->backlight_cfg_1);
		lm36274_drvdata->enable = true;
	}
}

void lm36274_disable(void)
{
	if (lm36274_drvdata->enable) {
		lm36274_write_bytes(LM36274_BACKLIGHT_EN_REG, 0);
		
		lm36274_write_bytes(LM36274_BACKLIGHT_CONFIG_1_REG, 0x68);
		lm36274_drvdata->enable = false;
	}
}

void lm36274_set_brightness(int brightness)
{
	u8 brt_LSB, brt_MSB;

	if (!lm36274_drvdata->enable) {
		pr_info("[DISP] lm36274_set_brightness: lm36274 is not enabled. ignore brightness %d\n", brightness);
		return;
	}

	brightness = (brightness * LM36274_MAX_BRIGHTNESS)/255;
	if (brightness > LM36274_CALI_MAX_BRIGHTNESS) {
		pr_info("[DISP] lm36274_set_brightness: exceed max value, limit to 0x%x\n", LM36274_CALI_MAX_BRIGHTNESS);
		brightness = LM36274_CALI_MAX_BRIGHTNESS;
	}
	brt_LSB = brightness & 0x7;
	brt_MSB = (brightness >> 3) & 0xff;

	lm36274_write_bytes(LM36274_BRIGHTNESS_LSB_REG, brt_LSB);
	lm36274_write_bytes(LM36274_BRIGHTNESS_MSB_REG, brt_MSB);
}

void lm36274_work(struct work_struct *work)
{
	mutex_lock(&lm36274_drvdata->lock);
	lm36274_set_brightness(lm36274_drvdata->brightness);
	mutex_unlock(&lm36274_drvdata->lock);
}

int lm36274_write_brightness(int brightness)
{
	lm36274_drvdata->brightness = brightness;
	schedule_work(&lm36274_drvdata->work);

	return 0;
}

static int lm36274_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	pr_info("[DISP] lm36274_probe\n");

	lm36274_i2c_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : failed to register i2c driver\n", __func__);
		return -EIO;
	}

	lm36274_drvdata = kzalloc(sizeof(struct lm36274_data), GFP_KERNEL);
	if (!lm36274_drvdata) {
		pr_err("%s : kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	lm36274_drvdata->client = lm36274_i2c_client;
	lm36274_drvdata->adapter = client->adapter;
	lm36274_drvdata->addr = client->addr;
	
	lm36274_drvdata->enable = true;
	
	lm36274_drvdata->backlight_en = 0x1F;
	
	lm36274_drvdata->backlight_cfg_1 = 0x69;

	mutex_init(&lm36274_drvdata->lock);
	INIT_WORK(&lm36274_drvdata->work, lm36274_work);

	return 0;
}

static int lm36274_remove(struct i2c_client *client)
{
	lm36274_i2c_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

static int __init lm36274_i2c_init(void)
{
	int ret;

	printk("[DISP] lm36274_i2c_init\n");

	ret = i2c_add_driver(&lm36274_i2c_driver);
	if (ret)
		printk("[DISP][LM36274]: %s: i2c_add_driver failed! ret=%d\n", __FUNCTION__, ret);

	return 0;
}

static void __exit lm36274_i2c_exit(void)
{
	i2c_del_driver(&lm36274_i2c_driver);
}

module_init(lm36274_i2c_init);
module_exit(lm36274_i2c_exit);

MODULE_AUTHOR("HTC DISPLAY");
MODULE_DESCRIPTION("HTC LM36274 I2C DRIVER");
MODULE_LICENSE("GPL");
