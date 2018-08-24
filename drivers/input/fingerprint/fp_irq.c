/*
 * Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks, controlling GPIOs such as SPI chip select, sensor reset line, sensor
 * IRQ line, MISO and MOSI lines.
 *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node. This makes it possible for a user
 * space process to poll the input node and receive IRQ events easily. Usually
 * this node is available under /dev/input/eventX where 'X' is a number given by
 * the event system. A user space process will need to traverse all the event
 * nodes and ask for its parent's name (through EVIOCGNAME) which should match
 * the value in device tree named input-device-name.
 *
 * This driver will NOT send any SPI commands to the sensor it only controls the
 * electrical parts.
 *
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */


#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#define DEBUG
#include <linux/platform_device.h>
#include <linux/wakelock.h>

#define FP_RESET_LOW_US 1000
#define FP_RESET_HIGH1_US 100
#define FP_RESET_HIGH2_US 1250

#define FP_TTW_HOLD_TIME 1000

#define FP_RESET_RETRIES 5

#include "fp_irq.h"


#include <linux/input.h>

#ifdef CONFIG_HTC_SPI
#include <linux/of_irq.h>
#include <linux/spi/spi.h>
#include <mt_spi.h>
#include <mt_spi_hal.h>
#include <mt-plat/mt_gpio.h>
#include <mt_spi_hal.h>
#include <gpio_const.h>

#endif 

#ifdef CONFIG_FPC_HTC_DISABLE_CHARGING
#include <mt-plat/htc_battery.h>
#endif 

#ifdef CONFIG_HTC_SPI
extern void fp_enable_clk(void);
extern void fp_disable_clk(void);

void mt_spi_enable_clk(void)
{
	fp_enable_clk();
}
void mt_spi_disable_clk(void)
{
	fp_disable_clk();
}
#endif 

#ifdef CONFIG_HTC_PINCTRL
static const char * const pctl_names[] = {
	"fpc_pins_ctrl0",       
	"fpc_pins_ctrl1"        
};
#endif 

struct fp_data {
	struct device *dev;
	struct platform_device *pldev;
	int irq_gpio;
	int rst_gpio;
	int ldo_gpio;
#ifdef CONFIG_HTC_ID_PIN
	int id_gpio;
	int fp_source;
#endif 
#ifdef CONFIG_HTC_PINCTRL
	struct pinctrl *fingerprint_pinctrl;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
#endif 
	
#ifdef CONFIG_HTC_INPUT_DEVICE
	struct input_dev *idev;
	char idev_name[32];
	int event_type;
	int event_code;
#endif 

	struct mutex lock;
	bool wakeup_enabled;
	struct wake_lock ttw_wl;
};

#ifdef CONFIG_HTC_SPI
static ssize_t clk_enable_set(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	if (!strncmp(buf, "1", strlen("1")))
	{
		mt_spi_enable_clk();
	}
	else if (!strncmp(buf, "0", strlen("0")))
	{
		mt_spi_disable_clk();
	}
	return count;
}

DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

#endif 


#ifdef CONFIG_HTC_PINCTRL
static int select_pin_ctrl(struct fp_data *fp, const char *name)
{
    size_t i;
    int rc;
    struct device *dev = fp->dev;
    for (i = 0; i < ARRAY_SIZE(fp->pinctrl_state); i++) {
        const char *n = pctl_names[i];
        if (!strncmp(n, name, strlen(n))) {
            rc = pinctrl_select_state(fp->fingerprint_pinctrl,
                    fp->pinctrl_state[i]);
            if (rc)
                dev_err(dev, "cannot select '%s'\n", name);
			else{
                dev_dbg(dev, "Selected '%s'\n", name);
			} 
			goto exit;
			
		}
    }
    rc = -EINVAL;
    dev_err(dev, "%s:'%s' not found\n", __func__, name);
exit:
	dev_dbg(dev, "%s rc = %d\n",__func__,rc);
    return rc;
}
#endif 


static int hw_reset(struct  fp_data *fp)
{
#ifdef CONFIG_HTC_PINCTRL
	int error = 0;
	struct device *dev = fp->dev;

	if (!gpio_is_valid(fp->rst_gpio)) {
		dev_err( dev, "fp->rst_gpio not valid !\n");
		goto exit;
	}

	gpio_set_value(fp->rst_gpio, 1);
	usleep_range(FP_RESET_HIGH1_US, FP_RESET_HIGH1_US + 100);

	gpio_set_value(fp->rst_gpio, 0);
	usleep_range(FP_RESET_LOW_US, FP_RESET_LOW_US + 100);
	
	gpio_set_value(fp->rst_gpio, 1);
	usleep_range(FP_RESET_HIGH1_US, FP_RESET_HIGH1_US + 100);

	dev_info( dev, "Using GPIO#%d as IRQ.\n", fp->irq_gpio );
	dev_info( dev, "Using GPIO#%d as RST.\n", fp->rst_gpio );

exit :
	return error;
#endif 

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fp_data *fp = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset")))
		rc = hw_reset(fp);
	else
		return -EINVAL;
	return rc ? rc : count;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

#if 1
static ssize_t wakeup_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fp_data *fp = dev_get_drvdata(dev);

	if (!strncmp(buf, "enable", strlen("enable")))
	{
		fp->wakeup_enabled = true;
		smp_wmb();
	}
	else if (!strncmp(buf, "disable", strlen("disable")))
	{
		fp->wakeup_enabled = false;
		smp_wmb();
	}
	else
		return -EINVAL;

	return count;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);
#endif

static ssize_t irq_get(struct device *device,
			     struct device_attribute *attribute,
			     char* buffer)
{
	struct fp_data *fp = dev_get_drvdata(device);
	int irq = gpio_get_value(fp->irq_gpio);
	printk("irq_get %d\n",irq);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

extern void mt_eint_unmask(unsigned int eint_num);
extern unsigned int mt_eint_get_mask(unsigned int eint_num);

static ssize_t irq_ack(struct device *device,
			     struct device_attribute *attribute,
			     const char *buffer, size_t count)
{
    unsigned int maskres;
    struct fp_data *fp = dev_get_drvdata(device);
    dev_info(fp->dev, "%s\n", __func__);

    
    if (!strncmp(buffer, "0", strlen("0")))
    {
        maskres = mt_eint_get_mask(fp->irq_gpio);
        if(maskres == 1){
            dev_err(fp->dev, "irq is masked abnormally, unmask fp irq !\n");
            mt_eint_unmask(fp->irq_gpio);
        }
    }

    return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

#ifdef CONFIG_HTC_ID_PIN
static ssize_t id_pin_get(struct device *device,
					 struct device_attribute *attribute,
					 char* buffer){

	struct fp_data *fp = dev_get_drvdata(device);
	pr_debug("[FP] fp->fp_source = %d\n", fp->fp_source);
	return scnprintf(buffer, PAGE_SIZE, "%d\n", fp->fp_source);
}

static DEVICE_ATTR(id_pin, S_IRUSR, id_pin_get, NULL);
#endif 

#ifdef CONFIG_HTC_HAL_WRITE_TIME
static ssize_t hal_meature_time_set(struct device *device,
                                struct device_attribute *attribute,
                                const char *buffer, size_t count)
{
    static char time_str[128] = {0};

    snprintf(time_str, sizeof(time_str), "hal_time = %s",buffer);
    printk("[FP][HAL] %s\n",time_str);

    return count;
}
static DEVICE_ATTR(hal_meature_time, S_IWUSR, NULL, hal_meature_time_set);
#endif 

#ifdef CONFIG_FPC_HTC_DISABLE_CHARGING
static ssize_t fp_disable_charging_set(struct device *device,
              struct device_attribute *attribute, const char *buf, size_t count)
{
    if (!strncmp(buf, "enable", strlen("enable"))) {
        pr_info("[FP] %s:%s\n", __func__, buf);
        htc_battery_charger_switch_internal(ENABLE_PWRSRC_FINGERPRINT);
    } else if (!strncmp(buf, "disable", strlen("disable"))) {
        pr_info("[FP] %s:%s\n", __func__, buf);
        htc_battery_charger_switch_internal(DISABLE_PWRSRC_FINGERPRINT);
    } else {
        pr_err("[FP] Wrong Parameter!!:%s\n", buf);
        return -EINVAL;
    }
    return count;
}

static DEVICE_ATTR(fp_disable_charge, S_IWUSR, NULL, fp_disable_charging_set);
#endif 

static struct attribute * fp_attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
#ifdef CONFIG_HTC_ID_PIN
	&dev_attr_id_pin.attr,
#endif 
#ifdef CONFIG_HTC_HAL_WRITE_TIME
    &dev_attr_hal_meature_time.attr,
#endif 
#ifdef CONFIG_FPC_HTC_DISABLE_CHARGING
    &dev_attr_fp_disable_charge.attr,
#endif 
    NULL
};

static const struct attribute_group const fp_attribute_group = {
	.attrs = fp_attributes,
};

#ifdef CONFIG_HTC_ID_PIN
static int id_pin_detect(struct  fp_data *fp){

	
	int rc;
	int id_pin = 0;
	int id_t1 = 0;		
	int id_t2 = 0;		

	pr_debug("[FP] %s +++\n", __func__);
	fp->fp_source = 0;		

	rc = select_pin_ctrl(fp, "fpc_pins_ctrl0");
	if(rc){
		pr_err("%s, select_pin_ctrl(fp, fp_id_init1) failed !", __func__);
		goto error;
	}
	id_pin = gpio_get_value(fp->id_gpio);
	id_t1 = id_pin;
	pr_debug("[FP] 1st detect id pin = %s\n", (id_t1 ? "High" : "Low"));

	msleep(50);

	rc = select_pin_ctrl(fp, "fpc_pins_ctrl1");
	if(rc){
		pr_err("%s, select_pin_ctrl(fp, fp_id_init2) failed !", __func__);
		goto error;
	}
	id_pin = gpio_get_value(fp->id_gpio);
	id_t2 = id_pin;
	pr_debug("[FP] 2nd detect id pin = %s\n", (id_t2 ? "High" : "Low"));

#ifdef HTC_ALE
	if((id_t1 == 0)&&(id_t2 == 1)){
		fp->fp_source = 1;
		pr_debug("[FP] Fingerprint source = IDEX + CT\n");
	}
	else if((id_t1 == 1)&&(id_t2 == 1)){
		fp->fp_source = 1;
		pr_debug("[FP] Fingerprint source = IDEX + CT\n");
	}
	else if((id_t1 == 0)&&(id_t2 == 0)){
		fp->fp_source = 0;
		pr_debug("[FP] Fingerprint source = FPC + O-Film\n");
	}
#else
	if((id_t1 == 0)&&(id_t2 == 1)){
		fp->fp_source = 0;
		pr_debug("[FP] Fingerprint source = FPC + CT\n");
	}
	else if((id_t1 == 1)&&(id_t2 == 1)){
		fp->fp_source = 1;
		pr_debug("[FP] Fingerprint source = IDEX + CT\n");
	}
	else if((id_t1 == 0)&&(id_t2 == 0)){
		fp->fp_source = 2;
		pr_debug("[FP] Fingerprint source = FPC + Primax\n");
	}
#endif
	
    pr_debug( "[FP] %s ---\n", __func__);

	rc = select_pin_ctrl(fp, "fpc_pins_ctrl0");
	if(rc){
		pr_err("%s, id pin change to sleep state failed !", __func__);
		goto error;
	}

	return fp->fp_source;

error:
	return rc;
}
#endif 

static irqreturn_t fp_irq_handler(int irq, void *handle)
{
	struct fp_data *fp = handle;
	dev_dbg(fp->dev, "%s\n", __func__);
	smp_rmb();

	if (fp->wakeup_enabled) {
		wake_lock_timeout(&fp->ttw_wl,
					msecs_to_jiffies(FP_TTW_HOLD_TIME));
	}

	sysfs_notify(&fp->dev->kobj, NULL, dev_attr_irq.attr.name);

	return IRQ_HANDLED;
}



static int fp_probe(struct platform_device *pldev)
{
	struct device *dev = &pldev->dev;
	int rc = 0;
	int irqf;
	struct device_node *np = dev->of_node;
	u32 val;
	const char *idev_name;
	struct fp_data *fp;
	int i = 0;

	dev_dbg(dev, "%s ++\n", __func__);

	fp = devm_kzalloc(dev, sizeof(*fp), GFP_KERNEL);
	if (!fp) {
		dev_err(dev,
			"failed to allocate memory for struct fp_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	fp->dev = dev;
	dev_set_drvdata(dev, fp);
	fp->pldev = pldev;

	if (!np) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}
	dev_info(dev, "%s fonud node\n",__func__);

	rc = of_property_read_u32(np, "fp,ldo_gpio", &val);
	if(rc < 0)
		fp->ldo_gpio = -EINVAL;
	else {
		gpio_request(val, "fp_ldo");
		fp->ldo_gpio = val;
	}

	rc = of_property_read_u32(np, "fp,irq_gpio", &val);
	if(rc < 0)
		fp->irq_gpio = -EINVAL;
	else {
		gpio_request(val, "fp_irq");
		fp->irq_gpio = val;
	}

	rc = of_property_read_u32(np, "fp,rst_gpio", &val);
	if(rc < 0)
		fp->rst_gpio = -EINVAL;
	else {
		gpio_request(val, "fp_rst");
		fp->rst_gpio = val;
	}

#ifdef CONFIG_HTC_ID_PIN
	rc = of_property_read_u32(np, "fp,id_gpio", &val);
	if(rc < 0)
		fp->id_gpio = -EINVAL;
	else {
		gpio_request(val, "fp_id");
		fp->id_gpio = val;
	}
	dev_info( dev, "Using GPIO#%d as ID.\n", fp->id_gpio );
#endif 

	dev_info( dev, "Using GPIO#%d as IRQ.\n", fp->irq_gpio );
	dev_info( dev, "Using GPIO#%d as RST.\n", fp->rst_gpio );

#ifdef CONFIG_HTC_PINCTRL
	dev_info(dev, "devm_pinctrl_get ++\n");
    fp->fingerprint_pinctrl = devm_pinctrl_get(dev);
    if (IS_ERR(fp->fingerprint_pinctrl)) {
        if (PTR_ERR(fp->fingerprint_pinctrl) == -EPROBE_DEFER) {
            dev_info(dev, "pinctrl not ready\n");
            rc = -EPROBE_DEFER;
            goto exit;
        }
        dev_err(dev, "Target does not use pinctrl\n");
        fp->fingerprint_pinctrl = NULL;
        rc = -EINVAL;
        goto exit;
    }
	dev_info(dev, "devm_pinctrl_get --\n");

	dev_info(dev,"devm_pinctrl_lookup_state ++\n");
    for ( i = 0; i < ARRAY_SIZE(fp->pinctrl_state); i++) {
        const char *n = pctl_names[i];
        struct pinctrl_state *state =
            pinctrl_lookup_state(fp->fingerprint_pinctrl, n);
        if (IS_ERR(state)) {
            dev_err(dev, "cannot find '%s'\n", n);
            rc = -EINVAL;
            goto exit;
        }
        dev_info(dev, "found pin control %s\n", n);
        fp->pinctrl_state[i] = state;
        printk("i=%d\n",(int) i);
    }
	dev_info(dev,"devm_pinctrl_lookup_state --\n");

#ifdef CONFIG_HTC_ID_PIN
	id_pin_detect(fp);
#endif 

	rc = hw_reset(fp);
	if(rc){
		dev_err(fp->dev, "%s hw reset fail\n",__func__);
		goto exit;
	}

	select_pin_ctrl(fp, "fpc_pins_ctrl0");
#endif 

#ifdef CONFIG_HTC_INPUT_DEVICE
	rc = of_property_read_u32(np, "fp,event-type", &val);
    fp->event_type = rc < 0 ? EV_MSC : val;

    rc = of_property_read_u32(np, "fp,event-code", &val);
    fp->event_code = rc < 0 ? MSC_SCAN : val;

    dev_err( dev, "Using #%d as event_type.\n", fp->event_type );
    dev_err( dev, "Using #%d as event_code.\n", fp->event_code );

    fp->idev = devm_input_allocate_device(dev);
    if (!fp->idev) {
        dev_err(dev, "failed to allocate input device\n");
        rc = -ENOMEM;
        goto exit;
    }
    input_set_capability(fp->idev, fp->event_type,
            fp->event_code);
    if (!of_property_read_string(np, "input-device-name", &idev_name)) {
        fp->idev->name = idev_name;
    } else {
        fp->idev->name = "fp_irq";
    }

	
    set_bit(EV_KEY, fp->idev->evbit);
    
    set_bit(KEY_WAKEUP, fp->idev->keybit);
    
    rc = input_register_device(fp->idev);

    if (rc) {
        dev_err(dev, "failed to register input device\n");
        goto exit;
    }
#endif 

	fp->wakeup_enabled = false;

	irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fp,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}
	mutex_init(&fp->lock);
	rc = devm_request_threaded_irq(dev, gpio_to_irq(fp->irq_gpio),
			NULL, fp_irq_handler, irqf,
			dev_name(dev), fp);
	if (rc) {
		dev_err(dev, "could not request irq %d\n",
				gpio_to_irq(fp->irq_gpio));
		goto exit;
	}
	dev_info(dev, "requested irq %d\n", gpio_to_irq(fp->irq_gpio));

	
	enable_irq_wake( gpio_to_irq( fp->irq_gpio ) );
	wake_lock_init(&fp->ttw_wl, WAKE_LOCK_SUSPEND, "fp_ttw_wl");

	rc = sysfs_create_group(&dev->kobj, &fp_attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	dev_info(dev, "%s: ok\n", __func__);
exit:
	return rc;
}

static int fp_remove(struct platform_device *pldev)
{
	struct  fp_data *fp = dev_get_drvdata(&pldev->dev);

	sysfs_remove_group(&pldev->dev.kobj, &fp_attribute_group);
	mutex_destroy(&fp->lock);
	wake_lock_destroy(&fp->ttw_wl);
	dev_info(&pldev->dev, "%s\n", __func__);
	return 0;
}

static struct of_device_id fp_of_match[] = {
	{ .compatible = "fp,fp_irq", },
	{}
};
MODULE_DEVICE_TABLE(of, fp_of_match);

static struct platform_driver fp_driver = {
	.driver = {
		.name	= "fp_irq",
		.owner	= THIS_MODULE,
		.of_match_table = fp_of_match,
	},
	.probe	= fp_probe,
	.remove	= fp_remove
};


static int __init fp_init(void)
{
	printk(KERN_INFO "%s\n", __func__);
	return (platform_driver_register(&fp_driver) != 0)? EINVAL : 0;
}

static void __exit fp_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);
	platform_driver_unregister(&fp_driver);
}

module_init(fp_init);
module_exit(fp_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aleksej Makarov");
MODULE_AUTHOR("Henrik Tillman <henrik.tillman@fingerprints.com>");
MODULE_DESCRIPTION("Fingerprint sensor device driver.");
