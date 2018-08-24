#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include "tps6128x.h"

/**********************************************************
  *   Define
  *********************************************************/
#define tps6128x_REG_NUM 6
#define tps6128x_SLAVE_ADDR_WRITE   0xEA
#define tps6128x_SLAVE_ADDR_Read    0xEB
#define I2C3 4
#define I2C_BUSNUM I2C3
#define tps6128x_BUSNUM I2C3
#define DRIVER_NAME "tps6128x"

/**********************************************************
  *   Function Prototype
  *********************************************************/
static int tps6128x_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps6128x_remove(struct i2c_client *client);

/**********************************************************
  *  Data Structure
  *********************************************************/
static const struct i2c_device_id tps6128x_i2c_id[] = {
	{DRIVER_NAME, 0},
	{}
};

static struct of_device_id tps6128x_match_table[] = {
	{.compatible = "htc,i2c_boost_bypass",}
};

static struct i2c_driver tps6128x_driver = {
	.id_table    = tps6128x_i2c_id,
	.probe       = tps6128x_driver_probe,
	.remove		= tps6128x_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name    = DRIVER_NAME,
		.of_match_table = tps6128x_match_table,
	},
};

static struct i2c_board_info __initdata i2c_tps6128x = {
	I2C_BOARD_INFO(DRIVER_NAME, (tps6128x_SLAVE_ADDR_WRITE >> 1))
};

/**********************************************************
  *   [Global Variable]
  *********************************************************/
static struct i2c_client *tps6128x_client = NULL;
static DEFINE_MUTEX(tps6128x_i2c_access);
uint8_t tps6128x_reg[tps6128x_REG_NUM] = {0};
int is_tps6128x_hw_exist = 0;
#if defined(CONFIG_DEBUG_FS)
static struct dentry *tps6128x_debugfs_base;
#endif

/**********************************************************
  *   I2C Function For Read/Write tps6128x
  *********************************************************/
uint32_t tps6128x_read_byte(uint8_t addr, uint8_t *value)
{
	int ret = 0;
	char buf[1] = {0};

	mutex_lock(&tps6128x_i2c_access);
	tps6128x_client->ext_flag = ((tps6128x_client->ext_flag ) & I2C_MASK_FLAG ) | 
		I2C_WR_FLAG | I2C_DIRECTION_FLAG;

	buf[0] = addr;
	ret = i2c_master_send(tps6128x_client, &buf[0], (1<<8 | 1));
	if (ret < 0)
	{
		TPS_ERR("i2c read failed. addr=0x%x\n", addr);
		tps6128x_client->ext_flag = 0;
		mutex_unlock(&tps6128x_i2c_access);
		return ret;
	}

	*value = buf[0];

	tps6128x_client->ext_flag = 0;
	mutex_unlock(&tps6128x_i2c_access);

	return ret;
}

uint32_t tps6128x_write_byte(uint8_t addr, uint8_t value)
{
	int ret = 0;
	char buf[2] = {0};

	mutex_lock(&tps6128x_i2c_access);

	buf[0] = addr;
	buf[1] = value;

	tps6128x_client->ext_flag = ((tps6128x_client->ext_flag ) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;

    ret = i2c_master_send(tps6128x_client, buf, 2);

	if (ret < 0) {
		TPS_ERR("i2c write error! addr=0x%x, ret=%d\n", addr, ret);
	} else {
		TPS_DBG("i2c write success. addr=0x%x\n", addr);
	}

	tps6128x_client->ext_flag = 0;
	mutex_unlock(&tps6128x_i2c_access);

	return ret;
}

uint32_t tps6128x_read_interface (uint8_t addr, uint8_t *val, uint8_t mask, uint8_t shift)
{
	uint32_t ret = 0;
	uint8_t reg_value = 0;

	ret = tps6128x_read_byte(addr, &reg_value);

	TPS_DBG("addr[%x]=0x%x\n", addr, reg_value);

	reg_value &= (mask << shift);
	*val = (reg_value >> shift);

	TPS_DBG("value=0x%x\n", *val);

	return ret;
}
EXPORT_SYMBOL(tps6128x_read_interface);

uint32_t tps6128x_write_interface (uint8_t addr, uint8_t val, uint8_t mask, uint8_t shift)
{
	uint32_t ret = 0;
	uint8_t reg_value = 0;

	ret = tps6128x_read_byte(addr, &reg_value);
	TPS_DBG("addr[%x]=0x%x\n", addr, reg_value);

	reg_value &= ~(mask << shift);
	reg_value |= (val << shift);

	ret = tps6128x_write_byte(addr, reg_value);
	TPS_DBG("write addr[%x]=0x%x\n", addr, reg_value);

	/* Read it back to check */
	tps6128x_read_byte(addr, &reg_value);
	TPS_DBG("check addr[%x]=0x%x\n", addr, reg_value);

	return ret;
}
EXPORT_SYMBOL(tps6128x_write_interface);

/**********************************************************
  *   Internal Function
  *********************************************************/
bool tps6128x_hw_detect(void)
{
	uint32_t ret = 0;
	uint8_t val = 0;

	// Read chip version
	ret = tps6128x_read_interface(TPS6128x_CHIPVERSION_ADDR, &val,
		TPS6128x_CHIPVERSION_MASK, TPS6128x_CHIPVERSION_SHIFT);
	TPS_LOG("chip version is %d \n", val);

	// Read output voltage threshold
	ret = tps6128x_read_interface(TPS6128x_VOUTROOFSET_VOUTROOF_TH_ADDR, &val,
		TPS6128x_VOUTROOFSET_VOUTROOF_TH_MASK, TPS6128x_VOUTROOFSET_VOUTROOF_TH_SHIFT);

	TPS_DBG("addr[0x%x]=0x%x\n", TPS6128x_VOUTROOFSET_VOUTROOF_TH_ADDR, val);

	if(val == 0) // If output_voltage _threshold is 0, it means there is no TPS6128x
		return false;
	else
		return true;
}

void tps6128x_hw_init(void)
{
	// MODE_CTRL[1:0] = Auto mode
	tps6128x_write_interface(TPS6128x_CONFIG_MODE_CTRL_ADDR, 0x1,
		TPS6128x_CONFIG_MODE_CTRL_MASK, TPS6128x_CONFIG_MODE_CTRL_SHIFT);

	// Output voltage threshold, boost / pass-through mode change = 3.4V
	tps6128x_write_interface(TPS6128x_VOUTROOFSET_VOUTROOF_TH_ADDR, 0xB,
		TPS6128x_VOUTROOFSET_VOUTROOF_TH_MASK, TPS6128x_VOUTROOFSET_VOUTROOF_TH_SHIFT);

	// Current limit in boost mode = max = 5000mA
	tps6128x_write_interface(TPS6128x_ILIMSET_ILIM_ADDR, 0xF,
		TPS6128x_ILIMSET_ILIM_MASK,  TPS6128x_ILIMSET_ILIM_SHIFT);

	TPS_LOG("HW init done \n");
}

#if defined(CONFIG_DEBUG_FS)
#define TPS6128x_ACCESS_NAME "tps6128x_access"
uint8_t tps6128x_reg_addr = 0;
static int tps6128x_reg_data_set(void *data, u64 val)
{
	tps6128x_write_interface(tps6128x_reg_addr, val, 0xFF, 0);

	return 0;
}

static int tps6128x_reg_data_get(void *data, u64 *val)
{
	uint8_t reg_val = 0;

	tps6128x_read_interface(tps6128x_reg_addr, &reg_val, 0xFF, 0x0);
	*val = reg_val;

	return 0;
}

static int tps6128x_reg_address_set(void *data, u64 val)
{
	tps6128x_reg_addr = val;

	return 0;
}

static int tps6128x_reg_address_get(void *data, u64 *val)
{
	*val = tps6128x_reg_addr;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tps6128x_data_fops, tps6128x_reg_data_get,
			tps6128x_reg_data_set, "%llx\n");


DEFINE_SIMPLE_ATTRIBUTE(tps6128x_address_fops, tps6128x_reg_address_get,
			tps6128x_reg_address_set, "%llx\n");
#endif

static int tps6128x_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{
	int ret = 0;
	struct dentry *temp;

	if (!(tps6128x_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
		ret = -ENOMEM;
		goto exit;
	}
	memset(tps6128x_client, 0, sizeof(struct i2c_client));

	tps6128x_client = client;

	if (tps6128x_hw_detect()) { // If tps6128x, init it again
		tps6128x_hw_init();
	} else {
		TPS_ERR("tps6128x doesn't exist\n");
		goto exit;
	}

#if defined(CONFIG_DEBUG_FS)
	tps6128x_debugfs_base = debugfs_create_dir(TPS6128x_ACCESS_NAME, NULL);
	if (IS_ERR_OR_NULL(tps6128x_debugfs_base))
		TPS_ERR("tps6128x debugfs base directory creation failed\n");

	temp = debugfs_create_file("data", S_IRUGO, tps6128x_debugfs_base,
					client, &tps6128x_data_fops);
	if (IS_ERR_OR_NULL(temp)) {
		TPS_ERR("access node creation failed\n");
	}

	temp = debugfs_create_file("address", S_IRUGO, tps6128x_debugfs_base,
					client, &tps6128x_address_fops);
	if (IS_ERR_OR_NULL(temp)) {
		TPS_ERR("access node creation failed\n");
	}
#endif

	return 0;

exit:
	return ret;

}

static int tps6128x_remove(struct i2c_client *client)
{
	tps6128x_client = NULL;
	i2c_unregister_device(client);
	debugfs_remove_recursive(tps6128x_debugfs_base);
	return 0;
}

static int __init tps6128x_init(void)
{
	int ret = 0;

	ret = i2c_register_board_info(tps6128x_BUSNUM, &i2c_tps6128x, 1);
	if (ret)
		TPS_ERR("i2c_register_board_info failed! ret=%d\n", ret);

	ret = i2c_add_driver(&tps6128x_driver);
	if (ret)
		TPS_ERR("i2c_add_driver failed! ret=%d\n", ret);

	return 0;
}

static void __exit tps6128x_exit(void)
{
	i2c_del_driver(&tps6128x_driver);
}

module_init(tps6128x_init);
module_exit(tps6128x_exit);

MODULE_AUTHOR("HTC PMIC TEAM");
MODULE_DESCRIPTION("HTC TPS6128x I2C DRIVER");
MODULE_LICENSE("GPL");
