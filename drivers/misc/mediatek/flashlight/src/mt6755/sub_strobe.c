/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_typedef.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#ifdef CONFIG_COMPAT
#include <linux/fs.h>
#include <linux/compat.h>
#endif
#include "kd_flashlight.h"
#define TAG_NAME "[sub_strobe.c]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(TAG_NAME "%s: " fmt, __func__ , ##arg)
#define PK_WARN(fmt, arg...)        pr_warn(TAG_NAME "%s: " fmt, __func__ , ##arg)
#define PK_NOTICE(fmt, arg...)      pr_notice(TAG_NAME "%s: " fmt, __func__ , ##arg)
#define PK_INFO(fmt, arg...)        pr_info(TAG_NAME "%s: " fmt, __func__ , ##arg)
#define PK_TRC_FUNC(f)              pr_debug(TAG_NAME "<%s>\n", __func__)
#define PK_TRC_VERBOSE(fmt, arg...) pr_debug(TAG_NAME fmt, ##arg)
#define PK_ERROR(fmt, arg...)       pr_err(TAG_NAME "%s: " fmt, __func__ , ##arg)

#define DEBUG_LEDS_STROBE
#ifdef DEBUG_LEDS_STROBE
#define PK_DBG PK_DBG_FUNC
#define PK_VER PK_TRC_VERBOSE
#define PK_ERR PK_ERROR
#else
#define PK_DBG(a, ...)
#define PK_VER(a, ...)
#define PK_ERR(a, ...)
#endif


static BOOL g_strobe_On = 0;
static int g_duty=0;
static int g_timeOutTimeMs=0;
static int gGetPreOnDuty=0;
#ifdef ALPINE_CAMERA
static int g_duty_array[] = {60, 60, 60, 60};
#else
static int g_duty_array[] = {70, 70, 70, 70};
#endif

#define PRE_FLASH_LEVEL (20)

extern int mt65xx_flash_leds_brightness_set(unsigned  char level,int flash_time) ;
extern void  Flashlight_OutTimer_cancel(void) ;

#define MAX_TIME_OUT (10*1000)

extern void mt65xx_cam_flash_leds_brightness_restore(void);

static int sub_strobe_ioctl(unsigned int cmd, unsigned long arg)
{
    int temp;
	int i4RetValue = 0;
	
	int ior;
	int iow;
	int iowr;
	kdStrobeDrvArg kdArg;
	ior = _IOR(FLASHLIGHT_MAGIC,0, int);
	iow = _IOW(FLASHLIGHT_MAGIC,0, int);
	iowr = _IOWR(FLASHLIGHT_MAGIC,0, int);
    switch(cmd)
    {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",(int)arg);
		g_timeOutTimeMs=arg;
		break;

    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",(int)arg);
              if(arg >= 0 && arg <= 3)
                  g_duty = g_duty_array[arg];
    		break;
        case FLASH_IOC_PRE_ON:
    		PK_DBG("FLASH_IOC_PRE_ON\n");
#ifdef ALPINE_CAMERA
              mt65xx_flash_leds_brightness_set(PRE_FLASH_LEVEL, 2000);
#else
	       mt65xx_flash_leds_brightness_set(PRE_FLASH_LEVEL, 3000);
#endif
    		break;

        case FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY:
            PK_DBG("FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY: %d\n",(int)arg);
            
            break;

        case FLASH_IOC_GET_PRE_ON_TIME_MS:
    		PK_DBG("FLASH_IOC_GET_PRE_ON_TIME_MS: %d\n",(int)arg);
    		
             temp = gGetPreOnDuty;
    		kdArg.arg = temp;
            if(copy_to_user((void __user *) arg , (void*)&kdArg , sizeof(kdStrobeDrvArg)))
            {
                PK_DBG(" ioctl copy to user failed\n");
				PK_DBG(" arg = %lu",arg);
                return -1;
            }
    		break;
        case FLASH_IOC_GET_FLASH_DRIVER_NAME_ID:
            PK_DBG("FLASH_IOC_GET_FLASH_DRIVER_NAME_ID: %d\n",(int)arg);
            temp = e_FLASH_DRIVER_6332;
    		kdArg.arg = temp;
            if(copy_to_user((void __user *) arg , (void*)&kdArg , sizeof(kdStrobeDrvArg)))
            {
                PK_DBG(" ioctl copy to user failed\n");
                return -1;
            }
    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d, g_duty = %d, g_timeOutTimeMs = %d \n", (int)arg, g_duty, g_timeOutTimeMs);
    		if(arg==1)
    		{
		     if(g_timeOutTimeMs != 0)
	            {
#ifdef ALPINE_CAMERA
                      mt65xx_flash_leds_brightness_set(g_duty, 500);
#else
	               mt65xx_flash_leds_brightness_set(g_duty, MAX_TIME_OUT);
#endif
	            }
                   else
#ifdef ALPINE_CAMERA
                      mt65xx_flash_leds_brightness_set(g_duty, 500);
#else
                      mt65xx_flash_leds_brightness_set(g_duty, MAX_TIME_OUT);
#endif
    		     g_strobe_On=1;
    		}
    		else
    		{
    		    PK_DBG("\n tff: turn off flash \n");

#ifdef ALPINE_CAMERA
                    if (g_strobe_On == 0) {
                        PK_DBG("flash is already off, no need to turn off flash \n");
                    } else {
#endif
                
                mt65xx_cam_flash_leds_brightness_restore();
#ifdef ALPINE_CAMERA
                    }
#endif

                 g_strobe_On=0;
    		}
    		break;
        case FLASHLIGHTIOC_G_FLASHTYPE:
    		break;
    }
    return i4RetValue;
}

static int sub_strobe_open(void *pArg)
{
    PK_DBG("sub LCM open");
    return 0;

}

static int sub_strobe_release(void *pArg)
{
    PK_DBG("sub LCM release");
    return 0;

}

FLASHLIGHT_FUNCTION_STRUCT	subStrobeFunc=
{
	sub_strobe_open,
	sub_strobe_release,
	sub_strobe_ioctl
};


MUINT32 subStrobeInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &subStrobeFunc;
    }
    return 0;
}





