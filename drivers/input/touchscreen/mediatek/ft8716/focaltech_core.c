/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/sysfs.h>
#include "include/tpd_ft5x0x_common.h"

#include "focaltech_core.h"
#include "focaltech_flash.h"

#include "tpd.h"
#include "base.h"
#include "charging.h"

#include "battery_common.h"

#ifdef CONFIG_SYNC_TOUCH_STATUS
#include <linux/CwMcuSensor.h>
#endif

#ifdef TIMER_DEBUG
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/input/mt.h>
#ifdef CONFIG_MTK_SH_SUPPORT
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#endif

#define CC_DEBUG	1
#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
#include <linux/input/touch_fw_update.h>
#include <linux/async.h>
#include <linux/wakelock.h>
#include <linux/firmware.h>
#endif

#ifndef	CTP_VERSION_COMPAT
#define CTP_VERSION_COMPAT
#endif
#ifdef CTP_VERSION_COMPAT

#include <linux/htc_devices_dtb.h>

typedef enum{
	FOCAL_CTP_MAIN,
	FOCAL_CTP_SECOND
}FOCAL_CTP_MODULE;

#endif


enum{
	GPIO_LOW,
	GPIO_HIGH,
};

static bool FT_INT_IS_EDGE = true;		
static int irq_enable_count = 0;		

static struct fts_ts_data *ts_data;

int  power_switch_gesture = 0;
#if FT_ESD_PROTECT
extern int  fts_esd_protection_init(void);
extern int  fts_esd_protection_exit(void);
extern int  fts_esd_protection_notice(void);
extern int  fts_esd_protection_suspend(void);
extern int  fts_esd_protection_resume(void);


  int apk_debug_flag = 0; 

#define TPD_ESD_CHECK_CIRCLE        		200
 void gtp_esd_check_func(void);
static int count_irq = 0;
#ifndef FTS_SHOW_INTR
static unsigned long esd_check_circle = TPD_ESD_CHECK_CIRCLE;
static u8 run_check_91_register = 0;
#endif
#endif


#if FT_ESD_PROTECT
#ifndef FTS_SHOW_INTR
 static void force_reset_guitar(void)
 {
	
	tpd_gpio_output(tpd_rst_gpio_number, 0);
	msleep(20);
	tpd_gpio_output(tpd_rst_gpio_number, 1);
	msleep(400);
 }
#endif

#define A3_REG_VALUE					0x87
#define RESET_91_REGVALUE_SAMECOUNT 	5
#ifndef FTS_SHOW_INTR
static u8 g_old_91_Reg_Value = 0x00;
static u8 g_first_read_91 = 0x01;
static u8 g_91value_same_count = 0;
#endif

#ifdef FTS_SHOW_INTR
u16 intr_cnt = 0;
u8 buf_last[5];
void print_info(u8 *buf)
{
    int i;

    D("[%s][%d] start!\n", __func__, __LINE__);

    for(i = 0; i < 4; i++)
    {
        if(buf_last[i] != buf[i])
            break;
    }

    if (i >= 4 ) 
    { 
        if(buf_last[4] == buf[4])
        {
            I("sys-status=0x%02x FWValid=0x%02x IntCountL=0x%02x IntCountM=0x%02x FlowCount=%02x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
            I("intr cnt in driver=0x%x\n", intr_cnt);
        }
            
    }
    else
    {
        I("sys-status=0x%02x FWValid=0x%02x IntCountL=0x%02x IntCountM=0x%02x FlowCount=%02x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
        I("intr cnt in driver=0x%x\n", intr_cnt);
    }
    D("[%s][%d] end!\n", __func__, __LINE__);
}

int show_intr_status(void)
{
    u8 buf[5] = { 0 };
    buf[0] = 0x8F;
    fts_i2c_read(fts_i2c_client, buf, 1, buf, 5);
    print_info(buf);
    memcpy(buf_last, buf, 5);
    
    return 0;
}

void enable_fw_debug(void)
{
    fts_write_reg(fts_i2c_client, 0xE0, 0x5A);
}
#endif
 void gtp_esd_check_func(void)
 {
 #ifdef FTS_SHOW_INTR
    show_intr_status();
 #else
	 int i;
	 int ret = -1;
	 u8 data;
	 
	 
	 int reset_flag = 0;
	 
     #if 0
	 if (tpd_halt ) 
	 {
		 
	 }
	 if(apk_debug_flag) 
	 {
		 
		 return;
	 }
     #endif
	 run_check_91_register = 0;
	 for (i = 0; i < 3; i++) 
	 {
		 
		 ret = fts_read_reg(fts_i2c_client, 0xA3,&data);
		 if (ret<0) 
		 {
			 printk("[Focal][Touch] read value fail");
			 
		 }
		 if (ret==1 && A3_REG_VALUE==data) 
		 {
			 break;
		 }
	 }
 
	 if (i >= 3) 
	 {
		 force_reset_guitar();
		 printk("focal--tpd reset. i >= 3  ret = %d  A3_Reg_Value = 0x%02x\n ", ret, data);
		 reset_flag = 1;
		 goto FOCAL_RESET_A3_REGISTER;
	 }
 #if 1
	 
 	
 	
	 run_check_91_register = 1;
	 
	 ret = fts_read_reg(fts_i2c_client, 0x91,&data);
	 if (ret<0) 
	 {
		 printk("[Focal][Touch] read value of 0x91 register fail  !!!");
		 
	 }
	 printk("focal---------91 register value = 0x%02x	 old value = 0x%02x\n",  data, g_old_91_Reg_Value);
	 if(0x01 == g_first_read_91) 
	 {
		 g_old_91_Reg_Value = data;
		 g_first_read_91 = 0x00;
	 } 
	 else 
	 {
		 if(g_old_91_Reg_Value == data)
		 {
			 g_91value_same_count++;
			 printk("focal 91 value ==============, g_91value_same_count=%d\n", g_91value_same_count);
			 if(RESET_91_REGVALUE_SAMECOUNT == g_91value_same_count) 
			 {
				 force_reset_guitar();
				 printk("focal--tpd reset. g_91value_same_count = 5\n");
				 g_91value_same_count = 0;
				 reset_flag = 1;
			 }
			 
			 
			 esd_check_circle = TPD_ESD_CHECK_CIRCLE / 2;
			 g_old_91_Reg_Value = data;
		 } 
		 else 
		 {
			 g_old_91_Reg_Value = data;
			 g_91value_same_count = 0;
			 
			 esd_check_circle = TPD_ESD_CHECK_CIRCLE;
		 }
	 }
#endif
 
 FOCAL_RESET_A3_REGISTER:
	 count_irq=0;
	 data=0;
	 
	 ret = fts_write_reg(fts_i2c_client, 0x8F,data);
	 if (ret<0) 
	 {
		 printk("[Focal][Touch] write value fail");
		 
	 }
	 if(0 == run_check_91_register)
	 {
		 g_91value_same_count = 0;
	 }
	 
     #if 0
	 if (!tpd_halt)
	 {
		 
		 
	 }
     #endif
 #endif    
	 return;
 }
#endif


#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
enum DOZE_T {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
};
static DOZE_T doze_status = DOZE_DISABLED;
#endif

#ifdef CONFIG_MTK_SH_SUPPORT
static s8 ftp_enter_doze(struct i2c_client *client);

enum TOUCH_IPI_CMD_T {
	
	IPI_COMMAND_SA_GESTURE_TYPE,
	
	IPI_COMMAND_AS_CUST_PARAMETER,
	IPI_COMMAND_AS_ENTER_DOZEMODE,
	IPI_COMMAND_AS_ENABLE_GESTURE,
	IPI_COMMAND_AS_GESTURE_SWITCH,
};

struct Touch_Cust_Setting {
	u32 i2c_num;
	u32 int_num;
	u32 io_int;
	u32 io_rst;
};

struct Touch_IPI_Packet {
	u32 cmd;
	union {
		u32 data;
		struct Touch_Cust_Setting tcs;
	} param;
};

static bool tpd_scp_doze_en = TRUE;
DEFINE_MUTEX(i2c_access);
#endif

#define TPD_SUPPORT_POINTS	10
#define GESTURE_CONTROL
u8 ver;
struct i2c_client *i2c_client = NULL;
struct task_struct *thread_tpd = NULL;
struct i2c_client *fts_i2c_client 			= NULL;
struct input_dev *fts_input_dev				= NULL;
#ifdef TPD_AUTO_UPGRADE
static bool is_update = false;
#endif
#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT
u8 *tpd_i2c_dma_va = NULL;
dma_addr_t tpd_i2c_dma_pa = 0;
#endif

spinlock_t  gesture_lock;
static struct kobject *touchscreen_dir=NULL;
static struct kobject *virtual_dir=NULL;
static struct kobject *touchscreen_dev_dir=NULL;
static char *vendor_name=NULL;
static u8 ctp_fw_version;
u32 gesture_switch;
EXPORT_SYMBOL_GPL(gesture_switch);
#if USB_CHARGE_DETECT
extern PMU_ChargerStruct BMT_status;
int close_to_ps_flag_value = 1;
int charging_flag = 0;
#endif


static DECLARE_WAIT_QUEUE_HEAD(waiter);

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id);


static int focal_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int focal_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int focal_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
static void focal_resume(struct device *h);
static void focal_suspend(struct device *h);
static int tpd_flag = 0;

unsigned int tpd_rst_gpio_number = 0;
unsigned int tpd_int_gpio_number = 1;
unsigned int touch_irq = 0;
#define TPD_OK 0


#define DEVICE_MODE	0x00
#define GEST_ID		0x01
#define TD_STATUS	0x02

#define TOUCH1_XH	0x03
#define TOUCH1_XL	0x04
#define TOUCH1_YH	0x05
#define TOUCH1_YL	0x06

#define TOUCH2_XH	0x09
#define TOUCH2_XL	0x0A
#define TOUCH2_YH	0x0B
#define TOUCH2_YL	0x0C

#define TOUCH3_XH	0x0F
#define TOUCH3_XL	0x10
#define TOUCH3_YH	0x11
#define TOUCH3_YL	0x12

#define TPD_RESET_ISSUE_WORKAROUND
#define TPD_MAX_RESET_COUNT	3


#ifdef CONFIG_SLT_DRV_DEVINFO_SUPPORT
#define SLT_DEVINFO_CTP_DEBUG
#include  <linux/dev_info.h>
u8 ver_id;
u8 ver_module;
static char* temp_ver;
static char* temp_comments;
static struct devinfo_struct s_DEVINFO_ctp;   
static void devinfo_ctp_regchar(char *module,char * vendor,char *version,char *used, char* comments)
{

	
	s_DEVINFO_ctp.device_type="CTP";
	s_DEVINFO_ctp.device_module=module;
	s_DEVINFO_ctp.device_vendor=vendor;
	s_DEVINFO_ctp.device_ic="FT8716";
	s_DEVINFO_ctp.device_info=DEVINFO_NULL;
	s_DEVINFO_ctp.device_version=version;
	s_DEVINFO_ctp.device_used=used;
	s_DEVINFO_ctp.device_comments=comments;
#ifdef SLT_DEVINFO_CTP_DEBUG
	printk("[DEVINFO CTP]registe CTP device! type:<%s> module:<%s> vendor<%s> ic<%s> version<%s> info<%s> used<%s> comment<%s>\n",
		s_DEVINFO_ctp.device_type,s_DEVINFO_ctp.device_module,s_DEVINFO_ctp.device_vendor,
		s_DEVINFO_ctp.device_ic,s_DEVINFO_ctp.device_version,s_DEVINFO_ctp.device_info,
                s_DEVINFO_ctp.device_used, s_DEVINFO_ctp.device_comments);
#endif
       DEVINFO_CHECK_DECLARE_COMMEN(s_DEVINFO_ctp.device_type,s_DEVINFO_ctp.device_module,s_DEVINFO_ctp.device_vendor,
                                    s_DEVINFO_ctp.device_ic,s_DEVINFO_ctp.device_version,s_DEVINFO_ctp.device_info,
                                    s_DEVINFO_ctp.device_used,s_DEVINFO_ctp.device_comments);
      

}
#endif


#ifdef TIMER_DEBUG

static struct timer_list test_timer;

static void timer_func(unsigned long data)
{
	tpd_flag = 1;
	wake_up_interruptible(&waiter);

	mod_timer(&test_timer, jiffies + 100*(1000/HZ));
}

static int init_test_timer(void)
{
	memset((void *)&test_timer, 0, sizeof(test_timer));
	test_timer.expires  = jiffies + 100*(1000/HZ);
	test_timer.function = timer_func;
	test_timer.data     = 0;
	init_timer(&test_timer);
	add_timer(&test_timer);
	return 0;
}
#endif


#if defined(CONFIG_TPD_ROTATE_90) || defined(CONFIG_TPD_ROTATE_270) || defined(CONFIG_TPD_ROTATE_180)

static void tpd_swap_xy(int *x, int *y)
{
	int temp = 0;

	temp = *x;
	*x = *y;
	*y = temp;
}

static void tpd_rotate_90(int *x, int *y)
{

	*x = TPD_RES_X + 1 - *x;

	*x = (*x * TPD_RES_Y) / TPD_RES_X;
	*y = (*y * TPD_RES_X) / TPD_RES_Y;

	tpd_swap_xy(x, y);
}

static void tpd_rotate_180(int *x, int *y)
{
	*y = TPD_RES_Y + 1 - *y;
	*x = TPD_RES_X + 1 - *x;
}

static void tpd_rotate_270(int *x, int *y)
{

	*y = TPD_RES_Y + 1 - *y;

	*x = (*x * TPD_RES_Y) / TPD_RES_X;
	*y = (*y * TPD_RES_X) / TPD_RES_Y;

	tpd_swap_xy(x, y);
}

#endif
struct touch_info {
	int y[TPD_SUPPORT_POINTS];
	int x[TPD_SUPPORT_POINTS];
	int p[TPD_SUPPORT_POINTS];
	int id[TPD_SUPPORT_POINTS];
	int count;
};

#define __MSG_DMA_MODE__
#ifdef __MSG_DMA_MODE__
	u8 *g_dma_buff_va = NULL;
	dma_addr_t g_dma_buff_pa = 0;
#endif

#ifdef __MSG_DMA_MODE__

	static void msg_dma_alloct(void)
	{
	    if (NULL == g_dma_buff_va)
    		{
       		 tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
       		 g_dma_buff_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev, 128, &g_dma_buff_pa, GFP_KERNEL);
    		}

	    	if(!g_dma_buff_va)
	    	{
	        	TPD_DMESG("[DMA][Error] Allocate DMA I2C Buffer failed!\n");
	    	}
	}
	static void msg_dma_release(void){
		if(g_dma_buff_va)
		{
	     		dma_free_coherent(NULL, 128, g_dma_buff_va, g_dma_buff_pa);
	        	g_dma_buff_va = NULL;
	        	g_dma_buff_pa = 0;
			TPD_DMESG("[DMA][release] Allocate DMA I2C Buffer release!\n");
	    	}
	}
#endif

static DEFINE_MUTEX(i2c_access);
static DEFINE_MUTEX(i2c_rw_access);

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
static int tpd_def_calmat_local_normal[8]  = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif


static const struct i2c_device_id ft8716_tpd_id[] = {{"FT8716", 0}, {} };
static const struct of_device_id ft8716_dt_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};
MODULE_DEVICE_TABLE(of, ft8716_dt_match);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(ft8716_dt_match),
		.name = "FT8716",
	},
	.probe = focal_probe,
	.remove = focal_remove,
	.id_table = ft8716_tpd_id,
	.detect = focal_i2c_detect,
};

static int of_get_ft5x0x_platform_data(struct device *dev)
{
	

	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(ft8716_dt_match), dev);
		if (!match) {
			TPD_DMESG("Error: No device match found\n");
			return -ENODEV;
		}
	}
#ifdef MTK
	tpd_rst_gpio_number = GPIO_TP_RST;
	tpd_int_gpio_number = GPIO_TP_INT;
#else
	
	

#endif
	TPD_DMESG("tpd_rst_gpio_number: %d\n", tpd_rst_gpio_number);
	TPD_DMESG("tpd_int_gpio_number: %d\n", tpd_int_gpio_number);
	return 0;
}

#ifdef CONFIG_MTK_SH_SUPPORT
static ssize_t show_scp_ctrl(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t store_scp_ctrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u32 cmd;
	Touch_IPI_Packet ipi_pkt;

	if (kstrtoul(buf, 10, &cmd)) {
		TPD_DEBUG("[SCP_CTRL]: Invalid values\n");
		return -EINVAL;
	}

	TPD_DEBUG("SCP_CTRL: Command=%d", cmd);
	switch (cmd) {
	case 1:
	    
	    tpd_scp_wakeup_enable(TRUE);
	    focal_suspend(NULL);
	    break;
	case 2:
	    focal_resume(NULL);
	    break;
	case 5:
		{
				Touch_IPI_Packet ipi_pkt;

				ipi_pkt.cmd = IPI_COMMAND_AS_CUST_PARAMETER;
			    ipi_pkt.param.tcs.i2c_num = TPD_I2C_NUMBER;
			ipi_pkt.param.tcs.int_num = CUST_EINT_TOUCH_NUM;
				ipi_pkt.param.tcs.io_int = tpd_int_gpio_number;
			ipi_pkt.param.tcs.io_rst = tpd_rst_gpio_number;
			if (md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0) < 0)
				TPD_DEBUG("[TOUCH] IPI cmd failed (%d)\n", ipi_pkt.cmd);

			break;
		}
	default:
	    TPD_DEBUG("[SCP_CTRL] Unknown command");
	    break;
	}

	return size;
}
static DEVICE_ATTR(tpd_scp_ctrl, 0664, show_scp_ctrl, store_scp_ctrl);
#endif

static struct device_attribute *ft8716_attrs[] = {
#ifdef CONFIG_MTK_SH_SUPPORT
	&dev_attr_tpd_scp_ctrl,
#endif
};

#define CONFIG_TOUCHSCREEN_FOCAL_DEBUG
#ifdef CONFIG_TOUCHSCREEN_FOCAL_DEBUG

static struct kobject *android_touch_kobj = NULL;

static ssize_t focal_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fts_ts_data *ts;
	ts = ts_data;
	ret += sprintf(buf, "%s_FW:%#x_ID:%x%x\n", "FocalTech", ts->fw_ver, ts->fw_chip_id[1], ts->fw_chip_id[0]);

	return ret;
}

static DEVICE_ATTR(vendor, (S_IRUGO), focal_vendor_show, NULL);


static ssize_t focal_attn_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	sprintf(buf, "attn=%d\n", gpio_get_value(tpd_int_gpio_number));
	ret = strlen(buf) + 1;

	return ret;
}
static DEVICE_ATTR(attn, (S_IRUGO), focal_attn_show, NULL);

static ssize_t focal_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", ts_data->irq_enabled);

	return ret;
}

void focal_int_enable(int irqnum, int enable, int log_print)
{
	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
	}
if(log_print)
	TPD_DEBUG("irq_enable_count = %d\n", irq_enable_count);
}

static ssize_t focal_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts = ts_data;
	int value, ret = 0;

	if (sysfs_streq (buf, "0"))
		value = false;
	else if (sysfs_streq (buf, "1"))
		value = true;
	else
		return -EINVAL;

	if (value)
	{
		if (FT_INT_IS_EDGE)
		{
			
			
			ret = request_irq(touch_irq, tpd_eint_interrupt_handler, IRQF_TRIGGER_FALLING, "TOUCH_PANEL-eint", NULL);
			

		}
		else
		{
			
			
			ret = request_irq(touch_irq, tpd_eint_interrupt_handler, IRQF_TRIGGER_LOW, "TOUCH_PANEL-eint", NULL);
			
		}
		if (ret > 0)
			TPD_DMESG("tpd request_irq IRQ LINE NOT AVAILABLE!.");
		else if(ret == 0)
		{
			ts->irq_enabled = 1;
			irq_enable_count = 1;
		}

	}
	else
	{
		focal_int_enable(touch_irq, 0, 1);

		free_irq (touch_irq, ts);
		ts->irq_enabled = 0;
	}

	return count;
}
static DEVICE_ATTR(enable, (S_IWUSR|S_IRUGO), focal_enable_show, focal_enable_store);

static ssize_t focal_debug_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", ts_data->debug_level);

	return ret;
}

static ssize_t focal_debug_level_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts = ts_data;
	char tmp[11] = {0};
	int i;

	ts->debug_level = 0;
	memcpy(tmp, buf, count);

	for(i = 0; i < count; i++)
	{
		if( tmp[i]>='0' && tmp[i]<='9' )
			ts->debug_level |= (tmp[i]-'0');
		else if( tmp[i]>='A' && tmp[i]<='F' )
			ts->debug_level |= (tmp[i]-'A'+10);
		else if( tmp[i]>='a' && tmp[i]<='f' )
			ts->debug_level |= (tmp[i]-'a'+10);
	}

	return count;
}

static DEVICE_ATTR(debug_level, (S_IWUSR|S_IRUGO), focal_debug_level_show, focal_debug_level_store);

static int focal_touch_sysfs_init(void)
{
	int ret;
	android_touch_kobj = kobject_create_and_add("android_touch", NULL);
	if (android_touch_kobj == NULL) {
		E("%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_vendor failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_attn.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_attn failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_enable.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_enable failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug_level.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_enable failed\n", __func__);
		return ret;
	}
	return 0;
}

static void focal_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_attn.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_enable.attr);
}

#endif


#ifdef FT_SYS_DEBUG
static ssize_t focal_debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}
#endif
static ssize_t mtk_ctp_firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

  int ret;
  ret=sprintf(buf, "%s:%s:0x%02x\n",vendor_name,"FT8716",ts_data->fw_ver);

  return ret;
}

static struct kobj_attribute ctp_firmware_version_attr = {
	.attr = {
		 .name = "firmware_version",                    
		 .mode = S_IRUGO,
		 },
	.show = &mtk_ctp_firmware_version_show,
};

static ssize_t mtk_ctp_firmware_update_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
    char fwname[128];
	struct i2c_client *client = fts_i2c_client;
	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';
	#if FT_ESD_PROTECT
		esd_switch(0);apk_debug_flag = 1;
	#endif
	mutex_lock(&fts_input_dev->mutex);
	
	disable_irq(client->irq);
	fts_ctpm_fw_upgrade_with_app_file(client, fwname);
	enable_irq(client->irq);
	
	mutex_unlock(&fts_input_dev->mutex);
	#if FT_ESD_PROTECT
		esd_switch(1);apk_debug_flag = 0;
	#endif
	return count; 
 
}

static struct kobj_attribute ctp_firmware_update_attr = {
	.attr = {
		 .name = "firmware_update",
		 .mode = S_IWUGO,
		 },
	.store = &mtk_ctp_firmware_update_store,

};

#ifdef GESTURE_CONTROL
static ssize_t mtk_ctp_gesture_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
    unsigned int gctl=0;
	unsigned long flags=0;
	spin_lock_irqsave(&gesture_lock, flags);
    if (sscanf(buf,"%d",&gctl) != 1) 
	{
     printk("%s:sscanf  fail gctl=%d\n", __func__,gctl);
	 spin_unlock_irqrestore(&gesture_lock, flags);
	 return -1;
    }
    
    
	
	if(gctl!=0)
	{
     gesture_switch=1;
	}
	else 
	{
     gesture_switch=0;
	}
	spin_unlock_irqrestore(&gesture_lock, flags);
    return count;
}
static ssize_t mtk_ctp_gesture_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

  int ret;
  u8 state;
  fts_read_reg(i2c_client, 0xd0, &state);   
  ret=sprintf(buf, "%s=%d:%s=%d\n","gesture_switch",gesture_switch,"[0xd0]",(int)state);
			   		     
  return ret;
 
}

static struct kobj_attribute ctp_gesture_ctrl_attr = {
         .attr = {
                  .name = "gesture_ctrl",
                  .mode = S_IRWXUGO,             
         },    

         .store = &mtk_ctp_gesture_ctrl_store,  
         .show  = &mtk_ctp_gesture_ctrl_show,
};
#endif
static struct attribute *mtk_properties_attrs[] = {
		&ctp_firmware_version_attr.attr,
		&ctp_firmware_update_attr.attr,
#ifdef GESTURE_CONTROL
		&ctp_gesture_ctrl_attr.attr,
#endif
	NULL
};

static struct attribute_group mtk_ctp_attr_group = {
	.attrs = mtk_properties_attrs,
};


static int create_ctp_node(void)
{
  int ret;
  virtual_dir = virtual_device_parent(NULL);
  if(!virtual_dir)
  {
   printk("Get virtual dir failed\n");
   return -ENOMEM;
  }
  touchscreen_dir=kobject_create_and_add("touchscreen",virtual_dir);
  if(!touchscreen_dir)
  {
   printk("Create touchscreen dir failed\n");
   return -ENOMEM;
  }
  touchscreen_dev_dir=kobject_create_and_add("touchscreen_dev",touchscreen_dir);
  if(!touchscreen_dev_dir)
  {
   printk("Create touchscreen_dev dir failed\n");
   return -ENOMEM;
  }
  ret=sysfs_create_group(touchscreen_dev_dir, &mtk_ctp_attr_group);
  if(ret)
  {
    printk("create mtk_ctp_firmware_vertion_attr_group error\n");
  } 
 
  return 0;
}


int focal_read_power_mode(struct i2c_client *client)
{
	u8 mode = -1;
	int ret = -1;

	ret = fts_read_reg (client, 0xA5, &mode);
	if(ret < 0)
	{
		E("[%s|%d]Fail to read ID_G_PMODE %d\n", __func__, __LINE__, ret);
		return ret;
	}

	I("focal read ID_G_PMODE: %d.\n", mode);
	ret = (int)mode;

	return ret;
}

#ifdef CONFIG_MTK_I2C_EXTENSION

int fts_i2c_read(struct i2c_client *client, char *writebuf,int writelen, char *readbuf, int readlen)
{
	int ret=0;

	
	mutex_lock(&i2c_rw_access);

	if((NULL!=client) && (writelen>0) && (writelen<=128))
	{
		
		memcpy(g_dma_buff_va, writebuf, writelen);
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
		client->timing = 400;
		if((ret=i2c_master_send(client, (unsigned char *)g_dma_buff_pa, writelen))!=writelen)
			E("i2c write failed\n");
		client->addr = (client->addr & I2C_MASK_FLAG) &(~ I2C_DMA_FLAG);
	}

	
	if((NULL!=client) && (readlen>0) && (readlen<=128))
	{
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;

		ret = i2c_master_recv(client, (unsigned char *)g_dma_buff_pa, readlen);
		memcpy(readbuf, g_dma_buff_va, readlen);

		client->addr = (client->addr & I2C_MASK_FLAG) &(~ I2C_DMA_FLAG);
	}
	mutex_unlock(&i2c_rw_access);

	return ret;
}

int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int i = 0;
	int ret = 0;

	client->timing = 400;
	if (writelen <= 8) {
	    client->ext_flag = client->ext_flag & (~I2C_DMA_FLAG);
		return i2c_master_send(client, writebuf, writelen);
	}
	else if((writelen > 8)&&(NULL != tpd_i2c_dma_va))
	{
		for (i = 0; i < writelen; i++)
			tpd_i2c_dma_va[i] = writebuf[i];

		client->addr = (client->addr & I2C_MASK_FLAG )| I2C_DMA_FLAG;
	    
	    ret = i2c_master_send(client, (unsigned char *)tpd_i2c_dma_pa, writelen);
	    client->addr = client->addr & I2C_MASK_FLAG & ~I2C_DMA_FLAG;
		
	    
		return ret;
	}
	return 1;
}

#else
int fts_i2c_read(struct i2c_client *client, char *writebuf,int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	#if FT_ESD_PROTECT
    int i;
	
		for(i = 0; i < 3; i++)
		{
			ret = fts_esd_protection_notice();
			if(0 == ret)
				break;	
			else
			{
				printk("[focal] fts_esd_protection_notice return :%d \n", ret);
				continue;
			}
		}
		if(3 == i)
		{
			printk("[focal] ESD are still use I2C. \n");
		}
	#endif

	mutex_lock(&i2c_rw_access);

	if(readlen > 0)
	{
		if (writelen > 0) {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = 0,
					 .len = writelen,
					 .buf = writebuf,
				 },
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0)
				E("i2c read error\n");
		} else {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0)
				E("i2c read error.\n");
		}
	}

	mutex_unlock(&i2c_rw_access);
	
	return ret;
}


int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret=0;
    struct i2c_msg msgs[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};
#if FT_ESD_PROTECT
	fts_esd_protection_notice();
#endif
	
	mutex_lock(&i2c_rw_access);

	if(writelen > 0)
	{
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			E("i2c write error.\n");
	}

	mutex_unlock(&i2c_rw_access);
	
	return ret;
}
#endif

int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = {0};

	buf[0] = regaddr;
	buf[1] = regvalue;

	return fts_i2c_write(client, buf, sizeof(buf));
}
int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{

	return fts_i2c_read(client, &regaddr, 1, regvalue, 1);

}
#ifdef MT_PROTOCOL_B
static int fts_read_Touchdata(struct ts_event *data)
{
	u8 buf[POINT_READ_BUF] = { 0 };
#ifdef CONFIG_FOCAL_STATUS_DEBUG
	u8 fw_status[8] = {0};
	u8 reg  = 0;
#endif
	int ret = -1;
	int i = 0;
	u8 pointid = FTS_MAX_ID;
	D("%s fts_read_Touchdata start.\n",__func__);
	
	ret = fts_i2c_read(fts_i2c_client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) 
	{
		E("%s read touch data failed.\n",__func__);
		mutex_unlock(&i2c_access);
		return ret;
	}
	else
	{
		D("%s read touch data success.\n",__func__);
	}
	#if REPORT_TOUCH_DEBUG	
		for(i=0;i<POINT_READ_BUF;i++)
		{
			D("[fts] zax buf[%d] =(0x%02x)  \n", i,buf[i]);
		}
	#endif
	
	memset(data, 0, sizeof(struct ts_event));
	data->touch_point = 0;	
	data->touch_point_num = buf[FT_TOUCH_POINT_NUM] & 0x0F;
	
	
	for (i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)
	{
		pointid = (buf[FTS_TOUCH_ID_POS + FTS_TOUCH_STEP * i]) >> 4;
 #ifdef FTS_SHOW_INTR
		
        
        if(pointid == 0x0C)
        {
            I("point:%02x %02x %02x %02x %02x %02x\n", buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
            return 1;
        }
 #endif   

		if (pointid >= FTS_MAX_ID)
			break;
		else
			data->touch_point++;
		data->au16_x[i] =
		    (s16) (buf[FTS_TOUCH_X_H_POS + FTS_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_X_L_POS + FTS_TOUCH_STEP * i];
		data->au16_y[i] =
		    (s16) (buf[FTS_TOUCH_Y_H_POS + FTS_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_Y_L_POS + FTS_TOUCH_STEP * i];
		data->au8_touch_event[i] =
		    buf[FTS_TOUCH_EVENT_POS + FTS_TOUCH_STEP * i] >> 6;
		data->au8_finger_id[i] =
		    (buf[FTS_TOUCH_ID_POS + FTS_TOUCH_STEP * i]) >> 4;
		data->press[i] =	(buf[FTS_TOUCH_XY_POS + FTS_TOUCH_STEP * i]);
		data->area[i] =	(buf[FTS_TOUCH_MISC + FTS_TOUCH_STEP * i]) >> 4;
		if((data->au8_touch_event[i]==0 || data->au8_touch_event[i]==2)&&((data->touch_point_num==0)||(data->press[i]==0 && data->area[i]==0  )))
			return 1;
		#if REPORT_TOUCH_DEBUG
		TPD_DEBUG("[fts] zax data (id= %d ,x=(0x%02x),y= (0x%02x))\n ", data->au8_finger_id[i],data->au16_x[i],data->au16_y[i]);
		#endif
	}
#ifdef CONFIG_FOCAL_STATUS_DEBUG
	I("%s read touch fw status:\n",__func__);
	ret = fts_write_reg(fts_i2c_client, FTS_REG_FW_STATUS, FTS_REG_FW_STATUS);
	if (ret < 0)
	{
		E("%s read touch firmware debug status failed.\n",__func__);
		mutex_unlock(&i2c_access);
		return ret;
	}

	reg = FTS_REG_FW_STATUS;
	ret = fts_i2c_read(fts_i2c_client, &reg, 1, fw_status, 8);
	if (ret < 0)
	{
		E("%s read touch data failed.\n",__func__);
		mutex_unlock(&i2c_access);
		return ret;
	}
	else
	{
		I("%s read touch fw status:\n",__func__);
		I("NoiseLevel:0x%x\n",fw_status[0]);
		I("ucVK1DIFF:0x%x\n",fw_status[1]);
		I("ucVK2DIFF:0x%x\n",fw_status[2]);
		I("ucVK3DIFF:0x%x\n",fw_status[3]);
		I("ucPeaknum:0x%x\n",fw_status[4]);
		I("ucFliterStatus:0x%x\n",fw_status[5]);
		I("AfeRunMode:0x%x\n",fw_status[6]);
		I("CurWorkFreqIdx:0x%x\n",fw_status[7]);
	}

#endif

	#if REPORT_TOUCH_DEBUG
	D("[fts] zax data (data->touch_point= %d \n ", data->touch_point);
	#endif
	D("%s fts_read_Touchdata end.\n",__func__);
	return 0;
}

static int fts_report_value(struct ts_event *data)
 {
	int i = 0,j=0;
	int up_point = 0;
 	int touchs = 0;	

 	if(ts_data->debug_level & BIT(1))
		I("report data touch_point number:%d\n", data->touch_point);
	for (i = 0; i < data->touch_point; i++) 
	{
		 input_mt_slot(tpd->dev, data->au8_finger_id[i]);
 
		if (data->au8_touch_event[i]== DOWN_EVT || data->au8_touch_event[i] == CONTACT_EVT)
		{
			input_mt_report_slot_state (tpd->dev, MT_TOOL_FINGER, true);
			input_report_key (tpd->dev, BTN_TOUCH, 1);
			
			input_report_abs (tpd->dev, ABS_MT_PRESSURE, data->press[i]);	
			input_report_abs (tpd->dev, ABS_MT_TOUCH_MAJOR, data->area[i]);		
			input_report_abs (tpd->dev, ABS_MT_POSITION_X, data->au16_x[i]);
			input_report_abs (tpd->dev, ABS_MT_POSITION_Y, data->au16_y[i]);
			
			touchs |= BIT (data->au8_finger_id[i]);
			data->touchs |= BIT (data->au8_finger_id[i]);
			if(data->au8_touch_event[i]== DOWN_EVT)
			{
				if(ts_data->debug_level & BIT(3))
					I("status: down, id=%d ,x=%d, y=%d, pres=%d, area=%d\n", data->au8_finger_id[i], data->au16_x[i],
					data->au16_y[i], data->press[i], data->area[i]);
			}
			else if(data->au8_touch_event[i]== CONTACT_EVT)
			{
				if(ts_data->debug_level & BIT(1))
					I("status: contact, id=%d ,x=%d, y=%d, pres=%d, area=%d\n",
					  data->au8_finger_id[i], data->au16_x[i], data->au16_y[i], data->press[i], data->area[i]);
			}
		}
		else
		{
			if(ts_data->debug_level & BIT(3))
			{
				I("status: up, id=%d ,x=%d, y=%d, pres=%d, area=%d\n",
				  data->au8_finger_id[i], data->au16_x[i], data->au16_y[i], data->press[i], data->area[i]);
			}
			up_point++;
			input_mt_report_slot_state (tpd->dev, MT_TOOL_FINGER, false);

			data->touchs &= ~BIT (data->au8_finger_id[i]);
		}				 
		 
	}
	for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++){   
		if(BIT(i) & (data->touchs ^ touchs)){
			#if REPORT_TOUCH_DEBUG
				D("zax add up  id=%d \n", i);
			#endif
			data->touchs &= ~BIT(i);
			input_mt_slot(tpd->dev, i);
			input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
		}
	}
	if(ts_data->debug_level & BIT(1))
		I("touchs=%d, data-touchs=%d, touch_point=%d, up_point=%d \n ", touchs, data->touchs, data->touch_point, up_point);

	data->touchs = touchs;

	
	 if((data->touch_point_num==0))    
	{	
		for(j = 0; j < fts_updateinfo_curr.TPD_MAX_POINTS; j++)
		{
			input_mt_slot( tpd->dev, j);
			input_mt_report_slot_state( tpd->dev, MT_TOOL_FINGER, false);
		}
		
		data->touchs=0;
		input_report_key(tpd->dev, BTN_TOUCH, 0);
		input_sync(tpd->dev);
		
		#if REPORT_TOUCH_DEBUG
		printk("[fts] zax  end 2 touchs=%d, data-touchs=%d, touch_point=%d, up_point=%d \n ", touchs,data->touchs,data->touch_point, up_point);
		printk("\n [fts] end 2 \n ");		
		#endif
		return 0;
    } 
	#if REPORT_TOUCH_DEBUG
	printk("[fts] zax 2 touchs=%d, data-touchs=%d, touch_point=%d, up_point=%d \n ", touchs,data->touchs,data->touch_point, up_point);
	#endif
	if(data->touch_point == up_point)
	{
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 
	}
	else
	input_report_key(tpd->dev, BTN_TOUCH, 1);
 
	input_sync(tpd->dev);
	
	return 0;
 }

#endif


static int touch_event_handler(void *unused)
{
	
	int ret = 0;
	#if FTS_GESTRUE_EN
	
	u8 state = 0;
	#endif
	#if USB_CHARGE_DETECT
	u8  data;
    #endif
	#ifdef MT_PROTOCOL_B
	struct ts_event pevent;
	#endif

	
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };


	sched_setscheduler(current, SCHED_RR, &param);
	I("touch_event_handler start\n");
	do {
		
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);

		tpd_flag = 0;

		set_current_state(TASK_RUNNING);
        #if USB_CHARGE_DETECT
        if((BMT_status.charger_exist!= 0)&&(charging_flag == 0))
		{
		 data = 0x1;
		 charging_flag = 1;
		 fts_write_reg(i2c_client,0x8B,0x01);
		}
		else
		{
		 if ((BMT_status.charger_exist == 0)&&(charging_flag == 1))
		 {
		  charging_flag = 0;
		  data = 0x0;
		  fts_write_reg(i2c_client,0x8B,0x00);
		 }
		
		}
 
        #endif 
		#if FTS_GESTRUE_EN
			ret = fts_read_reg(fts_i2c_client, 0xd0, &state);
			if (ret<0)
			{
				printk("[Focal][Touch] read value fail");
				
			}
			
		     	if(state ==1)
		     	{
			        fts_read_Gestruedata();
			        continue;
		    	}
		 #endif

#ifdef MT_PROTOCOL_B
	 {
		#if FT_ESD_PROTECT
			esd_switch(0);
			apk_debug_flag = 1;
		#endif

			ret = fts_read_Touchdata(&pevent);
			if (ret == 0)
			{
				 fts_report_value(&pevent);
#ifdef FTS_SHOW_INTR
				 intr_cnt++;
#endif
			}

		#if FT_ESD_PROTECT
			esd_switch(1);
			apk_debug_flag = 0;
		#endif
	}
#else
	{
			#if FT_ESD_PROTECT
			apk_debug_flag = 1;
			#endif
		
		if (tpd_touchinfo(&cinfo, &pinfo)) {
			if (tpd_dts_data.use_tpd_button) {
				if (cinfo.p[0] == 0)
					memcpy(&finfo, &cinfo, sizeof(struct touch_info));
			}

			if ((cinfo.y[0] >= TPD_RES_Y) && (pinfo.y[0] < TPD_RES_Y)
			&& ((pinfo.p[0] == 0) || (pinfo.p[0] == 2))) {
				TPD_DEBUG("Dummy release --->\n");
				tpd_up(pinfo.x[0], pinfo.y[0]);
				input_sync(tpd->dev);
				continue;
			}
             

			if (cinfo.count > 0) {
				for (i = 0; i < cinfo.count; i++)
					tpd_down(cinfo.x[i], cinfo.y[i], i + 1, cinfo.id[i]);
			} else {
	#ifdef TPD_SOLVE_CHARGING_ISSUE
				tpd_up(1, 48);
	#else
				tpd_up(cinfo.x[0], cinfo.y[0]);
	#endif

			}
			input_sync(tpd->dev);

			#if FT_ESD_PROTECT
				apk_debug_flag = 0;
			#endif
		}
	}
#endif
	} while (!kthread_should_stop());

	I("touch_event_handler exit\n");

	return 0;
}

static int focal_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);

	return 0;
}

static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	D("TPD interrupt has been triggered\n");
	tpd_flag = 1;
	#if FT_ESD_PROTECT
		count_irq ++;
	 #endif

	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}
static int focal_ts_register_interrupt(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = {0,0};

	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		
		of_property_read_u32_array(node,"debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);

		touch_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(touch_irq, tpd_eint_interrupt_handler, IRQF_TRIGGER_FALLING, "TOUCH_PANEL-eint", NULL);
		if (ret > 0)
			E("tpd request_irq IRQ LINE NOT AVAILABLE!.");
	} else {
		E("[%s] tpd request_irq can not find touch eint device node!.", __func__);
	}
	return 0;
}
 
#if MTK_CTP_NODE 
#define CTP_PROC_FILE "tp_info"

static int ctp_proc_read_show (struct seq_file* m, void* data)
{
	char temp[40] = {0};
 
	sprintf(temp, "[Vendor]%s,[Fw]0x%02x,[IC]FT8716\n",vendor_name,ctp_fw_version); 
	seq_printf(m, "%s\n", temp);
	 
	return 0;
}

static int ctp_proc_open (struct inode* inode, struct file* file) 
{
    return single_open(file, ctp_proc_read_show, inode->i_private);
}

static const struct file_operations g_ctp_proc = 
{
    .open = ctp_proc_open,
    .read = seq_read,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
struct ts_fwu{
	const struct firmware *fw;
	u8 *fw_data_start;
	u32 fw_size;
	atomic_t in_flash;	
};
static struct touch_fwu_notifier focal_tp_notifier;
#define FOCAL_FW_FLASH_TIMEOUT	200
#define TOUCH_VENDOR	"FocalTech"

void focal_power_on(struct fts_ts_data *ts)
{
	struct focal_ts_platform_data *pdata = ts->pdata;

	gpio_direction_output(pdata->gpio_vdd, GPIO_HIGH);
	msleep(1);
	gpio_direction_output(pdata->gpio_vee, GPIO_HIGH);
	msleep(5);
	gpio_direction_output(pdata->gpio_1v8, GPIO_HIGH);
}

void focal_power_off(struct fts_ts_data *ts)
{
	struct focal_ts_platform_data *pdata = ts->pdata;

	gpio_direction_input(pdata->gpio_vdd);
	msleep(1);
	gpio_direction_input(pdata->gpio_vee);
	msleep(5);
	gpio_direction_input(pdata->gpio_1v8);
	msleep(1);
}

int facal_touch_fw_update(struct firmware *fw)
{
	int i = 0, taglen = 0;
	int ret = 0;
	u8 tag[32];
#ifdef CTP_VERSION_COMPAT
	u8 buf[16] = {};
#endif
	struct fts_ts_data *ts = ts_data;

	if(atomic_read(&ts_data->suspend_mode))
	{
		
		focal_power_on(ts);
		if(!gpio_get_value(ts_data->pdata->gpio_1v8))
		{
			E("power open failed!\n");
			return -1;
		}
	}

	if(fw->data[0] == 'T' && fw->data[1] == 'P')
	{
		while((tag[i] = fw->data[i]) != '\n')
			i++;

		tag[i] = '\0';
		taglen = i+1;

#ifdef CTP_VERSION_COMPAT
		
		

		switch(ts_data->fw_type){
			case FOCAL_CTP_MAIN:
				sprintf(buf, "MAIN");
				break;
			case FOCAL_CTP_SECOND:
				sprintf(buf, "SECOND");
				break;
			default:
				sprintf(buf, "UNKNOW");
				break;
		}
		I("tag:%s, buf:%s, fw_type: %d\n", tag, buf, ts_data->fw_type);
		if(strstr(tag, buf) == NULL || strstr(tag, TOUCH_VENDOR) == NULL)
		{
			E("Update Bypass, unmatch sensor: %s, current sensor:%s\n", tag, buf);
			goto sensor_unmatch;
		}
#else
		if(strstr(tag, "SECOND") == NULL && strstr(tag, TOUCH_VENDOR) == NULL)
		{
			E("Update Bypass, unmatch sensor: %s, current sensor:%s\n", tag, "SECOND");
			goto sensor_unmatch;
		}
#endif
	}
	else
	{
		E("Can't detect FW header, force update!\n");
	}

	I("[%s|%d]taglen: %d, fw->size:%lu\n", __FILE__, __LINE__, taglen, fw->size);

	I("facal_touch_fw_update start\n");
	disable_irq(touch_irq);
	ret = fts_ctpm_fw_upgrade_with_sys_fs(fts_i2c_client, (unsigned char *)(fw->data + taglen), fw->size - taglen);

	enable_irq(touch_irq);

	msleep(100);
	
	if ((fts_read_reg (ts->client, FTS_REG_FW_VER, &ts->fw_ver)) < 0)
	{
		E("i2c access error!\n");
		return -1;
	}
	I("Upgrade focaltech_fw:0x%0x success\n", ts->fw_ver);
	I("facal_touch_fw_update end\n");
	return ret;

sensor_unmatch:
	return 1;
}

static int register_focal_touch_fw_update(void)
{
	focal_tp_notifier.fwupdate = facal_touch_fw_update;
	snprintf(focal_tp_notifier.fw_vendor, sizeof(focal_tp_notifier.fw_vendor), "%s", TOUCH_VENDOR);
	snprintf(focal_tp_notifier.fw_ver, sizeof(focal_tp_notifier.fw_ver), "%#x",	ts_data->fw_ver);
	return register_fw_update(&focal_tp_notifier);
}

#endif


void tpd_config_gpio(void)
{
	tpd_gpio_as_int(1);
}

#ifdef CONFIG_FOCAL_VIRTUAL_KEYS
static ssize_t focal_virtual_keys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{

    return sprintf(buf,
		__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":200:2000:100:40:"			
		__stringify(EV_KEY) ":" __stringify(KEY_HOME) ":600:2000:100:40:"			
		__stringify(EV_KEY) ":" __stringify(KEY_APPSELECT) ":800:2000:100:40\n");	

}

static struct kobj_attribute focal_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.focal-touchscreen",
		.mode = S_IRUGO,
	},
	.show = &focal_virtual_keys_show,
};

static struct attribute *primotd_attrs[] = {
	&focal_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group primotd_attr_group = {
	.attrs = primotd_attrs,
};

static int focal_register_virtual_key(void)
{
	struct kobject *properties_kobj;
	int rc = 0;
	properties_kobj = kobject_create_and_add ("board_properties", NULL);
	if (properties_kobj)
		rc = sysfs_create_group (properties_kobj, &primotd_attr_group);
	if (!properties_kobj || rc)
	{
		pr_err("failed to create /sys/board_properties\n");
		return false;
	}
	return true;
}
#endif


#define FOCAL_PINCTRL

#ifdef FOCAL_PINCTRL
static struct pinctrl *focal_pinctrl;
static struct pinctrl_state * focal_pin_default;
static int focal_pinctrl_init(struct device *dev)
{
	int ret = 0;

	focal_pinctrl = devm_pinctrl_get(dev);
	if(focal_pinctrl == NULL)
	{
		E("Fail to get pinctrl handler:%d\n", ret);
		return ret;
	}

	focal_pin_default = pinctrl_lookup_state(focal_pinctrl, "ft_ts_pin_default");
	if(IS_ERR_OR_NULL(focal_pin_default))
	{
		E("Can't find ft_ts_pin_default pin!\n");
		ret = PTR_ERR(focal_pin_default);
		focal_pinctrl = NULL;
		return ret;
	}

	ret = pinctrl_select_state(focal_pinctrl, focal_pin_default);
	if(ret < 0)
	{
		E("Fail select pins state!\n");
		return ret;
	}

	I("pinctrl init success:%d\n", ret);
	return ret;
}

static void focal_pinctrl_deinit(void)
{
	if(focal_pinctrl)
	{
		focal_pin_default = NULL;
		devm_pinctrl_put(focal_pinctrl);
	}
}

#endif


int focal_rst_set(int level)
{
	int ret = 0;
	if(level)
	{
		gpio_set_value(ts_data->pdata->gpio_reset, GPIO_HIGH);
	}
	else
	{
		gpio_set_value(ts_data->pdata->gpio_reset, GPIO_LOW);
	}
	return ret;
}


void focal_reset_off(struct fts_ts_data *ts)
{
	struct focal_ts_platform_data *pdata = ts->pdata;
	gpio_direction_input(pdata->gpio_reset);
}

void focal_reset_on(struct fts_ts_data *ts)
{
	struct focal_ts_platform_data *pdata = ts->pdata;

	gpio_direction_output(pdata->gpio_reset, GPIO_HIGH);
}

void TP_SET_RESET(int val)
{
	struct fts_ts_data *ts = ts_data;
	if(val)
		focal_reset_on(ts);
	else
		focal_reset_off(ts);
}
EXPORT_SYMBOL(TP_SET_RESET);
#ifdef CONFIG_SYNC_TOUCH_STATUS
int focal_i2c_sel_set(int value)
{
	int ret = 0;
	if(value)
	{
		gpio_set_value(ts_data->pdata->gpio_i2c_sel, GPIO_HIGH);
		I("[SensorHub] Switch touch i2c to CPU\n");
	}
	else
	{
		gpio_set_value(ts_data->pdata->gpio_i2c_sel, GPIO_LOW);
		I("[SensorHub] Switch touch i2c to MCU\n");
	}
	return ret;
}
EXPORT_SYMBOL(focal_i2c_sel_set);
#endif




static int focal_gpio_init(struct fts_ts_data *ts)
{
	int ret;
	struct focal_ts_platform_data *pdata = ts->pdata;

	I("[GPIO] irq:%d, reset:%d, i2c_sel:%d\n", pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_i2c_sel);
	if(!pdata)
	{
		E("pdata is NULL!\n");
		return -1;
	}

	ret = gpio_request(pdata->gpio_irq, "tp_irq");
	if(ret < 0)
	{
		E("Fail to request gpio%d!\n", pdata->gpio_irq);
		ret = -ENODEV;
	}
	gpio_direction_input(pdata->gpio_irq);
	ret = gpio_request(pdata->gpio_reset, "tp_reset");
	if(ret < 0)
	{
		E("Fail to request gpio%d!\n", pdata->gpio_reset);
		ret = -ENODEV;
	}
	gpio_direction_output(pdata->gpio_reset, GPIO_HIGH);

#ifdef	CONFIG_SYNC_TOUCH_STATUS
	ret = gpio_request(pdata->gpio_i2c_sel, "tp_i2c_sel");
	if(ret < 0)
	{
		E("Fail to request gpio%d!\n", pdata->gpio_i2c_sel);
		ret = -ENODEV;
	}
	gpio_direction_output(pdata->gpio_i2c_sel, GPIO_HIGH);
#endif





	return 0;
}

static int focal_parse_dt(struct fts_ts_data *ts, struct focal_ts_platform_data *pdata)
{
	int ret = 0;
	int coords_size = 0;
	u32 coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->client->dev.of_node;


	prop = of_find_property(dt, "focal,panel-coords", NULL);
	if(prop)
	{
		coords_size = prop->length / sizeof(u32);
		if(coords_size != 4)
		{
			E("invalid panel coords size : %d\n", coords_size);
			ret = -1;
		}
	}
	if(of_property_read_u32_array(dt, "focal,panel-coords", coords, coords_size) == 0)
	{
		pdata->abs_x_min = coords[0];
		pdata->abs_x_max = coords[1];
		pdata->abs_y_min = coords[2];
		pdata->abs_y_max = coords[3];
		I("DT:panel-coords = %d, %d, %d, %d\n", pdata->abs_x_min, pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "focal,display-coords", NULL);
	if(prop)
	{
		coords_size = prop->length / sizeof(u32);
		if(coords_size != 4)
			E("invalid panel coords size : %d\n", coords_size);
	}
	if(of_property_read_u32_array(dt, "focal,display-coords", coords, coords_size) == 0)
	{
		pdata->screenHeight = coords[1];
		pdata->screenWidth = coords[3];
		I("DT:display screenWidth = %d, screenWidth = %d\n", pdata->screenHeight, pdata->screenWidth);
	}

	prop = of_find_property(dt, "focal,irq-gpio", NULL);
	if(prop)
	{
		of_property_read_u32(dt, "focal,irq-gpio", &pdata->gpio_irq);
		if(!gpio_is_valid(pdata->gpio_irq))
		{
			E("DT: gpio_irq is not valid!\n");
			ret = -1;
		}
	}

	prop = of_find_property(dt, "focal,rst-gpio", NULL);
	if(prop)
	{
		of_property_read_u32(dt, "focal,rst-gpio", &pdata->gpio_reset);
		if(!gpio_is_valid(pdata->gpio_reset))
		{
			E("DT:gpio_reset is not valid!\n");
			ret = -1;
		}
	}
#ifdef CONFIG_SYNC_TOUCH_STATUS
	prop = of_find_property(dt, "focal,i2c-sel-gpio", NULL);
	if(prop)
	{
		of_property_read_u32(dt, "focal,i2c-sel-gpio", &pdata->gpio_i2c_sel);
		if(!gpio_is_valid(pdata->gpio_i2c_sel))
		{
			E("DT:gpio_reset is not valid!\n");
			ret = -1;
		}
	}
#endif
	prop = of_find_property(dt, "focal,1v8-gpio", NULL);
	if(prop)
	{
		of_property_read_u32(dt, "focal,1v8-gpio", &pdata->gpio_1v8);
		if(!gpio_is_valid(pdata->gpio_1v8))
		{
			E("DT:gpio_1v8 is not valid!\n");
			ret = -1;
		}
	}

	prop = of_find_property(dt, "focal,vdd-gpio", NULL);
	if(prop)
	{
		of_property_read_u32(dt, "focal,vdd-gpio", &pdata->gpio_vdd);
		if(!gpio_is_valid(pdata->gpio_vdd))
		{
			E("DT:gpio_vdd is not valid!\n");
			ret = -1;
		}
	}

	prop = of_find_property(dt, "focal,vee-gpio", NULL);
	if(prop)
	{
		of_property_read_u32(dt, "focal,vee-gpio", &pdata->gpio_vee);
		if(!gpio_is_valid(pdata->gpio_vee))
		{
			E("DT:gpio_vee is not valid!\n");
			ret = -1;
		}
	}

	I("DT: irq-gpio:%d, reset-gpio:%d, i2c-sel-gpio:%d, 1v8-gpio:%d, vdd-gpio:%d, vee-gpio:%d\n",
			pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_i2c_sel, pdata->gpio_1v8, pdata->gpio_vdd, pdata->gpio_vee);

	return ret;
}

#define FOCAL_INPUT
#ifdef FOCAL_INPUT
static int focal_input_register(struct tpd_device *tpd)
{
#if 0
	int ret = -1;

	tpd->dev = input_allocate_device();
	if(tpd->dev->name == NULL)
	{
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}
#endif
	tpd->dev->name = "focal-touchscreen";

	set_bit(EV_SYN, tpd->dev->evbit);
	set_bit(EV_ABS, tpd->dev->evbit);
	set_bit(EV_KEY, tpd->dev->evbit);

	set_bit(KEY_BACK, tpd->dev->keybit);
	set_bit(KEY_HOME, tpd->dev->keybit);
	set_bit(KEY_MENU, tpd->dev->keybit);
	set_bit(KEY_APP_SWITCH, tpd->dev->keybit);

#ifdef MT_PROTOCOL_B
	#if (1)
		input_mt_init_slots(tpd->dev, MT_MAX_TOUCH_POINTS,2);
	#endif
	input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MAJOR,0, 255, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif
	return 0;
	
}
#endif
static int focal_read_chip_id(struct fts_ts_data *ts, u8 *chip_id)
{
	int ret = -1;
	u8 fw_ver = 0;
	
	if ((ret = fts_read_reg (ts->client, FTS_REG_CHIP_ID_LOW, &chip_id[0])) < 0)
	{
		E("[%s]i2c access error!\n",__func__);
		return ret;
	}

	if ((ret = fts_read_reg (ts->client, FTS_REG_CHIP_ID_HIGH, &chip_id[1])) < 0)
	{
		E("[%s]i2c access error!\n", __func__);
		return ret;
	}

	
	if ((ret = fts_read_reg (ts->client, FTS_REG_FW_VER, &fw_ver)) < 0)
	{
		E("i2c access error!\n");
		return ret;
	}
	I("focal_read_chip_id: %x%x_0x%x\n", chip_id[1], chip_id[0], fw_ver);

	return 0;
}
static int focal_read_chip_info(struct fts_ts_data *ts)
{
	int ret = 0;

	
	if ((ret = fts_read_reg (ts->client, FTS_REG_CHIP_ID_LOW, &(ts->fw_chip_id[0]))) < 0)
	{
		E("[%s]i2c access error!\n",__func__);
		return ret;
	}

	if ((ret = fts_read_reg (ts->client, FTS_REG_CHIP_ID_HIGH, &(ts->fw_chip_id[1]))) < 0)
	{
		E("[%s]i2c access error!\n", __func__);
		return ret;
	}
	I("TP FocalTech chip id: %x%x\n", ts->fw_chip_id[1], ts->fw_chip_id[0]);


	
	if ((ret = fts_read_reg (ts->client, FTS_REG_FW_VER, &ts->fw_ver)) < 0)
	{
		E("i2c access error!\n");
		return ret;
	}
	I(" fts_read_reg firmware version : %x\n", ts->fw_ver);

	
	if((ret = fts_read_reg (ts->client, FTS_REG_VENDOR_ID, &ts->fw_vendor_id)) < 0)
	{
		E("i2c access error!\n");
		return ret;
	}
	if (ts->fw_vendor_id == 0x8d)
	{
		vendor_name = "tianma";
	}
	else
	{
		vendor_name = "Rserve";
	}
	I("vendor_name=%s, fw version=%#x\n",vendor_name, ts->fw_ver);

	return 0;
}
#ifdef FOCAL_INT_MODE_DEBUG
static int focal_interrupt_mode_switch(struct i2c_client *client, u8 mode)
{
	int ret = -1;
	u8 val = 0x2;

	ret = fts_write_reg(client, 0xfd, mode);
	if(ret < 0)
	{
		E("0x%x: i2c write fail with error:%d\n", 0xfd, ret);
		return ret;
	}

	msleep(2);

	if(mode == 1)
	{
		ret = fts_read_reg(client, 0xfd, &val);
		if(ret < 0)
		{
			E("focal_suspend: 0x%x: i2c read fail with error: %d\n", 0xfd, ret);
		}
		else
		{
			I("focal_suspend: 0x%x: i2c read value: %d\n", 0xfd, val);
		}
	}
	else
	{
		ret = fts_read_reg(client, 0xfd, &val);
		if(ret < 0)
		{
			E("focal_resume: 0x%x: i2c read fail with error: %d\n", 0xfd, ret);
		}
		else
		{
			I("focal_resume: 0x%x: i2c read value: %d\n", 0xfd, val);
		}
	}
	return ret;
}
#endif

static int focal_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	
	int reset_count = 0;
	char data;
	struct focal_ts_platform_data *pdata;

	i2c_client = client;
	fts_i2c_client = client;

	I("focal_probe start\n");
	msleep (100);
	ts_data = (struct fts_ts_data *)devm_kzalloc(&client->dev, sizeof(struct fts_ts_data), GFP_KERNEL);
	if(!ts_data)
	{
		ret = -ENOMEM;
		E("Failed allocate ts_data memory!\n");
		goto ts_data_alloc_error;
	}
	i2c_set_clientdata(client, ts_data);
	ts_data->client = client;
	pdata = client->dev.platform_data;
	fts_input_dev=tpd->dev;


	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto fail_check_i2c;
	}

	if(client->dev.of_node)
	{
		pdata = devm_kzalloc(&client->dev, sizeof(struct focal_ts_platform_data), GFP_KERNEL);
		if(!pdata)
		{
			ret = -ENOMEM;
			E("Failed allocate ts_data memory!\n");
			goto pdata_alloc_error;
		}
		ret = focal_parse_dt(ts_data, pdata);
		if(ret < 0)
		{
			E("DT: Can't get dts data!\n");
			goto parse_dt_error;
		}
		ts_data->pdata = pdata;
	}

	if(i2c_client->addr != 0x38)
	{
		i2c_client->addr = 0x38;
		E("i2c_client->addr = 0x%x\n",i2c_client->addr);
	}

#ifdef CTP_VERSION_COMPAT
	ts_data->hw_id = of_machine_hwid();
	if(ts_data->hw_id == 0xff)
	{
		ts_data->fw_type = FOCAL_CTP_MAIN;
	}
	else
	{
		ts_data->fw_type = FOCAL_CTP_SECOND;
	}
#endif

	
	of_get_ft5x0x_platform_data(&client->dev);

	ret = focal_pinctrl_init(&client->dev);
	if(ret < 0)
	{
		E("focal_pinctrl_init failed!\n");
		goto pin_config_fail;
	}

	ret = focal_gpio_init(ts_data);
	if(ret < 0)
	{
		E("focal_gpio_init failed!\n");
		goto pin_config_fail;
	}

	
	ts_data->debug_level |= BIT(3);
	I("%s: Enable touch down/up debug log since not security-on device", __func__);

	
#ifdef CONFIG_FOCAL_VIRTUAL_KEYS
	focal_register_virtual_key();
#endif

    #if 0
	ret = regulator_enable(tpd->reg);
	if (ret != 0)
		E("Failed to enable reg-vgp6: %d\n", ret);
    #endif 
#ifdef OLD_API
	
	tpd_gpio_as_int(1);
#endif

	msg_dma_alloct();

    #ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT

    if (NULL == tpd_i2c_dma_va)
    {
        tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
        tpd_i2c_dma_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev, 250, &tpd_i2c_dma_pa, GFP_KERNEL);
    }
    if (!tpd_i2c_dma_va)
		TPD_DMESG("TPD dma_alloc_coherent error!\n");
	else
		TPD_DMESG("TPD dma_alloc_coherent success!\n");
    #endif

#if FTS_GESTRUE_EN
	fts_Gesture_init(tpd->dev);
#endif

#ifdef FOCAL_INPUT
	ret = focal_input_register(tpd);
	if(ret < 0)
	{
		E("focal_input_register failed!\n");
		input_free_device(ts_data->input_dev);
		return ret;
	}
#endif

	do{
		
		focal_rst_set(0);
		msleep (20);
		focal_rst_set(1);
		msleep (400);

		ret = fts_read_reg(i2c_client, 0x00, &data);
		if(ret < 0)
		{
			E("I2C transfer error, count: %d, line: %d\n", reset_count, __LINE__);
			msleep (800);
			reset_count ++;
		}
		else
		{
			D("I2C transfer success, count: %d, line: %d\n", reset_count, __LINE__);
		}
	}while((ret < 0 || data != 0) && reset_count < TPD_MAX_RESET_COUNT);
	if((ret < 0 || data != 0) && reset_count >= TPD_MAX_RESET_COUNT)
	{
		reset_count = 0;
		E("Fail to reset TP!\n");
		goto reset_fail;
	}
	else if(ret == 0 && data == 0)
	{
		D("Reset TP successfully!\n");
	}

	focal_read_chip_info(ts_data);

	tpd_load_status = 1;

    
	#ifdef  MTK_CTP_NODE
    if((proc_create(CTP_PROC_FILE,0444,NULL,&g_ctp_proc))==NULL)
    {
      E("proc_create tp vertion node error\n");
	}
	#endif

	#ifdef  MTK_CTP_NODE
	spin_lock_init(&gesture_lock);
	create_ctp_node();
    #endif

	#ifdef SYSFS_DEBUG
    fts_create_sysfs(fts_i2c_client);
	#endif
    focal_touch_sysfs_init();

	fts_get_upgrade_array();
	
	#ifdef FTS_CTL_IIC
		 if (fts_rw_iic_drv_init(fts_i2c_client) < 0)
			 E("create fts control iic driver failed\n");
	#endif

	#ifdef FTS_APK_DEBUG
		fts_create_apk_debug_channel(fts_i2c_client);
	#endif

	
	#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
		register_focal_touch_fw_update();
	#endif
 
	#ifdef TPD_AUTO_UPGRADE
		printk("********************Enter CTP Auto Upgrade********************\n");
		is_update = true; 
		disable_irq(touch_irq);
		fts_ctpm_auto_upgrade(fts_i2c_client);
		enable_irq(touch_irq);
		is_update = false;
	#endif

 
#ifdef DEBUG
	
	report_rate = 0x8;
	if ((fts_write_reg(i2c_client, 0x88, report_rate)) < 0) {
		if ((fts_write_reg(i2c_client, 0x88, report_rate)) < 0)
			TPD_DMESG("I2C write report rate error, line: %d\n", __LINE__);
	}
#endif

	
    
    #if FT_ESD_PROTECT
	 fts_esd_protection_init();
    #endif
	thread_tpd = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread_tpd)) {
		ret = PTR_ERR(thread_tpd);
		I("failed to create kernel thread_tpd: %d\n", ret);
	}

	I("Touch Panel Device Probe %s\n", (ret < TPD_OK) ? "FAIL" : "PASS");

#ifdef TIMER_DEBUG
	init_test_timer();
#endif
	{
		u8 mode = -1;
		fts_read_reg (client, FTS_REG_PMODE, &mode);
		I("focal read ID_G_PMODE: %d.\n", mode);

		fts_read_reg (client, FTS_REG_FW_VER, &ver);
		I(" fts_read_reg version : %d\n", ver);

		fts_read_reg (client, FTS_REG_VENDOR_ID, &ver);
		if (ver == 0x8d)
		{
			vendor_name = "tianma";
		}
		else
		{
			vendor_name = "Rserve";
		}
		I("vendor_name=%s fw version=%x\n", vendor_name, ver);
#if 0
		ret = fts_read_reg(client, 0xfd, &mode);
		if(ret < 0)
		{
			E("focal_suspend: 0x%x: i2c read fail with error: %d\n", 0xfd, ret);
		}
		else
		{
			I("focal_suspend: 0x%x: i2c read value: %d\n", 0xfd, val);
		}
		focal_interrupt_mode_switch(client, 1);
		msleep(2);
		focal_interrupt_mode_switch(client, 0);
		msleep(2);
#endif
	}
	
	focal_ts_register_interrupt();
   
#ifdef CONFIG_MTK_SH_SUPPORT
	int ret;

	ret = get_md32_semaphore(SEMAPHORE_TOUCH);
	if (ret < 0)
		pr_err("[TOUCH] HW semaphore reqiure timeout\n");
#endif

#ifdef FTS_SHOW_INTR
    enable_fw_debug();
#endif
	I("focal probe finished, ret: %d\n", ret);
	return 0;

reset_fail:
	msg_dma_release();

pin_config_fail:
	focal_pinctrl_deinit();

parse_dt_error:
	devm_kfree(&client->dev, pdata);
pdata_alloc_error:
	devm_kfree(&client->dev, ts_data);

ts_data_alloc_error:
	return ret;

fail_check_i2c:
	return ret;
}

static int focal_remove(struct i2c_client *client)
{
	D("focal_remove!\n");
#ifdef FTS_CTL_IIC
     		fts_rw_iic_drv_exit();
     #endif
     #ifdef SYSFS_DEBUG
     		fts_remove_sysfs(client);
     #endif  
     focal_touch_sysfs_deinit();
	#if FT_ESD_PROTECT
		
	#endif


     #ifdef FTS_APK_DEBUG
     		fts_release_apk_debug_channel();
     #endif
#ifdef CONFIG_FT_AUTO_UPGRADE_SUPPORT
	if (tpd_i2c_dma_va) {
		dma_free_coherent(NULL, 4096, tpd_i2c_dma_va, tpd_i2c_dma_pa);
		tpd_i2c_dma_va = NULL;
		tpd_i2c_dma_pa = 0;
	}
#endif
	devm_kfree(&client->dev, ts_data->pdata);
	devm_kfree(&client->dev, ts_data);
	gpio_free(tpd_rst_gpio_number);
	gpio_free(tpd_int_gpio_number);

	return 0;
}
void esd_switch(s32 on)
{
	return ;
}

static int focal_local_init(void)
{
	

	I("Focaltech FT8716 I2C Touchscreen Driver...\n");
	#if 0 
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);
	if (retval != 0) {
		TPD_DMESG("Failed to set reg-vgp6 voltage: %d\n", retval);
		return -1;
	}
    #endif
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		I("unable to add i2c driver.\n");
		return -1;
	}

     
#if 0
	if(tpd_dts_data.use_tpd_button){
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
		tpd_dts_data.tpd_key_dim_local);
	}
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))

	memcpy(tpd_calmat, tpd_def_calmat_local_factory, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local_factory, 8 * 4);

	memcpy(tpd_calmat, tpd_def_calmat_local_normal, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local_normal, 8 * 4);

#endif

	I("end %s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

static void fts_release_all_finger ( void )
{
	unsigned int finger_count=0;

#ifndef MT_PROTOCOL_B
	input_mt_sync ( tpd->dev );
#else
	for(finger_count = 0; finger_count < MT_MAX_TOUCH_POINTS; finger_count++)
	{
		input_mt_slot( tpd->dev, finger_count);
		input_mt_report_slot_state( tpd->dev, MT_TOOL_FINGER, false);
	}
	 input_report_key(tpd->dev, BTN_TOUCH, 0);
#endif
	input_sync ( tpd->dev );

}


#ifdef CONFIG_MTK_SH_SUPPORT
static s8 ftp_enter_doze(struct i2c_client *client)
{
	s8 ret = -1;
	s8 retry = 0;
	char gestrue_on = 0x01;
	char gestrue_data;
	int i;

	
	pr_alert("Entering doze mode...");

	
	ret = fts_write_reg(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, gestrue_on);
	if (ret < 0) {
		
		pr_alert("Failed to enter Doze %d", retry);
		return ret;
	}
	msleep(30);

	for (i = 0; i < 10; i++) {
		fts_read_reg(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, &gestrue_data);
		if (gestrue_data == 0x01) {
			doze_status = DOZE_ENABLED;
			
			pr_alert("FTP has been working in doze mode!");
			break;
		}
		msleep(20);
		fts_write_reg(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, gestrue_on);

	}

	return ret;
}
#endif

#ifdef CONFIG_SYNC_TOUCH_STATUS
static void switch_sensor_hub(struct fts_ts_data *ts, u8 mode)
{
	if(mode == 0)	
	{
#ifdef FOCAL_INT_MODE_DEBUG
		
		focal_interrupt_mode_switch(ts_data->client, 1);
#endif
		
		focal_i2c_sel_set(mode);
		ts->i2c_to_mcu = 1;


	}
	else if(mode == 1)	
	{
#ifdef FOCAL_INT_MODE_DEBUG
		
		focal_interrupt_mode_switch(ts_data->client, 0);
#endif
		
		focal_i2c_sel_set(mode);
		ts->i2c_to_mcu = 0;


	}
}
#endif

static void focal_resume(struct device *dev)
{
	struct fts_ts_data *ts;
	int ret = 0, count = 0;
	u8 chip_id[2] = {0};

	I("focal_resume start\n");
#if 0
	retval = regulator_enable(tpd->reg);
	if (retval != 0)
	TPD_DMESG("Failed to enable reg-vgp6: %d\n", retval);
#endif

	
	ts = ts_data;
	


#ifdef CONFIG_SYNC_TOUCH_STATUS
	
#endif
#ifdef CC_DEBUG
	while(count < 30)
	{
		msleep(20);
		if ((ret = focal_read_chip_id (ts, chip_id)) < 0)
		{
			E("focal_read_chip_id error: %d\n", ret);
		}
		if(chip_id[1] != 0x87 || chip_id[0] != 0x16 )
		{
			E("[%s] focal_read_chip_id error, count: %d\n", __func__, count);
			count ++;
		}
		else
		{
			I("TP FocalTech chip id: %x%x, count:%d\n", chip_id[1], chip_id[0], count);
			break;
		}
	}



#endif

#if FTS_GESTRUE_EN
	fts_write_reg(fts_i2c_client,0xD0,0x00);
#endif

#ifdef FTS_SHOW_INTR
    intr_cnt = 0;
    enable_fw_debug();
#endif

#ifdef CONFIG_MTK_SH_SUPPORT
	int ret;

	if (tpd_scp_doze_en)
	{
		ret = get_md32_semaphore(SEMAPHORE_TOUCH);
		if (ret < 0)
		{
			TPD_DEBUG("[TOUCH] HW semaphore reqiure timeout\n");
		}
		else
		{
			Touch_IPI_Packet ipi_pkt =
			{	.cmd = IPI_COMMAND_AS_ENABLE_GESTURE, .param.data = 0};

			md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0);
		}
	}
#endif

#ifdef CONFIG_MTK_SH_SUPPORT
	doze_status = DOZE_DISABLED;
	
	int data;

	data = 0x00;

	fts_write_reg(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, data);
#else
	fts_esd_protection_resume();
	enable_irq (touch_irq);
	D("focal_resume: enable ctp interrupt");
#endif
#ifdef CONFIG_SYNC_TOUCH_STATUS
	touch_status(0);
#endif
	atomic_set(&ts->suspend_mode, 0);
	ts->suspended = false;
	fts_release_all_finger ();

	I("focal_resume end\n");
#if USB_CHARGE_DETECT
	if(BMT_status.charger_exist != 0)
	{
		charging_flag = 0;
	}
	else
	{
		charging_flag=1;
	}
#endif
#if FT_ESD_PROTECT
	count_irq = 0;
	fts_esd_protection_resume();
#endif
}

#ifdef CONFIG_MTK_SH_SUPPORT
void tpd_scp_wakeup_enable(bool en)
{
	tpd_scp_doze_en = en;
}

void tpd_enter_doze(void)
{

}
#endif


static void focal_suspend(struct device *dev)
{
	
#if FTS_GESTRUE_EN
	int i = 0;
	u8 state = 0;
#endif
	
	struct fts_ts_data *ts;

	I("focal_suspend\n");

	
	ts = ts_data;
	if(!ts)
	{
		E("Fail to get fts_ts_data!\n");
		return ;
	}
	focal_reset_off(ts);

	if(ts->suspended)
	{
		I("TP already suspended, skip!\n");
	}
	else
	{
		ts->suspended = true;
		I("TP suspend!\n");
	}
#if FTS_GESTRUE_EN
	if(gesture_switch)
	{
		
		

		fts_write_reg(i2c_client, 0xd0, 0x01);
		fts_write_reg(i2c_client, 0xd1, 0xff);
		fts_write_reg(i2c_client, 0xd2, 0xff);
		fts_write_reg(i2c_client, 0xd5, 0xff);
		fts_write_reg(i2c_client, 0xd6, 0xff);
		fts_write_reg(i2c_client, 0xd7, 0xff);
		fts_write_reg(i2c_client, 0xd8, 0xff);

		msleep(10);

		for(i = 0; i < 10; i++)
		{
			fts_read_reg(i2c_client, 0xd0, &state);
			if(state == 1)
			{
				D("TP gesture write 0x01\n");
				return;
			}
			else
			{
				fts_write_reg(i2c_client, 0xd0, 0x01);
				fts_write_reg(i2c_client, 0xd1, 0xff);
				fts_write_reg(i2c_client, 0xd2, 0xff);
				fts_write_reg(i2c_client, 0xd5, 0xff);
				fts_write_reg(i2c_client, 0xd6, 0xff);
				fts_write_reg(i2c_client, 0xd7, 0xff);
				fts_write_reg(i2c_client, 0xd8, 0xff);
				msleep(10);
			}
		}

		if(i >= 9)
		{
			TPD_DMESG("TPD gesture write 0x01 to d0 fail \n");
			return;
		}
	}
	else
	{
#if FT_ESD_PROTECT
		fts_esd_protection_suspend();
#endif
		power_switch_gesture = 0;
	}
#endif


#ifdef CONFIG_MTK_SH_SUPPORT
	int sem_ret;

	tpd_enter_doze();

	int ret;
	char gestrue_data;
	char gestrue_cmd = 0x03;
	static int scp_init_flag;

	

	mutex_lock(&i2c_access);

	sem_ret = release_md32_semaphore(SEMAPHORE_TOUCH);

	if (scp_init_flag == 0)
	{
		struct Touch_IPI_Packet ipi_pkt;

		ipi_pkt.cmd = IPI_COMMAND_AS_CUST_PARAMETER;
		ipi_pkt.param.tcs.i2c_num = TPD_I2C_NUMBER;
		ipi_pkt.param.tcs.int_num = CUST_EINT_TOUCH_NUM;
		ipi_pkt.param.tcs.io_int = tpd_int_gpio_number;
		ipi_pkt.param.tcs.io_rst = tpd_rst_gpio_number;

		TPD_DEBUG("[TOUCH]SEND CUST command :%d ", IPI_COMMAND_AS_CUST_PARAMETER);

		ret = md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 0);
		if (ret < 0)
		TPD_DEBUG(" IPI cmd failed (%d)\n", ipi_pkt.cmd);

		msleep(20); 
		
		
	}

	if (tpd_scp_doze_en)
	{
		TPD_DEBUG("[TOUCH]SEND ENABLE GES command :%d ", IPI_COMMAND_AS_ENABLE_GESTURE);
		ret = ftp_enter_doze(i2c_client);
		if (ret < 0)
		{
			TPD_DEBUG("FTP Enter Doze mode failed\n");
		}
		else
		{
			int retry = 5;
			{
				
				fts_read_reg(i2c_client, FT_GESTRUE_MODE_SWITCH_REG, &gestrue_data);
				TPD_DEBUG("========================>0x%x", gestrue_data);
			}

			msleep(20);
			Touch_IPI_Packet ipi_pkt =
			{	.cmd = IPI_COMMAND_AS_ENABLE_GESTURE, .param.data = 1};

			do
			{
				if (md32_ipi_send(IPI_TOUCH, &ipi_pkt, sizeof(ipi_pkt), 1) == DONE)
				break;
				msleep(20);
				TPD_DEBUG("==>retry=%d", retry);
			}while (retry--);

			if (retry <= 0)
			TPD_DEBUG("############# md32_ipi_send failed retry=%d", retry);


		}
		
	}

	mutex_unlock(&i2c_access);
#endif

	disable_irq (touch_irq);
	I("focal_suspend: disable ctp interrupt\n");
#if 0
	ret = fts_write_reg (i2c_client, 0xA5, data); 
	if (ret < 0)
		E("focal_suspend: i2c access error!\n");
	else
		I("focal_suspend i2c access success!\n");
#endif
#ifdef CONFIG_SYNC_TOUCH_STATUS
	switch_sensor_hub(ts, 0);
	touch_status(1);
	
	
#endif

	atomic_set(&ts->suspend_mode, 1);
#if 0
	retval = regulator_disable(tpd->reg);
	if (retval != 0)
	TPD_DMESG("Failed to disable reg-vgp6: %d\n", retval);
#endif
	fts_esd_protection_suspend();
	fts_release_all_finger ();
}

static struct tpd_driver_t focal_tpd_device_driver = {
	.tpd_device_name = "FT8716",
	.tpd_local_init = focal_local_init,
	.suspend = focal_suspend,
	.resume = focal_resume,
	.attrs = {
		.attr = ft8716_attrs,
		.num  = ARRAY_SIZE(ft8716_attrs),
	},
};

static int __init focal_tpd_driver_init(void)
{
	I("MediaTek FT8716 touch panel driver init\n");
	tpd_get_dts_info();
	if (tpd_driver_add(&focal_tpd_device_driver) < 0)
		TPD_DMESG("add FT8716 driver failed\n");

	return 0;
}

static void __exit focal_tpd_driver_exit(void)
{
	I("MediaTek FT8716 touch panel driver exit\n");
	tpd_driver_remove(&focal_tpd_device_driver);
}

module_init(focal_tpd_driver_init);
module_exit(focal_tpd_driver_exit);

