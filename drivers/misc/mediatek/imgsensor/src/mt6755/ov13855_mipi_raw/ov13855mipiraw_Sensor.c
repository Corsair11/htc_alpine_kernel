
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov13855mipiraw_Sensor.h"

#define PFX "OV13855_camera_sensor"
#define LOG_1 LOG_INF("OV13855,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 2096*1552@30fps,640Mbps/lane; video 4192*3104@30fps,1.2Gbps/lane; capture 13M@30fps,1.2Gbps/lane\n")
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_WARNING(format, args...)    pr_warning(PFX "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

int ov13855_chip_ver = OV13855_R2A;
int m_bpc_select=0;
bool GroupHoldStart = false; 
bool SettingForDiffPD = false; 


extern void otp_cali(unsigned char writeid);



static imgsensor_info_struct imgsensor_info = {
    .sensor_id = OV13855_SENSOR_ID,        

    .checksum_value = 0xbde6b5f8,
    .pre = {
        .pclk = 110000000,                
        .linelength = 1122,                
        .framelength = 3268,            
        .startx = 0,                    
        .starty = 0,                    
        .grabwindow_width = 4224,        
        .grabwindow_height = 3136,        
        
        .mipi_data_lp2hs_settle_dc = 100,
        
        .max_framerate = 300,
    },
	.cap = { 
		.pclk = 110000000,				  
		.linelength = 1122, 			   
		.framelength = 3268,			
		.startx = 0,					
		.starty = 0,					
		.grabwindow_width = 4224,		 
		.grabwindow_height = 3136,		  
		
		.mipi_data_lp2hs_settle_dc = 100,
		
		.max_framerate = 300,
	},
    .cap1 = { 
        .pclk = 110000000,
        .linelength = 1122,
        .framelength = 3268,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4224,
        .grabwindow_height = 3136,
        .mipi_data_lp2hs_settle_dc = 100,
        .max_framerate = 300,
    },
    .normal_video = { 
        .pclk = 110000000,                
        .linelength = 1122,                
        .framelength = 3268,            
        .startx = 0,                    
        .starty = 0,                    
        .grabwindow_width = 4224,        
        .grabwindow_height = 3136,        
        
        .mipi_data_lp2hs_settle_dc = 100,
        
        .max_framerate = 300,
    },
    .hs_video = { 
        .pclk = 110000000,
        .linelength = 1122, 
        .framelength = 3268, 
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4224,  
        .grabwindow_height = 3136,  
        .mipi_data_lp2hs_settle_dc = 100,
        .max_framerate = 300,
    },
    .slim_video = {
        .pclk = 108000000,
        .linelength = 1122, 
        .framelength = 1002, 
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1024,
        .grabwindow_height = 576,
        .mipi_data_lp2hs_settle_dc = 100,
        .max_framerate = 960,
    },
#if 0
    .slim_video = {
        .pclk = 108000000,
        .linelength = 1122, 
        .framelength = 800, 
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 768,
        .grabwindow_height = 432,
        .mipi_data_lp2hs_settle_dc = 100,
        .max_framerate = 1200,
    },
#endif
    
    .custom1 = {
        .pclk = 108000000,               
        .linelength = 1122,             
        .framelength = 6432,         
        .startx = 0,                    
        .starty = 0,                    
        .grabwindow_width = 2112,       
        .grabwindow_height = 1568,       
        
        .mipi_data_lp2hs_settle_dc = 23,
        
        .max_framerate = 150,
    },
    
    .custom2 = {
        .pclk = 108000000,				 
        .linelength = 1122, 			
        .framelength = 3216,		 
        .startx = 0,					
        .starty = 0,					
        .grabwindow_width = 2112,		
        .grabwindow_height = 1188,		 
        
        .mipi_data_lp2hs_settle_dc = 23,
        
        .max_framerate = 300,
    },



    .margin = 8,            
    .min_shutter = 0x3,        
    .max_frame_length = 0x7fff,
    .ae_shut_delay_frame = 0,    
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,      
    .ihdr_le_firstline = 0,  
    .sensor_mode_num = 7,      

    .cap_delay_frame = 3,        
    .pre_delay_frame = 2,         
    .video_delay_frame = 2,        
    .hs_video_delay_frame = 2,    
    .slim_video_delay_frame = 2,
    .custom1_delay_frame = 2,
    .custom2_delay_frame = 2,

    .isp_driving_current = ISP_DRIVING_6MA, 
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, 
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = { 0x20,0x6c, 0xff},
};


static imgsensor_struct imgsensor = {
    .mirror = IMAGE_H_MIRROR,                
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
    .i2c_write_id = 0x00,
};


static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] =
{{ 4256, 3168, 16, 16, 4224, 3136, 4224, 3136, 0, 0, 4224, 3136, 0, 0, 4224, 3136}, 
 { 4256, 3168, 16, 16, 4224, 3136, 4224, 3136, 0, 0, 4224, 3136, 0, 0, 4224, 3136}, 
 { 4256, 3168, 16, 16, 4224, 3136, 4224, 3136, 0, 0, 4224, 3136, 0, 0, 4224, 3136}, 
 { 4256, 3168, 16, 16, 4224, 3136, 4224, 3136, 0, 0, 4224, 3136, 0, 0, 4224, 3136}, 
 { 4256, 3168, 16, 16, 4224, 3136, 1024, 576, 0, 0, 1024, 576, 0, 0, 1024, 576},
#if 0
 { 4256, 3168, 16, 16, 4224, 3136, 768, 432, 0, 0, 768, 432, 0, 0, 768, 432},
#endif
 { 4256, 3168, 16, 16, 4224, 3136, 2112, 1568, 0, 0, 2112, 1568, 0, 0, 2112, 1568},
 { 4256, 3168, 16, 16, 4224, 3136, 2112, 1188, 0, 0, 2112, 1188, 0, 0, 2112, 1188}};
#if 0
static SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX =  0,
    .i4OffsetY = 0,
    .i4PitchX  = 32,
    .i4PitchY  = 32,
    .i4PairNum  =8,
    .i4SubBlkW  =16,
    .i4SubBlkH  =8,
    .i4PosL = {{2,6},{18,6},{10,10},{26,10},{6,22},{22,22},{14,26},{30,26}},
    .i4PosR = {{2,2},{18,2},{10,14},{26,14},{6,18},{22,18},{14,30},{30,30}},
};
#else

static SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX =  64,
    .i4OffsetY = 64,
    .i4PitchX  = 32,
    .i4PitchY  = 32,
    .i4PairNum  =8,
    .i4SubBlkW  =16,
    .i4SubBlkH  =8,
    .i4PosL = {{78,70},{94,70},{70,74},{86,74},{78,86},{94,86},{70,90},{86,90}},
    .i4PosR = {{78,66},{94,66},{70,78},{86,78},{78,82},{94,82},{70,94},{86,94}},
};

#endif
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

static void set_dummy(void)
{
    LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
    
    write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
    write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
    write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
    write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
}    

static kal_uint32 return_sensor_id(void)
{
    
    return ((read_cmos_sensor(0x300B) << 8) | read_cmos_sensor(0x300C));
}
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    
    kal_uint32 frame_length = imgsensor.frame_length;
    

    LOG_INF("framerate = %d, min framelength should enable = %d \n", framerate,min_framelength_en);

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



static uint16_t adjusted_line_length_pclk = 0;
static uint16_t original_line_length_pclk = 0;
static void set_shutter(kal_uint32 shutter)
{
    unsigned long flags;
    
    uint16_t line_length_pclk = 0;
    
    static uint16_t pre_line_length_pclk = 0;
    
    kal_uint16 realtime_fps = 0;
    
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_INF("Enter! shutter =%d \n", shutter);
   
    
    
    

    
    
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;

      
     original_line_length_pclk = imgsensor.line_length;
     if(shutter > imgsensor_info.max_frame_length)
     {
         adjusted_line_length_pclk = ( shutter * original_line_length_pclk ) / (imgsensor_info.max_frame_length);
         LOG_INF("set shutter : long exposure , adjusted_line_length_pclk=0x%x original_line_length_pclk=0x%x . \n", adjusted_line_length_pclk, original_line_length_pclk);
     }
     else
     {
        LOG_INF("set shutter : adjusted_line_length_pclk=0 original_line_length_pclk=0x%x\n", original_line_length_pclk);
        adjusted_line_length_pclk = 0;
     }
       

    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    
    shutter = (shutter >> 1) << 1;
    imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

    
    if (adjusted_line_length_pclk > 0)
        line_length_pclk = adjusted_line_length_pclk;
    else if (pre_line_length_pclk > 0)
        line_length_pclk = original_line_length_pclk;  
    else
        line_length_pclk = 0; 
    


    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296,0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146,0);
        else {
        
        write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8)&0x7f);
        write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
        }
    } else {
        
        
        
         write_cmos_sensor(0x380e, (imgsensor.frame_length >> 8)&0x7f);
        write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
    }

     
    if (line_length_pclk > 0)
    {
        LOG_INF("Set line_length_pclk = 0x%x\n ", line_length_pclk);
        write_cmos_sensor(0x380c, (line_length_pclk >> 8)&0xff);
        write_cmos_sensor(0x380d, line_length_pclk & 0xFF);
    }

    
    
    

    pre_line_length_pclk = adjusted_line_length_pclk;
     

    
   
   
   
   
		write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
		write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	
    LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}    



static kal_uint16 gain2reg_a(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;
	

	if (ov13855_chip_ver == OV13855_R1A)
	{
		iReg = gain*32/BASEGAIN;
		if(iReg < 0x20)
		{
			iReg = 0x20;
		}
		if(iReg > 0xfc)
		{
			iReg = 0xfc;
		}
		
	}
	else if(ov13855_chip_ver == OV13855_R2A)
	{
		iReg = gain*128/BASEGAIN;
		if(iReg < 0x80)  
		{
			iReg = 0x100;
		}
	}
	return iReg;
}

static kal_uint16 gain2reg_d(const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;
	
	iReg = (gain * 1024)/((kal_uint16)(BASEGAIN * 15.5));
	if(iReg < 0x400)  
	{
		iReg = 0x400;
	}

	return iReg;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_a_gain;
    kal_uint16 reg_d_gain;
		reg_a_gain = gain2reg_a(gain);
		if(reg_a_gain > 0x7C0){
			reg_d_gain = gain2reg_d(gain);
			reg_a_gain = 0x7C0;
		}
		else
			reg_d_gain = 0x400; 
		spin_lock(&imgsensor_drv_lock);
		imgsensor.gain = reg_a_gain;
		spin_unlock(&imgsensor_drv_lock);
		LOG_INF("gain = %d , reg_a_gain = 0x%x , reg_d_gain = 0x%x\n ", gain, reg_a_gain, reg_d_gain);
	
		
		write_cmos_sensor(0x3508, reg_a_gain >> 8);
		write_cmos_sensor(0x3509, reg_a_gain & 0xFF);

		
		write_cmos_sensor(0x5100, reg_d_gain >> 8);
		write_cmos_sensor(0x5101, reg_d_gain & 0xFF);
		write_cmos_sensor(0x5102, reg_d_gain >> 8);
		write_cmos_sensor(0x5103, reg_d_gain & 0xFF);
		write_cmos_sensor(0x5104, reg_d_gain >> 8);
		write_cmos_sensor(0x5105, reg_d_gain & 0xFF);

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



static void set_mirror_flip(kal_uint8 image_mirror)
{
    LOG_INF("image_mirror = %d\n", image_mirror);


    switch (image_mirror) {
				case IMAGE_NORMAL:
					write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xFB) | 0x00));
					write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xFB) | 0x00));
					break;
				case IMAGE_H_MIRROR:
					write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xFB) | 0x00));
					write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xFF) | 0x04));
					break;
				case IMAGE_V_MIRROR:
					write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xFF) | 0x04));
					write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xFB) | 0x00));		
					break;
				case IMAGE_HV_MIRROR:
					write_cmos_sensor(0x3820,((read_cmos_sensor(0x3820) & 0xFF) | 0x04));
					write_cmos_sensor(0x3821,((read_cmos_sensor(0x3821) & 0xFF) | 0x04));
					break;
				default:
					LOG_INF("Error image_mirror setting\n");
    }

}

static void night_mode(kal_bool enable)
{
}    

static void sensor_init(void)
{
    LOG_INF("%s ,E\n",__func__);	
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0103, 0x01);
	write_cmos_sensor(0x0300, 0x02);
	write_cmos_sensor(0x0302, 0x5b);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x0304, 0x00);
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x030d, 0x36);
	write_cmos_sensor(0x3022, 0x01);
	write_cmos_sensor(0x3013, 0x12);
	write_cmos_sensor(0x3016, 0x72);
	write_cmos_sensor(0x301b, 0xd0);
	write_cmos_sensor(0x301f, 0xd0);
	write_cmos_sensor(0x3106, 0x15);
	write_cmos_sensor(0x3107, 0x23);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0xc8);
	write_cmos_sensor(0x3502, 0x60);
	write_cmos_sensor(0x3508, 0x02);
	write_cmos_sensor(0x3509, 0x00);
	write_cmos_sensor(0x350a, 0x00);
	write_cmos_sensor(0x350e, 0x00);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3600, 0x2b);
	write_cmos_sensor(0x3601, 0x52);
	write_cmos_sensor(0x3602, 0x60);
	write_cmos_sensor(0x3612, 0x05);
	write_cmos_sensor(0x3613, 0xa4);
	write_cmos_sensor(0x3620, 0x80);
	write_cmos_sensor(0x3621, 0x08);
	write_cmos_sensor(0x3622, 0x60);
	write_cmos_sensor(0x3661, 0x80);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3664, 0x73);
	write_cmos_sensor(0x3665, 0xa7);
	write_cmos_sensor(0x366e, 0xff);
	write_cmos_sensor(0x366f, 0xf4);
	write_cmos_sensor(0x3674, 0x00);
	write_cmos_sensor(0x3679, 0x0c);
	write_cmos_sensor(0x367f, 0x01);
	write_cmos_sensor(0x3680, 0x0c);
	write_cmos_sensor(0x3681, 0x60);
	write_cmos_sensor(0x3682, 0x17);
	write_cmos_sensor(0x3683, 0xa9);
	write_cmos_sensor(0x3684, 0x9a);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x371a, 0x3e);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3738, 0xcc);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x373d, 0x26);
	write_cmos_sensor(0x3764, 0x20);
	write_cmos_sensor(0x3765, 0x20);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37c3, 0xf1);
	write_cmos_sensor(0x37c5, 0x00);
	write_cmos_sensor(0x37d8, 0x03);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37da, 0xc2);
	write_cmos_sensor(0x37dc, 0x02);
	write_cmos_sensor(0x37e0, 0x00);
	write_cmos_sensor(0x37e1, 0x04);
	write_cmos_sensor(0x37e2, 0x08);
	write_cmos_sensor(0x37e3, 0x04);
	write_cmos_sensor(0x37e4, 0x1a);
	write_cmos_sensor(0x37e5, 0x03);
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x0c);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3811, 0x18);
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3821, 0x00);
	write_cmos_sensor(0x3822, 0xc2);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x3832, 0x00);
	write_cmos_sensor(0x3c80, 0x00);
	write_cmos_sensor(0x3c87, 0x01);
	write_cmos_sensor(0x3c8c, 0x19);
	write_cmos_sensor(0x3c8d, 0x1c);
	write_cmos_sensor(0x3c90, 0x00);
	write_cmos_sensor(0x3c91, 0x00);
	write_cmos_sensor(0x3c92, 0x00);
	write_cmos_sensor(0x3c93, 0x00);
	write_cmos_sensor(0x3c94, 0x41);
	write_cmos_sensor(0x3c95, 0x54);
	write_cmos_sensor(0x3c96, 0x34);
	write_cmos_sensor(0x3c97, 0x04);
	write_cmos_sensor(0x3c98, 0x00);
	write_cmos_sensor(0x3d8c, 0x73);
	write_cmos_sensor(0x3d8d, 0xc0);
	write_cmos_sensor(0x3f00, 0x0b);
	write_cmos_sensor(0x4001, 0xe0);
	write_cmos_sensor(0x4008, 0x00);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4011, 0xf0);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x405e, 0x20);
	write_cmos_sensor(0x4503, 0x00);
	write_cmos_sensor(0x450a, 0x04);
	write_cmos_sensor(0x4809, 0x04);
	write_cmos_sensor(0x480c, 0x12);
	write_cmos_sensor(0x4833, 0x10);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
	write_cmos_sensor(0x4d00, 0x02);
	write_cmos_sensor(0x4d01, 0xc0);
	write_cmos_sensor(0x4d02, 0xd1);
	write_cmos_sensor(0x4d03, 0x90);
	write_cmos_sensor(0x4d04, 0x66);
	write_cmos_sensor(0x4d05, 0x65);
	write_cmos_sensor(0x5000, 0xef);
	write_cmos_sensor(0x5001, 0x07);
	write_cmos_sensor(0x5040, 0x39);
	write_cmos_sensor(0x5041, 0x40);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x02);
	write_cmos_sensor(0x5183, 0x0f);
	write_cmos_sensor(0x5200, 0x1b);
	write_cmos_sensor(0x520b, 0x07);
	write_cmos_sensor(0x520c, 0x0f);
	write_cmos_sensor(0x5300, 0x70);
	write_cmos_sensor(0x5301, 0x40);
	write_cmos_sensor(0x5302, 0x40);
	write_cmos_sensor(0x5303, 0x80);
	write_cmos_sensor(0x5304, 0x00);
	write_cmos_sensor(0x5305, 0x40);
	write_cmos_sensor(0x5306, 0x00);
	write_cmos_sensor(0x5307, 0x40);
	write_cmos_sensor(0x5308, 0x00);
	write_cmos_sensor(0x5309, 0x9d);
	write_cmos_sensor(0x530a, 0x00);
	write_cmos_sensor(0x530b, 0xe0);
	write_cmos_sensor(0x530c, 0x00);
	write_cmos_sensor(0x530d, 0xf0);
	write_cmos_sensor(0x530e, 0x01);
	write_cmos_sensor(0x530f, 0x10);
	write_cmos_sensor(0x5310, 0x01);
	write_cmos_sensor(0x5311, 0x20);
	write_cmos_sensor(0x5312, 0x01);
	write_cmos_sensor(0x5313, 0x20);
	write_cmos_sensor(0x5314, 0x01);
	write_cmos_sensor(0x5315, 0x20);
	write_cmos_sensor(0x5316, 0x08);
	write_cmos_sensor(0x5317, 0x08);
	write_cmos_sensor(0x5318, 0x10);
	write_cmos_sensor(0x5319, 0xbb);
	write_cmos_sensor(0x531a, 0xaa);
	write_cmos_sensor(0x531b, 0xaa);
	write_cmos_sensor(0x531c, 0xaa);
	write_cmos_sensor(0x531d, 0x0a);
	write_cmos_sensor(0x5405, 0x02);
	write_cmos_sensor(0x5406, 0x67);
	write_cmos_sensor(0x5407, 0x01);
	write_cmos_sensor(0x5408, 0x4a);

	write_cmos_sensor(0x0100, 0x00);
}    

static void sensor_init_for_new_PD(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0103, 0x01);
	write_cmos_sensor(0x0300, 0x02);
	write_cmos_sensor(0x0301, 0x00);
	write_cmos_sensor(0x0302, 0x5b);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x0304, 0x00);
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x030d, 0x36);
	write_cmos_sensor(0x3022, 0x01);
	write_cmos_sensor(0x3013, 0x12);
	write_cmos_sensor(0x3016, 0x72);
	write_cmos_sensor(0x301b, 0xF0);
	write_cmos_sensor(0x301f, 0xd0);
	write_cmos_sensor(0x3106, 0x15);
	write_cmos_sensor(0x3107, 0x23);
	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0x80);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x3508, 0x02);
	write_cmos_sensor(0x3509, 0x00);
	write_cmos_sensor(0x350a, 0x00);
	write_cmos_sensor(0x350e, 0x00);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);
	write_cmos_sensor(0x3600, 0x2b);
	write_cmos_sensor(0x3601, 0x52);
	write_cmos_sensor(0x3602, 0x60);
	write_cmos_sensor(0x3612, 0x05);
	write_cmos_sensor(0x3613, 0xa4);
	write_cmos_sensor(0x3620, 0x80);
	write_cmos_sensor(0x3621, 0x08);
	write_cmos_sensor(0x3622, 0x30);
	write_cmos_sensor(0x3624, 0x1c);
	write_cmos_sensor(0x3661, 0x80);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3664, 0x73);
	write_cmos_sensor(0x3665, 0xa7);
	write_cmos_sensor(0x366e, 0xff);
	write_cmos_sensor(0x366f, 0xf4);
	write_cmos_sensor(0x3674, 0x00);
	write_cmos_sensor(0x3679, 0x0c);
	write_cmos_sensor(0x367f, 0x01);
	write_cmos_sensor(0x3680, 0x0c);
	write_cmos_sensor(0x3681, 0x60);
	write_cmos_sensor(0x3682, 0x17);
	write_cmos_sensor(0x3683, 0xa9);
	write_cmos_sensor(0x3684, 0x9a);
	write_cmos_sensor(0x3709, 0x68);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x371a, 0x3e);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3738, 0xcc);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x373d, 0x26);
	write_cmos_sensor(0x3764, 0x20);
	write_cmos_sensor(0x3765, 0x20);
	write_cmos_sensor(0x37a1, 0x36);
	write_cmos_sensor(0x37a8, 0x3b);
	write_cmos_sensor(0x37ab, 0x31);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37c3, 0xf1);
	write_cmos_sensor(0x37c5, 0x00);
	write_cmos_sensor(0x37d8, 0x03);
	write_cmos_sensor(0x37d9, 0x0c);
	write_cmos_sensor(0x37da, 0xc2);
	write_cmos_sensor(0x37dc, 0x02);
	write_cmos_sensor(0x37e0, 0x00);
	write_cmos_sensor(0x37e1, 0x0a);
	write_cmos_sensor(0x37e2, 0x14);
	write_cmos_sensor(0x37e3, 0x04);
	write_cmos_sensor(0x37e4, 0x26);
	write_cmos_sensor(0x37e5, 0x03);
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x380a, 0x0c);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3821, 0x00);
	write_cmos_sensor(0x3822, 0xc2);
	write_cmos_sensor(0x3823, 0x18);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x3832, 0x00);
	write_cmos_sensor(0x3c80, 0x00);
	write_cmos_sensor(0x3c87, 0x01);
	write_cmos_sensor(0x3c8c, 0x19);
	write_cmos_sensor(0x3c8d, 0x1c);
	write_cmos_sensor(0x3c90, 0x00);
	write_cmos_sensor(0x3c91, 0x00);
	write_cmos_sensor(0x3c92, 0x00);
	write_cmos_sensor(0x3c93, 0x00);
	write_cmos_sensor(0x3c94, 0x41);
	write_cmos_sensor(0x3c95, 0x54);
	write_cmos_sensor(0x3c96, 0x34);
	write_cmos_sensor(0x3c97, 0x04);
	write_cmos_sensor(0x3c98, 0x00);
	write_cmos_sensor(0x3d8c, 0x73);
	write_cmos_sensor(0x3d8d, 0xc0);
	write_cmos_sensor(0x3f00, 0x0b);
	write_cmos_sensor(0x3f03, 0x00);
	write_cmos_sensor(0x4001, 0xe0);
	write_cmos_sensor(0x4008, 0x00);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4011, 0xf0);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4052, 0x00);
	write_cmos_sensor(0x4053, 0x80);
	write_cmos_sensor(0x4054, 0x00);
	write_cmos_sensor(0x4055, 0x80);
	write_cmos_sensor(0x4056, 0x00);
	write_cmos_sensor(0x4057, 0x80);
	write_cmos_sensor(0x4058, 0x00);
	write_cmos_sensor(0x4059, 0x80);
	write_cmos_sensor(0x405e, 0x20);
	write_cmos_sensor(0x4500, 0x07);
	write_cmos_sensor(0x4503, 0x00);
	write_cmos_sensor(0x450a, 0x04);
	write_cmos_sensor(0x4809, 0x04);
	write_cmos_sensor(0x480c, 0x12);
	write_cmos_sensor(0x4833, 0x10);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
	write_cmos_sensor(0x4d00, 0x03);
	write_cmos_sensor(0x4d01, 0xc9);
	write_cmos_sensor(0x4d02, 0xbc);
	write_cmos_sensor(0x4d03, 0xd7);
	write_cmos_sensor(0x4d04, 0xf0);
	write_cmos_sensor(0x4d05, 0xa2);
	write_cmos_sensor(0x5000, 0xff);
	write_cmos_sensor(0x5001, 0x07);
	write_cmos_sensor(0x5040, 0x39);
	write_cmos_sensor(0x5041, 0x10);
	write_cmos_sensor(0x5042, 0x10);
	write_cmos_sensor(0x5043, 0x84);
	write_cmos_sensor(0x5044, 0x62);
	write_cmos_sensor(0x5180, 0x00);
	write_cmos_sensor(0x5181, 0x10);
	write_cmos_sensor(0x5182, 0x02);
	write_cmos_sensor(0x5183, 0x0f);
	write_cmos_sensor(0x5200, 0x1b);
	write_cmos_sensor(0x520b, 0x07);
	write_cmos_sensor(0x520c, 0x0f);
	write_cmos_sensor(0x5300, 0x04);
	write_cmos_sensor(0x5301, 0x0C);
	write_cmos_sensor(0x5302, 0x0C);
	write_cmos_sensor(0x5303, 0x0f);
	write_cmos_sensor(0x5304, 0x00);
	write_cmos_sensor(0x5305, 0x70);
	write_cmos_sensor(0x5306, 0x00);
	write_cmos_sensor(0x5307, 0x80);
	write_cmos_sensor(0x5308, 0x00);
	write_cmos_sensor(0x5309, 0xa5);
	write_cmos_sensor(0x530a, 0x00);
	write_cmos_sensor(0x530b, 0xd3);
	write_cmos_sensor(0x530c, 0x00);
	write_cmos_sensor(0x530d, 0xf0);
	write_cmos_sensor(0x530e, 0x01);
	write_cmos_sensor(0x530f, 0x10);
	write_cmos_sensor(0x5310, 0x01);
	write_cmos_sensor(0x5311, 0x20);
	write_cmos_sensor(0x5312, 0x01);
	write_cmos_sensor(0x5313, 0x20);
	write_cmos_sensor(0x5314, 0x01);
	write_cmos_sensor(0x5315, 0x20);
	write_cmos_sensor(0x5316, 0x08);
	write_cmos_sensor(0x5317, 0x08);
	write_cmos_sensor(0x5318, 0x10);
	write_cmos_sensor(0x5319, 0x88);
	write_cmos_sensor(0x531a, 0x88);
	write_cmos_sensor(0x531b, 0xa9);
	write_cmos_sensor(0x531c, 0xaa);
	write_cmos_sensor(0x531d, 0x0a);
	write_cmos_sensor(0x5405, 0x02);
	write_cmos_sensor(0x5406, 0x67);
	write_cmos_sensor(0x5407, 0x01);
	write_cmos_sensor(0x5408, 0x4a);

	write_cmos_sensor(0x0100, 0x00);
}    

static void preview_setting(void)
{
	
	LOG_WARNING("[sensor_pick]%s :E setting (New)\n",__func__);
	write_cmos_sensor(0x0100, 0x00);
	
	write_cmos_sensor(0x0302, 0x5b);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x030d, 0x37);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37d9, 0x0c);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e1, 0x04);
		write_cmos_sensor(0x37e2, 0x08);
	}else{
		write_cmos_sensor(0x37e1, 0x0a);	
		write_cmos_sensor(0x37e2, 0x14);
	}
	write_cmos_sensor(0x37e3, 0x04);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e4, 0x1a);
	}else{
		write_cmos_sensor(0x37e4, 0x26);
	}
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x380a, 0x0c);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0xc4);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3811, 0x18);
	}else{
		write_cmos_sensor(0x3811, 0x10);
	}
	write_cmos_sensor(0x3813, 0x08);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3501, 0xc8);
		write_cmos_sensor(0x3502, 0x60);
	}else{
		write_cmos_sensor(0x3501, 0x80);	
		write_cmos_sensor(0x3502, 0x00);
	}
	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);
}    

int capture_first_flag = 0;
int pre_currefps = 0;
static void capture_setting(kal_uint16 currefps)
{
    LOG_INF("E! currefps:%d\n",currefps);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	preview_setting();
}

static void normal_video_setting(kal_uint16 currefps)
{
#if 1
    LOG_INF("E! currefps:%d\n",currefps);
    LOG_WARNING("[sensor_pick]%s :E setting (Use full 4:3)\n",__func__);
    preview_setting();
#else
	
    LOG_INF("E! currefps:%d\n",currefps);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0100, 0x00);
	
	write_cmos_sensor(0x0302, 0x5b);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x3662, 0x12);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x3737, 0x04);
	write_cmos_sensor(0x3739, 0x12);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37d9, 0x0c);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e1, 0x04);
		write_cmos_sensor(0x37e2, 0x08);
	}else{
		write_cmos_sensor(0x37e1, 0x0a);
		write_cmos_sensor(0x37e2, 0x14);
	}
	write_cmos_sensor(0x37e3, 0x04);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e4, 0x1a);
	}else{
		write_cmos_sensor(0x37e4, 0x26);
	}
	write_cmos_sensor(0x37e6, 0x04);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x57);
	write_cmos_sensor(0x3808, 0x10);
	write_cmos_sensor(0x3809, 0x80);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x10);
	write_cmos_sensor(0x3812, 0x01);
	write_cmos_sensor(0x3813, 0x84);
	write_cmos_sensor(0x380a, 0x09);
	write_cmos_sensor(0x380b, 0x48);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x8e);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x01);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xa8);
	write_cmos_sensor(0x3826, 0x11);
	write_cmos_sensor(0x3827, 0x1c);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x0f);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x0e);
	write_cmos_sensor(0x4902, 0x01);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3501, 0xc8);
		write_cmos_sensor(0x3502, 0x60);
	}else{
		write_cmos_sensor(0x3501, 0x80);
		write_cmos_sensor(0x3502, 0x00);
	}
	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);
#endif
}
static void hs_video_setting(void)
{
    LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	preview_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0100, 0x00);
	
	write_cmos_sensor(0x0302, 0x2e);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x030d, 0x36);
	write_cmos_sensor(0x3662, 0x08);
	write_cmos_sensor(0x3714, 0x30);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x2c);
	write_cmos_sensor(0x37d9, 0x06);
	write_cmos_sensor(0x37e1, 0x08);
	write_cmos_sensor(0x37e2, 0x10);
	write_cmos_sensor(0x37e3, 0x08);
	write_cmos_sensor(0x37e4, 0x30);
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x40);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x40);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x5f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x5f);
	write_cmos_sensor(0x3808, 0x04);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x02);
	write_cmos_sensor(0x380b, 0x40);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x03);
	write_cmos_sensor(0x380f, 0xea);
	write_cmos_sensor(0x3811, 0x06);
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x07);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x07);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xac);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x48);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4050, 0x02);
	write_cmos_sensor(0x4051, 0x05);
	write_cmos_sensor(0x4837, 0x1d);
	write_cmos_sensor(0x4902, 0x02);
	write_cmos_sensor(0x3501, 0x31);
	write_cmos_sensor(0x3502, 0xc0);
	
	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);
}

#if 0
static void slim_video_setting(void)
{
	
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0100, 0x00);

	write_cmos_sensor(0x0302, 0x2e);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x3662, 0x08);
	write_cmos_sensor(0x3714, 0x30);
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x2c);
	write_cmos_sensor(0x37d9, 0x06);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e1, 0x08);
		write_cmos_sensor(0x37e2, 0x10);
	}else{
		write_cmos_sensor(0x37e1, 0x0a);
		write_cmos_sensor(0x37e2, 0x14);
	}
	write_cmos_sensor(0x37e3, 0x08);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e4, 0x30);
	}else{
		write_cmos_sensor(0x37e4, 0x34);
	}
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x40);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x40);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x5f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x5f);
	write_cmos_sensor(0x3808, 0x03);
	write_cmos_sensor(0x3809, 0x00);
	write_cmos_sensor(0x380a, 0x01);
	write_cmos_sensor(0x380b, 0xb0);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x03);
	write_cmos_sensor(0x380f, 0x20);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3811, 0x06);
	}else{
		write_cmos_sensor(0x3811, 0x04);
	}
	write_cmos_sensor(0x3813, 0x04);
	write_cmos_sensor(0x3814, 0x07);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x07);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xac);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x48);
	write_cmos_sensor(0x3829, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4050, 0x02);
	write_cmos_sensor(0x4051, 0x05);
	write_cmos_sensor(0x4837, 0x1d);
	write_cmos_sensor(0x4902, 0x02);
	write_cmos_sensor(0x3501, 0x31);
	write_cmos_sensor(0x3502, 0xc0);

	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);
}
#endif

static void video_call_setting(kal_uint16 currefps)
{
	
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0100, 0x00);
	
	write_cmos_sensor(0x0302, 0x2e);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x030d, 0x36);
	write_cmos_sensor(0x3662, 0x10);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3714, 0x14);
	}else{
		write_cmos_sensor(0x3714, 0x28);
	}
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x37d9, 0x0c);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e1, 0x08);
		write_cmos_sensor(0x37e2, 0x10);
	}else{
		write_cmos_sensor(0x37e1, 0x0a);
		write_cmos_sensor(0x37e2, 0x14);
	}
	write_cmos_sensor(0x37e3, 0x08);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e4, 0x30);
	}else{
		write_cmos_sensor(0x37e4, 0x34);
	}
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x4f);
	write_cmos_sensor(0x3808, 0x08);
	write_cmos_sensor(0x3809, 0x40);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x380a, 0x06);
	write_cmos_sensor(0x380b, 0x20);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x19);
	write_cmos_sensor(0x380f, 0x20);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3811, 0x0c);
	}else{
		write_cmos_sensor(0x3811, 0x08);
	}
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x03);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xab);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x90);
	write_cmos_sensor(0x3829, 0x07);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x1d);
	write_cmos_sensor(0x4902, 0x01);
	write_cmos_sensor(0x3501, 0x64);
	write_cmos_sensor(0x3502, 0x00);

	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);
}

static void hyperlapse_setting(void)
{
	
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor(0x0100, 0x00);

	write_cmos_sensor(0x0302, 0x2e);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x030d, 0x36);
	write_cmos_sensor(0x3662, 0x10);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x3714, 0x14);
	}else{
		write_cmos_sensor(0x3714, 0x28);
	}
	write_cmos_sensor(0x3737, 0x08);
	write_cmos_sensor(0x3739, 0x20);
	write_cmos_sensor(0x37c2, 0x14);
	write_cmos_sensor(0x37d9, 0x0c);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e1, 0x08);
		write_cmos_sensor(0x37e2, 0x10);
	}else{
		write_cmos_sensor(0x37e1, 0x0a);
		write_cmos_sensor(0x37e2, 0x14);
	}
	write_cmos_sensor(0x37e3, 0x08);
	if(SettingForDiffPD == false){
		write_cmos_sensor(0x37e4, 0x30);
	}else{
		write_cmos_sensor(0x37e4, 0x34);
	}
	write_cmos_sensor(0x37e6, 0x08);
	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x00);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x08);
	write_cmos_sensor(0x3804, 0x10);
	write_cmos_sensor(0x3805, 0x9f);
	write_cmos_sensor(0x3806, 0x0c);
	write_cmos_sensor(0x3807, 0x4f);
	write_cmos_sensor(0x3808, 0x08);
	write_cmos_sensor(0x3809, 0x40);
	write_cmos_sensor(0x3810, 0x00);
	write_cmos_sensor(0x3811, 0x08);
	write_cmos_sensor(0x3812, 0x00);
	write_cmos_sensor(0x3813, 0xC0);
	write_cmos_sensor(0x380a, 0x04);
	write_cmos_sensor(0x380b, 0xa4);
	write_cmos_sensor(0x380c, 0x04);
	write_cmos_sensor(0x380d, 0x62);
	write_cmos_sensor(0x380e, 0x0c);
	write_cmos_sensor(0x380f, 0x90);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x03);
	write_cmos_sensor(0x3817, 0x01);
	write_cmos_sensor(0x3820, 0xab);
	write_cmos_sensor(0x3826, 0x04);
	write_cmos_sensor(0x3827, 0x90);
	write_cmos_sensor(0x3829, 0x07);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x4050, 0x04);
	write_cmos_sensor(0x4051, 0x0b);
	write_cmos_sensor(0x4837, 0x1d);
	write_cmos_sensor(0x4902, 0x01);
	write_cmos_sensor(0x3501, 0x64);
	write_cmos_sensor(0x3502, 0x00);

	write_cmos_sensor(0x0100, 0x01);
	mdelay(10);
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        
        
        write_cmos_sensor(0x5E00, 0x80);
    } else {
        
        
        write_cmos_sensor(0x5E00, 0x00);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

extern bool OV13855_read_eeprom_module_info(BYTE* data);

#define OTP_DATA_SIZE 32
char ov13855_otp_data[OTP_DATA_SIZE];
static int read_ov13855_otp_flag = 0;
static int PDAF_force_disabled = 0;

static int read_ov13855_otp(void){
	int ret = 0;

	LOG_INF("==========read_ov13855_otp_flag =%d=======\n",read_ov13855_otp_flag);
	if(read_ov13855_otp_flag !=0){
		LOG_INF("==========ov13855 otp readed=======\n");
		return 0;
	}

	memset(ov13855_otp_data, 0x0, OTP_DATA_SIZE);
	ret = OV13855_read_eeprom_module_info(ov13855_otp_data);
#if 1
	LOG_INF("OTP Module Vendor = 0x%x\n",                    ov13855_otp_data[0x00]);
	LOG_INF("OTP Lens Vendor = 0x%x\n",                      ov13855_otp_data[0x01]);
	LOG_INF("OTP Lens Rev = 0x%x\n",                         ov13855_otp_data[0x02]);
	LOG_INF("OTP Sensor Vendor = 0x%x\n",                    ov13855_otp_data[0x03]);
	LOG_INF("OTP Sensor Rev = 0x%x\n",                       ov13855_otp_data[0x04]);
	LOG_INF("OTP Drive IC Info = 0x%x\n",                    ov13855_otp_data[0x05]);
	LOG_INF("OTP Drive IC Rev = 0x%x\n",                     ov13855_otp_data[0x06]);
	LOG_INF("OTP Actuator Info = 0x%x\n",                    ov13855_otp_data[0x07]);
	LOG_INF("OTP Actuator Rev = 0x%x\n",                     ov13855_otp_data[0x08]);
	LOG_INF("OTP EEPROM vendor = 0x%x\n",                    ov13855_otp_data[0x09]);
	LOG_INF("OTP EEPROM Rev = 0x%x\n",                       ov13855_otp_data[0x0A]);
	LOG_INF("OTP PCB vendor = 0x%x\n",                       ov13855_otp_data[0x0B]);
	LOG_INF("OTP PCB Rev = 0x%x\n",                          ov13855_otp_data[0x0C]);
	LOG_INF("OTP Module Serial Number = 0x%x 0x%x 0x%x 0x%x\n",
		ov13855_otp_data[0x0D], ov13855_otp_data[0x0E], ov13855_otp_data[0x0F], ov13855_otp_data[0x10]);

	LOG_INF("OTP AF Infinity Position (High byte) = 0x%x\n", ov13855_otp_data[0x13]);
	LOG_INF("OTP AF Infinity Position (Low byte) = 0x%x\n",  ov13855_otp_data[0x14]);
	LOG_INF("OTP AF Macro Position (High byte) = 0x%x\n",    ov13855_otp_data[0x15]);
	LOG_INF("OTP AF Macro Position (Low byte) = 0x%x\n",     ov13855_otp_data[0x16]);
#endif

	if(ret == true) {
		spin_lock(&imgsensor_drv_lock);
		read_ov13855_otp_flag = 1;

#if 0
		if (ov13855_otp_data[0x04] == 0x02)
			PDAF_force_disabled = 0;
		else
			PDAF_force_disabled = 1;
#else
		PDAF_force_disabled = 0;
#endif

		spin_unlock(&imgsensor_drv_lock);

		LOG_INF("[PDAF] PDAF_force_disabled = %d\n", PDAF_force_disabled);
	}

	LOG_INF("==========exit ov13855 read_otp=======\n");
	return ret;
}

BYTE pdaf_cal_data_dummy[2048];
kal_uint16 offset_dummy = 0x100;
bool read_ov13855_pdaf_data(void){
	return read_otp_pdaf_data(offset_dummy, pdaf_cal_data_dummy, sizeof(pdaf_cal_data_dummy));
}

static ssize_t sensor_otp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned short inf_val;
	unsigned short mac_val;

	LOG_INF("OTP AF Infinity Position (High byte) = 0x%x\n", ov13855_otp_data[0x13]);
	LOG_INF("OTP AF Infinity Position (Low byte) = 0x%x\n", ov13855_otp_data[0x14]);
	LOG_INF("OTP AF Macro Position (High byte) = 0x%x\n",    ov13855_otp_data[0x15]);
	LOG_INF("OTP AF Macro Position (Low byte) = 0x%x\n",    ov13855_otp_data[0x16]);
	inf_val = (ov13855_otp_data[0x13]<<8 | ov13855_otp_data[0x14]);
	mac_val = (ov13855_otp_data[0x15]<<8 | ov13855_otp_data[0x16]);
	sprintf(buf, "lenc:%u:%u\n",inf_val,mac_val);
	ret = strlen(buf) + 1;

	return ret;
}


static const char *ov13855Vendor = "OmniVision";
static const char *ov13855NAME = "ov13855_main";
static const char *ov13855Size = "13.0M";

static ssize_t sensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", ov13855Vendor, ov13855NAME, ov13855Size);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);
static DEVICE_ATTR(otp, 0444, sensor_otp_show, NULL);

static struct kobject *android_ov13855;
static int first = true;

static int ov13855_sysfs_init(void)
{
	int ret ;
	if(first){
		LOG_INF("kobject creat and add\n");

		android_ov13855 = kobject_create_and_add("android_camera", NULL);
		
		if (android_ov13855 == NULL) {
			LOG_INF(" subsystem_register " \
			"failed\n");
			ret = -ENOMEM;
			return ret ;
		}
		LOG_INF("sysfs_create_file\n");
		ret = sysfs_create_file(android_ov13855, &dev_attr_sensor.attr);
		if (ret) {
			LOG_INF("sysfs_create_file " \
			"failed\n");
			kobject_del(android_ov13855);
		}else
			first = false;

		ret = sysfs_create_file(android_ov13855, &dev_attr_otp.attr);
		if (ret) {
			LOG_INF("sysfs_create_file " \
			"failed\n");
			kobject_del(android_ov13855);
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
                
                ov13855_sysfs_init();
                read_ov13855_otp();
                read_ov13855_pdaf_data();
                
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
                return ERROR_NONE;
            }
            LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
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
            LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;
    
		if ((read_cmos_sensor(0x302a))==0xb1)
		{
				LOG_INF("----R1A---- \n");
				ov13855_chip_ver = OV13855_R1A;
		}else if((read_cmos_sensor(0x302a))==0xb2)
		{
				LOG_INF("----R2A---- \n");
				ov13855_chip_ver = OV13855_R2A;
		}

    if ((read_cmos_sensor(0x36f0)) == 0x06){
        SettingForDiffPD = true;
        LOG_WARNING("[sensor_pick]New Sensor version: 0x36f0 = 0x%x, use new setting",read_cmos_sensor(0x36f0));
    }else if(((read_cmos_sensor(0x36f0)) >= 0x01)&&((read_cmos_sensor(0x36f0)) <= 0x05)) {
        LOG_WARNING("[sensor_pick]Old Sensor version: 0x36f0 = 0x%x, use old setting",read_cmos_sensor(0x36f0));
    	SettingForDiffPD = false;
    }else {
        LOG_WARNING("[sensor_pick]ERROR for read 0x36f0 = 0x%x",read_cmos_sensor(0x36f0));
    }

    if (SettingForDiffPD == false) {
        
        sensor_init();
    }else {
        sensor_init_for_new_PD();
    }

	  
	  
	  write_cmos_sensor(0x0100, 0x00);
	  
    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = imgsensor_info.ihdr_support;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    
	capture_first_flag = 0;
	pre_currefps = 0;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}    



static kal_uint32 close(void)
{
    LOG_INF("E\n");

    
    if (GroupHoldStart){
        write_cmos_sensor(0x3208, 0x10);   
        write_cmos_sensor(0x3208, 0xA0);   
        GroupHoldStart = false;
    }
    

    

    return ERROR_NONE;
}    


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s :E\n",__func__);
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
    set_mirror_flip(imgsensor.mirror);
	
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
            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
	 set_mirror_flip(imgsensor.mirror);
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

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	LOG_WARNING("[sensor_pick]%s :E\n",__func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hyperlapse_setting();
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
	
		set_mirror_flip(imgsensor.mirror);
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
	
		set_mirror_flip(imgsensor.mirror);
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
	
		set_mirror_flip(imgsensor.mirror);
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


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight     = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth     = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

    sensor_resolution->SensorCustom2Width  = imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

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
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; 
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 1;
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
        case MSDK_SCENARIO_ID_CUSTOM2:
            sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
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
			capture_first_flag = 0;
			pre_currefps = 0;
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            capture(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			capture_first_flag = 0;
			pre_currefps = 0;
            normal_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			capture_first_flag = 0;
			pre_currefps = 0;
            hs_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
			capture_first_flag = 0;
			pre_currefps = 0;
            slim_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            Custom1(image_window, sensor_config_data); 
        break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            Custom2(image_window, sensor_config_data); 
        break;
        default:
            LOG_INF("Error ScenarioId setting");
			capture_first_flag = 0;
			pre_currefps = 0;
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

    LOG_WARNING("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            
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
            
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            framerate=960;
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            LOG_INF("ov13855 set framerate=960 ,  frame_length = 0x%x .  \n", frame_length);
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
        case MSDK_SCENARIO_ID_CUSTOM2:
            frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
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
        case MSDK_SCENARIO_ID_CUSTOM2:
            *framerate = imgsensor_info.custom2.max_framerate;
            break;
        default:
            break;
    }

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
          
    SET_PD_BLOCK_INFO_T *PDAFinfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
    
    if (GroupHoldStart && (feature_id != SENSOR_FEATURE_SET_GAIN)){
        write_cmos_sensor(0x3208, 0x10);   
        write_cmos_sensor(0x3208, 0xA0);   
        GroupHoldStart = false;
        LOG_WARNING("[GroupHold]feature_id != SENSOR_FEATURE_SET_GAIN\n");
    }
    
    switch (feature_id) {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            
            write_cmos_sensor(0x3208, 0x00);   
            GroupHoldStart = true;
            
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
            
            if(GroupHoldStart == true){
                write_cmos_sensor(0x3208, 0x10);   
                write_cmos_sensor(0x3208, 0xA0);   
                GroupHoldStart = false;
            }
            
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
        case SENSOR_FEATURE_GET_PDAF_DATA:
          
			read_otp_pdaf_data((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
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
            LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.ihdr_en = (BOOL)*feature_data;
            spin_unlock(&imgsensor_drv_lock);
			
            break;
        case SENSOR_FEATURE_GET_CROP_INFO:
            

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
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
             break;
        
        case SENSOR_FEATURE_GET_PDAF_INFO:
            
            PDAFinfo= (SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

            switch (*feature_data) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(SET_PD_BLOCK_INFO_T));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    break;
            }
            break;
        case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
            
            
            switch (*feature_data) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; 
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
                    break;
                default:
                    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
                    break;
            }
            if (PDAF_force_disabled)
                *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            LOG_INF("[PDAF] SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY = %d\n", *(MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            
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

UINT32 OV13855_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;

    return ERROR_NONE;
}    
