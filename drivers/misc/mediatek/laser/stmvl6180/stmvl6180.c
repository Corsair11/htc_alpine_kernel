/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense Time-of-Flight sensor
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/fs.h>
#include <asm/uaccess.h>

#ifdef CONFIG_TI_TCA6418
#include <linux/tca6418_ioexpander.h>
#endif

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "stmvl6180.h"

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/device.h>

#ifndef ABS
#define ABS(x)              (((x) > 0) ? (x) : (-(x)))
#endif

#define VL6180_OFFSET_CALIB	0x02
#define VL6180_XTALK_CALIB		0x03

#define VL6180_MAGIC 'A'

struct laser_power_flag{
        int laser_power_state;
        int keep_power_alive;
};

#define VL6180_IOCTL_INIT                       _IO(VL6180_MAGIC,       0x01)
#define VL6180_IOCTL_GETOFFCALB                 _IOR(VL6180_MAGIC,      VL6180_OFFSET_CALIB, int)
#define VL6180_IOCTL_GETXTALKCALB	        _IOR(VL6180_MAGIC, 	VL6180_XTALK_CALIB, int)
#define VL6180_IOCTL_SETOFFCALB                 _IOW(VL6180_MAGIC, 	0x04, int)
#define VL6180_IOCTL_SETXTALKCALB               _IOW(VL6180_MAGIC, 	0x05, int)
#define VL6180_IOCTL_GET_LASER_POWERSTATE       _IOR(VL6180_MAGIC, 	0x06, struct laser_power_flag) 
#define VL6180_IOCTL_SET_POWERALIVE_FLAG        _IOR(VL6180_MAGIC, 	0x07, int) 
#define VL6180_IOCTL_GET_ST_OFFSET_VALUE        _IOR(VL6180_MAGIC, 	0x08, int) 
#define VL6180_IOCTL_GETDATA                    _IOR(VL6180_MAGIC, 	0x0a, int)
#define VL6180_IOCTL_GETALLDATA        _IOR(VL6180_MAGIC, 0x0b, VL6180x_RangeData_t)
#define VL6180_IOCTL_SETPOWER                 _IOW(VL6180_MAGIC, 0x0c, int)

#define LASER_I2C_BUSNUM 0

#define LASER_DRVNAME "vl6180"
#define I2C_SLAVE_ADDRESS        0x52
#define I2C_REGISTER_ID            0x52
#define LASER_DRIVER_CLASS_NAME "htc_laser"
#define LASER_SYS_NAME "laser"

#ifdef AF_DEBUG
#define LOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO,    LASER_DRVNAME, "[%s] " format, __FUNCTION__, ##args)
#else
#define LOG_INF(format, args...)
#endif


#define CALIBRATE_ERROR_NUMBER 0x7FFFFFFF
#define HTC_TRY_TIMES 10
#define LASER_VL6180_SENSOR_ID 0xb4

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
#define OFFSET_FILE "/data/offset.txt"
#define XTALK_FILE "/data/xtalk.txt"
#endif

static struct laser_power_flag stmvl6180_power_flag={0,0};
static spinlock_t g_Laser_SpinLock;

static struct i2c_client * g_pstLaser_I2Cclient = NULL;

static dev_t g_Laser_devno;
static struct cdev * g_pLaser_CharDrv = NULL;
static struct class *actuator_class = NULL;
static struct device* laser_device = NULL;

static int g_s4Laser_Opened = 0;
static int g_Laser_OffsetCalib = 0xFFFFFFFF;
static int g_Laser_XTalkCalib = 0xFFFFFFFF;
static int laser_cali_status_flag = 0;
static int is_device_first_open = 0;
static int manufacture_offset_value = 0;

static int poweron_sensor(void);
static int powerdown_sensor(void);

stmvl6180x_dev vl6180x_dev;

unsigned int g_laser_en;
unsigned int g_laser_irq;

static int laser_gpio_init(struct platform_device *pdev)
{
   
	struct pinctrl *laserctrl = NULL;
	struct pinctrl_state *laser_init_cfg = NULL;
	struct device_node *node1 = pdev->dev.of_node;

    printk("%s: +++ \n", __func__);

    laserctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(laserctrl)) {
        printk("%s: laserctrl is error \n", __func__);
        return -1;
    }
   laser_init_cfg = pinctrl_lookup_state(laserctrl, "laser_init_cfg");
   if(IS_ERR(laser_init_cfg)) 
	   printk("%s: laser_init_cfg error \n", __func__);
   else {
        pinctrl_select_state(laserctrl, laser_init_cfg);
		if(node1){
			if(of_property_read_u32(node1, "laser_en", &g_laser_en) == 0) {
				if(gpio_request(g_laser_en, "laser_en") < 0)
						printk("gpio %d request failed\n", g_laser_en);
			}
			if(of_property_read_u32(node1, "laser_irq", &g_laser_irq) == 0) {
				if(gpio_request(g_laser_irq, "laser_irq") < 0)
						printk("gpio %d request failed\n", g_laser_irq);
			}
		}
   }

    return 0;
}



int s4VL6180_ReadRegByte(u16 addr, u8 *data)
{
        u8 pu_send_cmd[2] = {(u8)(addr >> 8) & 0xFF, (u8)(addr & 0xFF) };

        g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
        
        if (i2c_master_send(g_pstLaser_I2Cclient, pu_send_cmd, 2) < 0)
        {
                pr_err("stmvl6180:I2C send addr failed!! \n");
                return -1;
        }

        if (i2c_master_recv(g_pstLaser_I2Cclient, data , 1) < 0)
        {
                pr_err("stmvl6180:I2C read data failed!! \n");
                return -1;
        }

        LOG_INF("I2C read addr 0x%x data 0x%x\n", addr,*data );

        return 0;
}

int s4VL6180_WriteRegByte(u16 addr, u8 data)
{
        u8 puSendCmd[3] = {	(u8)((addr >>  8)&0xFF), 
                (u8)( addr       &0xFF),
                (u8)( data       &0xFF)};

        g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;

        if (i2c_master_send(g_pstLaser_I2Cclient, puSendCmd , 3) < 0)
        {
                pr_err("stmvl6180:I2C write failed!! \n");
                return -1;
        }

        LOG_INF("I2C write addr 0x%x data 0x%x\n", addr, data );

        return 0;
}

int s4VL6180_ReadRegWord(u16 addr, u16 *data)
{
        u8 pu_send_cmd[2] = {(u8)(addr >> 8) , (u8)(addr & 0xFF) };
        u16 vData;

        g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;

        if (i2c_master_send(g_pstLaser_I2Cclient, pu_send_cmd, 2) < 0)
        {
                pr_err("stmvl6180:I2C read 1 failed!! \n");
                return -1;
        }

        if (i2c_master_recv(g_pstLaser_I2Cclient,(char *) data , 2) < 0)
        {
                pr_err("stmvl6180:I2C read 2 failed!! \n");
                return -1;
        }

        vData = *data;	

        *data = ( (vData & 0xFF) << 8 ) + ( (vData >> 8) & 0xFF ) ;

        LOG_INF("I2C read addr 0x%x data 0x%x\n", addr,*data );

        return 0;
}

int s4VL6180_WriteRegWord(u16 addr, u16 data)
{
        char puSendCmd[4] = {	(u8)((addr >>  8)&0xFF), 
                (u8)( addr       &0xFF),
                (u8)((data >>  8)&0xFF), 
                (u8)( data       &0xFF)};


        g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;

        if (i2c_master_send(g_pstLaser_I2Cclient, puSendCmd , 4) < 0)
        {
                pr_err("stmvl6180:I2C write failed!! \n");
                return -1;
        }

        LOG_INF("I2C write addr 0x%x data 0x%x\n", addr, data );

        return 0;
}

int s4VL6180_ReadRegDWord(u16 addr, u32 *data)
{
        u8 pu_send_cmd[2] = {(u8)(addr >> 8) , (u8)(addr & 0xFF) };

        u32 vData;

        g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
        if (i2c_master_send(g_pstLaser_I2Cclient, pu_send_cmd, 2) < 0)
        {
                pr_err("stmvl6180:I2C read 1 failed!! \n");
                return -1;
        }
        if (i2c_master_recv(g_pstLaser_I2Cclient, (char *)data , 4) < 0)
        {
                pr_err("stmvl6180:I2C read 2 failed!! \n");
                return -1;
        }

        vData = *data;

        *data= (	(vData        &0xFF) <<24) +
                (((vData>> 8)&0xFF) <<16) +
                (((vData>>16)&0xFF) << 8) +
                (((vData>>24)&0xFF) );

        LOG_INF("I2C read addr 0x%x data 0x%x\n", addr,*data );

        return 0;
}

int s4VL6180_WriteRegDWord(u16 addr, u32 data)
{
        char puSendCmd[6] = {	(u8)((addr >>  8)&0xFF), 
                (u8)( addr       &0xFF),
                (u8)((data >> 24)&0xFF), 
                (u8)((data >> 16)&0xFF), 
                (u8)((data >>  8)&0xFF), 
                (u8)( data       &0xFF)};

        g_pstLaser_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;

        if (i2c_master_send(g_pstLaser_I2Cclient, puSendCmd , 6) < 0)
        {
                pr_err("stmvl6180:I2C write failed!! \n");
                return -1;
        }

        LOG_INF("I2C write addr 0x%x data 0x%x\n", addr, data );

        return 0;
}

int VL6180x_RangeCheckMeasurement(int RangeValue)
{
        static int RangeArray[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        static int ArrayCnt = 0;
        int Idx = 0;
        int CheckRes = 0;

        for( Idx = 0; Idx < 16; Idx++ )
        {
                if( RangeArray[Idx] == RangeValue )
                {
                        CheckRes++;
                }
        }

        RangeArray[ArrayCnt] = RangeValue;
        ArrayCnt++;
        ArrayCnt &= 0xF;

        if( CheckRes == 16 )
        {
                RangeValue = 765;
                LOG_INF("Laser Check Data Failed \n");
        }

        return RangeValue;
}

void VL6180x_SystemInit(int Scaling, int EnWAF, int CalibMode)
{
        
        VL6180x_InitData(vl6180x_dev);

        VL6180x_Prepare(vl6180x_dev);
        VL6180x_UpscaleSetScaling(vl6180x_dev, Scaling);
        VL6180x_FilterSetState(vl6180x_dev, EnWAF); 
        VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
        VL6180x_RangeClearInterrupt(vl6180x_dev);

        
        if( CalibMode == VL6180_OFFSET_CALIB )
        {
                VL6180x_WrWord(vl6180x_dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, 0);
                VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);
                g_Laser_OffsetCalib = 0xFFFFFFFF;
        }
        else if( CalibMode == VL6180_XTALK_CALIB )
        {
                VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);
                g_Laser_XTalkCalib = 0xFFFFFFFF;
        }

        if( g_Laser_OffsetCalib != 0xFFFFFFFF )
        {
                VL6180x_SetOffsetCalibrationData(vl6180x_dev, g_Laser_OffsetCalib);
                pr_info("VL6180 Set Offset Calibration: Set the offset value as %d\n", g_Laser_OffsetCalib);
        }
        if( g_Laser_XTalkCalib != 0xFFFFFFFF )
        {
                VL6180x_SetXTalkCompensationRate(vl6180x_dev, g_Laser_XTalkCalib);
                pr_info("VL6180 Set XTalk Calibration: Set the XTalk value as %d\n", g_Laser_XTalkCalib);
        }

        VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
        msleep(10);
}

int VL6180x_GetRangeValue(VL6180x_RangeData_t *Range)
{
        int Result = 1;

        int ParamVal = 765;

        u8 u8status=0;

#if 0

        VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status);

        if (u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY)
        {
                VL6180x_RangeGetMeasurement(vl6180x_dev, Range);
                VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);

                ParamVal = Range->range_mm;

                
        }
        else
        {
                ParamVal = 765;
                LOG_INF("Laser Get Data Failed \n");
        }

#else

        int Count = 0;

        while(1)
        {
                Range->range_mm = 765;

                if(VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status) != 0){
                        pr_err("stmvl6180:laser i2c transfer failed\n");
                        return 0;
                }

                if ( u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY )
                {
                        VL6180x_RangeGetMeasurement(vl6180x_dev, Range);

                        ParamVal = Range->range_mm;

                        VL6180x_RangeClearInterrupt(vl6180x_dev);

                        break;
                }

                if( Count > 21 )
                {
                        pr_err("Laser Get Data Failed!!!\n");
                        VL6180x_RangeClearInterrupt(vl6180x_dev);

                        break;
                }

                mdelay(3);

                Count++;
        }

        VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
        mdelay(3);

#endif

        if( ParamVal == 765 )
        {
                Result = 0;
        }

        return Result;
}

#define N_MEASURE_AVG   20
#define MAX_TRY_CALIBRATE_TIMES 70
#define OFFSET_CALIB_TARGET_DISTANCE 100 
#define XTALK_CALIB_TARGET_DISTANCE 400 

static int read_sensor_id(void)
{
        u8 Device_Model_ID = 0;
        int ret = 0;

        s4VL6180_ReadRegByte(VL6180_MODEL_ID_REG, &Device_Model_ID);
        LOG_INF("STM VL6180 ID : %x\n", Device_Model_ID);

        if( Device_Model_ID != LASER_VL6180_SENSOR_ID )
        {
                LOG_INF("Not found STM VL6180\n");
                ret  = -1;
        }
        return Device_Model_ID;
}

static int read_manufacture_offset_value(void)
{
        int ret = 0;
        int8_t st_offset_value = 0;

        ret = VL6180x_RdByte(vl6180x_dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, (uint8_t *)&st_offset_value);
        if(ret != 0){
                pr_err("stmvl6180:failed to read SYSRANGE_PART_TO_PART_RANGE_OFFSET\n");
        }

        manufacture_offset_value = (int)st_offset_value;
        return ret;
}


#ifdef HTC_SAVE_CALIBRATION_TO_DATA
static int stmvl6180_read_calibration_file(char *path,int *para)
{
        struct file *f;
        char buf[8];
        mm_segment_t fs;
        int i,is_sign=0;
        int ret = 0;
        int temp_value = 0;

        f = filp_open(path, O_RDONLY, 0);
        if (f!= NULL && !IS_ERR(f) && f->f_dentry!=NULL)
        {
                fs = get_fs();
                set_fs(get_ds());
                
                for (i=0;i<8;i++)
                        buf[i]=0;
                f->f_op->read(f, buf, 8, &f->f_pos);
                set_fs(fs);
                LOG_INF("the txt content is:%s, buf[0]=%c\n",buf, buf[0]);
                for (i=0;i<8;i++)
                {
                        if (i==0 && buf[0]=='-')
                                is_sign=1;
                        else if (buf[i]>='0' && buf[i]<='9')
                                temp_value = temp_value*10 + (buf[i]-'0');
                        else
                                break;
                }
                if (is_sign==1)
                        temp_value=-temp_value;
                printk("offset_calib as %d\n", temp_value);

                *para = temp_value;
                filp_close(f, NULL);
        }
        else{
                pr_err("Calibration file is not exist!\n");
                ret = -1;
        }

        return ret;
}
static int stmvl6180_write_calibration_result_to_file( char *path, int calibration_result)
{
        struct file *f;
        char buf[8];
        mm_segment_t fs;
        int ret = 0;

        f = filp_open(path, O_WRONLY|O_CREAT, 0644);
        if (f!= NULL)
        {
                fs = get_fs();
                set_fs(get_ds());
                sprintf(buf,"%d",calibration_result);
                LOG_INF("write calibration_result as:%s, buf[0]:%c\n",buf, buf[0]);
                f->f_op->write(f, buf, 8, &f->f_pos);
                set_fs(fs);

                filp_close(f, NULL);
        }else{
                pr_err("stmvl6180:failed to open %s\n",path);
                ret = -1;
        }

        return ret;
}

#endif


static int stmvl6180_offset_calibrate_func(int *offset_value)
{
        int i = 0;
        int RangeSum =0,RangeAvg=0;
        VL6180x_RangeData_t Range;
        int ret = 0;
        int cal_times = 0;
        int8_t offset_val = g_Laser_OffsetCalib;

        VL6180x_SystemInit(3, 1, VL6180_OFFSET_CALIB);

        for( i = 0; i < N_MEASURE_AVG; )
        {
                if( VL6180x_GetRangeValue(&Range) )
                {
                        pr_info("VL6180 Offset Calibration Orignal: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
                        i++;
                        RangeSum += Range.range_mm;
                }
                cal_times++;
                if( cal_times > MAX_TRY_CALIBRATE_TIMES ){
                        pr_err("stmvl6180:Failed in offset calibration\n");
                        RangeSum = CALIBRATE_ERROR_NUMBER;
                        break;
                }
        }

        RangeAvg = RangeSum / N_MEASURE_AVG;

        if( RangeAvg >= ( OFFSET_CALIB_TARGET_DISTANCE - 3 ) && RangeAvg <= ( OFFSET_CALIB_TARGET_DISTANCE + 3) )
        {
                *offset_value = offset_val;
                LOG_INF("VL6180 Offset Calibration: Original offset is OK, finish offset calibration\n");
        }
        else
        {
                LOG_INF("VL6180 Offset Calibration: Start offset calibration\n");

                VL6180x_SystemInit(1, 0, VL6180_OFFSET_CALIB);

                RangeSum = 0;
                cal_times = 0;
                for( i = 0; i < N_MEASURE_AVG; )
                {
                        if( VL6180x_GetRangeValue(&Range) )
                        {
                                pr_info("VL6180 Offset Calibration: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
                                i++;
                                RangeSum += Range.range_mm;
                        }
                        msleep(10);

                        cal_times++;
                        if( cal_times > MAX_TRY_CALIBRATE_TIMES ){
                                pr_err("stmvl6180:Failed in offset calibration,cal_times=%d\n",cal_times);
                                RangeSum = CALIBRATE_ERROR_NUMBER;
                                break;
                        }
                }

                RangeAvg = RangeSum / N_MEASURE_AVG;
                LOG_INF("VL6180 Offset Calibration: Get the average Range as %d\n", RangeAvg);

                *offset_value = OFFSET_CALIB_TARGET_DISTANCE - RangeAvg;
                LOG_INF("VL6180 Offset Calibration: Set the offset value(pre-scaling) as %d\n", *offset_value);

                if( ABS(*offset_value) > 127 ) 
                {
                        *offset_value = CALIBRATE_ERROR_NUMBER;
                        ret = -1;
                }
                LOG_INF("VL6180 Offset Calibration: End\n");
        }

        return ret;
}


static int stmvl6180_xtalk_calibrate_func(void)
{
        int i=0;
        int RangeSum =0;
        int RateSum = 0;
        int cal_times = 0;
        int xtalkInt_value = 0;

        VL6180x_RangeData_t Range;

        VL6180x_SystemInit(3, 1, VL6180_XTALK_CALIB);

        for( i = 0; i < N_MEASURE_AVG; )
        {
                if( VL6180x_GetRangeValue(&Range) )
                {
                        RangeSum += Range.range_mm;
                        RateSum += Range.signalRate_mcps;
                        pr_info("VL6180 XTalk Calibration: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
                        i++;
                }

                cal_times++;
                if( cal_times > MAX_TRY_CALIBRATE_TIMES ){
                        pr_err("stmvl6180:Failed in xtalk calibration,cal_times=%d\n",cal_times);
                        return -1;
                }
        }

        xtalkInt_value = ( RateSum * ( N_MEASURE_AVG * XTALK_CALIB_TARGET_DISTANCE - RangeSum ) ) /( N_MEASURE_AVG * XTALK_CALIB_TARGET_DISTANCE * N_MEASURE_AVG) ;
        return xtalkInt_value;
}

static long Laser_Ioctl(
                struct file * a_pstFile,
                unsigned int a_u4Command,
                unsigned long a_u4Param)
{
        long i4RetValue = 0;
        void __user *p_u4Param = (void __user *)a_u4Param;
        VL6180x_RangeData_t Range;

        memset(&Range, 0, sizeof(VL6180x_RangeData_t));

        switch(a_u4Command)
        {
                case VL6180_IOCTL_SETPOWER:
                        {
                                int laser_power_on;
                                LOG_INF("VL6180_IOCTL_SETPOWER\n");
                                if(copy_from_user(&laser_power_on , p_u4Param , sizeof(int))){
                                        pr_err("copy from user failed when getting VL6180_IOCTL_SETOFFCALB \n");
                                        return  -1;
                                }
                                if( laser_power_on == 0){
                                        powerdown_sensor();
                                }else{
                                        poweron_sensor();
                                }
                                break;
                        }

                case VL6180_IOCTL_INIT:
                        if( g_s4Laser_Opened == 1 )
                        {
#ifdef HTC_SAVE_CALIBRATION_TO_DATA
                                LOG_INF("load saved calibrate data to system!\n");
                                if (-1 == stmvl6180_read_calibration_file(OFFSET_FILE,&g_Laser_OffsetCalib)){
                                        pr_err("stmvl6180:failed to read offset calibrate result from %s!\n",OFFSET_FILE);
                                        
                                        i4RetValue = -1;
                                        break;
                                }
                                if (-1 == stmvl6180_read_calibration_file(XTALK_FILE,&g_Laser_XTalkCalib)){
                                        pr_err("stmvl6180:failed to read xtalk calibrate result to from %s!\n",XTALK_FILE);
                                        
                                        i4RetValue = -1;
                                }
#endif
                                LOG_INF("VL6180_IOCTL_INIT\n");
                                if(read_sensor_id() < 0){
                                        pr_err("stmvl6180:failed to get laser id!\n");
                                        
                                        break;
                                }
                                if(!is_device_first_open){
                                        is_device_first_open = 1;
                                        if(read_manufacture_offset_value() != 0){
                                                pr_err("stmvl6180:read the manufacture_offset_value failed!\n");
                                                
                                        }
                                }
                        }
                        break;

                case VL6180_IOCTL_GETDATA:
                        LOG_INF("VL6180_IOCTL_GETALLDATA\n");
                        if( g_s4Laser_Opened == 1 )
                        {
                                VL6180x_SystemInit(3, 1, 0);
                                spin_lock(&g_Laser_SpinLock);
                                g_s4Laser_Opened = 2;
                                spin_unlock(&g_Laser_SpinLock);
                        }else if( g_s4Laser_Opened == 2 ){
                                if(VL6180x_GetRangeValue(&Range) == 0){
                                        pr_err("stmvl6180:Failed to get range!\n");
                                }else{
                                        pr_info("Laser Range : %d / %d \n", Range.range_mm, Range.DMax);
                                }
                        }else{
                                pr_err("stmvl6180:Failed to get range!g_s4Laser_Opened = %d\n",g_s4Laser_Opened);
                                i4RetValue = -1;
                        }
                        if(copy_to_user(p_u4Param , &Range.range_mm, sizeof(int)))
                        {
                                pr_err("copy to user failed when getting motor information \n");
                                i4RetValue = -1;
                        }
                        break;

                case VL6180_IOCTL_GETALLDATA:
                        LOG_INF("VL6180_IOCTL_GETALLDATA\n");

                        if( g_s4Laser_Opened == 1 )
                        {
                                VL6180x_SystemInit(3, 1, 0);
                                spin_lock(&g_Laser_SpinLock);
                                g_s4Laser_Opened = 2;
                                spin_unlock(&g_Laser_SpinLock);
                        }else if( g_s4Laser_Opened == 2 ){
                                if(VL6180x_GetRangeValue(&Range) == 0){
                                        pr_err("stmvl6180:Failed to get range!\n");
                                }else{
                                        pr_info("Laser Range : %d / %d \n", Range.range_mm, Range.DMax);
                                }
                        }else{
                                pr_err("stmvl6180:Failed to get range!g_s4Laser_Opened = %d\n",g_s4Laser_Opened);
                                i4RetValue = -1;
                        }
                        if(copy_to_user(p_u4Param , &Range, sizeof(VL6180x_RangeData_t)))
                        {
                                pr_err("copy to user failed when getting motor information \n");
                                i4RetValue = -1;
                        }
                        break;

                case VL6180_IOCTL_GETOFFCALB:  
                        {
                                int OffsetIn = 0;

                                LOG_INF("VL6180_IOCTL_GETOFFCALB\n");
                                spin_lock(&g_Laser_SpinLock);
                                g_s4Laser_Opened = 3;
                                spin_unlock(&g_Laser_SpinLock);

                                i4RetValue = stmvl6180_offset_calibrate_func(&OffsetIn);

                                spin_lock(&g_Laser_SpinLock);
                                g_s4Laser_Opened = 2;
                                spin_unlock(&g_Laser_SpinLock);

                                if(copy_to_user(p_u4Param , &OffsetIn , sizeof(int)))
                                {
                                        pr_err("copy to user failed when getting VL6180_IOCTL_GETOFFCALB \n");
                                        i4RetValue = -1;
                                }
                        }
                        break;
                case VL6180_IOCTL_SETOFFCALB:
                        {
                                int OffsetInt;
                                if(copy_from_user(&OffsetInt , p_u4Param , sizeof(int))){
                                        pr_err("copy from user failed when getting VL6180_IOCTL_SETOFFCALB \n");
                                        return  -1;
                                }

                                pr_info("VL6180_IOCTL_SETOFFCALB,offset value=%d\n",OffsetInt);
                                if( OffsetInt > 127 || OffsetInt < -128){
                                        pr_err("stmvl6180:error!The offset value is illegal! \n");
                                        return  -1;
                                }

                                g_Laser_OffsetCalib = OffsetInt;
                                

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
                                i4RetValue = stmvl6180_write_calibration_result_to_file(OFFSET_FILE,g_Laser_OffsetCalib);
#endif

                                LOG_INF("VL6180_IOCTL_SETOFFCALB end: g_Laser_OffsetCalib= %d\n", g_Laser_OffsetCalib);
                        }
                        break;

                case VL6180_IOCTL_GETXTALKCALB: 
                        {
                                int XtalkInt = 0;
                                int now_tries = 0;

                                LOG_INF("VL6180_IOCTL_GETXTALKCALB\n");
                                spin_lock(&g_Laser_SpinLock);
                                g_s4Laser_Opened = 3;
                                spin_unlock(&g_Laser_SpinLock);

                                while(now_tries  < HTC_TRY_TIMES){
                                        XtalkInt = stmvl6180_xtalk_calibrate_func();
                                        if(XtalkInt >= 0){
                                                pr_info("xtalk calibrate value=%d\n",XtalkInt);
                                                i4RetValue = 0;
                                                break;
                                        }
                                        now_tries++;
                                        pr_err("xtalk calibrate failed  at first %d time, we will try to calibrate %d times,XtalkInt=%d\n",
                                                        now_tries,HTC_TRY_TIMES,XtalkInt);
                                        i4RetValue = -1;
                                }
                                spin_lock(&g_Laser_SpinLock);
                                g_s4Laser_Opened = 2;
                                spin_unlock(&g_Laser_SpinLock);

                                if(copy_to_user(p_u4Param , &XtalkInt , sizeof(int)))
                                {
                                        pr_err("copy to user failed when getting VL6180_IOCTL_GETOFFCALB \n");
                                        i4RetValue = -1;
                                }
                        }
                        break;

                case VL6180_IOCTL_SETXTALKCALB:
                        {
                                int XtalkInt =(int)a_u4Param;

                                if(copy_from_user(&XtalkInt , p_u4Param , sizeof(int))){
                                        pr_err("copy from user failed when getting VL6180_IOCTL_SETOFFCALB \n");
                                        i4RetValue = -1;
                                        break;
                                }

                                pr_info("VL6180_IOCTL_SETXTALKCALB,xtalk value=%d\n",XtalkInt);
                                if( XtalkInt < 0 )
                                {
                                        i4RetValue = -1;
                                        pr_err("xtalk value should not less than 0 \n");
                                        break;
                                }
                                g_Laser_XTalkCalib = XtalkInt;

                                
                                

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
                                i4RetValue = stmvl6180_write_calibration_result_to_file(XTALK_FILE,g_Laser_XTalkCalib);
#endif

                                LOG_INF("VL6180 XTalk Calibration: End\n");
                        }
                        break;
                case VL6180_IOCTL_GET_LASER_POWERSTATE:
                        LOG_INF("VL6180_IOCTL_GET_LASER_POWERSTATE\n");
                        if(copy_to_user(p_u4Param , &stmvl6180_power_flag , sizeof(struct laser_power_flag)))
                        {
                                LOG_INF("copy to user failed when getting VL6180_IOCTL_GET_LASER_POWERSTATE \n");
                                i4RetValue = -1;
                        }
                        break;
                case VL6180_IOCTL_SET_POWERALIVE_FLAG:
                        {
                                int power_alive_flag =(int)a_u4Param;

                                LOG_INF("VL6180_IOCTL_SET_POWERALIVE_FLAG\n");
                                if(copy_from_user(&power_alive_flag , p_u4Param , sizeof(int))){
                                        pr_err("copy from user failed when getting VL6180_IOCTL_SETOFFCALB \n");
                                        return  -1;
                                }

                                spin_lock(&g_Laser_SpinLock);
                                if( power_alive_flag == 1){
                                        stmvl6180_power_flag.keep_power_alive = 1;
                                }else{
                                        stmvl6180_power_flag.keep_power_alive = 0;
                                }
                                spin_unlock(&g_Laser_SpinLock);
                        }
                        break;
                case VL6180_IOCTL_GET_ST_OFFSET_VALUE:
                        LOG_INF("VL6180_IOCTL_GET_ST_OFFSET_VALUE\n");
                        if(copy_to_user(p_u4Param , &manufacture_offset_value, sizeof(int)))
                        {
                                pr_err("copy to user failed when getting VL6180_IOCTL_GET_LASER_POWERSTATE \n");
                                i4RetValue = -1;
                        }
                        break;
                default :
                        pr_err("No CMD \n");
                        i4RetValue = -EPERM;
                        break;
        }

        return i4RetValue;
}



static int poweron_sensor(void)
{
         printk("Laser sensor power on +++\n");
       
		gpio_set_value(g_laser_en, 1);
        
        

        spin_lock(&g_Laser_SpinLock);
        stmvl6180_power_flag.laser_power_state = 1;
        spin_unlock(&g_Laser_SpinLock);

        printk("Laser sensor power on --- \n");
        return 0;
}

static int powerdown_sensor(void)
{
        if(stmvl6180_power_flag.keep_power_alive == 1){
            LOG_INF("No need to power off!\n");
            return 0;
        }

      
		gpio_set_value(g_laser_en, 0);
        mdelay(20);
		gpio_set_value(g_laser_en, 1);

        spin_lock(&g_Laser_SpinLock);
        stmvl6180_power_flag.laser_power_state = 0;
        spin_unlock(&g_Laser_SpinLock);

        LOG_INF("Laser sensor power down\n");
        return 0;
}

static int Laser_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
        LOG_INF("Start \n");

        if( g_s4Laser_Opened )
        {
                LOG_INF("The device is opened \n");
                return -EBUSY;
        }

        spin_lock(&g_Laser_SpinLock);
        g_s4Laser_Opened = 1;
        spin_unlock(&g_Laser_SpinLock);

        LOG_INF("End \n");
        return 0;
}

static int Laser_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
        LOG_INF("Start \n");

        if (g_s4Laser_Opened)
        {
                LOG_INF("Free \n");

                spin_lock(&g_Laser_SpinLock);
                g_s4Laser_Opened = 0;
                spin_unlock(&g_Laser_SpinLock);
        }

        LOG_INF("End \n");

        return 0;
}

static const struct file_operations g_stLaser_fops =
{
        .owner = THIS_MODULE,
        .open = Laser_Open,
        .release = Laser_Release,
        .unlocked_ioctl = Laser_Ioctl,
#ifdef CONFIG_COMPAT
        .compat_ioctl = Laser_Ioctl,
#endif
};

#define REPORT_EVENT_LASER_RANGE_LEN 2
#define VL6180_IC_ID 0xb4

static ssize_t power_laser_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        return sprintf(buf,"laser_power_state= %d,keep_power_alive = %d \n",
                        stmvl6180_power_flag.laser_power_state,
                        stmvl6180_power_flag.keep_power_alive);
}
static ssize_t power_laser_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf,
                size_t count)
{
        long tmp;
        int error;
        int target_state = 0;

        error = kstrtol(buf, 0, &tmp);
        if (error) {
                pr_err("stmvl6180-%s: kstrtoul fails, error = %d\n", __func__, error);
        }
        target_state = (int)tmp;
        if( target_state == 0 && (stmvl6180_power_flag.keep_power_alive == 0)){
                powerdown_sensor();
        }else if( target_state == 10){
                spin_lock(&g_Laser_SpinLock);
                stmvl6180_power_flag.keep_power_alive = 1;
                spin_unlock(&g_Laser_SpinLock);
        }else if( target_state == 100){
                spin_lock(&g_Laser_SpinLock);
                stmvl6180_power_flag.keep_power_alive = 0;
                spin_unlock(&g_Laser_SpinLock);
        }else{
                poweron_sensor();
                VL6180x_SystemInit(3, 1, 0);
        }
        return count;
}

static ssize_t laser_hwid_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        int id = read_sensor_id();

        if( id < 0){
                return sprintf(buf,"please check if you have power on the sensor!\n");
        }

        return sprintf(buf,"0x%x\n",id);
}

static ssize_t laser_range_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        VL6180x_RangeData_t Range;

        memset(&Range, 0, sizeof(VL6180x_RangeData_t));

        if( read_sensor_id() < 0)
                return sprintf(buf,"please check if you have power on the sensor!\n");

        if(!VL6180x_GetRangeValue(&Range)){
                pr_err("stmvl6180:failed to get the data!\n");
        }

        if( Range.range_mm == 765 ){
                return sprintf(buf,"Invalid!---Range= %3d, DMax= %3d, Rtn= %4d, -----Invaild output!,Error code=%s\n",
                                Range.range_mm, Range.DMax, Range.signalRate_mcps,
                                vl6180_error_stings[Range.errorStatus]);
        }else{
                return sprintf(buf,"  Valid!---Range= %3d, DMax= %3d, Rtn= %4d\n",
                                Range.range_mm, Range.DMax, Range.signalRate_mcps);
        }
}
static ssize_t laser_offset_calibrate_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        int ret;
        int OffsetInt;
        int origin_offset_value = 0;
        int diff_range;

        if( read_sensor_id() < 0)
                return sprintf(buf,"please check if you have power on the sensor!\n");

        origin_offset_value = g_Laser_OffsetCalib;
        ret = stmvl6180_offset_calibrate_func(&OffsetInt);
        if(ret){
                g_Laser_OffsetCalib = origin_offset_value;
                VL6180x_SystemInit(3, 1, 0);
                return sprintf(buf,"Invalid!---offset_para = 0x%x, distance may too far\n",OffsetInt);
        }

        g_Laser_OffsetCalib = OffsetInt;
        VL6180x_SystemInit(3, 1, 0);

        diff_range = manufacture_offset_value-OffsetInt;
        if(ABS(diff_range)>13){
                g_Laser_OffsetCalib = origin_offset_value;
                VL6180x_SystemInit(3, 1, 0);
                return sprintf(buf,"st_offset_calibrate_value = %d\n"
                                "htc_offset_calibrate_value=%d\n"
                                "\n\t the diff calibrate_value = %d > 13mm, offset calibration failed!!!\n",
                                OffsetInt,manufacture_offset_value,diff_range);
        }

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
        if( -1 == stmvl6180_write_calibration_result_to_file(OFFSET_FILE,g_Laser_OffsetCalib))
                pr_err("stmvl6180:failed to write result to %s\n",OFFSET_FILE);
#endif

        LOG_INF("VL6180 offset Calibration: End\n");
        return sprintf(buf,"  Valid!---offset_para= %d\n",OffsetInt);
}

static ssize_t laser_xtalk_calibrate_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        int XtalkInt;

        if( read_sensor_id() < 0)
                return sprintf(buf,"please check if you have power on the sensor!\n");

        XtalkInt = stmvl6180_xtalk_calibrate_func();
        if(XtalkInt > 38 ){
                pr_err("xtalk calibrate failed!\n");
                return sprintf(buf,"Invalid!---xtalk_para= %d\n",XtalkInt);
        }

        g_Laser_XTalkCalib = XtalkInt;
        VL6180x_SystemInit(3, 1, 0);

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
        if( -1 == stmvl6180_write_calibration_result_to_file(XTALK_FILE,g_Laser_XTalkCalib))
                pr_err("stmvl6180:failed to write result to %s\n",XTALK_FILE);
#endif

        LOG_INF("VL6180 XTalk Calibration: End\n");
        return sprintf(buf,"  Valid!---xtalk_para= %d\n",XtalkInt);
}

static ssize_t offset_para_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        return sprintf(buf,"offset_calibrate_value= %d\n",g_Laser_OffsetCalib);
}
static ssize_t offset_para_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf,
                size_t count)
{
        int tmp;
        int error;

        error = kstrtoint(buf, 0, &tmp);
        if (error) {
                pr_err("%s[%d]: kstrtoul fails, error = %d\n", __func__, __LINE__, error);
        }else {
                LOG_INF("%s(%d): tmp=:%d\n", __func__, __LINE__, tmp);
        }
        g_Laser_OffsetCalib = tmp;

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
        if( -1 == stmvl6180_write_calibration_result_to_file(OFFSET_FILE,g_Laser_OffsetCalib))
                pr_err("stmvl6180:failed to write result to %s\n",OFFSET_FILE);
#endif

        return count;
}

static ssize_t xtalk_para_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        if( g_Laser_XTalkCalib == 0xFFFFFFFF){
                return sprintf(buf,"xtalk calbiration was not performed or it failed\n");
        }else{
                return sprintf(buf,"xtalk_calibrate_value= %d\n",g_Laser_XTalkCalib);
        }
}
static ssize_t xtalk_para_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf,
                size_t count)
{
        int tmp;
        int error;

        error = kstrtoint(buf, 0, &tmp);
        if (error) {
                pr_err("%s[%d]: kstrtoul fails, error = %d\n", __func__, __LINE__, error);
        }else {
                LOG_INF("%s(%d): dload_mode_enabled:%d\n", __func__, __LINE__, tmp);
        }
        g_Laser_XTalkCalib = tmp;

#ifdef HTC_SAVE_CALIBRATION_TO_DATA
        if( -1 == stmvl6180_write_calibration_result_to_file(XTALK_FILE,g_Laser_XTalkCalib))
                pr_err("stmvl6180:failed to write result to %s\n",XTALK_FILE);
#endif
        return count;
}

static ssize_t laser_cali_status_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf,
                size_t count)
{
        int tmp = 0;
        int error = 0;

        error = kstrtoint(buf, 0, &tmp);
        if (error) {
                pr_err("%s[%d]: kstrtoul fails, error = %d\n", __func__, __LINE__, error);
        }else {
        	printk("%s(%d): tmp=:%d\n", __func__, __LINE__, tmp);
	        laser_cali_status_flag = tmp;
        }

        return count;
}

static ssize_t laser_cali_status_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        return snprintf(buf,5,"0x%2x",laser_cali_status_flag);
}

static DEVICE_ATTR(laser_power, 0664, power_laser_show, power_laser_store);
static DEVICE_ATTR(laser_hwid, 0444, laser_hwid_show, NULL);
static DEVICE_ATTR(laser_range, 0444, laser_range_show, NULL);
static DEVICE_ATTR(laser_offset_calibrate, 0444, laser_offset_calibrate_show, NULL);
static DEVICE_ATTR(laser_xtalk_calibrate, 0444, laser_xtalk_calibrate_show, NULL);
static DEVICE_ATTR(laser_offset, 0664, offset_para_show, offset_para_store);
static DEVICE_ATTR(laser_xtalk, 0664, xtalk_para_show, xtalk_para_store);
static DEVICE_ATTR(laser_cali_status, 0664, laser_cali_status_show, laser_cali_status_store);

inline static int Register_Laser_CharDrv(void)
{
        int ret;

        LOG_INF("Start\n");

        
        if( alloc_chrdev_region(&g_Laser_devno, 0, 1,LASER_SYS_NAME) )
        {
                pr_err("%s:Allocate device no failed\n",__func__);
                return -EAGAIN;
        }

        
        g_pLaser_CharDrv = cdev_alloc();
        if(NULL == g_pLaser_CharDrv)
        {
                unregister_chrdev_region(g_Laser_devno, 1);
                pr_err("%s,Allocate mem for kobject failed\n",__func__);
                return -ENOMEM;
        }

        
        cdev_init(g_pLaser_CharDrv, &g_stLaser_fops);
        g_pLaser_CharDrv->owner = THIS_MODULE;

        
        if(cdev_add(g_pLaser_CharDrv, g_Laser_devno, 1))
        {
                pr_err("%s:Attatch file operation failed\n",__func__);
                unregister_chrdev_region(g_Laser_devno, 1);
                return -EAGAIN;
        }

        actuator_class = class_create(THIS_MODULE, LASER_DRIVER_CLASS_NAME);
        if (IS_ERR(actuator_class)) {
                int ret = PTR_ERR(actuator_class);
                pr_err("%s:Unable to create class, err = %d\n", __func__,ret);
                return ret;
        }

        laser_device = device_create(actuator_class, NULL, g_Laser_devno, NULL, LASER_SYS_NAME);
        if(NULL == laser_device)
        {
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_power);
        if (ret) {
                pr_err("%s,failed to create laser power file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_hwid);
        if (ret) {
                pr_err("%s,failed to create laser id file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_range);
        if (ret) {
                pr_err("%s,failed to create laser range file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_offset_calibrate);
        if (ret) {
                pr_err("%s,failed to create laser offset calibrate file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_xtalk_calibrate);
        if (ret) {
                pr_err("%s,failed to create laser xtalk calibrate file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_offset);
        if (ret) {
                pr_err("%s,failed to create laser offset para file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_xtalk);
        if (ret) {
                pr_err("%s,failed to create laser xtalk para file!\n", __func__);
                return -EIO;
        }

        ret = device_create_file(laser_device, &dev_attr_laser_cali_status);
        if (ret) {
                pr_err("%s,failed to create laser xtalk para file!\n", __func__);
                return -EIO;
        }

        LOG_INF("End\n");
        return 0;
}

inline static void UnRegister_Laser_CharDrv(void)
{
        LOG_INF("Start\n");

        
        cdev_del(g_pLaser_CharDrv);

        unregister_chrdev_region(g_Laser_devno, 1);

        device_destroy(actuator_class, g_Laser_devno);

        class_destroy(actuator_class);

        LOG_INF("End\n");
}


static int laser_i2c_remove(struct i2c_client *client) {
        return 0;
}

static int laser_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        int i4RetValue = 0;

        printk("stmvl6180:%s enter\n",__func__);

        
        g_pstLaser_I2Cclient = client;
        g_pstLaser_I2Cclient->addr = I2C_SLAVE_ADDRESS>>1;

        g_pstLaser_I2Cclient->timing = 380;
        
        i4RetValue = Register_Laser_CharDrv();
        if(i4RetValue){
                pr_err(" register char device failed!\n");
                return i4RetValue;
        }

        spin_lock_init(&g_Laser_SpinLock);
        printk("stmvl6180:%s exit\n",__func__);
        return 0;
}

#ifdef CONFIG_PM
static int Laser_suspend(struct device *dev)
{
        return 0;
}

static int Laser_resume(struct device *dev)
{
        return 0;
}

static const struct dev_pm_ops laser_i2c_pm_ops = {
	.suspend = Laser_suspend,
	.resume = Laser_resume,
};
#endif 

#if 1
static struct of_device_id laser_i2c_match_table[] = {
	{.compatible = "mediatek,vl6180" },
	{},
};
#else
#define laser_i2c_match_table NULL
#endif 

static const struct i2c_device_id laser_i2c_id[] = {{LASER_SYS_NAME, 0},{}};







static struct i2c_driver Laser_i2c_driver = {
	.driver = {
		.name		= LASER_SYS_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = laser_i2c_match_table,
		#ifdef CONFIG_PM
		.pm		= &laser_i2c_pm_ops,
		#endif
	},
	.probe		= laser_i2c_probe,
	.remove		= laser_i2c_remove,
	.id_table	= laser_i2c_id,
};

#if 1

struct platform_device* pLaser_platform_device = NULL;

static const struct of_device_id laser_of_match[] = {
	{.compatible = "mediatek,vl6180"},
	{},
};

static int laser_probe(struct platform_device *pdev)
{
       printk("%s: +++ \n", __func__);
	i2c_add_driver(&Laser_i2c_driver);
	pLaser_platform_device = pdev;
       return 0;
}

static int laser_remove(struct platform_device *pdev)
{
	i2c_del_driver(&Laser_i2c_driver);
	return 0;
}



static struct platform_driver g_laser_Driver = {
	.probe = laser_probe,
	.remove = laser_remove,
	
	
	.driver = {
		   .name = LASER_SYS_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = laser_of_match,
		   }
};


static int __init laser_i2C_init(void)
{
	
       
	
	printk("%s: +++ \n", __func__);
	#if 0
	if (platform_device_register(&g_laser_device)) {
		printk("failed to register AF driver\n");
		return -ENODEV;
	}
       #endif
	if (platform_driver_register(&g_laser_Driver)) {
		printk("Failed to register AF driver\n");
		return -ENODEV;
	}
	
	
	if(pLaser_platform_device)
           laser_gpio_init(pLaser_platform_device);
	
       printk("%s: --- \n", __func__);
	return 0;
}

static void __exit laser_i2C_exit(void)
{
	platform_driver_unregister(&g_laser_Driver);
}

module_init(laser_i2C_init);
module_exit(laser_i2C_exit);

#endif


MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_LICENSE("GPL");

