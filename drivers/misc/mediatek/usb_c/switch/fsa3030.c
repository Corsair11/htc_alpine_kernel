/*
 * Copyright (C) 2016 HTC Corporation.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/fsa3030.h>

#define FSA3030_DRV_VERSION	"0.0.1"

static int fsa3030_listener_current[FSA3030_SWITCH_TYPE_NUM] = {0}; 
static int fsa3030_listener_registered[FSA3030_SWITCH_TYPE_NUM] = {0}; 
static struct pinctrl *fsa3030_pinctrl = NULL;
static spinlock_t fsa3030_lock;
static bool fsa3030_ready = false;


static int fsa3030_pinctrl_configure(enum fsa3030_switch_type type)
{
	struct pinctrl_state *set_state;
	int retval;

	if (!fsa3030_pinctrl) {
		pr_err("%s pinctrl not ready\n", __func__);
		return -EINVAL;
	}

	switch (type) {
	case FSA3030_SWITCH_TYPE_USB:
		set_state = pinctrl_lookup_state(fsa3030_pinctrl,"usb_type_c_switch_usb");
		if (IS_ERR(set_state)) {
			pr_err("%s cannot get pinctrl usb state\n", __func__);
			return PTR_ERR(set_state);
		}
		break;
	case FSA3030_SWITCH_TYPE_AUDIO:
		set_state = pinctrl_lookup_state(fsa3030_pinctrl,"usb_type_c_switch_audio");
		if (IS_ERR(set_state)) {
			pr_err("%s cannot get pinctrl audio state\n", __func__);
			return PTR_ERR(set_state);
		}
		break;
	case FSA3030_SWITCH_TYPE_MHL:
		set_state = pinctrl_lookup_state(fsa3030_pinctrl,"usb_type_c_switch_mhl");
		if (IS_ERR(set_state)) {
			pr_err("%s cannot get pinctrl audio state\n", __func__);
			return PTR_ERR(set_state);
		}
		break;
	default:
		pr_err("%s invalid switch type %d\n", __func__, type);
		return -EINVAL;
	}

	retval = pinctrl_select_state(fsa3030_pinctrl, set_state);
	if (retval) {
		pr_err("%s cannot set pinctrl state (err %d)\n", __func__, retval);
		return retval;
	}
	pr_info("%s successfuly switched to %d\n", __func__, type);

	return 0;
}

int fsa3030_switch_set(int handle, enum fsa3030_switch_type type, bool enable)
{
	unsigned long flags;

	if (!fsa3030_ready) {
		pr_err("%s not ready\n", __func__);
		return -EIO;
	}

	if (type < FSA3030_SWITCH_TYPE_USB || type >= FSA3030_SWITCH_TYPE_NUM) {
		pr_err("%s invalid type %d\n", __func__, type);
		return -EINVAL;
	}

	spin_lock_irqsave(&fsa3030_lock, flags);
	
	if (!(fsa3030_listener_registered[type] & handle) || __builtin_popcount(handle) != 1) {
		pr_err("%s invalid handle 0x%x\n", __func__, handle);
		spin_unlock_irqrestore(&fsa3030_lock, flags);
		return -EINVAL;
	}

	if (enable) {
		fsa3030_listener_current[type] |= handle;
		if (fsa3030_listener_current[type] == fsa3030_listener_registered[type])
			fsa3030_pinctrl_configure(type);
	}
	else {
		fsa3030_listener_current[type] &= ~handle;
		if (fsa3030_listener_current[type] != fsa3030_listener_registered[type])
			fsa3030_pinctrl_configure(FSA3030_SWITCH_TYPE_USB);
	}
	spin_unlock_irqrestore(&fsa3030_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(fsa3030_switch_set);

int fsa3030_switch_register(enum fsa3030_switch_type type)
{
	int index = 0;
	int handle = 0;
	unsigned long flags;

	if (!fsa3030_ready) {
		pr_err("%s not ready\n", __func__);
		return -EIO;
	}

	if (type < FSA3030_SWITCH_TYPE_USB || type >= FSA3030_SWITCH_TYPE_NUM) {
		pr_err("%s invalid type %d\n", __func__, type);
		return -EINVAL;
	}

	spin_lock_irqsave(&fsa3030_lock, flags);
	
	index = sizeof(int)*8 - __builtin_clz(fsa3030_listener_registered[type]);
	if (index == sizeof(int)*8) {
		spin_unlock_irqrestore(&fsa3030_lock, flags);
		return -ENOMEM;
	}
	handle = (1 << index);
	fsa3030_listener_registered[type] |= handle;
	spin_unlock_irqrestore(&fsa3030_lock, flags);

	pr_info("%s type %d, new index %d, fsa3030_listener_registered 0x%02x\n", __func__, type, index, fsa3030_listener_registered[type]);

	return handle;
}
EXPORT_SYMBOL_GPL(fsa3030_switch_register);

int fsa3030_switch_unregister(enum fsa3030_switch_type type)
{
	if (!fsa3030_ready) {
		pr_err("%s not ready\n", __func__);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fsa3030_switch_unregister);

static int fsa3030_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;

	for (i = 0 ; i < FSA3030_SWITCH_TYPE_NUM ; i++) {
		fsa3030_listener_registered[i] = 0;
		fsa3030_listener_current[i] = 0;
	}
	fsa3030_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(fsa3030_pinctrl)) {
		pr_err("%s Cannot find usb/audio switch pinctrl!\n", __func__);
		return -EINVAL;
	}
	else {
		ret = fsa3030_pinctrl_configure(FSA3030_SWITCH_TYPE_USB);
		if (ret < 0)
			return ret;
	}
	spin_lock_init(&fsa3030_lock);
	fsa3030_ready = true;

	pr_info("%s OK\n", __func__);
	return ret;
}

static int fsa3030_remove(struct platform_device *pdev)
{
	int ret = 0;
	fsa3030_ready = false;
	return ret;
}

static void fsa3030_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id fsa3030_match_table[] = {
	{.compatible = "fairchild-fsa3030",},
	{},
};

static struct platform_driver fsa3030_driver = {
	.driver = {
		.name = "Fairchild FSA3030",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = fsa3030_match_table,
	},
	.probe = fsa3030_probe,
	.remove = fsa3030_remove,
	.shutdown = fsa3030_shutdown,
};

static int __init fsa3030_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&fsa3030_driver);
	if (ret) {
		pr_err("%s platform_driver_register failed: %d\n", __func__, ret);
		return ret;
	}
	pr_info("%s: version " FSA3030_DRV_VERSION "\n", __func__);
	return ret;
}
subsys_initcall(fsa3030_init);

static void __exit fsa3030_exit(void)
{
	platform_driver_unregister(&fsa3030_driver);
}
module_exit(fsa3030_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JJ Lee <jj_lee@htc.com>");
MODULE_DESCRIPTION("Fairchild FSA3030 Driver");
MODULE_VERSION(FSA3030_DRV_VERSION);
