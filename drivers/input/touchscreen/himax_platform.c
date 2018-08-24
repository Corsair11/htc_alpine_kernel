/* Himax Android Driver Sample Code for Himax chipset
*
* Copyright (C) 2014 Himax Corporation.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/himax_platform.h>

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define D(x...) printk(KERN_DEBUG "[TP] " x)
#define I(x...) printk(KERN_INFO "[TP] " x)
#define W(x...) printk(KERN_WARNING "[TP][WARNING] " x)
#define E(x...) printk(KERN_ERR "[TP][ERROR] " x)
#else
#define D(x...)
#define I(x...)
#define W(x...)
#define E(x...)
#define DIF(x...)
#endif

int irq_enable_count = 0;

#ifdef HX_MTK_DMA
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
static u8 *gpDMABuf_va = NULL;
static uint32_t gpDMABuf_pa = NULL;
#else
u8 *gpDMABuf_va = NULL;
u8 *gpDMABuf_pa = NULL;
#endif
#endif

#ifdef QCT
int i2c_himax_read(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &command,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		}
	};

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		msleep(10);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n",
		  __func__, toRetry);
		return -EIO;
	}
	return 0;

}

int i2c_himax_write(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry;
	uint8_t buf[length + 1];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	buf[0] = command;
	memcpy(buf + 1, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
		  __func__, toRetry);
		return -EIO;
	}
	return 0;

}

int i2c_himax_read_command(struct i2c_client *client, uint8_t length, uint8_t *data, uint8_t *readlength, uint8_t toRetry)
{
	int retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		}
	};

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n",
		  __func__, toRetry);
		return -EIO;
	}
	return 0;
}

int i2c_himax_write_command(struct i2c_client *client, uint8_t command, uint8_t toRetry)
{
	return i2c_himax_write(client, command, NULL, 0, toRetry);
}

int i2c_himax_master_write(struct i2c_client *client, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry;
	uint8_t buf[length];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = buf,
		}
	};

	memcpy(buf, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
		  __func__, toRetry);
		return -EIO;
	}
	return 0;
}

void himax_int_enable(int irqnum, int enable)
{
	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
	}
	I("irq_enable_count = %d", irq_enable_count);
}

void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
	       regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int himax_power_on(struct himax_i2c_platform_data *pdata, bool on)
{
	int rc;

	if (on == false)
		goto power_off;

	rc = reg_set_optimum_mode_check(pdata->vcc_ana, HX_ACTIVE_LOAD_UA);
	if (rc < 0) {
		E("Regulator vcc_ana set_opt failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(pdata->vcc_ana);
	if (rc) {
		E("Regulator vcc_ana enable failed rc=%d\n", rc);
		goto error_reg_en_vcc_ana;
	}

	if (pdata->digital_pwr_regulator) {
		rc = reg_set_optimum_mode_check(pdata->vcc_dig,
						HX_ACTIVE_LOAD_DIG_UA);
		if (rc < 0) {
			E("Regulator vcc_dig set_opt failed rc=%d\n",
			  rc);
			goto error_reg_opt_vcc_dig;
		}

		rc = regulator_enable(pdata->vcc_dig);
		if (rc) {
			E("Regulator vcc_dig enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_dig;
		}
	}

	if (pdata->i2c_pull_up) {
		rc = reg_set_optimum_mode_check(pdata->vcc_i2c, HX_I2C_LOAD_UA);
		if (rc < 0) {
			E("Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(pdata->vcc_i2c);
		if (rc) {
			E("Regulator vcc_i2c enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_i2c;
		}
	}

	msleep(130);

	return 0;

error_reg_en_vcc_i2c:
	if (pdata->i2c_pull_up)
		reg_set_optimum_mode_check(pdata->vcc_i2c, 0);
error_reg_opt_i2c:
	if (pdata->digital_pwr_regulator)
		regulator_disable(pdata->vcc_dig);
error_reg_en_vcc_dig:
	if (pdata->digital_pwr_regulator)
		reg_set_optimum_mode_check(pdata->vcc_dig, 0);
error_reg_opt_vcc_dig:
	regulator_disable(pdata->vcc_ana);
error_reg_en_vcc_ana:
	reg_set_optimum_mode_check(pdata->vcc_ana, 0);
	return rc;

power_off:
	reg_set_optimum_mode_check(pdata->vcc_ana, 0);
	regulator_disable(pdata->vcc_ana);
	if (pdata->digital_pwr_regulator) {
		reg_set_optimum_mode_check(pdata->vcc_dig, 0);
		regulator_disable(pdata->vcc_dig);
	}
	if (pdata->i2c_pull_up) {
		reg_set_optimum_mode_check(pdata->vcc_i2c, 0);
		regulator_disable(pdata->vcc_i2c);
	}
	msleep(50);
	return 0;
}

static int himax_regulator_configure(struct i2c_client *client, struct himax_i2c_platform_data *pdata, bool on)
{
	int rc;

	if (on == false)
		goto hw_shutdown;

	pdata->vcc_ana = regulator_get(&client->dev, "vdd_ana");
	if (IS_ERR(pdata->vcc_ana)) {
		rc = PTR_ERR(pdata->vcc_ana);
		E("Regulator get failed vcc_ana rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->vcc_ana) > 0) {
		rc = regulator_set_voltage(pdata->vcc_ana, HX_VTG_MIN_UV,
					   HX_VTG_MAX_UV);
		if (rc) {
			E("regulator set_vtg failed rc=%d\n", rc);
			goto error_set_vtg_vcc_ana;
		}
	}
	if (pdata->digital_pwr_regulator) {
		pdata->vcc_dig = regulator_get(&client->dev, "vdd_dig");
		if (IS_ERR(pdata->vcc_dig)) {
			rc = PTR_ERR(pdata->vcc_dig);
			E("Regulator get dig failed rc=%d\n", rc);
			goto error_get_vtg_vcc_dig;
		}

		if (regulator_count_voltages(pdata->vcc_dig) > 0) {
			rc = regulator_set_voltage(pdata->vcc_dig,
						   HX_VTG_DIG_MIN_UV, HX_VTG_DIG_MAX_UV);
			if (rc) {
				E("regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_vcc_dig;
			}
		}
	}
	if (pdata->i2c_pull_up) {
		pdata->vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
		if (IS_ERR(pdata->vcc_i2c)) {
			rc = PTR_ERR(pdata->vcc_i2c);
			E("Regulator get failed rc=%d\n",	rc);
			goto error_get_vtg_i2c;
		}
		if (regulator_count_voltages(pdata->vcc_i2c) > 0) {
			rc = regulator_set_voltage(pdata->vcc_i2c,
						   HX_I2C_VTG_MIN_UV, HX_I2C_VTG_MAX_UV);
			if (rc) {
				E("regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_i2c;
			}
		}
	}

	return 0;

error_set_vtg_i2c:
	regulator_put(pdata->vcc_i2c);
error_get_vtg_i2c:
	if (pdata->digital_pwr_regulator)
		if (regulator_count_voltages(pdata->vcc_dig) > 0)
			regulator_set_voltage(pdata->vcc_dig, 0,
					      HX_VTG_DIG_MAX_UV);
error_set_vtg_vcc_dig:
	if (pdata->digital_pwr_regulator)
		regulator_put(pdata->vcc_dig);
error_get_vtg_vcc_dig:
	if (regulator_count_voltages(pdata->vcc_ana) > 0)
		regulator_set_voltage(pdata->vcc_ana, 0, HX_VTG_MAX_UV);
error_set_vtg_vcc_ana:
	regulator_put(pdata->vcc_ana);
	return rc;

hw_shutdown:
	if (regulator_count_voltages(pdata->vcc_ana) > 0)
		regulator_set_voltage(pdata->vcc_ana, 0, HX_VTG_MAX_UV);
	regulator_put(pdata->vcc_ana);
	if (pdata->digital_pwr_regulator) {
		if (regulator_count_voltages(pdata->vcc_dig) > 0)
			regulator_set_voltage(pdata->vcc_dig, 0,
					      HX_VTG_DIG_MAX_UV);
		regulator_put(pdata->vcc_dig);
	}
	if (pdata->i2c_pull_up) {
		if (regulator_count_voltages(pdata->vcc_i2c) > 0)
			regulator_set_voltage(pdata->vcc_i2c, 0,
					      HX_I2C_VTG_MAX_UV);
		regulator_put(pdata->vcc_i2c);
	}
	return 0;
}

int himax_gpio_power_config(struct i2c_client *client, struct himax_i2c_platform_data *pdata)
{
	int error;

	error = himax_regulator_configure(client, pdata, true);
	if (error) {
		E("Failed to intialize hardware\n");
		goto err_regulator_not_on;
	}

	if (gpio_is_valid(pdata->gpio_reset)) {
		
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");
		if (error) {
			E("unable to request gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);
		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_req;
		}
	}

	error = himax_power_on(pdata, true);
	if (error) {
		E("Failed to power on hardware\n");
		goto err_gpio_reset_req;
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		
		error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");
		if (error) {
			E("unable to request gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_power_on;
		}
		error = gpio_direction_input(pdata->gpio_irq);
		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_gpio_irq_req;
		}
		client->irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_power_on;
	}

	msleep(20);
	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);
		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_irq_req;
		}
	}
	return 0;

err_gpio_irq_req:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
err_power_on:
	himax_power_on(pdata, false);
err_gpio_reset_req:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
err_regulator_on:
	himax_regulator_configure(client, pdata, false);
err_regulator_not_on:

	return error;
}

#else
int himax_gpio_power_config(struct i2c_client *client, struct himax_i2c_platform_data *pdata)
{
	int error = 0;

	if (gpio_is_valid(pdata->gpio_irq)) {
		
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");
		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			return error;
		}
		error = gpio_direction_input(pdata->gpio_irq);
		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
			return error;
		}
		client->irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		return error;
	}

	msleep(20);
	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");
		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			return error;
		}
	}

	if (gpio_is_valid(pdata->gpio_3v3_en)) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");
		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			return error;
		}
		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en pin =%d\n", gpio_get_value(pdata->gpio_3v3_en));
		mdelay(5);
		gpio_direction_output(pdata->gpio_reset, 0);
		mdelay(40);
		gpio_direction_output(pdata->gpio_reset, 1);
		mdelay(20);
	}

	return error;
}

#endif	
#endif	

#ifdef CONFIG_OF
static struct himax_i2c_platform_data *private_pdata = NULL;
int himax_pinctrl_init(struct device *dev)
{
	int ret;
	struct himax_i2c_platform_data *pdata = dev_get_platdata(dev);
	private_pdata = pdata;

	I("%s\n", __func__);
	
	pdata->ts_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pdata->ts_pinctrl)) {
		I("Target does not use pinctrl");
		ret = PTR_ERR(pdata->ts_pinctrl);
		pdata->ts_pinctrl = NULL;
		return ret;
	}

	pdata->gpio_state_active
		= pinctrl_lookup_state(pdata->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(pdata->gpio_state_active)) {
		I("Can not get ts default pinstate\n");
		ret = PTR_ERR(pdata->gpio_state_active);
		pdata->ts_pinctrl = NULL;
		return ret;
	}

	pdata->gpio_state_suspend
		= pinctrl_lookup_state(pdata->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(pdata->gpio_state_suspend)) {
		I("Can not get ts sleep pinstate\n");
		ret = PTR_ERR(pdata->gpio_state_suspend);
		pdata->ts_pinctrl = NULL;
		return ret;
	}

#if defined(MTK) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	pdata->gpio_reset_active
		= pinctrl_lookup_state(pdata->ts_pinctrl, "pmx_ts_reset_active");
	if (IS_ERR_OR_NULL(pdata->gpio_reset_active)) {
		I("Can not get ts reset default pinstate\n");
		ret = PTR_ERR(pdata->gpio_reset_active);
		pdata->ts_pinctrl = NULL;
		return ret;
	}

	pdata->gpio_reset_suspend
		= pinctrl_lookup_state(pdata->ts_pinctrl, "pmx_ts_reset_inactive");
	if (IS_ERR_OR_NULL(pdata->gpio_reset_suspend)) {
		I("Can not get ts reset inactive pinstate\n");
		ret = PTR_ERR(pdata->gpio_reset_suspend);
		pdata->ts_pinctrl = NULL;
		return ret;
	}
#endif
	return 0;
}

void himax_pinctrl_deinit(struct device *dev)
{
	struct himax_i2c_platform_data *pdata = dev_get_platdata(dev);

	I("%s\n", __func__);
	
	if (pdata->ts_pinctrl != NULL) {
		pdata->gpio_state_active = NULL;
		pdata->gpio_state_suspend = NULL;
		pdata->gpio_reset_active = NULL;
		pdata->gpio_reset_suspend = NULL;
		devm_pinctrl_put(pdata->ts_pinctrl);
		private_pdata = NULL;
	}
}

int himax_pinctrl_select(struct device *dev, int on)
{
	struct pinctrl_state *pins_state;
	struct himax_i2c_platform_data *pdata = dev_get_platdata(dev);
	int ret;

	I("%s: set to %d\n", __func__, on);
	pins_state = on ? pdata->gpio_state_active
		     : pdata->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(pdata->ts_pinctrl, pins_state);
		if (ret) {
			E("can not set %s pins\n",
			  on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else
		E("not a valid '%s' pinstate\n",
		  on ? "pmx_ts_active" : "pmx_ts_suspend");

	return 0;
}
#endif	

#ifdef MTK
DEFINE_MUTEX(hx_wr_access);

int i2c_himax_read(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
#ifdef HX_MTK_DMA
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	int rc, i;
	unsigned short addr = 0;
	u8 *pReadData = 0;
	pReadData = gpDMABuf_va;
	addr = client->addr ;

	toRetry = 0; 
	client->addr = client->addr & (I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG);
	client->timing = 400;

	if (!pReadData) {
		E(" dma_alloc_coherent failed!\n");
		return -1;
	}
	gpDMABuf_va[0] = command;
	rc = i2c_master_send(client, gpDMABuf_pa, 1);
	if (rc < 0) {
		E(" hx852xes_i2c_dma_recv_data sendcomand failed!\n");
	}
	rc = i2c_master_recv(client, gpDMABuf_pa, length);
	client->addr = addr;

	for (i = 0; i < length; i++) {
		data[i] = gpDMABuf_va[i];
	}
	return rc;
#else
	int ret = 0;
	s32 retry = 0;
	u8 buffer[1];

	struct i2c_msg msg[] = {
		{
			.addr = (client->addr & I2C_MASK_FLAG),
			.flags = 0,
			.buf = buffer,
			.len = 1,
			.timing = 400
		},
		{
			.addr = (client->addr & I2C_MASK_FLAG),
			.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = I2C_M_RD,
			.buf = gpDMABuf_pa,
			.len = length,
			.timing = 400
		},
	};
	mutex_lock(&hx_wr_access);
	buffer[0] = command;

	if (data == NULL) {
		mutex_unlock(&hx_wr_access);
		return -1;
	}
	for (retry = 0; retry < toRetry; ++retry) {
		ret = i2c_transfer(client->adapter, &msg[0], 2);
		if (ret < 0) {
			continue;
		}
		memcpy(data, gpDMABuf_va, length);
		mutex_unlock(&hx_wr_access);
		return 0;
	}
	E("Dma I2C Read Error: %d byte(s), err-code: %d", length, ret);
	mutex_unlock(&hx_wr_access);
	return ret;
#endif	
#else
	return i2c_smbus_read_i2c_block_data(client, command, length, data);
#endif

}

int i2c_himax_write(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{

#ifdef HX_MTK_DMA
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	int rc, i;
	unsigned short addr = 0;
	u8 *pWriteData = gpDMABuf_va;
	addr = client->addr ;

	toRetry = 0; 
	client->addr = client->addr & (I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG);
	client->timing = 400;

	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		return -1;
	}

	gpDMABuf_va[0] = command;

	for (i = 0; i < length; i++) {
		gpDMABuf_va[i + 1] = data[i];
	}

	rc = i2c_master_send(client, gpDMABuf_pa, length + 1);
	client->addr = addr;
	return rc;
#else
	int rc = 0, retry = 0;
	u8 *pWriteData = gpDMABuf_va;

	struct i2c_msg msg[] = {
		{
			.addr = (client->addr & I2C_MASK_FLAG),
			.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = 0,
			.buf = gpDMABuf_pa,
			.len = length + 1,
			.timing = 400
		},
	};

	mutex_lock(&hx_wr_access);
	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		mutex_unlock(&hx_wr_access);
		return -1;
	}

	gpDMABuf_va[0] = command;

	memcpy(gpDMABuf_va + 1, data, length);
	for (retry = 0; retry < toRetry; ++retry) {
		rc = i2c_transfer(client->adapter, &msg[0], 1);
		if (rc < 0) {
			continue;
		}
		mutex_unlock(&hx_wr_access);
		return 0;
	}
	E("Dma I2C master write Error: %d byte(s), err-code: %d", length, rc);
	mutex_unlock(&hx_wr_access);
	return rc;
#endif	
#else
	return i2c_smbus_write_i2c_block_data(client, command, length, data);
#endif

}

int i2c_himax_read_command(struct i2c_client *client, uint8_t length, uint8_t *data, uint8_t *readlength, uint8_t toRetry)
{
	return 0;
}

int i2c_himax_write_command(struct i2c_client *client, uint8_t command, uint8_t toRetry)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	return i2c_himax_write(client, command, NULL, 0, 0);
#else
	return i2c_himax_write(client, command, NULL, 0, toRetry);
#endif
}

int i2c_himax_master_write(struct i2c_client *client, uint8_t *data, uint8_t length, uint8_t toRetry)
{
#ifdef HX_MTK_DMA
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	int i, rc;
	unsigned short addr = 0;
	u8 *pWriteData = gpDMABuf_va;

	addr = client->addr ;

	toRetry = 0; 
	client->addr = client->addr & (I2C_MASK_FLAG | I2C_DMA_FLAG | I2C_ENEXT_FLAG);
	client->timing = 400;

	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		return -1;
	}

	for (i = 0; i < length; i++) {
		gpDMABuf_va[i] = data[i];
	}

	rc = i2c_master_send(client, gpDMABuf_pa, length);
	client->addr = addr;
	return rc;
#else
	int rc = 0, retry = 0;
	u8 *pWriteData = gpDMABuf_va;

	struct i2c_msg msg[] = {
		{
			.addr = (client->addr & I2C_MASK_FLAG),
			.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = 0,
			.buf = gpDMABuf_pa,
			.len = length,
			.timing = 400
		},
	};

	mutex_lock(&hx_wr_access);
	if (!pWriteData) {
		E("dma_alloc_coherent failed!\n");
		mutex_unlock(&hx_wr_access);
		return -1;
	}

	memcpy(gpDMABuf_va, data, length);
	for (retry = 0; retry < toRetry; ++retry) {
		rc = i2c_transfer(client->adapter, &msg[0], 1);
		if (rc < 0) {
			continue;
		}
		mutex_unlock(&hx_wr_access);
		return 0;
	}
	E("Dma I2C master write Error: %d byte(s), err-code: %d", length, rc);
	mutex_unlock(&hx_wr_access);
	return rc;
#endif	
#else
	int i;
	uint8_t command;
	uint8_t dmabuf[length + 1];

	command = data[0];
	for (i = 1; i < length; i++) {
		dmabuf[i - 1] = data[i];
	}
	return i2c_smbus_write_i2c_block_data(client, command, length, dmabuf);
#endif

}

void himax_int_enable(int irqnum, int enable)
{
	if (enable == 1 && irq_enable_count == 0) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		enable_irq(irqnum);
#endif
		irq_enable_count++;
	} else if (enable == 0 && irq_enable_count == 1) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#else
		disable_irq_nosync(irqnum);
#endif
		irq_enable_count--;
	}
	I("irq_enable_count = %d", irq_enable_count);
}

void himax_rst_gpio_set(int pinnum, uint8_t value)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);

	if (value) {
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	} else {
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	}
#else
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = value ? private_pdata->gpio_reset_suspend
		     : private_pdata->gpio_reset_active;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(private_pdata->ts_pinctrl, pins_state);
		if (ret) {
			E("can not set %s pins\n",
			  value ? "pmx_ts_reset_inactive" : "pmx_ts_reset_active");
		}
	} else
		E("not a valid '%s' pinstate\n",
		  value ? "pmx_ts_reset_inactive" : "pmx_ts_reset_active");
#endif
}

uint8_t himax_int_gpio_read(int pinnum)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	return mt_get_gpio_in(GPIO_CTP_EINT_PIN);
#else
	return  gpio_get_value(pinnum);
#endif
}

int himax_gpio_power_config(struct i2c_client *client, struct himax_i2c_platform_data *pdata)
{
	int error = 0;

	
	
	
	

	
	if (gpio_is_valid(pdata->gpio_irq)) {
		
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");
		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			return error;
		}
		
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
		mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
		mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_DISABLE);
#endif
		
	} else {
		E("irq gpio not provided\n");
		return error;
	}

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");
		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			return error;
		}
	}

	if (gpio_is_valid(pdata->gpio_1v8_en)) {
		error = gpio_request(pdata->gpio_1v8_en, "himax-1v8_en");
		if (error < 0) {
			E("%s: request 1v8_en pin failed\n", __func__);
			return error;
		}
	}

	if (gpio_is_valid(pdata->gpio_3v3_en)) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");
		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			return error;
		}
	}

	
	
	
	I("3v3_en pin =%d, 1v8_en pin =%d, irq pin = %d, reset pin = %d\n",
	  gpio_get_value(pdata->gpio_3v3_en),
	  gpio_get_value(pdata->gpio_1v8_en),
	  himax_int_gpio_read(pdata->gpio_irq),
	  gpio_get_value(pdata->gpio_reset));

	
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	msleep(10);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
#else
	msleep(10);
	himax_rst_gpio_set(pdata->gpio_reset, GPIO_OUT_ONE);
#endif
	msleep(20);
	I("reset pin = %d\n", gpio_get_value(pdata->gpio_reset));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
#else
	himax_rst_gpio_set(pdata->gpio_reset, GPIO_OUT_ZERO);
#endif
	msleep(40);
	I("reset pin = %d\n", gpio_get_value(pdata->gpio_reset));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
#else
	himax_rst_gpio_set(pdata->gpio_reset, GPIO_OUT_ONE);
#endif
	msleep(20);
	I("reset pin = %d\n", gpio_get_value(pdata->gpio_reset));

	I("himax reset over\n");

	return error;
}

#endif	

void himax_gpio_power_deconfig(struct i2c_client *client, struct himax_i2c_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
	else
		E("irq gpio not provided\n");

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

	if (gpio_is_valid(pdata->gpio_3v3_en))
		gpio_free(pdata->gpio_3v3_en);
}
