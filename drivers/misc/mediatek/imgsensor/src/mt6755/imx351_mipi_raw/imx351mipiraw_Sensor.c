
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

#include "imx351mipiraw_Sensor.h"

#define PFX "IMX351_camera_sensor"
#define LOG_1 LOG_INF("IMX351,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 4656*3496@30fps; video 4656*2620@30fps; capture 16M@30fps\n")
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_WARNING(format, args...)    pr_warning(PFX "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static kal_uint16 g_a_gain = 0;
static kal_uint16 g_d_gain = 0;
static kal_uint16 g_shutter = 0;
static kal_uint32 g_frame_length = 0;

 
#define EEPROM_DATA_SIZE (2024)
BYTE imx351_eeprom_data[EEPROM_DATA_SIZE];
static int read_imx351_otp_flag = 0;

static kal_uint16 module_cut_ver = 0;
static int module_new_setting = 0;


static imgsensor_info_struct imgsensor_info = {
    .sensor_id = IMX351_SENSOR_ID,        

    .checksum_value = 0xafd83a68,        

    .pre = {
        .pclk = 703200000,                
        .linelength = 6032,                
        .framelength = 3880,            
        .startx = 0,                    
        .starty = 0,                    
        .grabwindow_width = 4656,        
        .grabwindow_height = 3496,        
        
        .mipi_data_lp2hs_settle_dc = 85,
        
        .max_framerate = 300,
    },
    .cap = { 
        .pclk = 703200000,
        .linelength = 6032,
        .framelength = 3880,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4656,
        .grabwindow_height = 3496,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .cap1 = { 
        .pclk = 703200000,
        .linelength = 6032,
        .framelength = 3880,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4656,
        .grabwindow_height = 3496,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .normal_video = { 
        .pclk = 250800000,
        .linelength = 3220,
        .framelength = 2596,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2328,
        .grabwindow_height = 1744,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .hs_video = { 
        .pclk = 250800000,
        .linelength = 3220,
        .framelength = 2596,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2328,
        .grabwindow_height = 1744,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .slim_video = { 
        .pclk = 250800000,
        .linelength = 3220,
        .framelength = 2596,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2328,
        .grabwindow_height = 1744,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .custom1 = { 
        .pclk = 250800000,
        .linelength = 3220,
        .framelength = 5192,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2328,
        .grabwindow_height = 1744,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 150,
    },
    .custom2 = { 
        .pclk = 250800000,
        .linelength = 3220,
        .framelength = 2596,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2328,
        .grabwindow_height = 1744,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .custom3 = { 
        .pclk = 250800000,
        .linelength = 3220,
        .framelength = 2596,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2328,
        .grabwindow_height = 1744,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .margin = 4,            
    .min_shutter = 8,        
    .max_frame_length = 0x7fff,
    .ae_shut_delay_frame = 0,    
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,      
    .ihdr_le_firstline = 0,  
    .sensor_mode_num = 8,      

    .cap_delay_frame = 3,        
    .pre_delay_frame = 3,         
    .video_delay_frame = 3,        
    .hs_video_delay_frame = 3,    
    .slim_video_delay_frame = 3,
    .custom1_delay_frame = 3,
    .custom2_delay_frame = 3,
    .custom3_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_8MA, 
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, 
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B, 
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = {0x34, 0x20, 0xff},
};


static imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,                
    .sensor_mode = IMGSENSOR_MODE_INIT, 
    .shutter = 0x3D0,                    
    .gain = 0x100,                        
    .dummy_pixel = 0,                    
    .dummy_line = 0,                    
    .current_fps = 300,  
    .autoflicker_en = KAL_FALSE,  
    .test_pattern = KAL_FALSE,        
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_mode = 0, 
    .i2c_write_id = 0x34,
};


static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] =
{{ 4688, 3648,   16,  136, 4656, 3496, 4656, 3496, 0000, 0000, 4656, 3496,      0,    0, 4656, 3496}, 
 { 4688, 3648,   16,  136, 4656, 3496, 4656, 3496, 0000, 0000, 4656, 3496,      0,    0, 4656, 3496}, 
 { 4688, 3648,   16,  136, 4656, 3496, 2328, 1744, 0000, 0000, 2328, 1744,      0,    0, 2328, 1744}, 
 { 4688, 3648,   16,  136, 4656, 3496, 2328, 1744, 0000, 0000, 2328, 1744,      0,    0, 2328, 1744}, 
 { 4688, 3648,   16,  136, 4656, 3496, 2328, 1744, 0000, 0000, 2328, 1744,      0,    0, 2328, 1744}, 
 { 4688, 3648,   16,  136, 4656, 3496, 2328, 1744, 0000, 0000, 2328, 1744,      0,    0, 2328, 1744}, 
 { 4688, 3648,   16,  136, 4656, 3496, 2328, 1744, 0000, 0000, 2328, 1744,      0,    0, 2328, 1744}, 
 { 4688, 3648,   16,  136, 4656, 3496, 2328, 1744, 0000, 0000, 2328, 1744,      0,    0, 2328, 1744}};

 
 static SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3]=
 {
 {0x03, 0x0a,   0x00,   0x08, 0x40, 0x00, 
  0x00, 0x2b, 0x0A70, 0x07D8, 0x00, 0x35, 0x0280, 0x0001,
  0x00, 0x36, 0x0C48, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
  
  {0x03, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
   0x00, 0x2b, 0x14E0, 0x0FB0, 0x00, 0x35, 0x0280, 0x0001,
  0x00, 0x36, 0x1a18, 0x0001, 0x03, 0x00, 0x0000, 0x0000},
   
  {0x02, 0x0a,	 0x00,	 0x08, 0x40, 0x00,
   0x00, 0x2b, 0x14E0, 0x0FB0, 0x01, 0x00, 0x0000, 0x0000,
   0x02, 0x00, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000}
};

#define IMX351MIPI_MaxGainIndex (115)
kal_uint16 IMX351MIPI_sensorGainMapping[IMX351MIPI_MaxGainIndex][2] ={	
	{64 ,0	},
	{65 ,8	},
	{66 ,16 },
	{67 ,25 },
	{68 ,30 },
	{69 ,37 },
	{70 ,45 },
	{71 ,51 },
	{72 ,57 },
	{73 ,63 },
	{74 ,67 },
	{75 ,75 },
	{76 ,81 },
	{77 ,85 },
	{78 ,92 },
	{79 ,96 },
	{80 ,103},
	{81 ,107},
	{82 ,112},
	{83 ,118},
	{84 ,122},
	{86 ,133},
	{88 ,140},
	{89 ,144},
	{90 ,148},
	{93 ,159},
	{96 ,171},
	{97 ,175},
	{99 ,182},
	{101,188},
	{102,192},
	{104,197},
	{106,202},
	{107,206},
	{109,211},
	{112,220},
	{113,222},
	{115,228},
	{118,235},
	{120,239},
	{125,250},
	{126,252},
	{128,256},
	{129,258},
	{130,260},
	{132,264},
	{133,266},
	{135,269},
	{136,271},
	{138,274},
	{139,276},
	{141,279},
	{142,282},
	{144,285},
	{145,286},
	{147,290},
	{149,292},
	{150,294},
	{155,300},
	{157,303},
	{158,305},
	{161,309},
	{163,311},
	{170,319},
	{172,322},
	{174,324},
	{176,326},
	{179,329},
	{181,331},
	{185,335},
	{189,339},
	{193,342},
	{195,344},
	{196,345},
	{200,348},
	{202,350},
	{205,352},
	{207,354},
	{210,356},
	{211,357},
	{214,359},
	{217,361},
	{218,362},
	{221,364},
	{224,366},
	{231,370},
	{237,374},
	{246,379},
	{250,381},
	{252,382},
	{256,384},
	{260,386},
	{262,387},
	{273,392},
	{275,393},
	{280,395},
	{290,399},
	{306,405},
	{312,407},
	{321,410},
	{331,413},
	{345,417},
	{352,419},
	{360,421},
	{364,422},
	{372,424},
	{386,427},
	{400,430},
	{410,432},
	{420,434},
	{431,436},
	{437,437},
	{449,439},
	{468,442},
	{512,448},
};


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
    
	

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);	  
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

	
}    

static kal_uint32 return_sensor_id(void)
{
    return ((read_cmos_sensor(0x0016) << 8) | read_cmos_sensor(0x0017));
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



static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    
    
    

    
    
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

    
    write_cmos_sensor(0x0104, 0x01);
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

    
    write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, shutter  & 0xFF);	
    g_frame_length = imgsensor.frame_length;
    g_shutter = shutter;

    
    if (g_a_gain != 0) {
        write_cmos_sensor(0x0204, (g_a_gain>>8)& 0xFF);
        write_cmos_sensor(0x0205, g_a_gain & 0xFF);
    }
    
    if (g_d_gain != 0) {
        write_cmos_sensor(0x020E, (g_d_gain>>8)& 0xFF);
        write_cmos_sensor(0x020F, g_d_gain & 0xFF);
    }
    write_cmos_sensor(0x0104, 0x00);    
    LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

}    



static kal_uint16 gain2reg(const kal_uint16 gain)
{
#if 0
	kal_uint8 iI;
    LOG_INF("[IMX351MIPI]enter IMX351MIPIGain2Reg function\n");
    for (iI = 0; iI < IMX351MIPI_MaxGainIndex; iI++) 
	{
		if(gain < IMX351MIPI_sensorGainMapping[iI][0])
		{                
			return IMX351MIPI_sensorGainMapping[iI][1];       
		}
			

    }
    if(gain != IMX351MIPI_sensorGainMapping[iI][0])
    {
         LOG_INF("Gain mapping don't correctly:%d %d \n", gain, IMX351MIPI_sensorGainMapping[iI][0]);
    }
	LOG_INF("exit IMX351MIPIGain2Reg function\n");
    return IMX351MIPI_sensorGainMapping[iI-1][1];
#endif

	kal_uint16 reg_gain;

	reg_gain = 1024 - ( (1024 * BASEGAIN) / gain);
	if (reg_gain > 960)
		reg_gain = 960;

	return reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_a_gain;
    kal_uint16 reg_d_gain;

#if 0
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
        LOG_INF("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
    }
#else
    if (gain < BASEGAIN) {
        LOG_INF("Error gain setting");
        gain = BASEGAIN;
    }
#endif

    
    reg_a_gain = gain2reg(gain);

    
    if(gain > 16 * BASEGAIN) { 
        
        reg_d_gain = (kal_uint16)((((kal_uint32)gain*16))/BASEGAIN);
        if (reg_d_gain < 0x0100)
            reg_d_gain = 0x0100;
        if (reg_d_gain > 0x0FFF)
            reg_d_gain = 0x0FFF;
    } else
        reg_d_gain = 0x0100; 

    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_a_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_a_gain = 0x%x , reg_d_gain = 0x%x\n ", gain, reg_a_gain, reg_d_gain);

	write_cmos_sensor(0x0104, 0x01);
    if ( (g_shutter != 0) && (g_frame_length != 0) ) {
        write_cmos_sensor(0x0340, g_frame_length >> 8);
        write_cmos_sensor(0x0341, g_frame_length & 0xFF);

        write_cmos_sensor(0x0202, (g_shutter >> 8) & 0xFF);
        write_cmos_sensor(0x0203, g_shutter  & 0xFF);
    }

    g_a_gain = reg_a_gain;
    g_d_gain = reg_d_gain;
    
    write_cmos_sensor(0x0204, (reg_a_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_a_gain & 0xFF);
    
    write_cmos_sensor(0x020E, (reg_d_gain>>8)& 0xFF);
    write_cmos_sensor(0x020F, reg_d_gain & 0xFF);

    write_cmos_sensor(0x0104, 0x00);

    return gain;
}    

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

    kal_uint16 realtime_fps = 0;
    kal_uint16 reg_gain;
    LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
    spin_lock(&imgsensor_drv_lock);
    if (le > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = le + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;

    
    write_cmos_sensor(0x0104, 0x01);
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
    
    write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
    write_cmos_sensor(0x0203, le  & 0xFF);
    
    write_cmos_sensor(0x0224, (se >> 8) & 0xFF);
    write_cmos_sensor(0x0225, se  & 0xFF); 
    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain; 
    spin_unlock(&imgsensor_drv_lock);
    
    write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_gain & 0xFF);
    
    write_cmos_sensor(0x0216, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0217, reg_gain & 0xFF);
    write_cmos_sensor(0x0104, 0x00);

}


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
    LOG_INF("image_mirror = %d\n", image_mirror);


    switch (image_mirror) {
        case IMAGE_NORMAL:
			write_cmos_sensor(0x0101, 0x00);
			write_cmos_sensor(0x3A27, 0x00);
			write_cmos_sensor(0x3A28, 0x00);
			write_cmos_sensor(0x3A29, 0x01);
			write_cmos_sensor(0x3A2A, 0x00);
			write_cmos_sensor(0x3A2B, 0x00);
			write_cmos_sensor(0x3A2C, 0x00);
			write_cmos_sensor(0x3A2D, 0x01);
			write_cmos_sensor(0x3A2E, 0x01);
			break;
        case IMAGE_H_MIRROR:
			write_cmos_sensor(0x0101, 0x01);
			write_cmos_sensor(0x3A27, 0x01);
			write_cmos_sensor(0x3A28, 0x01);
			write_cmos_sensor(0x3A29, 0x00);
			write_cmos_sensor(0x3A2A, 0x00);
			write_cmos_sensor(0x3A2B, 0x01);
			write_cmos_sensor(0x3A2C, 0x00);
			write_cmos_sensor(0x3A2D, 0x00);
			write_cmos_sensor(0x3A2E, 0x01);
            break;
        case IMAGE_V_MIRROR:
			write_cmos_sensor(0x0101, 0x10);
			write_cmos_sensor(0x3A27, 0x10);
			write_cmos_sensor(0x3A28, 0x10);
			write_cmos_sensor(0x3A29, 0x01);
			write_cmos_sensor(0x3A2A, 0x01);
			write_cmos_sensor(0x3A2B, 0x00);
			write_cmos_sensor(0x3A2C, 0x01);
			write_cmos_sensor(0x3A2D, 0x01);
			write_cmos_sensor(0x3A2E, 0x00);
            break;
        case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x0101, 0x11);
			write_cmos_sensor(0x3A27, 0x11);
			write_cmos_sensor(0x3A28, 0x11);
			write_cmos_sensor(0x3A29, 0x00);
			write_cmos_sensor(0x3A2A, 0x01);
			write_cmos_sensor(0x3A2B, 0x01);
			write_cmos_sensor(0x3A2C, 0x01);
			write_cmos_sensor(0x3A2D, 0x00);
			write_cmos_sensor(0x3A2E, 0x00);
            break;
        default:
            LOG_INF("Error image_mirror setting\n");
    }

}
#endif

static void night_mode(kal_bool enable)
{
}    


 
static void PDAF_write_SPC_data(void)
{
	int i;

	if(read_imx351_otp_flag == 0) {
		LOG_INF("OTP data is NOT read !\n");
		return;
	}

	LOG_INF("PDAF write SPC data\n");
	
	for (i=0; i<=69; i++) {
		write_cmos_sensor(0x7500 + i, imx351_eeprom_data[(0x0758 + i)]);
	}
	
	for (i=0; i<=69; i++) {
		write_cmos_sensor(0x7548 + i, imx351_eeprom_data[(0x079E + i)]);
	}
}

static void ImageQuality_setting(void)
{
	write_cmos_sensor(0x7B80, 0x00);
	write_cmos_sensor(0x7B81, 0x00);
	write_cmos_sensor(0xA900, 0x20);
	write_cmos_sensor(0xA901, 0x20);
	write_cmos_sensor(0xA902, 0x20);
	write_cmos_sensor(0xA903, 0x15);
	write_cmos_sensor(0xA904, 0x15);
	write_cmos_sensor(0xA905, 0x15);
	write_cmos_sensor(0xA906, 0x20);
	write_cmos_sensor(0xA907, 0x20);
	write_cmos_sensor(0xA908, 0x20);
	write_cmos_sensor(0xA909, 0x15);
	write_cmos_sensor(0xA90A, 0x15);
	write_cmos_sensor(0xA90B, 0x15);
	write_cmos_sensor(0xA915, 0x3F);
	write_cmos_sensor(0xA916, 0x3F);
	write_cmos_sensor(0xA917, 0x3F);
	write_cmos_sensor(0xA949, 0x03);
	write_cmos_sensor(0xA94B, 0x03);
	write_cmos_sensor(0xA94D, 0x03);
	write_cmos_sensor(0xA94F, 0x06);
	write_cmos_sensor(0xA951, 0x06);
	write_cmos_sensor(0xA953, 0x06);
	write_cmos_sensor(0xA955, 0x03);
	write_cmos_sensor(0xA957, 0x03);
	write_cmos_sensor(0xA959, 0x03);
	write_cmos_sensor(0xA95B, 0x06);
	write_cmos_sensor(0xA95D, 0x06);
	write_cmos_sensor(0xA95F, 0x06);
	write_cmos_sensor(0xA98B, 0x1F);
	write_cmos_sensor(0xA98D, 0x1F);
	write_cmos_sensor(0xA98F, 0x1F);
	write_cmos_sensor(0xAA20, 0x20);
	write_cmos_sensor(0xAA21, 0x20);
	write_cmos_sensor(0xAA22, 0x20);
	write_cmos_sensor(0xAA23, 0x15);
	write_cmos_sensor(0xAA24, 0x15);
	write_cmos_sensor(0xAA25, 0x15);
	write_cmos_sensor(0xAA26, 0x20);
	write_cmos_sensor(0xAA27, 0x20);
	write_cmos_sensor(0xAA28, 0x20);
	write_cmos_sensor(0xAA29, 0x15);
	write_cmos_sensor(0xAA2A, 0x15);
	write_cmos_sensor(0xAA2B, 0x15);
	write_cmos_sensor(0xAA35, 0x3F);
	write_cmos_sensor(0xAA36, 0x3F);
	write_cmos_sensor(0xAA37, 0x3F);
	write_cmos_sensor(0xAA69, 0x03);
	write_cmos_sensor(0xAA6B, 0x03);
	write_cmos_sensor(0xAA6D, 0x03);
	write_cmos_sensor(0xAA6F, 0x06);
	write_cmos_sensor(0xAA71, 0x06);
	write_cmos_sensor(0xAA73, 0x06);
	write_cmos_sensor(0xAA75, 0x03);
	write_cmos_sensor(0xAA77, 0x03);
	write_cmos_sensor(0xAA79, 0x03);
	write_cmos_sensor(0xAA7B, 0x06);
	write_cmos_sensor(0xAA7D, 0x06);
	write_cmos_sensor(0xAA7F, 0x06);
	write_cmos_sensor(0xAAAB, 0x1F);
	write_cmos_sensor(0xAAAD, 0x1F);
	write_cmos_sensor(0xAAAF, 0x1F);
	write_cmos_sensor(0xAAB0, 0x20);
	write_cmos_sensor(0xAAB1, 0x20);
	write_cmos_sensor(0xAAB2, 0x20);
	write_cmos_sensor(0xAB53, 0x20);
	write_cmos_sensor(0xAB54, 0x20);
	write_cmos_sensor(0xAB55, 0x20);
	write_cmos_sensor(0xAB57, 0x40);
	write_cmos_sensor(0xAB59, 0x40);
	write_cmos_sensor(0xAB5B, 0x40);
	write_cmos_sensor(0xAB63, 0x03);
	write_cmos_sensor(0xAB65, 0x03);
	write_cmos_sensor(0xAB67, 0x03);
}

static void sensor_init(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	
	write_cmos_sensor(0x0136, 0x18);
	write_cmos_sensor(0x0137, 0x00);

	
	write_cmos_sensor(0x3C7D, 0x18);
	write_cmos_sensor(0x3C7E, 0x02);
	write_cmos_sensor(0x3C7F, 0x06);

	
	write_cmos_sensor(0x3140, 0x02);
	write_cmos_sensor(0x3F7F, 0x01);
	write_cmos_sensor(0x4430, 0x05);
	write_cmos_sensor(0x4431, 0xDC);
	write_cmos_sensor(0x5222, 0x02);
	write_cmos_sensor(0x5617, 0x0A);
	write_cmos_sensor(0x562B, 0x0A);
	write_cmos_sensor(0x562D, 0x0C);
	write_cmos_sensor(0x56B7, 0x74);
	write_cmos_sensor(0x6200, 0x95);
	write_cmos_sensor(0x6201, 0xB9);
	write_cmos_sensor(0x6202, 0x58);
	write_cmos_sensor(0x6220, 0x05);
	write_cmos_sensor(0x6222, 0x0C);
	write_cmos_sensor(0x6225, 0x19);
	write_cmos_sensor(0x6228, 0x32);
	write_cmos_sensor(0x6229, 0x70);
	write_cmos_sensor(0x622A, 0xC3);
	write_cmos_sensor(0x622B, 0x64);
	write_cmos_sensor(0x622C, 0x54);
	write_cmos_sensor(0x622E, 0xB8);
	write_cmos_sensor(0x622F, 0xA8);
	write_cmos_sensor(0x6231, 0x73);
	write_cmos_sensor(0x6233, 0x42);
	write_cmos_sensor(0x6234, 0xEA);
	write_cmos_sensor(0x6235, 0x8C);
	write_cmos_sensor(0x6236, 0x35);
	write_cmos_sensor(0x6237, 0xC5);
	write_cmos_sensor(0x6239, 0x0C);
	write_cmos_sensor(0x623A, 0x96);
	write_cmos_sensor(0x623C, 0x19);
	write_cmos_sensor(0x623F, 0x32);
	write_cmos_sensor(0x6240, 0x59);
	write_cmos_sensor(0x6241, 0x83);
	write_cmos_sensor(0x6242, 0x64);
	write_cmos_sensor(0x6243, 0x54);
	write_cmos_sensor(0x6245, 0xA8);
	write_cmos_sensor(0x6246, 0xA8);
	write_cmos_sensor(0x6248, 0x53);
	write_cmos_sensor(0x624A, 0x42);
	write_cmos_sensor(0x624B, 0xA5);
	write_cmos_sensor(0x624C, 0x98);
	write_cmos_sensor(0x624D, 0x35);
	write_cmos_sensor(0x6265, 0x45);
	write_cmos_sensor(0x6267, 0x0C);
	write_cmos_sensor(0x626A, 0x19);
	write_cmos_sensor(0x626D, 0x32);
	write_cmos_sensor(0x626E, 0x72);
	write_cmos_sensor(0x626F, 0xC3);
	write_cmos_sensor(0x6270, 0x64);
	write_cmos_sensor(0x6271, 0x58);
	write_cmos_sensor(0x6273, 0xC8);
	write_cmos_sensor(0x6276, 0x91);
	write_cmos_sensor(0x6279, 0x27);
	write_cmos_sensor(0x627B, 0x36);
	write_cmos_sensor(0x6282, 0x83);
	write_cmos_sensor(0x6283, 0x22);
	write_cmos_sensor(0x6284, 0x0A);
	write_cmos_sensor(0x6285, 0x60);
	write_cmos_sensor(0x6286, 0x09);
	write_cmos_sensor(0x6287, 0x85);
	write_cmos_sensor(0x6288, 0x18);
	write_cmos_sensor(0x6289, 0xC0);
	write_cmos_sensor(0x628A, 0x1C);
	write_cmos_sensor(0x628B, 0x8C);
	write_cmos_sensor(0x628C, 0x41);
	write_cmos_sensor(0x628D, 0x80);
	write_cmos_sensor(0x628E, 0x38);
	write_cmos_sensor(0x628F, 0x18);
	write_cmos_sensor(0x6290, 0x83);
	write_cmos_sensor(0x6292, 0x1A);
	write_cmos_sensor(0x6293, 0x20);
	write_cmos_sensor(0x6294, 0x88);
	write_cmos_sensor(0x6296, 0x6C);
	write_cmos_sensor(0x6297, 0x41);
	write_cmos_sensor(0x6298, 0x4C);
	write_cmos_sensor(0x6299, 0x01);
	write_cmos_sensor(0x629A, 0x38);
	write_cmos_sensor(0x629B, 0x93);
	write_cmos_sensor(0x629C, 0x18);
	write_cmos_sensor(0x629D, 0x03);
	write_cmos_sensor(0x629E, 0x91);
	write_cmos_sensor(0x629F, 0x87);
	write_cmos_sensor(0x62A0, 0x30);
	write_cmos_sensor(0x62B1, 0x14);
	write_cmos_sensor(0x62B2, 0x20);
	write_cmos_sensor(0x62B3, 0x48);
	write_cmos_sensor(0x62B5, 0x58);
	write_cmos_sensor(0x62B6, 0x40);
	write_cmos_sensor(0x62B7, 0x8E);
	write_cmos_sensor(0x62B8, 0x01);
	write_cmos_sensor(0x62B9, 0x08);
	write_cmos_sensor(0x62BA, 0x91);
	write_cmos_sensor(0x62BB, 0x1C);
	write_cmos_sensor(0x62BC, 0x03);
	write_cmos_sensor(0x62BD, 0x21);
	write_cmos_sensor(0x62BE, 0x83);
	write_cmos_sensor(0x62BF, 0x38);
	write_cmos_sensor(0x62D0, 0x14);
	write_cmos_sensor(0x62D1, 0x20);
	write_cmos_sensor(0x62D2, 0x67);
	write_cmos_sensor(0x62D4, 0x54);
	write_cmos_sensor(0x62D5, 0x40);
	write_cmos_sensor(0x62D6, 0xCE);
	write_cmos_sensor(0x62D8, 0xF8);
	write_cmos_sensor(0x62D9, 0x92);
	write_cmos_sensor(0x62DA, 0x9C);
	write_cmos_sensor(0x62DB, 0x02);
	write_cmos_sensor(0x62DC, 0xE1);
	write_cmos_sensor(0x62DD, 0x85);
	write_cmos_sensor(0x62DE, 0x38);
	write_cmos_sensor(0x637A, 0x11);
	write_cmos_sensor(0x637B, 0xD4);
	write_cmos_sensor(0x6388, 0x22);
	write_cmos_sensor(0x6389, 0x82);
	write_cmos_sensor(0x638A, 0xC8);
	write_cmos_sensor(0x7BA0, 0x01);
	write_cmos_sensor(0x7BA9, 0x00);
	write_cmos_sensor(0x7BAA, 0x01);
	write_cmos_sensor(0x7BAD, 0x00);
	write_cmos_sensor(0x9002, 0x00);
	write_cmos_sensor(0x9003, 0x00);
	write_cmos_sensor(0x9004, 0x09);
	write_cmos_sensor(0x9006, 0x01);
	write_cmos_sensor(0x9200, 0x93);
	write_cmos_sensor(0x9201, 0x85);
	write_cmos_sensor(0x9202, 0x93);
	write_cmos_sensor(0x9203, 0x87);
	write_cmos_sensor(0x9204, 0x93);
	write_cmos_sensor(0x9205, 0x8D);
	write_cmos_sensor(0x9206, 0x93);
	write_cmos_sensor(0x9207, 0x8F);
	write_cmos_sensor(0x9208, 0x6A);
	write_cmos_sensor(0x9209, 0x22);
	write_cmos_sensor(0x920A, 0x6A);
	write_cmos_sensor(0x920B, 0x23);
	write_cmos_sensor(0x920C, 0x6A);
	write_cmos_sensor(0x920D, 0x0F);
	write_cmos_sensor(0x920E, 0x71);
	write_cmos_sensor(0x920F, 0x03);
	write_cmos_sensor(0x9210, 0x71);
	write_cmos_sensor(0x9211, 0x0B);
	write_cmos_sensor(0x935D, 0x01);
	write_cmos_sensor(0x9389, 0x05);
	write_cmos_sensor(0x938B, 0x05);
	write_cmos_sensor(0x9391, 0x05);
	write_cmos_sensor(0x9393, 0x05);
	write_cmos_sensor(0x9395, 0x82);
	write_cmos_sensor(0x9397, 0x78);
	write_cmos_sensor(0x9399, 0x05);
	write_cmos_sensor(0x939B, 0x05);
	write_cmos_sensor(0xA91F, 0x04);
	write_cmos_sensor(0xA921, 0x03);
	write_cmos_sensor(0xA923, 0x02);
	write_cmos_sensor(0xA93D, 0x05);
	write_cmos_sensor(0xA93F, 0x03);
	write_cmos_sensor(0xA941, 0x02);
	write_cmos_sensor(0xA9AF, 0x04);
	write_cmos_sensor(0xA9B1, 0x03);
	write_cmos_sensor(0xA9B3, 0x02);
	write_cmos_sensor(0xA9CD, 0x05);
	write_cmos_sensor(0xA9CF, 0x03);
	write_cmos_sensor(0xA9D1, 0x02);
	write_cmos_sensor(0xAA3F, 0x04);
	write_cmos_sensor(0xAA41, 0x03);
	write_cmos_sensor(0xAA43, 0x02);
	write_cmos_sensor(0xAA5D, 0x05);
	write_cmos_sensor(0xAA5F, 0x03);
	write_cmos_sensor(0xAA61, 0x02);
	write_cmos_sensor(0xAACF, 0x04);
	write_cmos_sensor(0xAAD1, 0x03);
	write_cmos_sensor(0xAAD3, 0x02);
	write_cmos_sensor(0xAAED, 0x05);
	write_cmos_sensor(0xAAEF, 0x03);
	write_cmos_sensor(0xAAF1, 0x02);
	write_cmos_sensor(0xAB87, 0x04);
	write_cmos_sensor(0xAB89, 0x03);
	write_cmos_sensor(0xAB8B, 0x02);
	write_cmos_sensor(0xABA5, 0x05);
	write_cmos_sensor(0xABA7, 0x03);
	write_cmos_sensor(0xABA9, 0x02);
	write_cmos_sensor(0xABB7, 0x04);
	write_cmos_sensor(0xABB9, 0x03);
	write_cmos_sensor(0xABBB, 0x02);
	write_cmos_sensor(0xABD5, 0x05);
	write_cmos_sensor(0xABD7, 0x03);
	write_cmos_sensor(0xABD9, 0x02);
	write_cmos_sensor(0xB388, 0x28);
	write_cmos_sensor(0xBC40, 0x03);
	write_cmos_sensor(0xE0A5, 0x0B);
	write_cmos_sensor(0xE0A6, 0x0B);

 
	write_cmos_sensor(0x0101, 0x03);

 
	write_cmos_sensor(0x3FF9, 0x01);

 
	PDAF_write_SPC_data();

 
	ImageQuality_setting();

	write_cmos_sensor(0x0100, 0x00);
}    

static void sensor_init_new(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	
	write_cmos_sensor(0x0136, 0x18);
	write_cmos_sensor(0x0137, 0x00);

	
	write_cmos_sensor(0x3C7D, 0x18);
	write_cmos_sensor(0x3C7E, 0x02);
	write_cmos_sensor(0x3C7F, 0x07);

	
	write_cmos_sensor(0x3140, 0x02);
	write_cmos_sensor(0x3F7F, 0x01);
	write_cmos_sensor(0x4430, 0x05);
	write_cmos_sensor(0x4431, 0xDC);
	write_cmos_sensor(0x5222, 0x02);
	write_cmos_sensor(0x5617, 0x0A);
	write_cmos_sensor(0x562B, 0x0A);
	write_cmos_sensor(0x562D, 0x0C);
	write_cmos_sensor(0x56B7, 0x74);
	write_cmos_sensor(0x6282, 0x83);
	write_cmos_sensor(0x6283, 0x22);
	write_cmos_sensor(0x6284, 0x0A);
	write_cmos_sensor(0x6285, 0x60);
	write_cmos_sensor(0x6286, 0x09);
	write_cmos_sensor(0x6287, 0x85);
	write_cmos_sensor(0x6288, 0x18);
	write_cmos_sensor(0x6289, 0xC0);
	write_cmos_sensor(0x628A, 0x1C);
	write_cmos_sensor(0x628B, 0x8C);
	write_cmos_sensor(0x628C, 0x41);
	write_cmos_sensor(0x628D, 0x80);
	write_cmos_sensor(0x628E, 0x38);
	write_cmos_sensor(0x628F, 0x18);
	write_cmos_sensor(0x6290, 0x83);
	write_cmos_sensor(0x6292, 0x1A);
	write_cmos_sensor(0x6293, 0x20);
	write_cmos_sensor(0x6294, 0x88);
	write_cmos_sensor(0x6296, 0x6C);
	write_cmos_sensor(0x6297, 0x41);
	write_cmos_sensor(0x6298, 0x4C);
	write_cmos_sensor(0x6299, 0x01);
	write_cmos_sensor(0x629A, 0x38);
	write_cmos_sensor(0x629B, 0x93);
	write_cmos_sensor(0x629C, 0x18);
	write_cmos_sensor(0x629D, 0x03);
	write_cmos_sensor(0x629E, 0x91);
	write_cmos_sensor(0x629F, 0x87);
	write_cmos_sensor(0x62A0, 0x30);
	write_cmos_sensor(0x62B1, 0x14);
	write_cmos_sensor(0x62B2, 0x20);
	write_cmos_sensor(0x62B3, 0x48);
	write_cmos_sensor(0x62B5, 0x58);
	write_cmos_sensor(0x62B6, 0x40);
	write_cmos_sensor(0x62B7, 0x8E);
	write_cmos_sensor(0x62B8, 0x01);
	write_cmos_sensor(0x62B9, 0x08);
	write_cmos_sensor(0x62BA, 0x91);
	write_cmos_sensor(0x62BB, 0x1C);
	write_cmos_sensor(0x62BC, 0x03);
	write_cmos_sensor(0x62BD, 0x21);
	write_cmos_sensor(0x62BE, 0x83);
	write_cmos_sensor(0x62BF, 0x38);
	write_cmos_sensor(0x62D0, 0x14);
	write_cmos_sensor(0x62D1, 0x20);
	write_cmos_sensor(0x62D2, 0x67);
	write_cmos_sensor(0x62D4, 0x54);
	write_cmos_sensor(0x62D5, 0x40);
	write_cmos_sensor(0x62D6, 0xCE);
	write_cmos_sensor(0x62D8, 0xF8);
	write_cmos_sensor(0x62D9, 0x92);
	write_cmos_sensor(0x62DA, 0x9C);
	write_cmos_sensor(0x62DB, 0x02);
	write_cmos_sensor(0x62DC, 0xE1);
	write_cmos_sensor(0x62DD, 0x85);
	write_cmos_sensor(0x62DE, 0x38);
	write_cmos_sensor(0x637A, 0x11);
	write_cmos_sensor(0x7BA0, 0x01);
	write_cmos_sensor(0x7BA9, 0x00);
	write_cmos_sensor(0x7BAA, 0x01);
	write_cmos_sensor(0x7BAD, 0x00);
	write_cmos_sensor(0x9002, 0x00);
	write_cmos_sensor(0x9003, 0x00);
	write_cmos_sensor(0x9004, 0x09);
	write_cmos_sensor(0x9006, 0x01);
	write_cmos_sensor(0x9200, 0x93);
	write_cmos_sensor(0x9201, 0x85);
	write_cmos_sensor(0x9202, 0x93);
	write_cmos_sensor(0x9203, 0x87);
	write_cmos_sensor(0x9204, 0x93);
	write_cmos_sensor(0x9205, 0x8D);
	write_cmos_sensor(0x9206, 0x93);
	write_cmos_sensor(0x9207, 0x8F);
	write_cmos_sensor(0x9208, 0x6A);
	write_cmos_sensor(0x9209, 0x22);
	write_cmos_sensor(0x920A, 0x6A);
	write_cmos_sensor(0x920B, 0x23);
	write_cmos_sensor(0x920C, 0x6A);
	write_cmos_sensor(0x920D, 0x0F);
	write_cmos_sensor(0x920E, 0x71);
	write_cmos_sensor(0x920F, 0x03);
	write_cmos_sensor(0x9210, 0x71);
	write_cmos_sensor(0x9211, 0x0B);
	write_cmos_sensor(0x935D, 0x01);
	write_cmos_sensor(0x9389, 0x05);
	write_cmos_sensor(0x938B, 0x05);
	write_cmos_sensor(0x9391, 0x05);
	write_cmos_sensor(0x9393, 0x05);
	write_cmos_sensor(0x9395, 0x82);
	write_cmos_sensor(0x9397, 0x78);
	write_cmos_sensor(0x9399, 0x05);
	write_cmos_sensor(0x939B, 0x05);
	write_cmos_sensor(0xA91F, 0x04);
	write_cmos_sensor(0xA921, 0x03);
	write_cmos_sensor(0xA923, 0x02);
	write_cmos_sensor(0xA93D, 0x05);
	write_cmos_sensor(0xA93F, 0x03);
	write_cmos_sensor(0xA941, 0x02);
	write_cmos_sensor(0xA9AF, 0x04);
	write_cmos_sensor(0xA9B1, 0x03);
	write_cmos_sensor(0xA9B3, 0x02);
	write_cmos_sensor(0xA9CD, 0x05);
	write_cmos_sensor(0xA9CF, 0x03);
	write_cmos_sensor(0xA9D1, 0x02);
	write_cmos_sensor(0xAA3F, 0x04);
	write_cmos_sensor(0xAA41, 0x03);
	write_cmos_sensor(0xAA43, 0x02);
	write_cmos_sensor(0xAA5D, 0x05);
	write_cmos_sensor(0xAA5F, 0x03);
	write_cmos_sensor(0xAA61, 0x02);
	write_cmos_sensor(0xAACF, 0x04);
	write_cmos_sensor(0xAAD1, 0x03);
	write_cmos_sensor(0xAAD3, 0x02);
	write_cmos_sensor(0xAAED, 0x05);
	write_cmos_sensor(0xAAEF, 0x03);
	write_cmos_sensor(0xAAF1, 0x02);
	write_cmos_sensor(0xAB87, 0x04);
	write_cmos_sensor(0xAB89, 0x03);
	write_cmos_sensor(0xAB8B, 0x02);
	write_cmos_sensor(0xABA5, 0x05);
	write_cmos_sensor(0xABA7, 0x03);
	write_cmos_sensor(0xABA9, 0x02);
	write_cmos_sensor(0xABB7, 0x04);
	write_cmos_sensor(0xABB9, 0x03);
	write_cmos_sensor(0xABBB, 0x02);
	write_cmos_sensor(0xABD5, 0x05);
	write_cmos_sensor(0xABD7, 0x03);
	write_cmos_sensor(0xABD9, 0x02);
	write_cmos_sensor(0xB388, 0x28);
	write_cmos_sensor(0xBC40, 0x03);

 
	write_cmos_sensor(0x0101, 0x03);

 
	write_cmos_sensor(0x3FF9, 0x01);

 
	PDAF_write_SPC_data();

 
	ImageQuality_setting();

	write_cmos_sensor(0x0100, 0x00);
}    


static void fullsize_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	write_cmos_sensor(0x0100,0x00);

	
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);

	
	write_cmos_sensor(0x0342, 0x17);
	write_cmos_sensor(0x0343, 0x90);

	
	write_cmos_sensor(0x0340, 0x0F);
	write_cmos_sensor(0x0341, 0x28);

	
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x12);
	write_cmos_sensor(0x0349, 0x2F);
	write_cmos_sensor(0x034A, 0x0D);
	write_cmos_sensor(0x034B, 0xA7);

	
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	write_cmos_sensor(0x0902, 0x0A);
	write_cmos_sensor(0x3243, 0x00);
	write_cmos_sensor(0x3F4C, 0x01);
	write_cmos_sensor(0x3F4D, 0x01);
	write_cmos_sensor(0x4254, 0x7F);


	
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x12);
	write_cmos_sensor(0x040D, 0x30);
	write_cmos_sensor(0x040E, 0x0D);
	write_cmos_sensor(0x040F, 0xA8);

	
	write_cmos_sensor(0x034C, 0x12);
	write_cmos_sensor(0x034D, 0x30);
	write_cmos_sensor(0x034E, 0x0D);
	write_cmos_sensor(0x034F, 0xA8);

	
	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x02);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x0307, 0x25);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0xF0);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0820, 0x16);
	write_cmos_sensor(0x0821, 0x80);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0xBC41, 0x01);

	
	write_cmos_sensor(0x3E20, 0x01);
	write_cmos_sensor(0x3E37, 0x00);
	write_cmos_sensor(0x3E3B, 0x00);

	
	write_cmos_sensor(0x0106, 0x00);
	write_cmos_sensor(0x0B00, 0x00);
	write_cmos_sensor(0x3230, 0x00);
	write_cmos_sensor(0x3C00, 0x5B);
	write_cmos_sensor(0x3C01, 0x54);
	write_cmos_sensor(0x3C02, 0x77);
	write_cmos_sensor(0x3C03, 0x66);
	write_cmos_sensor(0x3C04, 0x00);
	write_cmos_sensor(0x3C05, 0xCC);
	write_cmos_sensor(0x3C06, 0x14);
	write_cmos_sensor(0x3C07, 0x00);
	write_cmos_sensor(0x3C08, 0x01);
	write_cmos_sensor(0x3F14, 0x01);
	write_cmos_sensor(0x3F17, 0x00);
	write_cmos_sensor(0x3F3C, 0x01);
	write_cmos_sensor(0x3F78, 0x02);
	write_cmos_sensor(0x3F79, 0xA4);
	write_cmos_sensor(0x3F7C, 0x00);
	write_cmos_sensor(0x3F7D, 0x00);
	write_cmos_sensor(0x97C5, 0x14);

	
	write_cmos_sensor(0x637A, 0x11);
	write_cmos_sensor(0x9A00, 0x0C);
	write_cmos_sensor(0x9A01, 0x0C);
	write_cmos_sensor(0x9A06, 0x0C);
	write_cmos_sensor(0x9A18, 0x0C);
	write_cmos_sensor(0x9A19, 0x0C);
	write_cmos_sensor(0xAA20, 0x3F);
	write_cmos_sensor(0xAA23, 0x3F);
	write_cmos_sensor(0xAA32, 0x3F);
	write_cmos_sensor(0xAA69, 0x3F);
	write_cmos_sensor(0xAA6F, 0x3F);

	
	write_cmos_sensor(0x0202, 0x0F);
	write_cmos_sensor(0x0203, 0x14);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);

	
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);

 
	write_cmos_sensor(0x0101, 0x03);

	write_cmos_sensor(0x0100,0x01);

	mdelay(10);
}

static void binning_mode_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	write_cmos_sensor(0x0100,0x00);

	
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);

	
	write_cmos_sensor(0x0342, 0x0C);
	write_cmos_sensor(0x0343, 0x94);

	
	write_cmos_sensor(0x0340, 0x0A);
	write_cmos_sensor(0x0341, 0x24);

	
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x12);
	write_cmos_sensor(0x0349, 0x2F);
	write_cmos_sensor(0x034A, 0x0D);
	write_cmos_sensor(0x034B, 0x9F);

	
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x09);  
	write_cmos_sensor(0x3243, 0x00);
	write_cmos_sensor(0x3F4C, 0x01);
	write_cmos_sensor(0x3F4D, 0x01);
	write_cmos_sensor(0x4254, 0x7F);

	
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x09);
	write_cmos_sensor(0x040D, 0x18);
	write_cmos_sensor(0x040E, 0x06);
	write_cmos_sensor(0x040F, 0xD0);

	
	write_cmos_sensor(0x034C, 0x09);
	write_cmos_sensor(0x034D, 0x18);
	write_cmos_sensor(0x034E, 0x06);
	write_cmos_sensor(0x034F, 0xD0);

	
	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x04);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0xD1);
	write_cmos_sensor(0x030B, 0x02);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0x0A);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0820, 0x0C);
	write_cmos_sensor(0x0821, 0x78);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0xBC41, 0x01);

	
	write_cmos_sensor(0x3E20, 0x01);
	write_cmos_sensor(0x3E37, 0x00);
	write_cmos_sensor(0x3E3B, 0x00);

	
	write_cmos_sensor(0x0106, 0x00);
	write_cmos_sensor(0x0B00, 0x00);
	write_cmos_sensor(0x3230, 0x00);
	write_cmos_sensor(0x3C00, 0x5B);
	write_cmos_sensor(0x3C01, 0x54);
	write_cmos_sensor(0x3C02, 0x77);
	write_cmos_sensor(0x3C03, 0x66);
	write_cmos_sensor(0x3C04, 0x00);
	write_cmos_sensor(0x3C05, 0x00);
	write_cmos_sensor(0x3C06, 0x14);
	write_cmos_sensor(0x3C07, 0x00);
	write_cmos_sensor(0x3C08, 0x01);
	write_cmos_sensor(0x3F14, 0x01);
	write_cmos_sensor(0x3F17, 0x00);
	write_cmos_sensor(0x3F3C, 0x01);
	write_cmos_sensor(0x3F78, 0x03);
	write_cmos_sensor(0x3F79, 0xBC);
	write_cmos_sensor(0x3F7C, 0x00);
	write_cmos_sensor(0x3F7D, 0x00);
	write_cmos_sensor(0x97C5, 0x0C);

	
	write_cmos_sensor(0x637A, 0x11);
	write_cmos_sensor(0x9A00, 0x0C);
	write_cmos_sensor(0x9A01, 0x0C);
	write_cmos_sensor(0x9A06, 0x0C);
	write_cmos_sensor(0x9A18, 0x0C);
	write_cmos_sensor(0x9A19, 0x0C);
	write_cmos_sensor(0xAAC2, 0x3F);
	write_cmos_sensor(0x97C1, 0x04);

	
	write_cmos_sensor(0x0202, 0x0A);
	write_cmos_sensor(0x0203, 0x10);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);

	
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);

 
	write_cmos_sensor(0x0101, 0x03);

	write_cmos_sensor(0x0100,0x01);

	mdelay(10);
}

static void binning_mode_15FPS_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);

	write_cmos_sensor(0x0100,0x00);

	
	write_cmos_sensor(0x0112, 0x0A);
	write_cmos_sensor(0x0113, 0x0A);
	write_cmos_sensor(0x0114, 0x03);

	
	write_cmos_sensor(0x0342, 0x0C);
	write_cmos_sensor(0x0343, 0x94);

	
	write_cmos_sensor(0x0340, 0x14);
	write_cmos_sensor(0x0341, 0x48);

	
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x12);
	write_cmos_sensor(0x0349, 0x2F);
	write_cmos_sensor(0x034A, 0x0D);
	write_cmos_sensor(0x034B, 0x9F);

	
	write_cmos_sensor(0x0220, 0x00);
	write_cmos_sensor(0x0221, 0x11);
	write_cmos_sensor(0x0222, 0x01);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x22);
	write_cmos_sensor(0x0902, 0x09);  
	write_cmos_sensor(0x3243, 0x00);
	write_cmos_sensor(0x3F4C, 0x01);
	write_cmos_sensor(0x3F4D, 0x01);
	write_cmos_sensor(0x4254, 0x7F);

	
	write_cmos_sensor(0x0401, 0x00);
	write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x09);
	write_cmos_sensor(0x040D, 0x18);
	write_cmos_sensor(0x040E, 0x06);
	write_cmos_sensor(0x040F, 0xD0);

	
	write_cmos_sensor(0x034C, 0x09);
	write_cmos_sensor(0x034D, 0x18);
	write_cmos_sensor(0x034E, 0x06);
	write_cmos_sensor(0x034F, 0xD0);

	
	write_cmos_sensor(0x0301, 0x05);
	write_cmos_sensor(0x0303, 0x04);
	write_cmos_sensor(0x0305, 0x04);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0xD1);
	write_cmos_sensor(0x030B, 0x02);
	write_cmos_sensor(0x030D, 0x04);
	write_cmos_sensor(0x030E, 0x01);
	write_cmos_sensor(0x030F, 0x0A);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x0820, 0x0C);
	write_cmos_sensor(0x0821, 0x78);
	write_cmos_sensor(0x0822, 0x00);
	write_cmos_sensor(0x0823, 0x00);
	write_cmos_sensor(0xBC41, 0x01);

	
	write_cmos_sensor(0x3E20, 0x01);
	write_cmos_sensor(0x3E37, 0x00);
	write_cmos_sensor(0x3E3B, 0x00);

	
	write_cmos_sensor(0x0106, 0x00);
	write_cmos_sensor(0x0B00, 0x00);
	write_cmos_sensor(0x3230, 0x00);
	write_cmos_sensor(0x3C00, 0x5B);
	write_cmos_sensor(0x3C01, 0x54);
	write_cmos_sensor(0x3C02, 0x77);
	write_cmos_sensor(0x3C03, 0x66);
	write_cmos_sensor(0x3C04, 0x00);
	write_cmos_sensor(0x3C05, 0x00);
	write_cmos_sensor(0x3C06, 0x14);
	write_cmos_sensor(0x3C07, 0x00);
	write_cmos_sensor(0x3C08, 0x01);
	write_cmos_sensor(0x3F14, 0x01);
	write_cmos_sensor(0x3F17, 0x00);
	write_cmos_sensor(0x3F3C, 0x01);
	write_cmos_sensor(0x3F78, 0x03);
	write_cmos_sensor(0x3F79, 0xBC);
	write_cmos_sensor(0x3F7C, 0x00);
	write_cmos_sensor(0x3F7D, 0x00);
	write_cmos_sensor(0x97C5, 0x0C);

	
	write_cmos_sensor(0x637A, 0x11);
	write_cmos_sensor(0x9A00, 0x0C);
	write_cmos_sensor(0x9A01, 0x0C);
	write_cmos_sensor(0x9A06, 0x0C);
	write_cmos_sensor(0x9A18, 0x0C);
	write_cmos_sensor(0x9A19, 0x0C);
	write_cmos_sensor(0xAAC2, 0x3F);
	write_cmos_sensor(0x97C1, 0x04);

	
	write_cmos_sensor(0x0202, 0x14);
	write_cmos_sensor(0x0203, 0x34);
	write_cmos_sensor(0x0224, 0x01);
	write_cmos_sensor(0x0225, 0xF4);

	
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x00);
	write_cmos_sensor(0x0216, 0x00);
	write_cmos_sensor(0x0217, 0x00);
	write_cmos_sensor(0x020E, 0x01);
	write_cmos_sensor(0x020F, 0x00);
	write_cmos_sensor(0x0218, 0x01);
	write_cmos_sensor(0x0219, 0x00);

 
	write_cmos_sensor(0x0101, 0x03);

	write_cmos_sensor(0x0100,0x01);

	mdelay(10);
}

static void preview_setting(void)
{
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	fullsize_setting();
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	fullsize_setting();
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	binning_mode_setting();
}

static void hs_video_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	binning_mode_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	binning_mode_setting();
}

static void custom1_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	binning_mode_15FPS_setting();
}

static void custom2_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	binning_mode_setting();
}


static void custom3_setting(void)
{
	LOG_INF("%s ,E\n",__func__);
	LOG_WARNING("[sensor_pick]%s :E setting\n",__func__);
	binning_mode_setting();
}


static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        write_cmos_sensor(0x0601, 0x02);
    } else {
        write_cmos_sensor(0x0601, 0x00);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


extern bool read_imx351_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size);

static int read_imx351_otp(void){
	int ret = 0;

	LOG_INF("==========read_imx351_otp_flag =%d=======\n",read_imx351_otp_flag);
	if(read_imx351_otp_flag != 0){
		LOG_INF("==========imx351 otp readed=======\n");
		return 0;
	}

	memset(imx351_eeprom_data, 0x0, EEPROM_DATA_SIZE);
	ret = read_imx351_eeprom(0, imx351_eeprom_data, EEPROM_DATA_SIZE);
#if 1
	LOG_INF("OTP EEPROM vendor = 0x%x\n",                   imx351_eeprom_data[0x04]);
	LOG_INF("OTP RFPC version = 0x%x\n",                    imx351_eeprom_data[0x05]);
	LOG_INF("OTP Holder version = 0x%x\n",                  imx351_eeprom_data[0x06]);

	LOG_INF("OTP IRCF Rev. = 0x%x\n",                       imx351_eeprom_data[0x0C]);
	LOG_INF("OTP Lens Rev. = 0x%x\n",                       imx351_eeprom_data[0x0D]);
	LOG_INF("OTP Module Serial Number = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		imx351_eeprom_data[0x0E], imx351_eeprom_data[0x0F], imx351_eeprom_data[0x10], imx351_eeprom_data[0x11],
		imx351_eeprom_data[0x12], imx351_eeprom_data[0x13], imx351_eeprom_data[0x14], imx351_eeprom_data[0x15]);
	LOG_INF("OTP Image sensor revision = 0x%x\n",           imx351_eeprom_data[0x1E]);
	LOG_INF("OTP Camera Module Revision = 0x%x\n",          imx351_eeprom_data[0x1F]);
#endif

	if(ret == true) {
		spin_lock(&imgsensor_drv_lock);
		read_imx351_otp_flag = 1;
		spin_unlock(&imgsensor_drv_lock);
	}

	LOG_INF("==========exit imx351 read_otp=======\n");
	return ret;
}

static void read_module_cut_ver(void) {
	module_cut_ver = read_cmos_sensor(0x0018);

	if (module_cut_ver >= 0x03)
		module_new_setting = 1;
	else
		module_new_setting = 0;

	LOG_WARNING("module_cut_ver = 0x%x , module_new_setting =%d\n", module_cut_ver, module_new_setting);
}


static const char *imx351Vendor = "Sony";
static const char *imx351NAME = "imx351_front";
static const char *imx351Size = "16.0M";
static const char *imx351_htcUltraPixel= "ultrapixel=2304x1728";

static ssize_t sensor_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s %s\n", imx351Vendor, imx351NAME, imx351Size, imx351_htcUltraPixel);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);
static struct kobject *android_imx351;

static int imx351_sysfs_init(void)
{
	int ret ;
	LOG_INF("kobject creat and add\n");
	if(android_imx351 == NULL){
		android_imx351 = kobject_create_and_add("android_camera2", NULL);
		if (android_imx351 == NULL) {
			LOG_INF("subsystem_register failed\n");
			ret = -ENOMEM;
			return ret ;
		}
		LOG_INF("sysfs_create_file\n");
		ret = sysfs_create_file(android_imx351, &dev_attr_sensor.attr);
		if (ret) {
			LOG_INF("sysfs_create_file " \
					"failed\n");
			kobject_del(android_imx351);
			android_imx351 = NULL;
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
				
				imx351_sysfs_init();
				read_imx351_otp();
				read_module_cut_ver();
				
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

    
    if(module_new_setting == 1)
        sensor_init_new();
    else
        sensor_init();

    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_mode = 0;
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

    return ERROR_NONE;
}    
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

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

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	return ERROR_NONE;
}   

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}   

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
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

    sensor_resolution->SensorCustom3Width  = imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

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
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; 
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2; 

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
        case MSDK_SCENARIO_ID_CUSTOM3:
            sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;

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
        case MSDK_SCENARIO_ID_CUSTOM2:
            Custom2(image_window, sensor_config_data); 
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            Custom3(image_window, sensor_config_data); 
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

    write_cmos_sensor(0x0104, 0x01);
    set_max_framerate(imgsensor.current_fps,1);
    write_cmos_sensor(0x0104, 0x00);

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
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
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
        case MSDK_SCENARIO_ID_CUSTOM3:
            *framerate = imgsensor_info.custom3.max_framerate;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}


static kal_uint32 imx351_awb_gain(SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
    UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

    LOG_INF("imx351_awb_gain\n");

    grgain_32 = (pSetSensorAWB->ABS_GAIN_GR << 8) >> 9;
    rgain_32 = (pSetSensorAWB->ABS_GAIN_R << 8) >> 9;
    bgain_32 = (pSetSensorAWB->ABS_GAIN_B << 8) >> 9;
    gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB << 8) >> 9;

    LOG_INF("[imx351_awb_gain] ABS_GAIN_GR:%d, grgain_32:%d\n", pSetSensorAWB->ABS_GAIN_GR, grgain_32);
    LOG_INF("[imx351_awb_gain] ABS_GAIN_R:%d, rgain_32:%d\n", pSetSensorAWB->ABS_GAIN_R, rgain_32);
    LOG_INF("[imx351_awb_gain] ABS_GAIN_B:%d, bgain_32:%d\n", pSetSensorAWB->ABS_GAIN_B, bgain_32);
    LOG_INF("[imx351_awb_gain] ABS_GAIN_GB:%d, gbgain_32:%d\n", pSetSensorAWB->ABS_GAIN_GB, gbgain_32);

    write_cmos_sensor(0x0b8e, (grgain_32 >> 8) & 0xFF);
    write_cmos_sensor(0x0b8f, grgain_32 & 0xFF);
    write_cmos_sensor(0x0b90, (rgain_32 >> 8) & 0xFF);
    write_cmos_sensor(0x0b91, rgain_32 & 0xFF);
    write_cmos_sensor(0x0b92, (bgain_32 >> 8) & 0xFF);
    write_cmos_sensor(0x0b93, bgain_32 & 0xFF);
    write_cmos_sensor(0x0b94, (gbgain_32 >> 8) & 0xFF);
    write_cmos_sensor(0x0b95, gbgain_32 & 0xFF);
    return ERROR_NONE;
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long*)feature_para;
	

    SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    SENSOR_VC_INFO_STRUCT *pvcinfo;
    SET_SENSOR_AWB_GAIN *pSetSensorAWB=(SET_SENSOR_AWB_GAIN *)feature_para;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
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
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
			night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
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
            set_video_mode(*feature_data_16);
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
            get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, (MUINT32 *)(uintptr_t)(*(feature_data+1)));
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
                case MSDK_SCENARIO_ID_CUSTOM1:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[5],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CUSTOM2:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[6],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CUSTOM3:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[7],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
            break;
		
		case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
			imgsensor.ihdr_mode = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            
            ihdr_write_shutter_gain(*feature_data,*(feature_data+1),*(feature_data+2));
            break;
		case SENSOR_FEATURE_GET_VC_INFO:
            LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
            pvcinfo = (SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
            switch (*feature_data_32) {
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],sizeof(SENSOR_VC_INFO_STRUCT));
                break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],sizeof(SENSOR_VC_INFO_STRUCT));
                break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],sizeof(SENSOR_VC_INFO_STRUCT));
                break;
            }
            break;
		case SENSOR_FEATURE_SET_AWB_GAIN:
            imx351_awb_gain(pSetSensorAWB);
            break;
		
		
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			
			
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0; 
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
			break;
		case SENSOR_FEATURE_SET_PDAF:
			LOG_INF("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode= *feature_data_16;
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

UINT32 IMX351_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;
    return ERROR_NONE;
}    
