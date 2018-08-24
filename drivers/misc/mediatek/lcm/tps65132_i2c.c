#include "tps65132_i2c.h"

#ifdef BUILD_LK

#include <platform/mt_i2c.h>

#define TPS65132_SLAVE_ADDR_WRITE  0x7C

static struct mt_i2c_t TPS65132_i2c;

int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
	unsigned int ret_code = I2C_OK;
	unsigned char write_data[2];
	unsigned int len;

	write_data[0] = addr;
	write_data[1] = value;

	
	TPS65132_i2c.id = I2C3;
	
	TPS65132_i2c.addr = (TPS65132_SLAVE_ADDR_WRITE >> 1);
	TPS65132_i2c.mode = ST_MODE;
	TPS65132_i2c.speed = 100;
	len = 2;

	ret_code = i2c_write(&TPS65132_i2c, write_data, len);
	if (ret_code)
		dprintf(ALWAYS, "[DISP][TPS65132] i2c write error! addr=0x%x, ret_code=%d\n", addr, ret_code);
	else
		dprintf(ALWAYS, "[DISP][TPS65132] i2c write success. addr=0x%x\n", addr);

	return ret_code;
}

#else 

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

#define I2C_ID_NAME "i2c_lcd_bias"

static struct i2c_client *tps65132_i2c_client = NULL;

static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);

struct tps65132_dev {
	struct i2c_client *client;
};

static const struct i2c_device_id tps65132_id[] = {
	{I2C_ID_NAME, 0},
	{}
};

static struct of_device_id tps65132_match_table[] = {
	{.compatible = "htc,i2c_lcd_bias",}
};

static struct i2c_driver tps65132_i2c_driver = {
	.id_table	= tps65132_id,
	.probe		= tps65132_probe,
	.remove		= tps65132_remove,
	.driver		= {
		.owner = THIS_MODULE,
		.name = I2C_ID_NAME,
		.of_match_table = tps65132_match_table,
	},
};


static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("[DISP][TPS65132] %s\n", __FUNCTION__);
	tps65132_i2c_client = client;
	return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
	tps65132_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	char write_data[2] = {0};
	struct i2c_client *client = tps65132_i2c_client;

	if (client == NULL){
		printk("[DISP][TPS65132] %s: client = NULL!\n", __FUNCTION__);
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;

	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		printk("[DISP][TPS65132] i2c write error! addr=0x%x, ret=%d\n", addr, ret);
	else
		printk("[DISP][TPS65132] i2c write success. addr=0x%x\n", addr);

	return ret ;
}

static int __init tps65132_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&tps65132_i2c_driver);
	if (ret)
		printk("[DISP][TPS65132]: %s: i2c_add_driver failed! ret=%d\n", __FUNCTION__, ret);

	return 0;
}

static void __exit tps65132_i2c_exit(void)
{
	i2c_del_driver(&tps65132_i2c_driver);
}

module_init(tps65132_i2c_init);
module_exit(tps65132_i2c_exit);

MODULE_AUTHOR("HTC DISPLAY");
MODULE_DESCRIPTION("HTC TPS65132 I2C DRIVER");
MODULE_LICENSE("GPL");

#endif
