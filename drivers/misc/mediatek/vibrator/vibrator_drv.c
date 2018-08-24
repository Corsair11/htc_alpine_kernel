/******************************************************************************
 * mt6575_vibrator.c - MT6575 Android Linux Vibrator Device Driver
 *
 * Copyright 2009-2010 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers vibrator relative functions
 *
 ******************************************************************************/
#ifdef CONFIG_VIB_TRIGGERS
#include <linux/vibtrig.h>
#include <linux/slab.h>
#endif
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include "timed_output.h"

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <linux/jiffies.h>
#include <linux/timer.h>


#include <vibrator.h>
#include <vibrator_hal.h>

#define VERSION					        "v 0.1"
#define VIB_DEVICE				"mtk_vibrator"

#define VIB_DBG_LOG(fmt, ...) \
	printk(KERN_DEBUG "[VIB][DBG] " fmt, ##__VA_ARGS__)
#define VIB_INFO_LOG(fmt, ...) \
	printk(KERN_INFO "[VIB] " fmt, ##__VA_ARGS__)
#define VIB_ERR_LOG(fmt, ...) \
	printk(KERN_ERR "[VIB][ERR] " fmt, ##__VA_ARGS__)

static int debug_enable_vib_hal = 1;
#define VIB_DEBUG(format, args...) do { \
	if (debug_enable_vib_hal) {\
		pr_debug(format, ##args);\
	} \
} while (0)

#define RSUCCESS        0


#define DBG_EVT_NONE		0x00000000	
#define DBG_EVT_INT			0x00000001	
#define DBG_EVT_TASKLET		0x00000002	

#define DBG_EVT_ALL			0xffffffff

#define DBG_EVT_MASK		(DBG_EVT_TASKLET)

#if 1
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		VIB_DEBUG(fmt, ##args); \
	} \
} while (0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __func__)
#else
#define MSG(evt, fmt, args...) do {} while (0)
#define MSG_FUNC_ENTRY(f)	   do {} while (0)
#endif

#ifdef CONFIG_VIB_TRIGGERS
struct mtk_vib {
	struct timed_output_dev timed_dev;
	struct vib_trigger_enabler enabler;
};
#endif

static struct workqueue_struct *vibrator_queue;
static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static spinlock_t vibe_lock;
static int vibe_state;
static int ldo_state;
static int shutdown_flag;

static int vibr_Enable(void)
{
	if (!ldo_state) {
		vibr_Enable_HW();
		VIB_INFO_LOG("%s\n", __func__);
		ldo_state = 1;
	}
	return 0;
}

static int vibr_Disable(void)
{
	if (ldo_state) {
		vibr_Disable_HW();
		VIB_INFO_LOG("%s\n", __func__);
		ldo_state = 0;
	}
	return 0;
}

static void update_vibrator(struct work_struct *work)
{
	if (!vibe_state)
		vibr_Disable();
	else
		vibr_Enable();
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);

		return ktime_to_ms(r);
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	unsigned long flags;

	struct vibrator_hw *hw = mt_get_cust_vibrator_hw();

	if(hw == NULL) {
		VIB_ERR_LOG("%s: hw is NULL!", __func__);
		return;
	}

	VIB_DEBUG("vibrator_enable: vibrator first in value = %d\n", value);

	spin_lock_irqsave(&vibe_lock, flags);
	while (hrtimer_cancel(&vibe_timer)) {
		VIB_INFO_LOG("%s: try to cancel hrtimer\n", __func__);
	}

	if (value == 0 || shutdown_flag == 1) {
		if(shutdown_flag)
			VIB_INFO_LOG("%s: shutdown_flag = %d\n", __func__, shutdown_flag);
		vibe_state = 0;
	} else {
#if 1
		VIB_DEBUG("vibrator_enable: vibrator cust timer: %d\n",
			  hw->vib_timer);
#ifdef CUST_VIBR_LIMIT
		if (value > hw->vib_limit && value < hw->vib_timer)
#else
		if (value >= 10 && value < hw->vib_timer)
#endif
			value = hw->vib_timer;
#endif

		value = (value > 15000 ? 15000 : value);
		vibe_state = 1;
		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&vibe_lock, flags);
	VIB_INFO_LOG("%s: %d\n", __func__, value);
	queue_work(vibrator_queue, &vibrator_work);
}

#ifdef CONFIG_VIB_TRIGGERS
static void mtk_vib_trigger_enable(struct vib_trigger_enabler *enabler, int value)
{
	struct mtk_vib *vib;
	struct timed_output_dev *dev;

	vib = enabler->trigger_data;
	dev = &vib->timed_dev;

	VIB_INFO_LOG("vib_trigger=%d.\r\n", value);

	vibrator_enable(dev, value);
}
#endif

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	vibe_state = 0;
	VIB_DEBUG("vibrator_timer_func: vibrator will disable\n");
	queue_work(vibrator_queue, &vibrator_work);
	return HRTIMER_NORESTART;
}

static struct timed_output_dev mtk_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int vib_probe(struct platform_device *pdev)
{
	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	unsigned long flags;
	VIB_INFO_LOG("%s: enter!\n", __func__);
	spin_lock_irqsave(&vibe_lock, flags);
	shutdown_flag = 1;
	if (vibe_state) {
		VIB_INFO_LOG("%s: vibrator will disable\n", __func__);
		vibe_state = 0;
		spin_unlock_irqrestore(&vibe_lock, flags);
		vibr_Disable();
	} else {
		spin_unlock_irqrestore(&vibe_lock, flags);
	}
}

static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.driver = {
		   .name = VIB_DEVICE,
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device vibrator_device = {
	.name = "mtk_vibrator",
	.id = -1,
};

static ssize_t store_vibr_on(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	if (buf != NULL && size != 0) {
		
		if (buf[0] == '0')
			vibr_Disable();
		else
			vibr_Enable();
	}
	return size;
}

static DEVICE_ATTR(vibr_on, 0220, NULL, store_vibr_on);


static int vib_mod_init(void)
{
	s32 ret;
#ifdef CONFIG_VIB_TRIGGERS
	struct vib_trigger_enabler *enabler;
#endif

	VIB_INFO_LOG("%s: MediaTek MTK vibrator driver register, version %s\n", __func__, VERSION);
#ifdef CONFIG_VIB_TRIGGERS
	enabler = kzalloc(sizeof(*enabler), GFP_KERNEL);
	if(!enabler)
	{
		VIB_INFO_LOG("%s: kzalloc enabler failed.\n", __func__);
		return -ENOMEM;
	}
#endif
	
	vibr_power_set();
	ret = platform_device_register(&vibrator_device);
	if (ret != 0) {
		VIB_INFO_LOG("%s: Unable to register vibrator device (%d)\n", __func__, ret);
		goto dev_reg_failed;
	}

	vibrator_queue = create_singlethread_workqueue(VIB_DEVICE);
	if (!vibrator_queue) {
		VIB_INFO_LOG("%s: Unable to create workqueue\n", __func__);
		ret = -ENODATA;
		goto err_create_work_queue;
	}
	INIT_WORK(&vibrator_work, update_vibrator);

	spin_lock_init(&vibe_lock);
	shutdown_flag = 0;
	vibe_state = 0;
	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&mtk_vibrator);

	ret = platform_driver_register(&vibrator_driver);

	if (ret) {
		VIB_INFO_LOG("%s: Unable to register vibrator driver (%d)\n", __func__, ret);
		goto drv_reg_failed;
	}

	ret = device_create_file(mtk_vibrator.dev, &dev_attr_vibr_on);
	if (ret) {
		VIB_INFO_LOG("%s: device_create_file vibr_on fail!\n", __func__);
	}

#ifdef CONFIG_VIB_TRIGGERS
	enabler->name = "mtk-vibrator";
	enabler->default_trigger = "vibrator";
	enabler->enable = mtk_vib_trigger_enable;
	vib_trigger_enabler_register(enabler);
#endif

	VIB_INFO_LOG("%s: Done\n", __func__);

	return RSUCCESS;

drv_reg_failed:
	timed_output_dev_unregister(&mtk_vibrator);
	hrtimer_cancel(&vibe_timer);
	destroy_work_on_stack(&vibrator_work);
	destroy_workqueue(vibrator_queue);
err_create_work_queue:
	platform_device_unregister(&vibrator_device);
dev_reg_failed:
#ifdef CONFIG_VIB_TRIGGERS
	kfree(enabler);
#endif
	return ret;
}


static void vib_mod_exit(void)
{
	VIB_INFO_LOG("%s: MediaTek MTK vibrator driver unregister, version %s\n", __func__, VERSION);
	if (vibrator_queue) {
		destroy_workqueue(vibrator_queue);
	}
	VIB_INFO_LOG("%s: Done\n", __func__);
}

late_initcall_sync(vib_mod_init);

module_exit(vib_mod_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
