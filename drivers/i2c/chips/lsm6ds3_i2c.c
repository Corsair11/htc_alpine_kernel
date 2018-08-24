/*
 * STMicroelectronics lsm6ds3 i2c driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 *
 * Giuseppe Barba <giuseppe.barba@st.com>
 * v 1.2.1
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/lsm6ds3.h>
#include <linux/lsm6ds3_core.h>

#define D(x...) pr_info(x)
#define I2C_MASTER_CLOCK 400
#define C_I2C_FIFO_SIZE		8



int hwmsen_read_byte_st(struct i2c_client *client, u8 addr, u8 *data)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,	.flags = 0,
			.len = 1,	.buf = &beg
		},
		{
			.addr = client->addr,	.flags = I2C_M_RD,
			.len = 1,	.buf = data,
		}
	};

	if (!client)
		return -EINVAL;

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) {
		D("i2c_transfer error: (%d %p) %d\n", addr, data, err);
		err = -EIO;
	}

	err = 0;

	return err;
}

int hwmsen_read_block_st(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = {{0}, {0} };

	if (len == 1)
		return hwmsen_read_byte_st(client, addr, data);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;


	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		D(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) {
		D("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	}
#if defined(HWMSEN_DEBUG)
	static char buf[128];
	int idx, buflen = 0;

	for (idx = 0; idx < len; idx++)
		buflen += snprintf(buf+buflen, sizeof(buf)-buflen, "%02X ", data[idx]);
	D("%s(0x%02X,%2d) = %s\n", __func__, addr, len, buf);
#endif
	err = 0;	

	return err;
}

static int lsm6ds3_i2c_read(struct lsm6ds3_data *cdata, u8 reg_addr, int len,
							u8 *data, bool b_lock)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(cdata->dev);

    
    if (len == 1)
    err = hwmsen_read_byte_st(client,reg_addr,data);
	if (len > 1)
	err = hwmsen_read_block_st(client,reg_addr,data,len);
	

	return err;
}

static int lsm6ds3_i2c_write(struct lsm6ds3_data *cdata, u8 reg_addr, int len,
							u8 *data, bool b_lock)
{

	struct i2c_client *client = to_i2c_client(cdata->dev);

        u8 buf[8];
		int num, idx,ret;

		num = 0;
		buf[num++] = reg_addr;
		for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];
		
		ret = i2c_master_send(client, buf, num);
		
		if (ret < 0) {
		D("I2C send command error!!\n");
		return -EFAULT;
		} else {
	        ret = 0;    
}

return ret;

}


static const struct lsm6ds3_transfer_function lsm6ds3_tf_i2c = {
	.write = lsm6ds3_i2c_write,
	.read = lsm6ds3_i2c_read,
};


#ifdef IS_MTK_PLATFORM
static int lsm6ds3_pinctrl_init(struct i2c_client *client)
{
	int retval;
	
	int ret;

	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_init;

	D("lsm6ds3_pinctrl_init");
	
	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("[GSN][lsm6ds3 error]%s: Target does not use pinctrl\n", __func__);
		retval = PTR_ERR(pinctrl);
		pinctrl = NULL;
		return retval;
	}

	gpio_state_init = pinctrl_lookup_state(pinctrl, "lsm6ds3_g_init");
	if (IS_ERR_OR_NULL(gpio_state_init)) {
		pr_err("[GSN][lsm6ds3 error]%s: Cannot get pintctrl state\n", __func__);
		retval = PTR_ERR(gpio_state_init);
		pinctrl = NULL;
		return retval;
	}

	ret = pinctrl_select_state(pinctrl,gpio_state_init);
	if (ret) {
		pr_err("[GSN][lsm6ds3 error]%s: Cannot init INT gpio\n", __func__);
		return ret;
	}

	return 0;
}

#endif

static int lsm6ds3_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int err;
	struct lsm6ds3_data *cdata;
	cdata = kmalloc(sizeof(*cdata), GFP_KERNEL);
	if (!cdata)
		return -ENOMEM;

	cdata->dev = &client->dev;
	cdata->name = client->name;
	cdata->tf = &lsm6ds3_tf_i2c;
	i2c_set_clientdata(client, cdata);

	lsm6ds3_pinctrl_init(client);
	err = lsm6ds3_common_probe(cdata, client->irq, BUS_I2C);
	if (err < 0)
		goto free_data;

	return 0;

free_data:
	kfree(cdata);
	return err;
}

static int lsm6ds3_i2c_remove(struct i2c_client *client)
{
	struct lsm6ds3_data *cdata = i2c_get_clientdata(client);

	lsm6ds3_common_remove(cdata, client->irq);
	dev_info(cdata->dev, "%s: removed\n", LSM6DS3_ACC_GYR_DEV_NAME);
	kfree(cdata);
	return 0;
}

#ifdef CONFIG_PM
static int lsm6ds3_suspend(struct device *dev)
{
	struct lsm6ds3_data *cdata = i2c_get_clientdata(to_i2c_client(dev));

	return lsm6ds3_common_suspend(cdata);
}

static int lsm6ds3_resume(struct device *dev)
{
	struct lsm6ds3_data *cdata = i2c_get_clientdata(to_i2c_client(dev));

	return lsm6ds3_common_resume(cdata);
}

static const struct dev_pm_ops lsm6ds3_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lsm6ds3_suspend, lsm6ds3_resume)
};

#define LSM6DS3_PM_OPS		(&lsm6ds3_pm_ops)
#else 
#define LSM6DS3_PM_OPS		NULL
#endif 

static const struct i2c_device_id lsm6ds3_ids[] = {
	{"lsm6ds3", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lsm6ds3_ids);

#ifdef CONFIG_OF
static const struct of_device_id lsm6ds3_id_table[] = {
	{.compatible = "st,lsm6ds3", },
	{ },
};
MODULE_DEVICE_TABLE(of, lsm6ds3_id_table);
#endif

static struct i2c_driver lsm6ds3_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = LSM6DS3_ACC_GYR_DEV_NAME,
		.pm = LSM6DS3_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = lsm6ds3_id_table,
#endif
	},
	.probe    = lsm6ds3_i2c_probe,
	.remove   = lsm6ds3_i2c_remove,
	.id_table = lsm6ds3_ids,
};

module_i2c_driver(lsm6ds3_i2c_driver);

MODULE_DESCRIPTION("STMicroelectronics lsm6ds3 i2c driver");
MODULE_AUTHOR("Giuseppe Barba");
MODULE_LICENSE("GPL v2");
