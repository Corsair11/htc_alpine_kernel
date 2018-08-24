
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "cam_cal.h"
#include "cam_cal_define.h"

#ifdef CAM_CAL_DEBUG
#define CAM_CALDB printk
#else
#define CAM_CALDB(x,...)
#endif



#if 1 
static struct kobject *led_status_obj; 
#endif 

static uint32_t led_hotspot_status_value;
static uint16_t led_low_temp_limit = 5;
static uint16_t led_low_cap_limit = 14;

static ssize_t led_hotspot_status_get(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    int length;
    length = sprintf(buf, "%d\n", led_hotspot_status_value);
    return length;
}

static ssize_t led_hotspot_status_set(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
    u32 tmp = 0;

    tmp = buf[0] - 0x30; 

    led_hotspot_status_value = tmp;
    CAM_CALDB("[CAM_CAL] led_hotspot_status_value = %d\n", led_hotspot_status_value);
    return count;
}

static ssize_t low_temp_limit_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t length = 0;
	if(buf)
	    length = sprintf(buf, "%d\n", led_low_temp_limit);
	return length;
}

static ssize_t low_cap_limit_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t length = 0;
	if(buf)
	    length = sprintf(buf, "%d\n", led_low_cap_limit);
	return length;
}

static DEVICE_ATTR(led_hotspot_status, 0644,
    led_hotspot_status_get,
    led_hotspot_status_set);

static DEVICE_ATTR(low_temp_limit, 0444,
	low_temp_limit_get,
	NULL);

static DEVICE_ATTR(low_cap_limit, 0444,
	low_cap_limit_get,
	NULL);


static int __init CamFlash_CAL_init(void)
{
    
    int ret = 0;
    printk("CamFlash_Cal_i2C_inti Abel start\n");
    
    led_status_obj = kobject_create_and_add("camera_led_status", NULL);
    if (led_status_obj == NULL) {
        pr_info("[CAM][FL] msm_camera_led: subsystem_register failed\n");
        ret = -ENOMEM;
        goto error;
	}
    ret = sysfs_create_file(led_status_obj, &dev_attr_led_hotspot_status.attr);
    if (ret) {
        pr_err("[CAM][FL] msm_camera_led: sysfs_create_file dev_attr_led_hotspot_status failed\n");
        ret = -EFAULT;
        goto error;
	}
	ret = sysfs_create_file(led_status_obj,
		&dev_attr_low_temp_limit.attr);
	if (ret) {
		pr_err("[CAM][FL] msm_camera_led: sysfs_create_file dev_attr_low_temp_limit failed\n");
		ret = -EFAULT;
		goto error;
	}
	ret = sysfs_create_file(led_status_obj,
		&dev_attr_low_cap_limit.attr);
	if (ret) {
		pr_err("[CAM][FL] msm_camera_led: sysfs_create_file dev_attr_low_cap_limit failed\n");
		ret = -EFAULT;
		goto error;
	}
    
    return 0;

error:
    kobject_del(led_status_obj);
    return ret;
    
}

module_init(CamFlash_CAL_init);

//MODULE_LICENSE("GPL");

