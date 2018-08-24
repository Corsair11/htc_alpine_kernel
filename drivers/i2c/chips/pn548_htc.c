
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mt-plat/mt_gpio.h>
#include <linux/htc_devices_dtb.h>
#include <linux/pn548.h>
#include "pn548_htc.h"


#define D(x...)	\
	if (is_debug) \
		printk(KERN_DEBUG "[NFC] " x)
#define I(x...) printk(KERN_INFO "[NFC] " x)
#define E(x...) printk(KERN_ERR "[NFC] [Err] " x)

static  struct regulator *regulator;
static   unsigned int NFC_I2C_SCL;
static   unsigned int NFC_I2C_SDA;
static   unsigned int NFC_PVDD_GPIO;
int is_regulator = 0;
int is_pvdd_gpio = 0;
const char *regulator_name;

#if NFC_READ_RFSKUID_MTK6795
#define HAS_NFC_CHIP 0x7000000
#endif 

int pn548_htc_check_rfskuid(int in_is_alive){
#if NFC_READ_RFSKUID_MTK6795
	int i;
#if NFC_READ_RFSKUID_A53ML
	if(of_machine_hwid() < 2) {
		I("%s: of_machine_hwid() = %d\n",__func__,of_machine_hwid());
		return 0;
	}
#endif  
	for ( i = 2; i <= 9; i++) {
		I("%s: get_sku_data(%d) = 0x%x\n",__func__,i,get_sku_data(i));
		if (get_sku_data(i) == HAS_NFC_CHIP) {
			I("%s: Check get_sku_data done device has NFC chip\n",__func__);
			return 1;
		}
	}
	I("%s: Check get_sku_data done remove NFC\n",__func__);
	return 0;
#else 
	return in_is_alive;
#endif 
}


int pn548_htc_get_bootmode(void) {
	char sbootmode[30] = "others";
#if NFC_GET_BOOTMODE
	strcpy(sbootmode,htc_get_bootmode());
#endif  
	if (strcmp(sbootmode, "offmode_charging") == 0) {
		I("%s: Check bootmode done NFC_BOOT_MODE_OFF_MODE_CHARGING\n",__func__);
		return NFC_BOOT_MODE_OFF_MODE_CHARGING;
	} else if (strcmp(sbootmode, "charger") == 0) {
		I("%s: Check bootmode done NFC_BOOT_MODE_OFF_MODE_CHARGING\n",__func__);
		return NFC_BOOT_MODE_OFF_MODE_CHARGING;
	} else if (strcmp(sbootmode, "ftm") == 0 || strcmp(sbootmode, "factory") == 0) {
		I("%s: Check bootmode done NFC_BOOT_MODE_FTM\n",__func__);
		return NFC_BOOT_MODE_FTM;
	} else if (strcmp(sbootmode, "download") == 0) {
		I("%s: Check bootmode done NFC_BOOT_MODE_DOWNLOAD\n",__func__);
		return NFC_BOOT_MODE_DOWNLOAD;
	} else {
		I("%s: Check bootmode done NFC_BOOT_MODE_NORMAL mode = %s\n",__func__,sbootmode);
		return NFC_BOOT_MODE_NORMAL;
	}
}

bool pn548_htc_parse_dt(struct device *dev) {
        struct property *prop;
        int ret;
	u32 buf = 0;
        I("%s:\n", __func__);

	prop = of_find_property(dev->of_node, "nfc_i2c_scl", NULL);
	if(prop) {
		of_property_read_u32(dev->of_node, "nfc_i2c_scl", &buf);
		NFC_I2C_SCL = buf;
	} else {
		I("%s: invalid nfc_i2c_scl pin\n", __func__);
		return false;
	}
        prop = of_find_property(dev->of_node, "nfc_i2c_sda", NULL);
        if(prop) {
                of_property_read_u32(dev->of_node, "nfc_i2c_sda", &buf);
                NFC_I2C_SDA = buf;
        } else {
                I("%s: invalid nfc_i2c_sda pin\n", __func__);
                return false;
        }
        prop = of_find_property(dev->of_node, "nfc_pvdd_regulator", NULL);
        if(prop)
        {
                ret = of_property_read_string(dev->of_node,"nfc_pvdd_regulator",&regulator_name);
                if(ret <0)
                        return false;
                is_regulator = 1;
        }

#ifndef NFC_IOEXT_PIN_CTRL_CONFIG
        prop = of_find_property(dev->of_node, "nfc_pvdd_gpio", NULL);
        if(prop) {
		of_property_read_u32(dev->of_node, "nfc_pvdd_gpio", &buf);
		NFC_PVDD_GPIO = buf;
		I("%s: NFC_PVDD_GPIO = %d\n", __func__, NFC_PVDD_GPIO);
                is_pvdd_gpio = 1;
        } else {
                is_pvdd_gpio = 0;
                I("%s: do not have NFC_PVDD_GPIO\n", __func__);
        }
#endif
        I("%s: End, NFC_I2C_SCL:%d, NFC_I2C_SDA:%d, is_regulator:%d, is_pvdd_gpio:%d\n", __func__,NFC_I2C_SCL, NFC_I2C_SDA, is_regulator, is_pvdd_gpio);

        return true;
}

void pn548_htc_turn_off_pvdd (void) {
	int ret;

	if(is_regulator)
	{
		ret = regulator_disable(regulator);
		I("%s : %s workaround regulator_disable\n", __func__, regulator_name);
		I("%s : %s workaround regulator_is_enabled = %d\n", __func__, regulator_name, regulator_is_enabled(regulator));
		if (ret < 0) {
			E("%s : %s workaround regulator_disable fail\n", __func__, regulator_name);
		}
	}

	if(is_pvdd_gpio)
	{
        	gpio_direction_output(NFC_PVDD_GPIO, 0);
		I("%s : NFC_PVDD_GPIO set 0 , chk nfc_pvdd:%d \n", __func__, gpio_get_value(NFC_PVDD_GPIO));
	}

}

bool pn548_htc_turn_on_pvdd (struct i2c_client *client)
{
	int ret;

	if(is_regulator)
	{
		if(!regulator) {
			regulator = regulator_get(&client->dev, regulator_name);
			I("%s : %s workaround regulator_get\n", __func__, regulator_name);
			if (regulator < 0) {
				E("%s : %s workaround regulator_get fail\n", __func__, regulator_name);
				return false;
			}
		}
		ret = regulator_set_voltage(regulator, 1800000, 1800000);
		I("%s : %s workaround regulator_set_voltage\n", __func__, regulator_name);
		if (ret < 0) {
			E("%s : %s workaround regulator_set_voltage fail\n", __func__, regulator_name);
			return false;
		}
		ret = regulator_enable(regulator);
		I("%s : %s workaround regulator_enable\n", __func__, regulator_name);
		I("%s : %s workaround regulator_is_enabled = %d\n", __func__, regulator_name, regulator_is_enabled(regulator));
		if (ret < 0) {
			E("%s : %s workaround regulator_enable fail\n", __func__, regulator_name);
			return false;
		}
		return true;
	}

	if(is_pvdd_gpio)
	{
		I("%s: config NFC_1V8_EN pin\n", __func__);
		ret = gpio_direction_output(NFC_PVDD_GPIO, 1);
		I("%s : NFC_PVDD_GPIO set 1 ret:%d, chk nfc_pvdd:%d \n", __func__,ret, gpio_get_value(NFC_PVDD_GPIO));

	}

	return true;
}


void pn548_power_off_seq(int is_alive) {
#if NFC_POWER_OFF_SEQUENCE
        if (is_alive) {
        int ret;
	I("%s: NFC PN548 Power off sequence\n", __func__);
        ret = gpio_request(NFC_I2C_SCL , "nfc_i2c_scl");
        if(ret) {
                E("%s request scl error\n",__func__);
        }
        ret = gpio_request(NFC_I2C_SDA , "nfc_i2c_sda");
        if(ret){
                E("%s request sda error\n",__func__);
        }

        ret = gpio_direction_output(NFC_I2C_SCL, 0);
        I("%s : NFC_I2C_SCL set 0 %d \n", __func__,ret);
        mdelay(1);
        ret = gpio_direction_output(NFC_I2C_SDA, 0);
        I("%s : NFC_I2C_SDA set 0 %d \n", __func__,ret);
        mdelay(50);
        }
#endif 
}

#ifdef NFC_PLL_CLOCK_ENABLE
extern bool clk_buf_ctrl(enum clk_buf_id id, bool onoff);

int pn548_pll_clk_ctrl(int onoff) {
	int ret = 1;

	
	ret = clk_buf_ctrl(CLK_BUF_NFC, onoff);
	if (!ret){
		E("%s : Setting PLL clock fail! onoff:%d, ret:%d", __func__, onoff, ret);
	}else {
		I("%s : Setting PLL clock success! onoff:%d, ret:%d", __func__, onoff, ret);
	}

	return ret;
}
#endif

#ifdef NFC_GPIO_PINCTRL_ENABLE
bool pn548_htc_check_pvdd_gpio(void)
{
	I("%s:  is_pvdd_gpio :%d \n", __func__, is_pvdd_gpio);
	return is_pvdd_gpio;
}

unsigned int pn548_htc_get_pvdd_gpio(void)
{
	I("%s:  NFC_PVDD_GPIO :%d \n", __func__, NFC_PVDD_GPIO);
	return NFC_PVDD_GPIO;
}
#endif

