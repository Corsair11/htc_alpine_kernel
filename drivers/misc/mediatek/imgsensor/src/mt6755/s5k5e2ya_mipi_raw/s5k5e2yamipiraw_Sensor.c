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


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k5e2yamipiraw_Sensor.h"

#define PFX "s5k5e2ya_camera_sensor"
#define LOG_1 LOG_INF("s5k5e2ya,MIPI 2LANE\n")
#define LOG_2 LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n")
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_WARNING(format, args...)    pr_warning(PFX "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MIPI_SETTLEDELAY_AUTO     0
#define MIPI_SETTLEDELAY_MANNUAL  1




static imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K5E2YA_SENSOR_ID,

	.checksum_value = 0xa48ebf5d,

	.pre = {
		.pclk = 179200000,				
		.linelength = 2950,				
		.framelength = 2025,			
		.startx = 0,					
		.starty = 0,					
		.grabwindow_width = 2560,		
		.grabwindow_height = 1920,		
		
		.mipi_data_lp2hs_settle_dc = 23,
		
		.max_framerate = 300,
	},
	.cap = { 
		.pclk = 179200000,
		.linelength = 2950,
		.framelength = 2025,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.cap1 = { 
		.pclk = 179200000,
		.linelength = 2950,
		.framelength = 2025,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1920,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 179200000,
		.linelength = 2950,
		.framelength = 2022,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1440,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 179200000,
		.linelength = 2950,
		.framelength = 2022,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1440,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 179200000,
		.linelength = 2950,
		.framelength = 2022,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1440,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 86400000,               
		.linelength = 2950,             
		.framelength = 1966,         
		.startx = 0,                    
		.starty = 0,                    
		.grabwindow_width = 1280,       
		.grabwindow_height = 960,       
		
		.mipi_data_lp2hs_settle_dc = 23,
		
		.max_framerate = 150,
	},
	.margin = 4,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  
	.ihdr_le_firstline = 0,  
	.sensor_mode_num = 6,	  

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, 
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x20, 0x6c, 0xff},
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				
	.sensor_mode = IMGSENSOR_MODE_INIT, 
	.shutter = 0x3D0,					
	.gain = 0x100,						
	.dummy_pixel = 0,					
	.dummy_line = 0,					
	.current_fps = 0,  
	.autoflicker_en = KAL_FALSE,  
	.test_pattern = KAL_FALSE,		
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0, 
	.i2c_write_id = 0x20,
};


static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] =
{
 { 2576, 1936,	  8,  248, 2560, 1440, 2560, 1440, 0000, 0000, 2560, 1440,	  0,	0, 2560, 1440}, 
 { 2576, 1936,	  8,	8, 2560, 1920, 2560, 1920, 0000, 0000, 2560, 1920,	  0,	0, 2560, 1920}, 
 { 2576, 1936,	  8,  248, 2560, 1440, 2560, 1440, 0000, 0000, 2560, 1440,	  0,	0, 2560, 1440}, 
 { 2576, 1936,	  8,  248, 2560, 1440, 2560, 1440, 0000, 0000, 2560, 1440,	  0,	0, 2560, 1440}, 
 { 2576, 1936,	  8,  248, 2560, 1440, 2560, 1440, 0000, 0000, 2560, 1440,	  0,	0, 2560, 1440}, 
 { 2576, 1936,   24,  260, 2560, 1440, 1280,  960, 0000, 0000, 1280,  960,    0,    0, 1280,  960}};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
    iWriteRegI2C(pusendcmd , 3, imgsensor.i2c_write_id);
}


static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
}	

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));

}


static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;
	

	LOG_INF("framerate = %d, min framelength should enable? %d \n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	
	
		
	
		
	
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	


static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

	
	

	
	
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);
		else {
		
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		}
	} else {
		
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	}

    if ((shutter + 5) > imgsensor.frame_length) {
		shutter = imgsensor.frame_length - 6;
    }

	
	
	write_cmos_sensor(0x0202, shutter >> 8);
	write_cmos_sensor(0x0203, shutter & 0xFF);
	
	LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

	

}	



static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	


#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	return gain>>1;
}
#endif
static kal_uint16 gain2reg_a(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	iReg = ((gain*0x20)/BASEGAIN);
	if(iReg < 0x20){  
		iReg = 0x20;
	}

	return iReg;
}

static kal_uint16 gain2reg_d(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	iReg = (((gain*0x100)/BASEGAIN)/0x10);
	if(iReg < 0x100)  
	{
		iReg = 0x100;
	}

	return iReg;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_a_gain;
    kal_uint16 reg_d_gain;

	reg_a_gain = gain2reg_a(gain);
	if(reg_a_gain > 0x200){
		reg_a_gain = 0x200; 
		reg_d_gain = gain2reg_d(gain);
	}else{
		reg_d_gain = 0x100; 
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_a_gain;
	spin_unlock(&imgsensor_drv_lock);

	
	

	
	write_cmos_sensor(0x0204, reg_a_gain >> 8);
	write_cmos_sensor(0x0205, reg_a_gain & 0xFF);

	
	write_cmos_sensor(0x020E, reg_d_gain >> 8);
	write_cmos_sensor(0x020F, reg_d_gain & 0xFF);
	write_cmos_sensor(0x0210, reg_d_gain >> 8);
	write_cmos_sensor(0x0211, reg_d_gain & 0xFF);
	write_cmos_sensor(0x0212, reg_d_gain >> 8);
	write_cmos_sensor(0x0213, reg_d_gain & 0xFF);
	write_cmos_sensor(0x0214, reg_d_gain >> 8);
	write_cmos_sensor(0x0215, reg_d_gain & 0xFF);

	
	

	return gain;
}	



static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
			if (le > imgsensor.min_frame_length - imgsensor_info.margin)
				imgsensor.frame_length = le + imgsensor_info.margin;
			else
				imgsensor.frame_length = imgsensor.min_frame_length;
			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
				imgsensor.frame_length = imgsensor_info.max_frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
			if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;


				
				write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
				write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);

		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);

		set_gain(gain);
	}

}


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);


	switch (image_mirror) {
		case IMAGE_NORMAL:
			write_cmos_sensor(0x0101,0x00);
			break;
		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x0101,0x01);
			break;
		case IMAGE_V_MIRROR:
			write_cmos_sensor(0x0101,0x02);
			break;
		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x0101,0x03);
			break;
		default:
			LOG_INF("Error image_mirror setting\n");
	}

}
#endif
#if 0
static void night_mode(kal_bool enable)
{
}	
#endif
static void sensor_init(void)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	
	write_cmos_sensor(0x0100,0x00); 
	

	
	write_cmos_sensor(0x3000,0x04);       
	write_cmos_sensor(0x3002,0x03);       
	write_cmos_sensor(0x3003,0x04);       
	write_cmos_sensor(0x3004,0x02);       
	write_cmos_sensor(0x3005,0x00);       
	write_cmos_sensor(0x3006,0x10);       
	write_cmos_sensor(0x3007,0x03);       
	write_cmos_sensor(0x3008,0x55);       
	write_cmos_sensor(0x3039,0x00);       
	write_cmos_sensor(0x303A,0x00);       
	write_cmos_sensor(0x303B,0x00);       
	write_cmos_sensor(0x3009,0x05);       
	write_cmos_sensor(0x300A,0x55);       
	write_cmos_sensor(0x300B,0x38);       
	write_cmos_sensor(0x300C,0x10);       
	write_cmos_sensor(0x3012,0x05);       
	write_cmos_sensor(0x3013,0x00);       
	write_cmos_sensor(0x3014,0x22);       
	write_cmos_sensor(0x300E,0x79);       
	write_cmos_sensor(0x3010,0x68);       
	write_cmos_sensor(0x3019,0x03);       
	write_cmos_sensor(0x301A,0x00);       
	write_cmos_sensor(0x301B,0x06);       
	write_cmos_sensor(0x301C,0x00);       
	write_cmos_sensor(0x301D,0x22);       
	write_cmos_sensor(0x301E,0x00);       
	write_cmos_sensor(0x301F,0x10);       
	write_cmos_sensor(0x3020,0x00);       
	write_cmos_sensor(0x3021,0x00);       
	write_cmos_sensor(0x3022,0x0A);       
	write_cmos_sensor(0x3023,0x1E);       
	write_cmos_sensor(0x3024,0x00);       
	write_cmos_sensor(0x3025,0x00);       
	write_cmos_sensor(0x3026,0x00);       
	write_cmos_sensor(0x3027,0x00);       
	write_cmos_sensor(0x3028,0x1A);       
	write_cmos_sensor(0x3015,0x00);       
	write_cmos_sensor(0x3016,0x84);       
	write_cmos_sensor(0x3017,0x00);       
	write_cmos_sensor(0x3018,0xA0);       
	write_cmos_sensor(0x302B,0x10);       
	write_cmos_sensor(0x302C,0x0A);       
	write_cmos_sensor(0x302D,0x06);       
	write_cmos_sensor(0x302E,0x05);       
	write_cmos_sensor(0x302F,0x0E);       
	write_cmos_sensor(0x3030,0x2F);       
	write_cmos_sensor(0x3031,0x08);       
	write_cmos_sensor(0x3032,0x05);       
	write_cmos_sensor(0x3033,0x09);       
	write_cmos_sensor(0x3034,0x05);       
	write_cmos_sensor(0x3035,0x00);       
	write_cmos_sensor(0x3036,0x00);       
	write_cmos_sensor(0x3037,0x00);       
	write_cmos_sensor(0x3038,0x00);       
	write_cmos_sensor(0x3088,0x06);       
	write_cmos_sensor(0x308A,0x08);       
	write_cmos_sensor(0x308C,0x05);       
	write_cmos_sensor(0x308E,0x07);       
	write_cmos_sensor(0x3090,0x06);       
	write_cmos_sensor(0x3092,0x08);       
	write_cmos_sensor(0x3094,0x05);       
	write_cmos_sensor(0x3096,0x21);       
	write_cmos_sensor(0x3099,0x0E);       
	write_cmos_sensor(0x3070,0x10);       
	write_cmos_sensor(0x3085,0x11);       
	write_cmos_sensor(0x3086,0x01);       

	write_cmos_sensor(0x3064,0x00);       
	write_cmos_sensor(0x3062,0x08);       
	write_cmos_sensor(0x3061,0x11);       
	write_cmos_sensor(0x307B,0x20);       
	write_cmos_sensor(0x3068,0x00);       
	write_cmos_sensor(0x3074,0x00);       
	write_cmos_sensor(0x307D,0x00);       
	write_cmos_sensor(0x3045,0x01);       
	write_cmos_sensor(0x3046,0x05);       
	write_cmos_sensor(0x3047,0x78);
	write_cmos_sensor(0x307F,0xB1);       
	write_cmos_sensor(0x3098,0x01);       
	write_cmos_sensor(0x305C,0xF6);       
	write_cmos_sensor(0x306B,0x10);
	write_cmos_sensor(0x3063,0x27);       
	write_cmos_sensor(0x320C,0x07);
	write_cmos_sensor(0x320D,0x00);
	write_cmos_sensor(0x3400,0x01);       
	write_cmos_sensor(0x3235,0x49);       
	write_cmos_sensor(0x3233,0x00);       
	write_cmos_sensor(0x3234,0x00);
	write_cmos_sensor(0x3300,0x0C);       
	write_cmos_sensor(0x3061,0x12);
	write_cmos_sensor(0x3203,0x45);       
	write_cmos_sensor(0x3205,0x4D);       
	write_cmos_sensor(0x320B,0x40);       
	write_cmos_sensor(0x320C,0x06);       
	write_cmos_sensor(0x320D,0xC0);
	write_cmos_sensor(0x3931,0x02);
	write_cmos_sensor(0x3929,0x07);       

	
	write_cmos_sensor(0x0100,0x01);
}	


static void preview_setting(void)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	
	write_cmos_sensor(0x0100,0x00);

	write_cmos_sensor(0x0305,0x06);
	write_cmos_sensor(0x0306,0x00);
	write_cmos_sensor(0x0307,0xE0);
	write_cmos_sensor(0x3C1F,0x00);
	write_cmos_sensor(0x0820,0x03);
	write_cmos_sensor(0x0821,0x80);
	write_cmos_sensor(0x3C1C,0x58);
	write_cmos_sensor(0x0340,0x07);
	write_cmos_sensor(0x0341,0xE9);
	write_cmos_sensor(0x0342,0x0B);
	write_cmos_sensor(0x0343,0x86);
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x0A);
	write_cmos_sensor(0x0349,0x07);
	write_cmos_sensor(0x034A,0x07);
	write_cmos_sensor(0x034B,0x87);
	write_cmos_sensor(0x034C,0x0A);
	write_cmos_sensor(0x034D,0x00);
	write_cmos_sensor(0x034E,0x07);
	write_cmos_sensor(0x034F,0x80);
	write_cmos_sensor(0x0900,0x00);
	write_cmos_sensor(0x0901,0x20);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x0202,0x02);
	write_cmos_sensor(0x0203,0x00);
	write_cmos_sensor(0x0204,0x00);
	write_cmos_sensor(0x0205,0x20);
	write_cmos_sensor(0x0101,0x03);

	write_cmos_sensor(0x0100,0x01);
}	

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	preview_setting();
}

static void video_call_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	write_cmos_sensor(0x0100,0x00);

	write_cmos_sensor(0x0305,0x05);
	write_cmos_sensor(0x0306,0x00);
	write_cmos_sensor(0x0307,0xB4);
	write_cmos_sensor(0x3C1F,0x01);
	write_cmos_sensor(0x0820,0x01);
	write_cmos_sensor(0x0821,0xB0);
	write_cmos_sensor(0x3C1C,0x54);
	write_cmos_sensor(0x0340,0x07);
	write_cmos_sensor(0x0341,0xAE);
	write_cmos_sensor(0x0342,0x0B);
	write_cmos_sensor(0x0343,0x86);
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x0A);
	write_cmos_sensor(0x0349,0x07);
	write_cmos_sensor(0x034A,0x07);
	write_cmos_sensor(0x034B,0x87);
	write_cmos_sensor(0x034C,0x05);
	write_cmos_sensor(0x034D,0x00);
	write_cmos_sensor(0x034E,0x03);
	write_cmos_sensor(0x034F,0xC0);
	write_cmos_sensor(0x0900,0x01);
	write_cmos_sensor(0x0901,0x22);
	write_cmos_sensor(0x0387,0x03);
	write_cmos_sensor(0x0202,0x02);
	write_cmos_sensor(0x0203,0x00);
	write_cmos_sensor(0x0204,0x00);
	write_cmos_sensor(0x0205,0x20);
	write_cmos_sensor(0x0101,0x03);

	write_cmos_sensor(0x0100,0x01);
}

static void normal_video_setting(kal_uint16 currefps)
{
    LOG_INF("E! currefps:%d\n",currefps);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	
	write_cmos_sensor(0x0100,0x00);

	write_cmos_sensor(0x0305,0x06);
	write_cmos_sensor(0x0306,0x00);
	write_cmos_sensor(0x0307,0xE0);
	write_cmos_sensor(0x3C1F,0x00);
	write_cmos_sensor(0x0820,0x03);
	write_cmos_sensor(0x0821,0x80);
	write_cmos_sensor(0x3C1C,0x58);
	write_cmos_sensor(0x0340,0x07);
	write_cmos_sensor(0x0341,0xE6);
	write_cmos_sensor(0x0342,0x0B);
	write_cmos_sensor(0x0343,0x86);
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0xF8);
	write_cmos_sensor(0x0348,0x0A);
	write_cmos_sensor(0x0349,0x07);
	write_cmos_sensor(0x034A,0x06);
	write_cmos_sensor(0x034B,0x97);
	write_cmos_sensor(0x034C,0x0A);
	write_cmos_sensor(0x034D,0x00);
	write_cmos_sensor(0x034E,0x05);
	write_cmos_sensor(0x034F,0xA0);
	write_cmos_sensor(0x0900,0x00);
	write_cmos_sensor(0x0901,0x20);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x0202,0x04);
	write_cmos_sensor(0x0203,0x98);
	write_cmos_sensor(0x0204,0x00);
	write_cmos_sensor(0x0205,0x20);
	write_cmos_sensor(0x0101,0x03);

	write_cmos_sensor(0x0100,0x01);
}
static void hs_video_setting(void)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	preview_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	preview_setting();
}


static const char *s5k5e2yaVendor = "SamSung";
static const char *s5k5e2yaNAME = "s5k5e2ya_front";
static const char *s5k5e2yaSize = "5.0M";

static ssize_t sensor_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", s5k5e2yaVendor, s5k5e2yaNAME, s5k5e2yaSize);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);
static struct kobject *android_s5k5e2ya;

static int s5k5e2ya_sysfs_init(void)
{
	int ret ;
	LOG_INF("kobject creat and add\n");
	if(android_s5k5e2ya == NULL){
		android_s5k5e2ya = kobject_create_and_add("android_camera2", NULL);
		if (android_s5k5e2ya == NULL) {
			LOG_INF("subsystem_register failed\n");
			ret = -ENOMEM;
			return ret ;
		}
		LOG_INF("sysfs_create_file\n");
		ret = sysfs_create_file(android_s5k5e2ya, &dev_attr_sensor.attr);
		if (ret) {
			LOG_INF("sysfs_create_file " \
					"failed\n");
			kobject_del(android_s5k5e2ya);
			android_s5k5e2ya = NULL;
		}
	}
	return 0 ;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				
				s5k5e2ya_sysfs_init();
				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail,i2c_write_id 0x%x id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


static kal_uint32 open(void)
{
	
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	LOG_1;
	LOG_2;
	
	kdSetI2CSpeed(400);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail,i2c_write_id 0x%x id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	



static kal_uint32 close(void)
{
	LOG_INF("E\n");

	

	return ERROR_NONE;
}	


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}	

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);


	return ERROR_NONE;
}	

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	video_call_setting(15);
	return ERROR_NONE;
}   

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);


	return ERROR_NONE;
}	

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	return ERROR_NONE;
}	

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();

	return ERROR_NONE;
}	



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}	

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	
	
	

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; 
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; 
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; 
	sensor_info->SensorResetActiveHigh = FALSE; 
	sensor_info->SensorResetDelayCount = 5; 

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; 
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; 
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; 
	sensor_info->SensorPixelClockCount = 3; 
	sensor_info->SensorDataLatchCount = 2; 

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  
	sensor_info->SensorHightSampling = 0;	
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
			break;
		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			Custom1(image_window, sensor_config_data); 
		break;
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	
	if (framerate == 0)
		
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) 
		imgsensor.autoflicker_en = KAL_TRUE;
	else 
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
            }
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			framerate=150;
			frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			
			break;
		default:  
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*framerate = imgsensor_info.custom1.max_framerate;
			break;
		default:
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		
		
		write_cmos_sensor(0x0601, 0x02);
	} else {
		
		
		write_cmos_sensor(0x0601, 0x00);
	}
	write_cmos_sensor(0x3200, 0x00);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;


	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			break;
		case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
			break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			if((sensor_reg_data->RegData>>8)>0)
			   write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			else
				write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			
			
			*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: 
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", (UINT32)*feature_data);
			spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_SET_HDR:
			
			LOG_INF("Warning! Not Support IHDR Feature");
			spin_lock(&imgsensor_drv_lock);
			
            imgsensor.ihdr_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
            wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

			switch (*feature_data_32) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_CUSTOM1:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[5],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
			}
            break;
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			break;
		default:
			break;
	}

	return ERROR_NONE;
}	

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K5E2YA_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;

	return ERROR_NONE;
}	
