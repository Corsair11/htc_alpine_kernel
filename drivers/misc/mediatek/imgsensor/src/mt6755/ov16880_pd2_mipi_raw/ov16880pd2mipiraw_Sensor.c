
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov16880pd2mipiraw_Sensor.h"

#define PFX "OV16880_camera_sensor"
#define LOG_1 LOG_INF("OV16880,MIPI 4LANE,PDAF\n")
#define LOG_2 LOG_INF("preview 2336*1752@30fps,768Mbps/lane; video 4672*3504@30fps,1440Mbps/lane; capture 16M@30fps,1440Mbps/lane\n")
#define LOG_INF(fmt, args...)   pr_debug(PFX "[%s] " fmt, __FUNCTION__, ##args)
#define LOGE(format, args...)   pr_err("[%s] " format, __FUNCTION__, ##args)

#define sensor_flip

bool Is_OIStest = false;

bool Is_Capture = false; 
bool Is_FirstAE = false; 

static DEFINE_SPINLOCK(imgsensor_drv_lock);

extern bool read_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size);

#ifdef PDAF_TEST
extern bool wrtie_eeprom(kal_uint16 addr, BYTE data[],kal_uint32 size );
char data[4096]= {
0x50,	0x44,	0x30,	0x31,	0x7e,	0xff,	0xd6,	0x0,	0xe4,	0x1,	0x0,	0x0,	0x4,	0x0,	0x14,	0x0 ,
0x53,	0x4,	0x53,	0x4,	0x6b,	0x4,	0x85,	0x4,	0x9d,	0x4,	0xb4,	0x4,	0xb6,	0x4,	0xa4,	0x4 ,
0x76,	0x4,	0x4c,	0x4,	0x1d,	0x4,	0xea,	0x3,	0xb9,	0x3,	0x99,	0x3,	0x80,	0x3,	0x75,	0x3 ,
0x76,	0x3,	0x75,	0x3,	0x7b,	0x3,	0x87,	0x3,	0x7c,	0x4,	0x83,	0x4,	0xb0,	0x4,	0xdd,	0x4 ,
0xfe,	0x4,	0x9,	0x5,	0x2,	0x5,	0xdc,	0x4,	0x9b,	0x4,	0x5d,	0x4,	0x1b,	0x4,	0xda,	0x3 ,
0x9d,	0x3,	0x74,	0x3,	0x56,	0x3,	0x48,	0x3,	0x45,	0x3,	0x42,	0x3,	0x4e,	0x3,	0x5d,	0x3 ,
0x89,	0x4,	0x99,	0x4,	0xc2,	0x4,	0xf1,	0x4,	0xd,	0x5,	0x16,	0x5,	0x9,	0x5,	0xdd,	0x4 ,
0x99,	0x4,	0x4c,	0x4,	0x8,	0x4,	0xc8,	0x3,	0x8a,	0x3,	0x5f,	0x3,	0x48,	0x3,	0x39,	0x3 ,
0x36,	0x3,	0x32,	0x3,	0x38,	0x3,	0x40,	0x3,	0x80,	0x4,	0x82,	0x4,	0x9e,	0x4,	0xc2,	0x4 ,
0xd4,	0x4,	0xce,	0x4,	0xc3,	0x4,	0xaa,	0x4,	0x74,	0x4,	0x32,	0x4,	0xfa,	0x3,	0xc3,	0x3 ,
0x94,	0x3,	0x73,	0x3,	0x5c,	0x3,	0x53,	0x3,	0x4d,	0x3,	0x44,	0x3,	0x41,	0x3,	0x4e,	0x3 ,
0x74,	0x0,	0x7b,	0x0,	0x85,	0x0,	0x91,	0x0,	0x9d,	0x0,	0xaa,	0x0,	0xb5,	0x0,	0xbc,	0x0 ,
0xbe,	0x0,	0xbd,	0x0,	0xb7,	0x0,	0xae,	0x0,	0xa1,	0x0,	0x95,	0x0,	0x8a,	0x0,	0x80,	0x0 ,
0x78,	0x0,	0x71,	0x0,	0x6a,	0x0,	0x65,	0x0,	0x7b,	0x0,	0x84,	0x0,	0x91,	0x0,	0xa1,	0x0 ,
0xb1,	0x0,	0xc2,	0x0,	0xd0,	0x0,	0xd8,	0x0,	0xdb,	0x0,	0xd9,	0x0,	0xcf,	0x0,	0xc2,	0x0 ,
0xb1,	0x0,	0xa2,	0x0,	0x92,	0x0,	0x85,	0x0,	0x7b,	0x0,	0x73,	0x0,	0x6c,	0x0,	0x67,	0x0 ,
0x7a,	0x0,	0x84,	0x0,	0x90,	0x0,	0x9f,	0x0,	0xaf,	0x0,	0xbe,	0x0,	0xca,	0x0,	0xd3,	0x0 ,
0xd5,	0x0,	0xd0,	0x0,	0xc7,	0x0,	0xba,	0x0,	0xaa,	0x0,	0x9c,	0x0,	0x8e,	0x0,	0x82,	0x0 ,
0x79,	0x0,	0x70,	0x0,	0x6a,	0x0,	0x65,	0x0,	0x73,	0x0,	0x7b,	0x0,	0x85,	0x0,	0x8f,	0x0 ,
0x9a,	0x0,	0xa3,	0x0,	0xab,	0x0,	0xb0,	0x0,	0xb1,	0x0,	0xad,	0x0,	0xa7,	0x0,	0x9f,	0x0 ,
0x95,	0x0,	0x8b,	0x0,	0x81,	0x0,	0x79,	0x0,	0x72,	0x0,	0x6b,	0x0,	0x64,	0x0,	0x5f,	0x0 ,
0x6b,	0x0,	0x72,	0x0,	0x79,	0x0,	0x80,	0x0,	0x88,	0x0,	0x91,	0x0,	0x9a,	0x0,	0xa2,	0x0 ,
0xab,	0x0,	0xb0,	0x0,	0xb2,	0x0,	0xb2,	0x0,	0xae,	0x0,	0xa6,	0x0,	0x9e,	0x0,	0x94,	0x0 ,
0x8b,	0x0,	0x83,	0x0,	0x7a,	0x0,	0x73,	0x0,	0x6e,	0x0,	0x75,	0x0,	0x7c,	0x0,	0x84,	0x0 ,
0x8e,	0x0,	0x9a,	0x0,	0xa6,	0x0,	0xb2,	0x0,	0xbf,	0x0,	0xc7,	0x0,	0xca,	0x0,	0xc9,	0x0 ,
0xc4,	0x0,	0xbb,	0x0,	0xb0,	0x0,	0xa3,	0x0,	0x97,	0x0,	0x8d,	0x0,	0x83,	0x0,	0x7b,	0x0 ,
0x6c,	0x0,	0x73,	0x0,	0x7a,	0x0,	0x81,	0x0,	0x8b,	0x0,	0x96,	0x0,	0xa1,	0x0,	0xae,	0x0 ,
0xb9,	0x0,	0xc2,	0x0,	0xc6,	0x0,	0xc5,	0x0,	0xc1,	0x0,	0xba,	0x0,	0xae,	0x0,	0xa2,	0x0 ,
0x97,	0x0,	0x8d,	0x0,	0x84,	0x0,	0x7c,	0x0,	0x67,	0x0,	0x6d,	0x0,	0x73,	0x0,	0x79,	0x0 ,
0x80,	0x0,	0x87,	0x0,	0x90,	0x0,	0x97,	0x0,	0x9f,	0x0,	0xa5,	0x0,	0xa8,	0x0,	0xa9,	0x0 ,
0xa6,	0x0,	0xa1,	0x0,	0x99,	0x0,	0x92,	0x0,	0x8a,	0x0,	0x83,	0x0,	0x7b,	0x0,	0x73,	0x0 ,
0x50,	0x44,	0x30,	0x32,	0x7e,	0xff,	0xd6,	0x0,	0x1a,	0x3,	0x0,	0x0,	0x8,	0x8,	0xa,	0x0 ,
0x6e,	0xba,	0x4a,	0x95,	0x47,	0x87,	0xd5,	0x89,	0xfd,	0x82,	0xe,	0x88,	0xb8,	0x90,	0x1,	0xb6,
0x18,	0xb1,	0x1a,	0x8a,	0x64,	0x84,	0x84,	0x81,	0x68,	0x7f,	0xde,	0x81,	0xae,	0x87,	0xcf,	0xaa,
0xcc,	0xbe,	0x6b,	0x8c,	0x15,	0x85,	0xec,	0x84,	0x2,	0x7c,	0x27,	0x85,	0xcf,	0x8d,	0x32,	0xaa,
0x15,	0xcc,	0xa5,	0x89,	0x31,	0x86,	0xc,	0x81,	0x6,	0x7e,	0x52,	0x86,	0x11,	0x85,	0x40,	0xa8,
0x3a,	0xbb,	0xae,	0x87,	0xa5,	0x89,	0x1b,	0x87,	0xe9,	0x80,	0x3f,	0x86,	0xdb,	0x87,	0xaf,	0xa8,
0x3b,	0xbb,	0xa5,	0x89,	0xc5,	0x85,	0xa,	0x81,	0xcb,	0x7f,	0xa,	0x81,	0xdc,	0x89,	0xed,	0xb6,
0xae,	0xbc,	0x22,	0x99,	0x90,	0x8b,	0x14,	0x82,	0x87,	0x81,	0x40,	0x8c,	0x2a,	0x90,	0x61,	0xb6,
0xf1,	0xb3,	0xc3,	0x96,	0x1a,	0x8a,	0x7e,	0x8c,	0x60,	0x84,	0xe8,	0x90,	0x1,	0x98,	0x15,	0xcc,
0x78,	0x4,	0x26,	0x2,	0x58,	0x2,	0x8a,	0x2,	0xbc,	0x2,	0xee,	0x2,	0x20,	0x3,	0x52,	0x3 ,
0x84,	0x3,	0xb6,	0x3,	0xe8,	0x3,	0x4c,	0x4c,	0x52,	0x56,	0x5c,	0x60,	0x64,	0x66,	0x6a,	0x72,
0x3e,	0x44,	0x4a,	0x4e,	0x52,	0x58,	0x5e,	0x64,	0x6a,	0x6e,	0x38,	0x3c,	0x42,	0x48,	0x4e,	0x56,
0x5a,	0x60,	0x66,	0x6c,	0x32,	0x38,	0x3e,	0x44,	0x4a,	0x50,	0x54,	0x5a,	0x62,	0x66,	0x32,	0x3a,
0x40,	0x46,	0x4c,	0x50,	0x58,	0x5e,	0x64,	0x6a,	0x34,	0x3e,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x5e,
0x66,	0x6a,	0x3e,	0x44,	0x4a,	0x50,	0x56,	0x5c,	0x60,	0x66,	0x6a,	0x70,	0x46,	0x4c,	0x50,	0x52,
0x5a,	0x5c,	0x62,	0x64,	0x6a,	0x6e,	0x4a,	0x52,	0x52,	0x58,	0x5a,	0x60,	0x64,	0x6a,	0x6e,	0x74,
0x3c,	0x42,	0x48,	0x4c,	0x52,	0x58,	0x60,	0x64,	0x6a,	0x70,	0x34,	0x3a,	0x42,	0x48,	0x4e,	0x54,
0x5a,	0x5e,	0x66,	0x6a,	0x2e,	0x36,	0x3a,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x60,	0x66,	0x30,	0x36,
0x3a,	0x44,	0x4a,	0x50,	0x56,	0x5a,	0x62,	0x68,	0x34,	0x3a,	0x40,	0x48,	0x4c,	0x52,	0x5a,	0x60,
0x66,	0x6a,	0x3c,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x60,	0x66,	0x6c,	0x70,	0x46,	0x4c,	0x50,	0x56,
0x5a,	0x60,	0x64,	0x68,	0x6c,	0x70,	0x4c,	0x50,	0x56,	0x58,	0x5c,	0x62,	0x66,	0x68,	0x6a,	0x74,
0x3e,	0x42,	0x4a,	0x50,	0x54,	0x5a,	0x60,	0x66,	0x6c,	0x70,	0x34,	0x3c,	0x42,	0x4a,	0x4e,	0x54,
0x5a,	0x60,	0x64,	0x6c,	0x30,	0x34,	0x3a,	0x40,	0x46,	0x4e,	0x54,	0x5a,	0x5e,	0x64,	0x2e,	0x36,
0x3a,	0x42,	0x46,	0x4e,	0x54,	0x5c,	0x62,	0x68,	0x34,	0x3a,	0x42,	0x48,	0x4e,	0x54,	0x58,	0x5e,
0x66,	0x6a,	0x3c,	0x44,	0x4a,	0x50,	0x54,	0x5a,	0x60,	0x66,	0x6a,	0x70,	0x48,	0x4c,	0x50,	0x56,
0x58,	0x60,	0x64,	0x68,	0x6c,	0x72,	0x50,	0x52,	0x56,	0x58,	0x5e,	0x60,	0x64,	0x6a,	0x6e,	0x72,
0x3c,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x60,	0x66,	0x6a,	0x70,	0x34,	0x3a,	0x42,	0x46,	0x4e,	0x54,
0x5a,	0x60,	0x64,	0x68,	0x2c,	0x34,	0x3a,	0x3e,	0x46,	0x4c,	0x54,	0x58,	0x5e,	0x64,	0x2c,	0x34,
0x3a,	0x40,	0x46,	0x4c,	0x54,	0x58,	0x60,	0x66,	0x34,	0x3a,	0x42,	0x46,	0x4e,	0x54,	0x58,	0x5e,
0x64,	0x6a,	0x3c,	0x42,	0x48,	0x50,	0x54,	0x5a,	0x62,	0x66,	0x6c,	0x72,	0x48,	0x4e,	0x50,	0x58,
0x5c,	0x60,	0x64,	0x68,	0x6e,	0x74,	0x52,	0x52,	0x56,	0x5a,	0x5e,	0x60,	0x66,	0x6a,	0x72,	0x76,
0x3c,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x60,	0x66,	0x6c,	0x70,	0x36,	0x3c,	0x42,	0x48,	0x4e,	0x54,
0x5a,	0x60,	0x64,	0x6a,	0x2e,	0x34,	0x3a,	0x40,	0x46,	0x4c,	0x52,	0x58,	0x5c,	0x64,	0x2e,	0x34,
0x3c,	0x40,	0x48,	0x4e,	0x52,	0x5a,	0x60,	0x66,	0x34,	0x3c,	0x42,	0x48,	0x4c,	0x52,	0x5a,	0x5e,
0x66,	0x6a,	0x3e,	0x44,	0x4a,	0x50,	0x56,	0x5a,	0x62,	0x68,	0x6e,	0x72,	0x48,	0x4c,	0x52,	0x56,
0x5a,	0x60,	0x66,	0x68,	0x6e,	0x72,	0x4e,	0x52,	0x54,	0x58,	0x5c,	0x62,	0x68,	0x6a,	0x70,	0x72,
0x3e,	0x44,	0x48,	0x4e,	0x54,	0x5a,	0x60,	0x66,	0x6c,	0x72,	0x36,	0x3c,	0x42,	0x4a,	0x50,	0x56,
0x5a,	0x60,	0x66,	0x6c,	0x2e,	0x34,	0x3c,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x60,	0x66,	0x2e,	0x36,
0x3c,	0x42,	0x4a,	0x4e,	0x54,	0x5c,	0x62,	0x66,	0x34,	0x3a,	0x42,	0x48,	0x4e,	0x54,	0x5a,	0x60,
0x66,	0x6c,	0x3e,	0x44,	0x4a,	0x50,	0x58,	0x5c,	0x60,	0x66,	0x6e,	0x72,	0x4a,	0x4e,	0x52,	0x58,
0x5c,	0x60,	0x64,	0x68,	0x6c,	0x72,	0x4e,	0x50,	0x56,	0x56,	0x5c,	0x60,	0x64,	0x6c,	0x6a,	0x74,
0x40,	0x46,	0x4a,	0x50,	0x54,	0x5a,	0x60,	0x66,	0x6a,	0x6e,	0x3a,	0x3e,	0x46,	0x4a,	0x50,	0x56,
0x5c,	0x62,	0x68,	0x6c,	0x30,	0x36,	0x3e,	0x42,	0x4a,	0x4e,	0x56,	0x5c,	0x60,	0x68,	0x30,	0x38,
0x3e,	0x44,	0x4c,	0x50,	0x56,	0x5e,	0x62,	0x68,	0x38,	0x3c,	0x44,	0x4a,	0x50,	0x56,	0x5a,	0x60,
0x66,	0x6a,	0x40,	0x46,	0x4a,	0x50,	0x56,	0x5a,	0x62,	0x66,	0x6c,	0x72,	0x4a,	0x4c,	0x52,	0x56,
0x5e,	0x60,	0x64,	0x6a,	0x6c,	0x6e,	0x4a,	0x4c,	0x52,	0x52,	0x5a,	0x60,	0x64,	0x68,	0x6e,	0x6c,
0x40,	0x46,	0x4e,	0x52,	0x56,	0x5c,	0x62,	0x66,	0x6c,	0x70,	0x3a,	0x40,	0x44,	0x4c,	0x50,	0x58,
0x5c,	0x62,	0x68,	0x6e,	0x34,	0x3a,	0x40,	0x44,	0x4e,	0x52,	0x58,	0x5c,	0x62,	0x66,	0x34,	0x3a,
0x40,	0x44,	0x4c,	0x52,	0x58,	0x5e,	0x64,	0x6a,	0x3a,	0x40,	0x44,	0x4a,	0x50,	0x56,	0x5a,	0x60,
0x66,	0x6c,	0x40,	0x46,	0x4e,	0x52,	0x58,	0x5c,	0x60,	0x66,	0x6c,	0x70,	0x4c,	0x4e,	0x52,	0x56,
0x5a,	0x60,	0x62,	0x66,	0x6c,	0x6c,	0x50,	0x44,	0x30,	0x33,	0x7e,	0xff,	0xd6,	0x0,	0x5a,	0x0 ,
0x0,	0x0,	0xdf,	0x2,	0x62,	0x6a,	0x70,	0x74,	0x7a,	0x7e,	0x84,	0x8a,	0x90,	0x96,	0xd0,	0x14,
0xb8,	0xb,	0x10,	0x0,	0x8,	0x23,	0x40,	0x40,	0x10,	0x10,	0x53,	0x2e,	0x8,	0x23,	0x3c,	0x23,
0x18,	0x27,	0x2c,	0x27,	0xc,	0x37,	0x38,	0x37,	0x1c,	0x3b,	0x28,	0x3b,	0x1c,	0x43,	0x28,	0x43,
0xc,	0x47,	0x38,	0x47,	0x18,	0x57,	0x2c,	0x57,	0x8,	0x5b,	0x3c,	0x5b,	0x8,	0x27,	0x3c,	0x27,
0x18,	0x2b,	0x2c,	0x2b,	0xc,	0x33,	0x38,	0x33,	0x1c,	0x37,	0x28,	0x37,	0x1c,	0x47,	0x28,	0x47,
0xc,	0x4b,	0x38,	0x4b,	0x18,	0x53,	0x2c,	0x53,	0x8,	0x57,	0x3c,	0x57};
char data2[4096]= {0};
#endif

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV16880_SENSOR_ID,
	.checksum_value = 0xfe9e1a79,

	.pre = {
              
		.pclk = 588000000,
		.linelength = 5124, 
		.framelength = 3824,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4672,
		.grabwindow_height = 3504,
		.mipi_data_lp2hs_settle_dc = 105,
		.max_framerate = 300,
	},
	.cap = { 
		.pclk = 588000000,
		.linelength = 5124, 
		.framelength = 3824,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4672,
		.grabwindow_height = 3504,
		.mipi_data_lp2hs_settle_dc = 105,
		.max_framerate = 300,
		},
	.cap1 = {
		.pclk = 576000000,
		.linelength = 5024, 
		.framelength = 3824,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4672,
		.grabwindow_height = 3504,
		.mipi_data_lp2hs_settle_dc = 105,
		.max_framerate = 300,
	},
  .cap2 = {
		.pclk = 576000000,
		.linelength = 5024, 
		.framelength = 3824,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4672,
		.grabwindow_height = 3504,
		.mipi_data_lp2hs_settle_dc = 105,
		.max_framerate = 300,		
},
	.normal_video = { 
		.pclk = 576000000,
		.linelength = 5024, 
		.framelength = 3824,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2336,
		.grabwindow_height = 1752,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},

	.hs_video = {
		.pclk = 288000000,
		.linelength = 2512,
		.framelength = 956,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
	},

	.slim_video = {
		.pclk = 288000000,
		.linelength = 2512,
		.framelength = 956,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
	},

	.custom1 = {  
		.pclk = 576000000,
		.linelength = 5024, 
		.framelength = 7648,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2336,
		.grabwindow_height = 1752,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
	},
	.custom2 = { 
		.pclk = 576000000,
		.linelength = 5024, 
		.framelength = 3824,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2336,
		.grabwindow_height = 1752,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},

	.margin = 8,
	.min_shutter = 4,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 1,	  
	.ihdr_le_firstline = 0,  
	.sensor_mode_num = 7,	  

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_2MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, 
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x6c,0x20,0xff},
	.i2c_speed = 400,
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
	.ihdr_en = KAL_FALSE, 
	.i2c_write_id = 0x20,
};


static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] =
{{ 4704, 3536,	   0,     8, 4704, 3520, 4704, 3520, 16, 8, 4672, 3504,	  0,	0, 4672, 3504}, 
 { 4704, 3536,	   0,     8, 4704, 3520, 4704, 3520, 16, 8, 4672, 3504,	  0,	0, 4672, 3504}, 
 { 4704, 3536,	   0,     8, 4704, 3520, 2352, 1760,  8, 2, 2336, 1752,	  0,	0, 2336, 1752}, 
 { 4704, 3536,	1056,  1040, 2592, 1456, 1296,  728,  8, 4, 1280,  720,	  0,	0, 1280,  720}, 
 { 4704, 3536,	1056,  1040, 2592, 1456, 1296,  728,  8, 4, 1280,  720,	  0,	0, 1280,  720}, 
 { 4704, 3536,	   0,     8, 4704, 3520, 2352, 1760,  8, 2, 2336, 1752,	  0,	0, 2336, 1752}, 
 { 4704, 3536,	   0,     8, 4704, 3520, 2352, 1760,  8, 2, 2336, 1752,	  0,	0, 2336, 1752}}; 

static SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3]=
{
 {0x02, 0x0a,   0x00,   0x00, 0x00, 0x00,
  0x00, 0x2b, 0x0000, 0x0000, 0x01, 0x00, 0x0000, 0x0000,
  0x01, 0x2b, 0x0118, 0x0340, 0x03, 0x00, 0x0000, 0x0000},
  
 {0x02, 0x0a,   0x00,   0x00, 0x00, 0x00,
  0x00, 0x2b, 0x0000, 0x0000, 0x01, 0x00, 0x0000, 0x0000,
  0x01, 0x2b, 0x0118, 0x0340, 0x03, 0x00, 0x0000, 0x0000},
  
 {0x02, 0x0a,   0x00,   0x00, 0x00, 0x00,
  0x00, 0x2b, 0x0000, 0x0000, 0x01, 0x00, 0x0000, 0x0000,
  0x01, 0x2b, 0x0118, 0x0340, 0x03, 0x00, 0x0000, 0x0000},
};

static SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX =  96,
    .i4OffsetY = 88,
    .i4PitchX  = 32,
    .i4PitchY  = 32,
    .i4PairNum  =8,
    .i4SubBlkW  =16,
    .i4SubBlkH  =8,
    .i4PosL = {{110,94},{126,94},{102,98},{118,98},{110,110},{126,110},{102,114},{118,114}},
    .i4PosR = {{110,90},{126,90},{102,102},{118,102},{110,106},{126,106},{102,118},{118,118}},
    .iMirrorFlip = 3,	
    .i4BlockNumX = 140,
    .i4BlockNumY = 104,
    .i4LeFirst = 0,
    .i4Crop =  {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}}, 
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte, 2, imgsensor.i2c_write_id);
    return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,imgsensor.i2c_write_id);
    
    return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
    iWriteRegI2C(pusendcmd , 3, imgsensor.i2c_write_id);
    
}


static void set_dummy(void)
{
	 LOG_INF("dummyline = %d, dummypixels = %d ", imgsensor.dummy_line, imgsensor.dummy_pixel);
  
	write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);	 
	
	
	write_cmos_sensor_8(0x380c, (imgsensor.line_length) >> 8);
	write_cmos_sensor_8(0x380d, (imgsensor.line_length) & 0xFF);

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
static void write_shutter(kal_uint32 shutter)
{
	
	uint16_t line_length_pclk = 0;
	
	static uint16_t pre_line_length_pclk = 0;
	
	kal_uint16 realtime_fps = 0;
	kal_uint32 m_frame_length;
	kal_uint32 m_line_length;

    LOG_INF("START! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

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

    
    if (adjusted_line_length_pclk > 0)
        line_length_pclk = adjusted_line_length_pclk;
    else if (pre_line_length_pclk > 0)
        line_length_pclk = original_line_length_pclk;  
    else
        line_length_pclk = 0; 
    

	if (Is_OIStest) {
		shutter = (0x23 << 8) | 0x0E;
		m_frame_length = (0x23 << 8) | 0x18;
		m_line_length = (0xFF << 8) | 0xF0;
		LOG_INF("Under OIS test mode, LOCK :  shutter =%d(0x%x)), framelength =%d(0x%x), linelength =%d(0x%x)\n",
			shutter, shutter, m_frame_length, m_frame_length, m_line_length, m_line_length);

		
		write_cmos_sensor_8(0x380c, m_line_length >> 8);
		write_cmos_sensor_8(0x380d, m_line_length & 0xFF);
		
		write_cmos_sensor_8(0x380e, m_frame_length >> 8);
		write_cmos_sensor_8(0x380f, m_frame_length & 0xFF);
		
		write_cmos_sensor_8(0x3502, (shutter) & 0xFF);
		write_cmos_sensor_8(0x3501, (shutter >> 8) & 0x7F);
	} else {
		if (imgsensor.autoflicker_en) {
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
			if(realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296,0);
			else if(realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146,0);
			else {
				
				write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
				write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
			}
		}
		else {
			
			write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);
		}

        
        if (line_length_pclk > 0)
        {
            LOG_INF("Set line_length_pclk = 0x%x\n ", line_length_pclk);
            write_cmos_sensor_8(0x380c, (line_length_pclk >> 8)&0xff);
            write_cmos_sensor_8(0x380d, line_length_pclk & 0xFF);
        }

        
        
        

        pre_line_length_pclk = adjusted_line_length_pclk;
        

	
	write_cmos_sensor_8(0x3502, (shutter) & 0xFF);
	write_cmos_sensor_8(0x3501, (shutter >> 8) & 0x7F);	  
	
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
	}

}	



static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	



static kal_uint16 set_gain(kal_uint16 gain)
{
	
	
	

	
	



#if 1
	
	kal_uint16 reg_a_gain = 0x00;
	kal_uint16 d_gain = 1;
	kal_uint16 sensor_gain = 0;
	reg_a_gain = gain*16/BASEGAIN;
	if(reg_a_gain > 0xF8){
		if(gain <= 1984){ 
			reg_a_gain = gain*8/BASEGAIN;
			d_gain = 2; 
		}
		else if(gain > 1984 && sensor_gain <= 3968){
			reg_a_gain = gain*4/BASEGAIN;
			d_gain = 4; 
		}
		else{
			reg_a_gain = 0xF8; 
			d_gain = 4; 
		}
	}

	if(reg_a_gain < 0x10)
	{
		reg_a_gain = 0x10;
	}
	if(reg_a_gain > 0xf8)
	{
		reg_a_gain = 0xf8;
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_a_gain;
	spin_unlock(&imgsensor_drv_lock);

	if (Is_OIStest) {
		reg_a_gain = 0x10;
		d_gain= 0x06;
		printk("Under OIS test mode, LOCK : reg_a_gain = %d(0x%x) , d_gain = %d(0x%x)\n ", reg_a_gain, reg_a_gain, d_gain, d_gain);
		write_cmos_sensor_8(0x350a, reg_a_gain >> 8);
		write_cmos_sensor_8(0x350b, reg_a_gain & 0xFF);

		write_cmos_sensor_8(0x501f, d_gain);
	} else {
	printk("ov1688:gain = %d , reg_a_gain = 0x%x , d_gain = %d\n ", gain, reg_a_gain, d_gain);
	write_cmos_sensor_8(0x350a, reg_a_gain >> 8);
	write_cmos_sensor_8(0x350b, reg_a_gain & 0xFF);

	write_cmos_sensor_8(0x501f, ((d_gain-1) << 6 | 0x06));
	}
#else
	kal_uint16 reg_gain = 0;

	reg_gain = gain*16/BASEGAIN;
		if(reg_gain < 0x10)
		{
			reg_gain = 0x10;
		}
		if(reg_gain > 0xf8)
		{
			reg_gain = 0xf8;
		}
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain; 
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	printk("ov1688:gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x350a, reg_gain >> 8);
	write_cmos_sensor_8(0x350b, reg_gain & 0xFF);
#endif
	
	return gain;
}	

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
#if 1
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


		
		
		write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);

		write_cmos_sensor_8(0x3502, (le ) & 0xFF);
		write_cmos_sensor_8(0x3501, (le >> 8) & 0x7F);	 
		
		
		write_cmos_sensor_8(0x3508, (se ) & 0xFF); 
		write_cmos_sensor_8(0x3507, (se >> 8) & 0x7F);
		
		
	LOG_INF("iHDR:imgsensor.frame_length=%d\n",imgsensor.frame_length);
		set_gain(gain);
	}

#endif

}



static void sensor_read_alpha_parm(void);
static void sensor_write_alpha_parm(void);

static void normal_capture_setting(void)
{

    
	LOG_INF("[sensor_pick]%s :E setting\n",__func__);
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0302, 0x3d);
	write_cmos_sensor_8(0x0303, 0x0);
	write_cmos_sensor_8(0x030d, 0x31);
	write_cmos_sensor_8(0x030f, 0x3);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x3501, 0xe);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3606, 0x88);
	write_cmos_sensor_8(0x3607, 0x8);
	write_cmos_sensor_8(0x3622, 0x75);
	write_cmos_sensor_8(0x3720, 0xaa);
	write_cmos_sensor_8(0x3725, 0xf0);
	write_cmos_sensor_8(0x3726, 0x0);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3728, 0x22);
	write_cmos_sensor_8(0x3729, 0x0);
	write_cmos_sensor_8(0x372a, 0x20);
	write_cmos_sensor_8(0x372b, 0x40);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3730, 0x0);
	write_cmos_sensor_8(0x3731, 0x0);
	write_cmos_sensor_8(0x3732, 0x0);
	write_cmos_sensor_8(0x3733, 0x0);
	write_cmos_sensor_8(0x3748, 0x0);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x37a1, 0x2);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x4);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x3800, 0x0);
	write_cmos_sensor_8(0x3801, 0x0);
	write_cmos_sensor_8(0x3802, 0x0);
	write_cmos_sensor_8(0x3803, 0x8);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0xd);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0xd);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x14);
	write_cmos_sensor_8(0x380d, 0x4);
	write_cmos_sensor_8(0x380e, 0xe);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x0);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3812, 0x0);
	write_cmos_sensor_8(0x3813, 0x8);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);
	write_cmos_sensor_8(0x3821, 0x0);
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3837, 0x2);
	write_cmos_sensor_8(0x3841, 0x4);
	write_cmos_sensor_8(0x4006, 0x0);
	write_cmos_sensor_8(0x4007, 0x0);
	write_cmos_sensor_8(0x4009, 0xf);
	write_cmos_sensor_8(0x4050, 0x4);
	write_cmos_sensor_8(0x4051, 0xf);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0xb);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5690, 0x0);
	write_cmos_sensor_8(0x5691, 0x0);
	write_cmos_sensor_8(0x5692, 0x0);
	write_cmos_sensor_8(0x5693, 0x8);
	write_cmos_sensor_8(0x5d24, 0x0);
	write_cmos_sensor_8(0x5d25, 0x0);
	write_cmos_sensor_8(0x5d26, 0x0);
	write_cmos_sensor_8(0x5d27, 0x8);
	write_cmos_sensor_8(0x5d3a, 0x58);
	write_cmos_sensor_8(0x3703, 0x48);
	write_cmos_sensor_8(0x3767, 0x31);
	write_cmos_sensor_8(0x3708, 0x43);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4640, 0x0);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4643, 0x8);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5d29, 0x0);
	write_cmos_sensor_8(0x5694, 0xc0);
	write_cmos_sensor_8(0x5000, 0x9b);
	write_cmos_sensor_8(0x501d, 0x0);
	write_cmos_sensor_8(0x569d, 0x68);
	write_cmos_sensor_8(0x569e, 0xd);
	write_cmos_sensor_8(0x569f, 0x68);
	write_cmos_sensor_8(0x5d2f, 0x68);
	write_cmos_sensor_8(0x5d30, 0xd);
	write_cmos_sensor_8(0x5d31, 0x68);
	write_cmos_sensor_8(0x5500, 0x0);
	write_cmos_sensor_8(0x5501, 0x0);
	write_cmos_sensor_8(0x5502, 0x0);
	write_cmos_sensor_8(0x5503, 0x0);
	write_cmos_sensor_8(0x5504, 0x0);
	write_cmos_sensor_8(0x5505, 0x0);
	write_cmos_sensor_8(0x5506, 0x0);
	write_cmos_sensor_8(0x5507, 0x0);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x0);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x0);
	write_cmos_sensor_8(0x550c, 0x0);
	write_cmos_sensor_8(0x550d, 0x0);
	write_cmos_sensor_8(0x550e, 0x0);
	write_cmos_sensor_8(0x550f, 0x0);
	write_cmos_sensor_8(0x5510, 0x0);
	write_cmos_sensor_8(0x5511, 0x0);
	write_cmos_sensor_8(0x5512, 0x0);
	write_cmos_sensor_8(0x5513, 0x0);
	write_cmos_sensor_8(0x5514, 0x0);
	write_cmos_sensor_8(0x5515, 0x0);
	write_cmos_sensor_8(0x5516, 0x0);
	write_cmos_sensor_8(0x5517, 0x0);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x0);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x0);
	write_cmos_sensor_8(0x551c, 0x0);
	write_cmos_sensor_8(0x551d, 0x0);
	write_cmos_sensor_8(0x551e, 0x0);
	write_cmos_sensor_8(0x551f, 0x0);
	write_cmos_sensor_8(0x5520, 0x0);
	write_cmos_sensor_8(0x5521, 0x0);
	write_cmos_sensor_8(0x5522, 0x0);
	write_cmos_sensor_8(0x5523, 0x0);
	write_cmos_sensor_8(0x5524, 0x0);
	write_cmos_sensor_8(0x5525, 0x0);
	write_cmos_sensor_8(0x5526, 0x0);
	write_cmos_sensor_8(0x5527, 0x0);
	write_cmos_sensor_8(0x5528, 0x0);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x0);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x552c, 0x0);
	write_cmos_sensor_8(0x552d, 0x0);
	write_cmos_sensor_8(0x552e, 0x0);
	write_cmos_sensor_8(0x552f, 0x0);
	write_cmos_sensor_8(0x5530, 0x0);
	write_cmos_sensor_8(0x5531, 0x0);
	write_cmos_sensor_8(0x5532, 0x0);
	write_cmos_sensor_8(0x5533, 0x0);
	write_cmos_sensor_8(0x5534, 0x0);
	write_cmos_sensor_8(0x5535, 0x0);
	write_cmos_sensor_8(0x5536, 0x0);
	write_cmos_sensor_8(0x5537, 0x0);
	write_cmos_sensor_8(0x5538, 0x0);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x0);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x553c, 0x0);
	write_cmos_sensor_8(0x553d, 0x0);
	write_cmos_sensor_8(0x553e, 0x0);
	write_cmos_sensor_8(0x553f, 0x0);
	write_cmos_sensor_8(0x5540, 0x0);
	write_cmos_sensor_8(0x5541, 0x0);
	write_cmos_sensor_8(0x5542, 0x0);
	write_cmos_sensor_8(0x5543, 0x0);
	write_cmos_sensor_8(0x5544, 0x0);
	write_cmos_sensor_8(0x5545, 0x0);
	write_cmos_sensor_8(0x5546, 0x0);
	write_cmos_sensor_8(0x5547, 0x0);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x0);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x0);
	write_cmos_sensor_8(0x554c, 0x0);
	write_cmos_sensor_8(0x554d, 0x0);
	write_cmos_sensor_8(0x554e, 0x0);
	write_cmos_sensor_8(0x554f, 0x0);
	write_cmos_sensor_8(0x5550, 0x0);
	write_cmos_sensor_8(0x5551, 0x0);
	write_cmos_sensor_8(0x5552, 0x0);
	write_cmos_sensor_8(0x5553, 0x0);
	write_cmos_sensor_8(0x5554, 0x0);
	write_cmos_sensor_8(0x5555, 0x0);
	write_cmos_sensor_8(0x5556, 0x0);
	write_cmos_sensor_8(0x5557, 0x0);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x0);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x0);
	write_cmos_sensor_8(0x555c, 0x0);
	write_cmos_sensor_8(0x555d, 0x0);
	write_cmos_sensor_8(0x555e, 0x0);
	write_cmos_sensor_8(0x555f, 0x0);
	write_cmos_sensor_8(0x5560, 0x0);
	write_cmos_sensor_8(0x5561, 0x0);
	write_cmos_sensor_8(0x5562, 0x0);
	write_cmos_sensor_8(0x5563, 0x0);
	write_cmos_sensor_8(0x5564, 0x0);
	write_cmos_sensor_8(0x5565, 0x0);
	write_cmos_sensor_8(0x5566, 0x0);
	write_cmos_sensor_8(0x5567, 0x0);
	write_cmos_sensor_8(0x5568, 0x0);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x0);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x556c, 0x0);
	write_cmos_sensor_8(0x556d, 0x0);
	write_cmos_sensor_8(0x556e, 0x0);
	write_cmos_sensor_8(0x556f, 0x0);
	write_cmos_sensor_8(0x5570, 0x0);
	write_cmos_sensor_8(0x5571, 0x0);
	write_cmos_sensor_8(0x5572, 0x0);
	write_cmos_sensor_8(0x5573, 0x0);
	write_cmos_sensor_8(0x5574, 0x0);
	write_cmos_sensor_8(0x5575, 0x0);
	write_cmos_sensor_8(0x5576, 0x0);
	write_cmos_sensor_8(0x5577, 0x0);
	write_cmos_sensor_8(0x5578, 0x0);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x0);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x557c, 0x0);
	write_cmos_sensor_8(0x557d, 0x0);
	write_cmos_sensor_8(0x557e, 0x0);
	write_cmos_sensor_8(0x557f, 0x0);
	write_cmos_sensor_8(0x5580, 0x0);
	write_cmos_sensor_8(0x5581, 0x0);
	write_cmos_sensor_8(0x5582, 0x0);
	write_cmos_sensor_8(0x5583, 0x0);
	write_cmos_sensor_8(0x5584, 0x0);
	write_cmos_sensor_8(0x5585, 0x0);
	write_cmos_sensor_8(0x5586, 0x0);
	write_cmos_sensor_8(0x5587, 0x0);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x0);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x0);
	write_cmos_sensor_8(0x558c, 0x0);
	write_cmos_sensor_8(0x558d, 0x0);
	write_cmos_sensor_8(0x558e, 0x0);
	write_cmos_sensor_8(0x558f, 0x0);
	write_cmos_sensor_8(0x5590, 0x0);
	write_cmos_sensor_8(0x5591, 0x0);
	write_cmos_sensor_8(0x5592, 0x0);
	write_cmos_sensor_8(0x5593, 0x0);
	write_cmos_sensor_8(0x5594, 0x0);
	write_cmos_sensor_8(0x5595, 0x0);
	write_cmos_sensor_8(0x5596, 0x0);
	write_cmos_sensor_8(0x5597, 0x0);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x0);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x0);
	write_cmos_sensor_8(0x559c, 0x0);
	write_cmos_sensor_8(0x559d, 0x0);
	write_cmos_sensor_8(0x559e, 0x0);
	write_cmos_sensor_8(0x559f, 0x0);
	write_cmos_sensor_8(0x55a0, 0x0);
	write_cmos_sensor_8(0x55a1, 0x0);
	write_cmos_sensor_8(0x55a2, 0x0);
	write_cmos_sensor_8(0x55a3, 0x0);
	write_cmos_sensor_8(0x55a4, 0x0);
	write_cmos_sensor_8(0x55a5, 0x0);
	write_cmos_sensor_8(0x55a6, 0x0);
	write_cmos_sensor_8(0x55a7, 0x0);
	write_cmos_sensor_8(0x55a8, 0x0);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x0);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55ac, 0x0);
	write_cmos_sensor_8(0x55ad, 0x0);
	write_cmos_sensor_8(0x55ae, 0x0);
	write_cmos_sensor_8(0x55af, 0x0);
	write_cmos_sensor_8(0x55b0, 0x0);
	write_cmos_sensor_8(0x55b1, 0x0);
	write_cmos_sensor_8(0x55b2, 0x0);
	write_cmos_sensor_8(0x55b3, 0x0);
	write_cmos_sensor_8(0x55b4, 0x0);
	write_cmos_sensor_8(0x55b5, 0x0);
	write_cmos_sensor_8(0x55b6, 0x0);
	write_cmos_sensor_8(0x55b7, 0x0);
	write_cmos_sensor_8(0x55b8, 0x0);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x0);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55bc, 0x0);
	write_cmos_sensor_8(0x55bd, 0x0);
	write_cmos_sensor_8(0x55be, 0x0);
	write_cmos_sensor_8(0x55bf, 0x0);
	write_cmos_sensor_8(0x55c0, 0x0);
	write_cmos_sensor_8(0x55c1, 0x0);
	write_cmos_sensor_8(0x55c2, 0x0);
	write_cmos_sensor_8(0x55c3, 0x0);
	write_cmos_sensor_8(0x55c4, 0x0);
	write_cmos_sensor_8(0x55c5, 0x0);
	write_cmos_sensor_8(0x55c6, 0x0);
	write_cmos_sensor_8(0x55c7, 0x0);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x0);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x0);
	write_cmos_sensor_8(0x55cc, 0x0);
	write_cmos_sensor_8(0x55cd, 0x0);
	write_cmos_sensor_8(0x55ce, 0x0);
	write_cmos_sensor_8(0x55cf, 0x0);
	write_cmos_sensor_8(0x55d0, 0x0);
	write_cmos_sensor_8(0x55d1, 0x0);
	write_cmos_sensor_8(0x55d2, 0x0);
	write_cmos_sensor_8(0x55d3, 0x0);
	write_cmos_sensor_8(0x55d4, 0x0);
	write_cmos_sensor_8(0x55d5, 0x0);
	write_cmos_sensor_8(0x55d6, 0x0);
	write_cmos_sensor_8(0x55d7, 0x0);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x0);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x0);
	write_cmos_sensor_8(0x55dc, 0x0);
	write_cmos_sensor_8(0x55dd, 0x0);
	write_cmos_sensor_8(0x55de, 0x0);
	write_cmos_sensor_8(0x55df, 0x0);
	write_cmos_sensor_8(0x55e0, 0x0);
	write_cmos_sensor_8(0x55e1, 0x0);
	write_cmos_sensor_8(0x55e2, 0x0);
	write_cmos_sensor_8(0x55e3, 0x0);
	write_cmos_sensor_8(0x55e4, 0x0);
	write_cmos_sensor_8(0x55e5, 0x0);
	write_cmos_sensor_8(0x55e6, 0x0);
	write_cmos_sensor_8(0x55e7, 0x0);
	write_cmos_sensor_8(0x55e8, 0x0);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x0);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55ec, 0x0);
	write_cmos_sensor_8(0x55ed, 0x0);
	write_cmos_sensor_8(0x55ee, 0x0);
	write_cmos_sensor_8(0x55ef, 0x0);
	write_cmos_sensor_8(0x55f0, 0x0);
	write_cmos_sensor_8(0x55f1, 0x0);
	write_cmos_sensor_8(0x55f2, 0x0);
	write_cmos_sensor_8(0x55f3, 0x0);
	write_cmos_sensor_8(0x55f4, 0x0);
	write_cmos_sensor_8(0x55f5, 0x0);
	write_cmos_sensor_8(0x55f6, 0x0);
	write_cmos_sensor_8(0x55f7, 0x0);
	write_cmos_sensor_8(0x55f8, 0x0);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x0);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x55fc, 0x0);
	write_cmos_sensor_8(0x55fd, 0x0);
	write_cmos_sensor_8(0x55fe, 0x0);
	write_cmos_sensor_8(0x55ff, 0x0);
	write_cmos_sensor_8(0x5022, 0x91);
	write_cmos_sensor_8(0x0302, 0x3d);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4640, 0x0);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4643, 0x8);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4809, 0x2b);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5d29, 0x0);

	sensor_write_alpha_parm();

	write_cmos_sensor_8(0x0100, 0x01);
	write_cmos_sensor_8(0x4202, 0x00);
	mDELAY(40);
  LOG_INF( "Exit!");
}

static void preview_setting(void)
{
       
	LOG_INF("[sensor_pick]%s :E setting\n",__func__);
	normal_capture_setting();
}

#if 0
static void pip_capture_setting(void)
{
	LOG_INF( "OV16880 PIP setting Enter!");

	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0302, 0x3c);
	write_cmos_sensor_8(0x030f, 0x03);
	write_cmos_sensor_8(0x3501, 0x0e);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3726, 0x00);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3729, 0x00);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x37a1, 0x02);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x04);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x3800, 0x00);
	write_cmos_sensor_8(0x3801, 0x00);
	write_cmos_sensor_8(0x3802, 0x00);
	write_cmos_sensor_8(0x3803, 0x08);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0x0d);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0x0d);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0x0e);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3813, 0x08);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0x80);
	write_cmos_sensor_8(0x3821, 0x04);
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3841, 0x04);
	write_cmos_sensor_8(0x4006, 0x00);
	write_cmos_sensor_8(0x4007, 0x00);
	write_cmos_sensor_8(0x4009, 0x0f);
	write_cmos_sensor_8(0x4050, 0x04);
	write_cmos_sensor_8(0x4051, 0x0f);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0x16);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5500, 0x00);
	write_cmos_sensor_8(0x5501, 0x00);
	write_cmos_sensor_8(0x5502, 0x00);
	write_cmos_sensor_8(0x5503, 0x00);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x00);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x00);
	write_cmos_sensor_8(0x5510, 0x00);
	write_cmos_sensor_8(0x5511, 0x00);
	write_cmos_sensor_8(0x5512, 0x00);
	write_cmos_sensor_8(0x5513, 0x00);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x00);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x00);
	write_cmos_sensor_8(0x5520, 0x00);
	write_cmos_sensor_8(0x5521, 0x00);
	write_cmos_sensor_8(0x5522, 0x00);
	write_cmos_sensor_8(0x5523, 0x00);
	write_cmos_sensor_8(0x5528, 0x00);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x00);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x5530, 0x00);
	write_cmos_sensor_8(0x5531, 0x00);
	write_cmos_sensor_8(0x5532, 0x00);
	write_cmos_sensor_8(0x5533, 0x00);
	write_cmos_sensor_8(0x5538, 0x00);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x00);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x5540, 0x00);
	write_cmos_sensor_8(0x5541, 0x00);
	write_cmos_sensor_8(0x5542, 0x00);
	write_cmos_sensor_8(0x5543, 0x00);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x00);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x00);
	write_cmos_sensor_8(0x5550, 0x00);
	write_cmos_sensor_8(0x5551, 0x00);
	write_cmos_sensor_8(0x5552, 0x00);
	write_cmos_sensor_8(0x5553, 0x00);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x00);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x00);
	write_cmos_sensor_8(0x5560, 0x00);
	write_cmos_sensor_8(0x5561, 0x00);
	write_cmos_sensor_8(0x5562, 0x00);
	write_cmos_sensor_8(0x5563, 0x00);
	write_cmos_sensor_8(0x5568, 0x00);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x00);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x5570, 0x00);
	write_cmos_sensor_8(0x5571, 0x00);
	write_cmos_sensor_8(0x5572, 0x00);
	write_cmos_sensor_8(0x5573, 0x00);
	write_cmos_sensor_8(0x5578, 0x00);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x00);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x5580, 0x00);
	write_cmos_sensor_8(0x5581, 0x00);
	write_cmos_sensor_8(0x5582, 0x00);
	write_cmos_sensor_8(0x5583, 0x00);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x00);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x00);
	write_cmos_sensor_8(0x5590, 0x00);
	write_cmos_sensor_8(0x5591, 0x00);
	write_cmos_sensor_8(0x5592, 0x00);
	write_cmos_sensor_8(0x5593, 0x00);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x00);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x00);
	write_cmos_sensor_8(0x55a0, 0x00);
	write_cmos_sensor_8(0x55a1, 0x00);
	write_cmos_sensor_8(0x55a2, 0x00);
	write_cmos_sensor_8(0x55a3, 0x00);
	write_cmos_sensor_8(0x55a8, 0x00);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x00);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55b0, 0x00);
	write_cmos_sensor_8(0x55b1, 0x00);
	write_cmos_sensor_8(0x55b2, 0x00);
	write_cmos_sensor_8(0x55b3, 0x00);
	write_cmos_sensor_8(0x55b8, 0x00);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x00);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55c0, 0x00);
	write_cmos_sensor_8(0x55c1, 0x00);
	write_cmos_sensor_8(0x55c2, 0x00);
	write_cmos_sensor_8(0x55c3, 0x00);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x00);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x00);
	write_cmos_sensor_8(0x55d0, 0x00);
	write_cmos_sensor_8(0x55d1, 0x00);
	write_cmos_sensor_8(0x55d2, 0x00);
	write_cmos_sensor_8(0x55d3, 0x00);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x00);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x00);
	write_cmos_sensor_8(0x55e0, 0x00);
	write_cmos_sensor_8(0x55e1, 0x00);
	write_cmos_sensor_8(0x55e2, 0x00);
	write_cmos_sensor_8(0x55e3, 0x00);
	write_cmos_sensor_8(0x55e8, 0x00);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x00);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55f0, 0x00);
	write_cmos_sensor_8(0x55f1, 0x00);
	write_cmos_sensor_8(0x55f2, 0x00);
	write_cmos_sensor_8(0x55f3, 0x00);
	write_cmos_sensor_8(0x55f8, 0x00);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x00);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x5690, 0x00);
	write_cmos_sensor_8(0x5691, 0x00);
	write_cmos_sensor_8(0x5692, 0x00);
	write_cmos_sensor_8(0x5693, 0x08);
	write_cmos_sensor_8(0x5d24, 0x00);
	write_cmos_sensor_8(0x5d25, 0x00);
	write_cmos_sensor_8(0x5d26, 0x00);
	write_cmos_sensor_8(0x5d27, 0x08);
	write_cmos_sensor_8(0x5d3a, 0x58);
	
#ifdef sensor_flip
#ifdef sensor_mirror
write_cmos_sensor_8(0x3820, 0xc4);
write_cmos_sensor_8(0x3821, 0x04);
write_cmos_sensor_8(0x4000, 0x57);
#else
write_cmos_sensor_8(0x3820, 0xc4);
write_cmos_sensor_8(0x3821, 0x00);
write_cmos_sensor_8(0x4000, 0x57);
#endif	
#else
#ifdef sensor_mirror
write_cmos_sensor_8(0x3820, 0x80);
write_cmos_sensor_8(0x3821, 0x04);
write_cmos_sensor_8(0x4000, 0x17);
#else
write_cmos_sensor_8(0x3820, 0x80);
write_cmos_sensor_8(0x3821, 0x00);
write_cmos_sensor_8(0x4000, 0x17);
#endif	
#endif	

	
	write_cmos_sensor_8(0x5000, 0x9b);
	write_cmos_sensor_8(0x5002, 0x0c);
	write_cmos_sensor_8(0x5b85, 0x0c);
	write_cmos_sensor_8(0x5b87, 0x16);
	write_cmos_sensor_8(0x5b89, 0x28);
	write_cmos_sensor_8(0x5b8b, 0x40);
	write_cmos_sensor_8(0x5b8f, 0x1a);
	write_cmos_sensor_8(0x5b90, 0x1a);
	write_cmos_sensor_8(0x5b91, 0x1a);
	write_cmos_sensor_8(0x5b95, 0x1a);
	write_cmos_sensor_8(0x5b96, 0x1a);
	write_cmos_sensor_8(0x5b97, 0x1a);
	
	write_cmos_sensor_8(0x5001, 0x4e);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3641, 0x01);
	write_cmos_sensor_8(0x5d29, 0x80);
	
	write_cmos_sensor_8(0x030a, 0x01);
	write_cmos_sensor_8(0x0311, 0x01);
	write_cmos_sensor_8(0x3606, 0x22);
	write_cmos_sensor_8(0x3607, 0x02);
	write_cmos_sensor_8(0x360b, 0x4f);
	write_cmos_sensor_8(0x360c, 0x1f);
	write_cmos_sensor_8(0x3720, 0x44);
	write_cmos_sensor_8(0x0100, 0x01);
	write_cmos_sensor_8(0x4202, 0x00);

}

static void pip_capture_15fps_setting(void)
{

	LOG_INF( "OV16880 PIP setting Enter!");

	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0302, 0x3c);
	write_cmos_sensor_8(0x030f, 0x03);
	write_cmos_sensor_8(0x3501, 0x0e);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3726, 0x00);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3729, 0x00);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x37a1, 0x02);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x04);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x3800, 0x00);
	write_cmos_sensor_8(0x3801, 0x00);
	write_cmos_sensor_8(0x3802, 0x00);
	write_cmos_sensor_8(0x3803, 0x08);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0x0d);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0x0d);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0x0e);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3813, 0x08);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0x80);
	write_cmos_sensor_8(0x3821, 0x04);
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3841, 0x04);
	write_cmos_sensor_8(0x4006, 0x00);
	write_cmos_sensor_8(0x4007, 0x00);
	write_cmos_sensor_8(0x4009, 0x0f);
	write_cmos_sensor_8(0x4050, 0x04);
	write_cmos_sensor_8(0x4051, 0x0f);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0x16);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5500, 0x00);
	write_cmos_sensor_8(0x5501, 0x00);
	write_cmos_sensor_8(0x5502, 0x00);
	write_cmos_sensor_8(0x5503, 0x00);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x00);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x00);
	write_cmos_sensor_8(0x5510, 0x00);
	write_cmos_sensor_8(0x5511, 0x00);
	write_cmos_sensor_8(0x5512, 0x00);
	write_cmos_sensor_8(0x5513, 0x00);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x00);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x00);
	write_cmos_sensor_8(0x5520, 0x00);
	write_cmos_sensor_8(0x5521, 0x00);
	write_cmos_sensor_8(0x5522, 0x00);
	write_cmos_sensor_8(0x5523, 0x00);
	write_cmos_sensor_8(0x5528, 0x00);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x00);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x5530, 0x00);
	write_cmos_sensor_8(0x5531, 0x00);
	write_cmos_sensor_8(0x5532, 0x00);
	write_cmos_sensor_8(0x5533, 0x00);
	write_cmos_sensor_8(0x5538, 0x00);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x00);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x5540, 0x00);
	write_cmos_sensor_8(0x5541, 0x00);
	write_cmos_sensor_8(0x5542, 0x00);
	write_cmos_sensor_8(0x5543, 0x00);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x00);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x00);
	write_cmos_sensor_8(0x5550, 0x00);
	write_cmos_sensor_8(0x5551, 0x00);
	write_cmos_sensor_8(0x5552, 0x00);
	write_cmos_sensor_8(0x5553, 0x00);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x00);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x00);
	write_cmos_sensor_8(0x5560, 0x00);
	write_cmos_sensor_8(0x5561, 0x00);
	write_cmos_sensor_8(0x5562, 0x00);
	write_cmos_sensor_8(0x5563, 0x00);
	write_cmos_sensor_8(0x5568, 0x00);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x00);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x5570, 0x00);
	write_cmos_sensor_8(0x5571, 0x00);
	write_cmos_sensor_8(0x5572, 0x00);
	write_cmos_sensor_8(0x5573, 0x00);
	write_cmos_sensor_8(0x5578, 0x00);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x00);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x5580, 0x00);
	write_cmos_sensor_8(0x5581, 0x00);
	write_cmos_sensor_8(0x5582, 0x00);
	write_cmos_sensor_8(0x5583, 0x00);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x00);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x00);
	write_cmos_sensor_8(0x5590, 0x00);
	write_cmos_sensor_8(0x5591, 0x00);
	write_cmos_sensor_8(0x5592, 0x00);
	write_cmos_sensor_8(0x5593, 0x00);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x00);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x00);
	write_cmos_sensor_8(0x55a0, 0x00);
	write_cmos_sensor_8(0x55a1, 0x00);
	write_cmos_sensor_8(0x55a2, 0x00);
	write_cmos_sensor_8(0x55a3, 0x00);
	write_cmos_sensor_8(0x55a8, 0x00);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x00);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55b0, 0x00);
	write_cmos_sensor_8(0x55b1, 0x00);
	write_cmos_sensor_8(0x55b2, 0x00);
	write_cmos_sensor_8(0x55b3, 0x00);
	write_cmos_sensor_8(0x55b8, 0x00);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x00);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55c0, 0x00);
	write_cmos_sensor_8(0x55c1, 0x00);
	write_cmos_sensor_8(0x55c2, 0x00);
	write_cmos_sensor_8(0x55c3, 0x00);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x00);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x00);
	write_cmos_sensor_8(0x55d0, 0x00);
	write_cmos_sensor_8(0x55d1, 0x00);
	write_cmos_sensor_8(0x55d2, 0x00);
	write_cmos_sensor_8(0x55d3, 0x00);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x00);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x00);
	write_cmos_sensor_8(0x55e0, 0x00);
	write_cmos_sensor_8(0x55e1, 0x00);
	write_cmos_sensor_8(0x55e2, 0x00);
	write_cmos_sensor_8(0x55e3, 0x00);
	write_cmos_sensor_8(0x55e8, 0x00);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x00);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55f0, 0x00);
	write_cmos_sensor_8(0x55f1, 0x00);
	write_cmos_sensor_8(0x55f2, 0x00);
	write_cmos_sensor_8(0x55f3, 0x00);
	write_cmos_sensor_8(0x55f8, 0x00);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x00);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x5690, 0x00);
	write_cmos_sensor_8(0x5691, 0x00);
	write_cmos_sensor_8(0x5692, 0x00);
	write_cmos_sensor_8(0x5693, 0x08);
	write_cmos_sensor_8(0x5d24, 0x00);
	write_cmos_sensor_8(0x5d25, 0x00);
	write_cmos_sensor_8(0x5d26, 0x00);
	write_cmos_sensor_8(0x5d27, 0x08);
	write_cmos_sensor_8(0x5d3a, 0x58);

#ifdef sensor_flip
	#ifdef sensor_mirror
	write_cmos_sensor_8(0x3820, 0xc4);
	write_cmos_sensor_8(0x3821, 0x04);
	write_cmos_sensor_8(0x4000, 0x57);
	#else
	write_cmos_sensor_8(0x3820, 0xc4);
	write_cmos_sensor_8(0x3821, 0x00);
	write_cmos_sensor_8(0x4000, 0x57);
	#endif	
#else
	#ifdef sensor_mirror
	write_cmos_sensor_8(0x3820, 0x80);
	write_cmos_sensor_8(0x3821, 0x04);
	write_cmos_sensor_8(0x4000, 0x17);
	#else
	write_cmos_sensor_8(0x3820, 0x80);
	write_cmos_sensor_8(0x3821, 0x00);
	write_cmos_sensor_8(0x4000, 0x17);
	#endif	
#endif	

	
	write_cmos_sensor_8(0x5000, 0x9b);
	write_cmos_sensor_8(0x5002, 0x0c);
	write_cmos_sensor_8(0x5b85, 0x0c);
	write_cmos_sensor_8(0x5b87, 0x16);
	write_cmos_sensor_8(0x5b89, 0x28);
	write_cmos_sensor_8(0x5b8b, 0x40);
	write_cmos_sensor_8(0x5b8f, 0x1a);
	write_cmos_sensor_8(0x5b90, 0x1a);
	write_cmos_sensor_8(0x5b91, 0x1a);
	write_cmos_sensor_8(0x5b95, 0x1a);
	write_cmos_sensor_8(0x5b96, 0x1a);
	write_cmos_sensor_8(0x5b97, 0x1a);
	
	write_cmos_sensor_8(0x5001, 0x4e);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3641, 0x01);
	write_cmos_sensor_8(0x5d29, 0x80);
	
	write_cmos_sensor_8(0x030a, 0x01);
	write_cmos_sensor_8(0x0311, 0x01);
	write_cmos_sensor_8(0x3606, 0x22);
	write_cmos_sensor_8(0x3607, 0x02);
	write_cmos_sensor_8(0x360b, 0x4f);
	write_cmos_sensor_8(0x360c, 0x1f);
	write_cmos_sensor_8(0x3720, 0x44);
	write_cmos_sensor_8(0x0100, 0x01);
	write_cmos_sensor_8(0x4202, 0x00);

}
#endif

static void capture_setting(kal_uint16 currefps)
{

#if 1
       
	LOG_INF("[sensor_pick]%s :E setting, currefps = %d\n",__func__, currefps);
	normal_capture_setting();
#else
	if(currefps==300)
		normal_capture_setting();
	else if(currefps==200) 
		pip_capture_setting();
	else if(currefps==150)
        pip_capture_15fps_setting();
  else
	normal_capture_setting();
#endif
}

static void normal_video_setting(kal_uint16 currefps)
{
    
	LOG_INF("[sensor_pick]%s :E setting, currefps = %d\n",__func__, currefps);
	write_cmos_sensor_8(0x0100, 0x0);
        write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0302, 0x21);
	write_cmos_sensor_8(0x0303, 0x0);
	write_cmos_sensor_8(0x030d, 0x30);
	write_cmos_sensor_8(0x030f, 0x3);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x3501, 0xe);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3606, 0x55);
	write_cmos_sensor_8(0x3607, 0x5);
	write_cmos_sensor_8(0x3622, 0x79);
	write_cmos_sensor_8(0x3720, 0x88);
	write_cmos_sensor_8(0x3725, 0xf0);
	write_cmos_sensor_8(0x3726, 0x22);
	write_cmos_sensor_8(0x3727, 0x44);
	write_cmos_sensor_8(0x3728, 0x22);
	write_cmos_sensor_8(0x3729, 0x22);
	write_cmos_sensor_8(0x372a, 0x20);
	write_cmos_sensor_8(0x372b, 0x40);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3730, 0x0);
	write_cmos_sensor_8(0x3731, 0x0);
	write_cmos_sensor_8(0x3732, 0x0);
	write_cmos_sensor_8(0x3733, 0x0);
	write_cmos_sensor_8(0x3748, 0x0);
	write_cmos_sensor_8(0x3760, 0x90);
	write_cmos_sensor_8(0x37a1, 0x0);
	write_cmos_sensor_8(0x37a5, 0xaa);
	write_cmos_sensor_8(0x37a2, 0x0);
	write_cmos_sensor_8(0x37a3, 0x42);
	write_cmos_sensor_8(0x3800, 0x0);
	write_cmos_sensor_8(0x3801, 0x0);
	write_cmos_sensor_8(0x3802, 0x0);
	write_cmos_sensor_8(0x3803, 0x8);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0xd);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x9);
	write_cmos_sensor_8(0x3809, 0x20);
	write_cmos_sensor_8(0x380a, 0x6);
	write_cmos_sensor_8(0x380b, 0xd8);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0xe);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x0);
	write_cmos_sensor_8(0x3811, 0xa);
	write_cmos_sensor_8(0x3812, 0x0);
	write_cmos_sensor_8(0x3813, 0x4);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);
	write_cmos_sensor_8(0x3821, 0x0);
	write_cmos_sensor_8(0x3836, 0xc);
	write_cmos_sensor_8(0x3837, 0x2);
	write_cmos_sensor_8(0x3841, 0x2);
	write_cmos_sensor_8(0x4006, 0x0);
	write_cmos_sensor_8(0x4007, 0x0);
	write_cmos_sensor_8(0x4009, 0x9);
	write_cmos_sensor_8(0x4050, 0x0);
	write_cmos_sensor_8(0x4051, 0x1);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0x14);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5690, 0x0);
	write_cmos_sensor_8(0x5691, 0x0);
	write_cmos_sensor_8(0x5692, 0x0);
	write_cmos_sensor_8(0x5693, 0x8);
	write_cmos_sensor_8(0x5d24, 0x0);
	write_cmos_sensor_8(0x5d25, 0x0);
	write_cmos_sensor_8(0x5d26, 0x0);
	write_cmos_sensor_8(0x5d27, 0x8);
	write_cmos_sensor_8(0x5d3a, 0x58);
	write_cmos_sensor_8(0x3703, 0x42);
	write_cmos_sensor_8(0x3767, 0x30);
	write_cmos_sensor_8(0x3708, 0x42);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4640, 0x0);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4643, 0x8);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5d29, 0x0);
	write_cmos_sensor_8(0x5694, 0xc0);
	write_cmos_sensor_8(0x5000, 0xfb);
	write_cmos_sensor_8(0x501d, 0x50);
	write_cmos_sensor_8(0x569d, 0x68);
	write_cmos_sensor_8(0x569e, 0xd);
	write_cmos_sensor_8(0x569f, 0x68);
	write_cmos_sensor_8(0x5d2f, 0x68);
	write_cmos_sensor_8(0x5d30, 0xd);
	write_cmos_sensor_8(0x5d31, 0x68);
	write_cmos_sensor_8(0x5500, 0x0);
	write_cmos_sensor_8(0x5501, 0x0);
	write_cmos_sensor_8(0x5502, 0x0);
	write_cmos_sensor_8(0x5503, 0x0);
	write_cmos_sensor_8(0x5504, 0x0);
	write_cmos_sensor_8(0x5505, 0x0);
	write_cmos_sensor_8(0x5506, 0x0);
	write_cmos_sensor_8(0x5507, 0x0);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x0);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x0);
	write_cmos_sensor_8(0x550c, 0x0);
	write_cmos_sensor_8(0x550d, 0x0);
	write_cmos_sensor_8(0x550e, 0x0);
	write_cmos_sensor_8(0x550f, 0x0);
	write_cmos_sensor_8(0x5510, 0x0);
	write_cmos_sensor_8(0x5511, 0x0);
	write_cmos_sensor_8(0x5512, 0x0);
	write_cmos_sensor_8(0x5513, 0x0);
	write_cmos_sensor_8(0x5514, 0x0);
	write_cmos_sensor_8(0x5515, 0x0);
	write_cmos_sensor_8(0x5516, 0x0);
	write_cmos_sensor_8(0x5517, 0x0);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x0);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x0);
	write_cmos_sensor_8(0x551c, 0x0);
	write_cmos_sensor_8(0x551d, 0x0);
	write_cmos_sensor_8(0x551e, 0x0);
	write_cmos_sensor_8(0x551f, 0x0);
	write_cmos_sensor_8(0x5520, 0x0);
	write_cmos_sensor_8(0x5521, 0x0);
	write_cmos_sensor_8(0x5522, 0x0);
	write_cmos_sensor_8(0x5523, 0x0);
	write_cmos_sensor_8(0x5524, 0x0);
	write_cmos_sensor_8(0x5525, 0x0);
	write_cmos_sensor_8(0x5526, 0x0);
	write_cmos_sensor_8(0x5527, 0x0);
	write_cmos_sensor_8(0x5528, 0x0);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x0);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x552c, 0x0);
	write_cmos_sensor_8(0x552d, 0x0);
	write_cmos_sensor_8(0x552e, 0x0);
	write_cmos_sensor_8(0x552f, 0x0);
	write_cmos_sensor_8(0x5530, 0x0);
	write_cmos_sensor_8(0x5531, 0x0);
	write_cmos_sensor_8(0x5532, 0x0);
	write_cmos_sensor_8(0x5533, 0x0);
	write_cmos_sensor_8(0x5534, 0x0);
	write_cmos_sensor_8(0x5535, 0x0);
	write_cmos_sensor_8(0x5536, 0x0);
	write_cmos_sensor_8(0x5537, 0x0);
	write_cmos_sensor_8(0x5538, 0x0);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x0);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x553c, 0x0);
	write_cmos_sensor_8(0x553d, 0x0);
	write_cmos_sensor_8(0x553e, 0x0);
	write_cmos_sensor_8(0x553f, 0x0);
	write_cmos_sensor_8(0x5540, 0x0);
	write_cmos_sensor_8(0x5541, 0x0);
	write_cmos_sensor_8(0x5542, 0x0);
	write_cmos_sensor_8(0x5543, 0x0);
	write_cmos_sensor_8(0x5544, 0x0);
	write_cmos_sensor_8(0x5545, 0x0);
	write_cmos_sensor_8(0x5546, 0x0);
	write_cmos_sensor_8(0x5547, 0x0);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x0);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x0);
	write_cmos_sensor_8(0x554c, 0x0);
	write_cmos_sensor_8(0x554d, 0x0);
	write_cmos_sensor_8(0x554e, 0x0);
	write_cmos_sensor_8(0x554f, 0x0);
	write_cmos_sensor_8(0x5550, 0x0);
	write_cmos_sensor_8(0x5551, 0x0);
	write_cmos_sensor_8(0x5552, 0x0);
	write_cmos_sensor_8(0x5553, 0x0);
	write_cmos_sensor_8(0x5554, 0x0);
	write_cmos_sensor_8(0x5555, 0x0);
	write_cmos_sensor_8(0x5556, 0x0);
	write_cmos_sensor_8(0x5557, 0x0);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x0);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x0);
	write_cmos_sensor_8(0x555c, 0x0);
	write_cmos_sensor_8(0x555d, 0x0);
	write_cmos_sensor_8(0x555e, 0x0);
	write_cmos_sensor_8(0x555f, 0x0);
	write_cmos_sensor_8(0x5560, 0x0);
	write_cmos_sensor_8(0x5561, 0x0);
	write_cmos_sensor_8(0x5562, 0x0);
	write_cmos_sensor_8(0x5563, 0x0);
	write_cmos_sensor_8(0x5564, 0x0);
	write_cmos_sensor_8(0x5565, 0x0);
	write_cmos_sensor_8(0x5566, 0x0);
	write_cmos_sensor_8(0x5567, 0x0);
	write_cmos_sensor_8(0x5568, 0x0);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x0);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x556c, 0x0);
	write_cmos_sensor_8(0x556d, 0x0);
	write_cmos_sensor_8(0x556e, 0x0);
	write_cmos_sensor_8(0x556f, 0x0);
	write_cmos_sensor_8(0x5570, 0x0);
	write_cmos_sensor_8(0x5571, 0x0);
	write_cmos_sensor_8(0x5572, 0x0);
	write_cmos_sensor_8(0x5573, 0x0);
	write_cmos_sensor_8(0x5574, 0x0);
	write_cmos_sensor_8(0x5575, 0x0);
	write_cmos_sensor_8(0x5576, 0x0);
	write_cmos_sensor_8(0x5577, 0x0);
	write_cmos_sensor_8(0x5578, 0x0);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x0);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x557c, 0x0);
	write_cmos_sensor_8(0x557d, 0x0);
	write_cmos_sensor_8(0x557e, 0x0);
	write_cmos_sensor_8(0x557f, 0x0);
	write_cmos_sensor_8(0x5580, 0x0);
	write_cmos_sensor_8(0x5581, 0x0);
	write_cmos_sensor_8(0x5582, 0x0);
	write_cmos_sensor_8(0x5583, 0x0);
	write_cmos_sensor_8(0x5584, 0x0);
	write_cmos_sensor_8(0x5585, 0x0);
	write_cmos_sensor_8(0x5586, 0x0);
	write_cmos_sensor_8(0x5587, 0x0);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x0);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x0);
	write_cmos_sensor_8(0x558c, 0x0);
	write_cmos_sensor_8(0x558d, 0x0);
	write_cmos_sensor_8(0x558e, 0x0);
	write_cmos_sensor_8(0x558f, 0x0);
	write_cmos_sensor_8(0x5590, 0x0);
	write_cmos_sensor_8(0x5591, 0x0);
	write_cmos_sensor_8(0x5592, 0x0);
	write_cmos_sensor_8(0x5593, 0x0);
	write_cmos_sensor_8(0x5594, 0x0);
	write_cmos_sensor_8(0x5595, 0x0);
	write_cmos_sensor_8(0x5596, 0x0);
	write_cmos_sensor_8(0x5597, 0x0);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x0);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x0);
	write_cmos_sensor_8(0x559c, 0x0);
	write_cmos_sensor_8(0x559d, 0x0);
	write_cmos_sensor_8(0x559e, 0x0);
	write_cmos_sensor_8(0x559f, 0x0);
	write_cmos_sensor_8(0x55a0, 0x0);
	write_cmos_sensor_8(0x55a1, 0x0);
	write_cmos_sensor_8(0x55a2, 0x0);
	write_cmos_sensor_8(0x55a3, 0x0);
	write_cmos_sensor_8(0x55a4, 0x0);
	write_cmos_sensor_8(0x55a5, 0x0);
	write_cmos_sensor_8(0x55a6, 0x0);
	write_cmos_sensor_8(0x55a7, 0x0);
	write_cmos_sensor_8(0x55a8, 0x0);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x0);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55ac, 0x0);
	write_cmos_sensor_8(0x55ad, 0x0);
	write_cmos_sensor_8(0x55ae, 0x0);
	write_cmos_sensor_8(0x55af, 0x0);
	write_cmos_sensor_8(0x55b0, 0x0);
	write_cmos_sensor_8(0x55b1, 0x0);
	write_cmos_sensor_8(0x55b2, 0x0);
	write_cmos_sensor_8(0x55b3, 0x0);
	write_cmos_sensor_8(0x55b4, 0x0);
	write_cmos_sensor_8(0x55b5, 0x0);
	write_cmos_sensor_8(0x55b6, 0x0);
	write_cmos_sensor_8(0x55b7, 0x0);
	write_cmos_sensor_8(0x55b8, 0x0);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x0);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55bc, 0x0);
	write_cmos_sensor_8(0x55bd, 0x0);
	write_cmos_sensor_8(0x55be, 0x0);
	write_cmos_sensor_8(0x55bf, 0x0);
	write_cmos_sensor_8(0x55c0, 0x0);
	write_cmos_sensor_8(0x55c1, 0x0);
	write_cmos_sensor_8(0x55c2, 0x0);
	write_cmos_sensor_8(0x55c3, 0x0);
	write_cmos_sensor_8(0x55c4, 0x0);
	write_cmos_sensor_8(0x55c5, 0x0);
	write_cmos_sensor_8(0x55c6, 0x0);
	write_cmos_sensor_8(0x55c7, 0x0);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x0);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x0);
	write_cmos_sensor_8(0x55cc, 0x0);
	write_cmos_sensor_8(0x55cd, 0x0);
	write_cmos_sensor_8(0x55ce, 0x0);
	write_cmos_sensor_8(0x55cf, 0x0);
	write_cmos_sensor_8(0x55d0, 0x0);
	write_cmos_sensor_8(0x55d1, 0x0);
	write_cmos_sensor_8(0x55d2, 0x0);
	write_cmos_sensor_8(0x55d3, 0x0);
	write_cmos_sensor_8(0x55d4, 0x0);
	write_cmos_sensor_8(0x55d5, 0x0);
	write_cmos_sensor_8(0x55d6, 0x0);
	write_cmos_sensor_8(0x55d7, 0x0);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x0);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x0);
	write_cmos_sensor_8(0x55dc, 0x0);
	write_cmos_sensor_8(0x55dd, 0x0);
	write_cmos_sensor_8(0x55de, 0x0);
	write_cmos_sensor_8(0x55df, 0x0);
	write_cmos_sensor_8(0x55e0, 0x0);
	write_cmos_sensor_8(0x55e1, 0x0);
	write_cmos_sensor_8(0x55e2, 0x0);
	write_cmos_sensor_8(0x55e3, 0x0);
	write_cmos_sensor_8(0x55e4, 0x0);
	write_cmos_sensor_8(0x55e5, 0x0);
	write_cmos_sensor_8(0x55e6, 0x0);
	write_cmos_sensor_8(0x55e7, 0x0);
	write_cmos_sensor_8(0x55e8, 0x0);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x0);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55ec, 0x0);
	write_cmos_sensor_8(0x55ed, 0x0);
	write_cmos_sensor_8(0x55ee, 0x0);
	write_cmos_sensor_8(0x55ef, 0x0);
	write_cmos_sensor_8(0x55f0, 0x0);
	write_cmos_sensor_8(0x55f1, 0x0);
	write_cmos_sensor_8(0x55f2, 0x0);
	write_cmos_sensor_8(0x55f3, 0x0);
	write_cmos_sensor_8(0x55f4, 0x0);
	write_cmos_sensor_8(0x55f5, 0x0);
	write_cmos_sensor_8(0x55f6, 0x0);
	write_cmos_sensor_8(0x55f7, 0x0);
	write_cmos_sensor_8(0x55f8, 0x0);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x0);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x55fc, 0x0);
	write_cmos_sensor_8(0x55fd, 0x0);
	write_cmos_sensor_8(0x55fe, 0x0);
	write_cmos_sensor_8(0x55ff, 0x0);
	write_cmos_sensor_8(0x5022, 0x93);
	write_cmos_sensor_8(0x0302, 0x21);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4640, 0x0);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4643, 0x8);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4809, 0x2b);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5d29, 0x0);

	
	sensor_write_alpha_parm();
	

	write_cmos_sensor_8(0x0100, 0x1);
        write_cmos_sensor_8(0x4202, 0x00);

    mDELAY(40);
}
static void hs_video_setting(void)
{
	
	LOG_INF("[sensor_pick]%s :E setting\n",__func__);
	
	

	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0302, 0x20);
	write_cmos_sensor_8(0x030f, 0x07);
	write_cmos_sensor_8(0x3501, 0x03);
	write_cmos_sensor_8(0x3502, 0xb6);
	write_cmos_sensor_8(0x3726, 0x22);
	write_cmos_sensor_8(0x3727, 0x44);
	write_cmos_sensor_8(0x3729, 0x22);
	write_cmos_sensor_8(0x372f, 0x89);
	write_cmos_sensor_8(0x3760, 0x90);
	write_cmos_sensor_8(0x37a1, 0x00);
	write_cmos_sensor_8(0x37a5, 0xaa);
	write_cmos_sensor_8(0x37a2, 0x00);
	write_cmos_sensor_8(0x37a3, 0x42);
	write_cmos_sensor_8(0x3800, 0x04);
	write_cmos_sensor_8(0x3801, 0x20);
	write_cmos_sensor_8(0x3802, 0x04);
	write_cmos_sensor_8(0x3803, 0x10);
	write_cmos_sensor_8(0x3804, 0x0e);
	write_cmos_sensor_8(0x3805, 0x3f);
	write_cmos_sensor_8(0x3806, 0x09);
	write_cmos_sensor_8(0x3807, 0xbf);
	write_cmos_sensor_8(0x3808, 0x05);
	write_cmos_sensor_8(0x3809, 0x00);
	write_cmos_sensor_8(0x380a, 0x02);
	write_cmos_sensor_8(0x380b, 0xd0);
	write_cmos_sensor_8(0x380c, 0x09);
	write_cmos_sensor_8(0x380d, 0xd0);
	write_cmos_sensor_8(0x380e, 0x03);
	write_cmos_sensor_8(0x380f, 0xbc);
	write_cmos_sensor_8(0x3811, 0x08);
	write_cmos_sensor_8(0x3813, 0x04);
	write_cmos_sensor_8(0x3814, 0x31);
	write_cmos_sensor_8(0x3815, 0x31);
	write_cmos_sensor_8(0x3820, 0x81);
	write_cmos_sensor_8(0x3821, 0x06);
	write_cmos_sensor_8(0x3836, 0x0c);
	write_cmos_sensor_8(0x3841, 0x02);
	write_cmos_sensor_8(0x4006, 0x01);
	write_cmos_sensor_8(0x4007, 0x80);
	write_cmos_sensor_8(0x4009, 0x05);
	write_cmos_sensor_8(0x4050, 0x00);
	write_cmos_sensor_8(0x4051, 0x01);
	write_cmos_sensor_8(0x4501, 0x3c);
	write_cmos_sensor_8(0x4837, 0x14);
	write_cmos_sensor_8(0x5201, 0x62);
	write_cmos_sensor_8(0x5203, 0x08);
	write_cmos_sensor_8(0x5205, 0x48);
	write_cmos_sensor_8(0x5500, 0x80);
	write_cmos_sensor_8(0x5501, 0x80);
	write_cmos_sensor_8(0x5502, 0x80);
	write_cmos_sensor_8(0x5503, 0x80);
	write_cmos_sensor_8(0x5508, 0x80);
	write_cmos_sensor_8(0x5509, 0x80);
	write_cmos_sensor_8(0x550a, 0x80);
	write_cmos_sensor_8(0x550b, 0x80);
	write_cmos_sensor_8(0x5510, 0x08);
	write_cmos_sensor_8(0x5511, 0x08);
	write_cmos_sensor_8(0x5512, 0x08);
	write_cmos_sensor_8(0x5513, 0x08);
	write_cmos_sensor_8(0x5518, 0x08);
	write_cmos_sensor_8(0x5519, 0x08);
	write_cmos_sensor_8(0x551a, 0x08);
	write_cmos_sensor_8(0x551b, 0x08);
	write_cmos_sensor_8(0x5520, 0x80);
	write_cmos_sensor_8(0x5521, 0x80);
	write_cmos_sensor_8(0x5522, 0x80);
	write_cmos_sensor_8(0x5523, 0x80);
	write_cmos_sensor_8(0x5528, 0x80);
	write_cmos_sensor_8(0x5529, 0x80);
	write_cmos_sensor_8(0x552a, 0x80);
	write_cmos_sensor_8(0x552b, 0x80);
	write_cmos_sensor_8(0x5530, 0x08);
	write_cmos_sensor_8(0x5531, 0x08);
	write_cmos_sensor_8(0x5532, 0x08);
	write_cmos_sensor_8(0x5533, 0x08);
	write_cmos_sensor_8(0x5538, 0x08);
	write_cmos_sensor_8(0x5539, 0x08);
	write_cmos_sensor_8(0x553a, 0x08);
	write_cmos_sensor_8(0x553b, 0x08);
	write_cmos_sensor_8(0x5540, 0x80);
	write_cmos_sensor_8(0x5541, 0x80);
	write_cmos_sensor_8(0x5542, 0x80);
	write_cmos_sensor_8(0x5543, 0x80);
	write_cmos_sensor_8(0x5548, 0x80);
	write_cmos_sensor_8(0x5549, 0x80);
	write_cmos_sensor_8(0x554a, 0x80);
	write_cmos_sensor_8(0x554b, 0x80);
	write_cmos_sensor_8(0x5550, 0x08);
	write_cmos_sensor_8(0x5551, 0x08);
	write_cmos_sensor_8(0x5552, 0x08);
	write_cmos_sensor_8(0x5553, 0x08);
	write_cmos_sensor_8(0x5558, 0x08);
	write_cmos_sensor_8(0x5559, 0x08);
	write_cmos_sensor_8(0x555a, 0x08);
	write_cmos_sensor_8(0x555b, 0x08);
	write_cmos_sensor_8(0x5560, 0x80);
	write_cmos_sensor_8(0x5561, 0x80);
	write_cmos_sensor_8(0x5562, 0x80);
	write_cmos_sensor_8(0x5563, 0x80);
	write_cmos_sensor_8(0x5568, 0x80);
	write_cmos_sensor_8(0x5569, 0x80);
	write_cmos_sensor_8(0x556a, 0x80);
	write_cmos_sensor_8(0x556b, 0x80);
	write_cmos_sensor_8(0x5570, 0x08);
	write_cmos_sensor_8(0x5571, 0x08);
	write_cmos_sensor_8(0x5572, 0x08);
	write_cmos_sensor_8(0x5573, 0x08);
	write_cmos_sensor_8(0x5578, 0x08);
	write_cmos_sensor_8(0x5579, 0x08);
	write_cmos_sensor_8(0x557a, 0x08);
	write_cmos_sensor_8(0x557b, 0x08);
	write_cmos_sensor_8(0x5580, 0x80);
	write_cmos_sensor_8(0x5581, 0x80);
	write_cmos_sensor_8(0x5582, 0x80);
	write_cmos_sensor_8(0x5583, 0x80);
	write_cmos_sensor_8(0x5588, 0x80);
	write_cmos_sensor_8(0x5589, 0x80);
	write_cmos_sensor_8(0x558a, 0x80);
	write_cmos_sensor_8(0x558b, 0x80);
	write_cmos_sensor_8(0x5590, 0x08);
	write_cmos_sensor_8(0x5591, 0x08);
	write_cmos_sensor_8(0x5592, 0x08);
	write_cmos_sensor_8(0x5593, 0x08);
	write_cmos_sensor_8(0x5598, 0x08);
	write_cmos_sensor_8(0x5599, 0x08);
	write_cmos_sensor_8(0x559a, 0x08);
	write_cmos_sensor_8(0x559b, 0x08);
	write_cmos_sensor_8(0x55a0, 0x80);
	write_cmos_sensor_8(0x55a1, 0x80);
	write_cmos_sensor_8(0x55a2, 0x80);
	write_cmos_sensor_8(0x55a3, 0x80);
	write_cmos_sensor_8(0x55a8, 0x80);
	write_cmos_sensor_8(0x55a9, 0x80);
	write_cmos_sensor_8(0x55aa, 0x80);
	write_cmos_sensor_8(0x55ab, 0x80);
	write_cmos_sensor_8(0x55b0, 0x08);
	write_cmos_sensor_8(0x55b1, 0x08);
	write_cmos_sensor_8(0x55b2, 0x08);
	write_cmos_sensor_8(0x55b3, 0x08);
	write_cmos_sensor_8(0x55b8, 0x08);
	write_cmos_sensor_8(0x55b9, 0x08);
	write_cmos_sensor_8(0x55ba, 0x08);
	write_cmos_sensor_8(0x55bb, 0x08);
	write_cmos_sensor_8(0x55c0, 0x80);
	write_cmos_sensor_8(0x55c1, 0x80);
	write_cmos_sensor_8(0x55c2, 0x80);
	write_cmos_sensor_8(0x55c3, 0x80);
	write_cmos_sensor_8(0x55c8, 0x80);
	write_cmos_sensor_8(0x55c9, 0x80);
	write_cmos_sensor_8(0x55ca, 0x80);
	write_cmos_sensor_8(0x55cb, 0x80);
	write_cmos_sensor_8(0x55d0, 0x08);
	write_cmos_sensor_8(0x55d1, 0x08);
	write_cmos_sensor_8(0x55d2, 0x08);
	write_cmos_sensor_8(0x55d3, 0x08);
	write_cmos_sensor_8(0x55d8, 0x08);
	write_cmos_sensor_8(0x55d9, 0x08);
	write_cmos_sensor_8(0x55da, 0x08);
	write_cmos_sensor_8(0x55db, 0x08);
	write_cmos_sensor_8(0x55e0, 0x80);
	write_cmos_sensor_8(0x55e1, 0x80);
	write_cmos_sensor_8(0x55e2, 0x80);
	write_cmos_sensor_8(0x55e3, 0x80);
	write_cmos_sensor_8(0x55e8, 0x80);
	write_cmos_sensor_8(0x55e9, 0x80);
	write_cmos_sensor_8(0x55ea, 0x80);
	write_cmos_sensor_8(0x55eb, 0x80);
	write_cmos_sensor_8(0x55f0, 0x08);
	write_cmos_sensor_8(0x55f1, 0x08);
	write_cmos_sensor_8(0x55f2, 0x08);
	write_cmos_sensor_8(0x55f3, 0x08);
	write_cmos_sensor_8(0x55f8, 0x08);
	write_cmos_sensor_8(0x55f9, 0x08);
	write_cmos_sensor_8(0x55fa, 0x08);
	write_cmos_sensor_8(0x55fb, 0x08);
	write_cmos_sensor_8(0x5690, 0x02);
	write_cmos_sensor_8(0x5691, 0x10);
	write_cmos_sensor_8(0x5692, 0x02);
	write_cmos_sensor_8(0x5693, 0x08);
	write_cmos_sensor_8(0x5d24, 0x02);
	write_cmos_sensor_8(0x5d25, 0x10);
	write_cmos_sensor_8(0x5d26, 0x02);
	write_cmos_sensor_8(0x5d27, 0x08);
	write_cmos_sensor_8(0x5d3a, 0x58);
#ifdef sensor_flip
	#ifdef sensor_mirror
	write_cmos_sensor_8(0x3820, 0xc5);
	write_cmos_sensor_8(0x3821, 0x06);
	write_cmos_sensor_8(0x4000, 0x57);
	#else
	write_cmos_sensor_8(0x3820, 0xc5);
	write_cmos_sensor_8(0x3821, 0x02);
	write_cmos_sensor_8(0x4000, 0x57);
	#endif	
#else
	#ifdef sensor_mirror
	write_cmos_sensor_8(0x3820, 0x81);
	write_cmos_sensor_8(0x3821, 0x06);
	write_cmos_sensor_8(0x4000, 0x17);
	#else
	write_cmos_sensor_8(0x3820, 0x81);
	write_cmos_sensor_8(0x3821, 0x02);
	write_cmos_sensor_8(0x4000, 0x17);
	#endif	
#endif	
	
	write_cmos_sensor_8(0x5000, 0x9b);
	write_cmos_sensor_8(0x5002, 0x1c);
	write_cmos_sensor_8(0x5b85, 0x10);
	write_cmos_sensor_8(0x5b87, 0x20);
	write_cmos_sensor_8(0x5b89, 0x40);
	write_cmos_sensor_8(0x5b8b, 0x80);
	write_cmos_sensor_8(0x5b8f, 0x18);
	write_cmos_sensor_8(0x5b90, 0x18);
	write_cmos_sensor_8(0x5b91, 0x18);
	write_cmos_sensor_8(0x5b95, 0x18);
	write_cmos_sensor_8(0x5b96, 0x18);
	write_cmos_sensor_8(0x5b97, 0x18);
	
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x3018, 0x5a);
	write_cmos_sensor_8(0x366c, 0x00);
	write_cmos_sensor_8(0x3641, 0x00);
	write_cmos_sensor_8(0x5d29, 0x00);
	
	write_cmos_sensor_8(0x030a, 0x00);
	write_cmos_sensor_8(0x0311, 0x00);
	write_cmos_sensor_8(0x3606, 0x55);
	write_cmos_sensor_8(0x3607, 0x05);
	write_cmos_sensor_8(0x360b, 0x48);
	write_cmos_sensor_8(0x360c, 0x18);
	write_cmos_sensor_8(0x3720, 0x88);

	sensor_write_alpha_parm();

	write_cmos_sensor_8(0x0100, 0x01);
	write_cmos_sensor_8(0x4202, 0x00);

	mDELAY(40);
	
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");

  hs_video_setting();
}

static void video_call_setting(kal_uint16 currefps)
{
	
	LOG_INF("[sensor_pick]%s :E setting\n",__func__);

	write_cmos_sensor_8(0x0100, 0x0);
        write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0302, 0x21);
	write_cmos_sensor_8(0x0303, 0x0);
	write_cmos_sensor_8(0x030d, 0x30);
	write_cmos_sensor_8(0x030f, 0x3);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x3501, 0xe);
	write_cmos_sensor_8(0x3502, 0xea);
	write_cmos_sensor_8(0x3606, 0x55);
	write_cmos_sensor_8(0x3607, 0x5);
	write_cmos_sensor_8(0x3622, 0x79);
	write_cmos_sensor_8(0x3720, 0x88);
	write_cmos_sensor_8(0x3725, 0xf0);
	write_cmos_sensor_8(0x3726, 0x22);
	write_cmos_sensor_8(0x3727, 0x44);
	write_cmos_sensor_8(0x3728, 0x22);
	write_cmos_sensor_8(0x3729, 0x22);
	write_cmos_sensor_8(0x372a, 0x20);
	write_cmos_sensor_8(0x372b, 0x40);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3730, 0x0);
	write_cmos_sensor_8(0x3731, 0x0);
	write_cmos_sensor_8(0x3732, 0x0);
	write_cmos_sensor_8(0x3733, 0x0);
	write_cmos_sensor_8(0x3748, 0x0);
	write_cmos_sensor_8(0x3760, 0x90);
	write_cmos_sensor_8(0x37a1, 0x0);
	write_cmos_sensor_8(0x37a5, 0xaa);
	write_cmos_sensor_8(0x37a2, 0x0);
	write_cmos_sensor_8(0x37a3, 0x42);
	write_cmos_sensor_8(0x3800, 0x0);
	write_cmos_sensor_8(0x3801, 0x0);
	write_cmos_sensor_8(0x3802, 0x0);
	write_cmos_sensor_8(0x3803, 0x8);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0xd);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x9);
	write_cmos_sensor_8(0x3809, 0x20);
	write_cmos_sensor_8(0x380a, 0x6);
	write_cmos_sensor_8(0x380b, 0xd8);
	write_cmos_sensor_8(0x380c, 0x13);
	write_cmos_sensor_8(0x380d, 0xa0);
	write_cmos_sensor_8(0x380e, 0x1d);
	write_cmos_sensor_8(0x380f, 0xe0);
	write_cmos_sensor_8(0x3810, 0x0);
	write_cmos_sensor_8(0x3811, 0xa);
	write_cmos_sensor_8(0x3812, 0x0);
	write_cmos_sensor_8(0x3813, 0x4);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);
	write_cmos_sensor_8(0x3821, 0x0);
	write_cmos_sensor_8(0x3836, 0xc);
	write_cmos_sensor_8(0x3837, 0x2);
	write_cmos_sensor_8(0x3841, 0x2);
	write_cmos_sensor_8(0x4006, 0x0);
	write_cmos_sensor_8(0x4007, 0x0);
	write_cmos_sensor_8(0x4009, 0x9);
	write_cmos_sensor_8(0x4050, 0x0);
	write_cmos_sensor_8(0x4051, 0x1);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4837, 0x14);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5690, 0x0);
	write_cmos_sensor_8(0x5691, 0x0);
	write_cmos_sensor_8(0x5692, 0x0);
	write_cmos_sensor_8(0x5693, 0x8);
	write_cmos_sensor_8(0x5d24, 0x0);
	write_cmos_sensor_8(0x5d25, 0x0);
	write_cmos_sensor_8(0x5d26, 0x0);
	write_cmos_sensor_8(0x5d27, 0x8);
	write_cmos_sensor_8(0x5d3a, 0x58);
	write_cmos_sensor_8(0x3703, 0x42);
	write_cmos_sensor_8(0x3767, 0x30);
	write_cmos_sensor_8(0x3708, 0x42);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4640, 0x0);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4643, 0x8);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5d29, 0x0);
	write_cmos_sensor_8(0x5694, 0xc0);
	write_cmos_sensor_8(0x5000, 0xfb);
	write_cmos_sensor_8(0x501d, 0x50);
	write_cmos_sensor_8(0x569d, 0x68);
	write_cmos_sensor_8(0x569e, 0xd);
	write_cmos_sensor_8(0x569f, 0x68);
	write_cmos_sensor_8(0x5d2f, 0x68);
	write_cmos_sensor_8(0x5d30, 0xd);
	write_cmos_sensor_8(0x5d31, 0x68);
	write_cmos_sensor_8(0x5500, 0x0);
	write_cmos_sensor_8(0x5501, 0x0);
	write_cmos_sensor_8(0x5502, 0x0);
	write_cmos_sensor_8(0x5503, 0x0);
	write_cmos_sensor_8(0x5504, 0x0);
	write_cmos_sensor_8(0x5505, 0x0);
	write_cmos_sensor_8(0x5506, 0x0);
	write_cmos_sensor_8(0x5507, 0x0);
	write_cmos_sensor_8(0x5508, 0x20);
	write_cmos_sensor_8(0x5509, 0x0);
	write_cmos_sensor_8(0x550a, 0x20);
	write_cmos_sensor_8(0x550b, 0x0);
	write_cmos_sensor_8(0x550c, 0x0);
	write_cmos_sensor_8(0x550d, 0x0);
	write_cmos_sensor_8(0x550e, 0x0);
	write_cmos_sensor_8(0x550f, 0x0);
	write_cmos_sensor_8(0x5510, 0x0);
	write_cmos_sensor_8(0x5511, 0x0);
	write_cmos_sensor_8(0x5512, 0x0);
	write_cmos_sensor_8(0x5513, 0x0);
	write_cmos_sensor_8(0x5514, 0x0);
	write_cmos_sensor_8(0x5515, 0x0);
	write_cmos_sensor_8(0x5516, 0x0);
	write_cmos_sensor_8(0x5517, 0x0);
	write_cmos_sensor_8(0x5518, 0x20);
	write_cmos_sensor_8(0x5519, 0x0);
	write_cmos_sensor_8(0x551a, 0x20);
	write_cmos_sensor_8(0x551b, 0x0);
	write_cmos_sensor_8(0x551c, 0x0);
	write_cmos_sensor_8(0x551d, 0x0);
	write_cmos_sensor_8(0x551e, 0x0);
	write_cmos_sensor_8(0x551f, 0x0);
	write_cmos_sensor_8(0x5520, 0x0);
	write_cmos_sensor_8(0x5521, 0x0);
	write_cmos_sensor_8(0x5522, 0x0);
	write_cmos_sensor_8(0x5523, 0x0);
	write_cmos_sensor_8(0x5524, 0x0);
	write_cmos_sensor_8(0x5525, 0x0);
	write_cmos_sensor_8(0x5526, 0x0);
	write_cmos_sensor_8(0x5527, 0x0);
	write_cmos_sensor_8(0x5528, 0x0);
	write_cmos_sensor_8(0x5529, 0x20);
	write_cmos_sensor_8(0x552a, 0x0);
	write_cmos_sensor_8(0x552b, 0x20);
	write_cmos_sensor_8(0x552c, 0x0);
	write_cmos_sensor_8(0x552d, 0x0);
	write_cmos_sensor_8(0x552e, 0x0);
	write_cmos_sensor_8(0x552f, 0x0);
	write_cmos_sensor_8(0x5530, 0x0);
	write_cmos_sensor_8(0x5531, 0x0);
	write_cmos_sensor_8(0x5532, 0x0);
	write_cmos_sensor_8(0x5533, 0x0);
	write_cmos_sensor_8(0x5534, 0x0);
	write_cmos_sensor_8(0x5535, 0x0);
	write_cmos_sensor_8(0x5536, 0x0);
	write_cmos_sensor_8(0x5537, 0x0);
	write_cmos_sensor_8(0x5538, 0x0);
	write_cmos_sensor_8(0x5539, 0x20);
	write_cmos_sensor_8(0x553a, 0x0);
	write_cmos_sensor_8(0x553b, 0x20);
	write_cmos_sensor_8(0x553c, 0x0);
	write_cmos_sensor_8(0x553d, 0x0);
	write_cmos_sensor_8(0x553e, 0x0);
	write_cmos_sensor_8(0x553f, 0x0);
	write_cmos_sensor_8(0x5540, 0x0);
	write_cmos_sensor_8(0x5541, 0x0);
	write_cmos_sensor_8(0x5542, 0x0);
	write_cmos_sensor_8(0x5543, 0x0);
	write_cmos_sensor_8(0x5544, 0x0);
	write_cmos_sensor_8(0x5545, 0x0);
	write_cmos_sensor_8(0x5546, 0x0);
	write_cmos_sensor_8(0x5547, 0x0);
	write_cmos_sensor_8(0x5548, 0x20);
	write_cmos_sensor_8(0x5549, 0x0);
	write_cmos_sensor_8(0x554a, 0x20);
	write_cmos_sensor_8(0x554b, 0x0);
	write_cmos_sensor_8(0x554c, 0x0);
	write_cmos_sensor_8(0x554d, 0x0);
	write_cmos_sensor_8(0x554e, 0x0);
	write_cmos_sensor_8(0x554f, 0x0);
	write_cmos_sensor_8(0x5550, 0x0);
	write_cmos_sensor_8(0x5551, 0x0);
	write_cmos_sensor_8(0x5552, 0x0);
	write_cmos_sensor_8(0x5553, 0x0);
	write_cmos_sensor_8(0x5554, 0x0);
	write_cmos_sensor_8(0x5555, 0x0);
	write_cmos_sensor_8(0x5556, 0x0);
	write_cmos_sensor_8(0x5557, 0x0);
	write_cmos_sensor_8(0x5558, 0x20);
	write_cmos_sensor_8(0x5559, 0x0);
	write_cmos_sensor_8(0x555a, 0x20);
	write_cmos_sensor_8(0x555b, 0x0);
	write_cmos_sensor_8(0x555c, 0x0);
	write_cmos_sensor_8(0x555d, 0x0);
	write_cmos_sensor_8(0x555e, 0x0);
	write_cmos_sensor_8(0x555f, 0x0);
	write_cmos_sensor_8(0x5560, 0x0);
	write_cmos_sensor_8(0x5561, 0x0);
	write_cmos_sensor_8(0x5562, 0x0);
	write_cmos_sensor_8(0x5563, 0x0);
	write_cmos_sensor_8(0x5564, 0x0);
	write_cmos_sensor_8(0x5565, 0x0);
	write_cmos_sensor_8(0x5566, 0x0);
	write_cmos_sensor_8(0x5567, 0x0);
	write_cmos_sensor_8(0x5568, 0x0);
	write_cmos_sensor_8(0x5569, 0x20);
	write_cmos_sensor_8(0x556a, 0x0);
	write_cmos_sensor_8(0x556b, 0x20);
	write_cmos_sensor_8(0x556c, 0x0);
	write_cmos_sensor_8(0x556d, 0x0);
	write_cmos_sensor_8(0x556e, 0x0);
	write_cmos_sensor_8(0x556f, 0x0);
	write_cmos_sensor_8(0x5570, 0x0);
	write_cmos_sensor_8(0x5571, 0x0);
	write_cmos_sensor_8(0x5572, 0x0);
	write_cmos_sensor_8(0x5573, 0x0);
	write_cmos_sensor_8(0x5574, 0x0);
	write_cmos_sensor_8(0x5575, 0x0);
	write_cmos_sensor_8(0x5576, 0x0);
	write_cmos_sensor_8(0x5577, 0x0);
	write_cmos_sensor_8(0x5578, 0x0);
	write_cmos_sensor_8(0x5579, 0x20);
	write_cmos_sensor_8(0x557a, 0x0);
	write_cmos_sensor_8(0x557b, 0x20);
	write_cmos_sensor_8(0x557c, 0x0);
	write_cmos_sensor_8(0x557d, 0x0);
	write_cmos_sensor_8(0x557e, 0x0);
	write_cmos_sensor_8(0x557f, 0x0);
	write_cmos_sensor_8(0x5580, 0x0);
	write_cmos_sensor_8(0x5581, 0x0);
	write_cmos_sensor_8(0x5582, 0x0);
	write_cmos_sensor_8(0x5583, 0x0);
	write_cmos_sensor_8(0x5584, 0x0);
	write_cmos_sensor_8(0x5585, 0x0);
	write_cmos_sensor_8(0x5586, 0x0);
	write_cmos_sensor_8(0x5587, 0x0);
	write_cmos_sensor_8(0x5588, 0x20);
	write_cmos_sensor_8(0x5589, 0x0);
	write_cmos_sensor_8(0x558a, 0x20);
	write_cmos_sensor_8(0x558b, 0x0);
	write_cmos_sensor_8(0x558c, 0x0);
	write_cmos_sensor_8(0x558d, 0x0);
	write_cmos_sensor_8(0x558e, 0x0);
	write_cmos_sensor_8(0x558f, 0x0);
	write_cmos_sensor_8(0x5590, 0x0);
	write_cmos_sensor_8(0x5591, 0x0);
	write_cmos_sensor_8(0x5592, 0x0);
	write_cmos_sensor_8(0x5593, 0x0);
	write_cmos_sensor_8(0x5594, 0x0);
	write_cmos_sensor_8(0x5595, 0x0);
	write_cmos_sensor_8(0x5596, 0x0);
	write_cmos_sensor_8(0x5597, 0x0);
	write_cmos_sensor_8(0x5598, 0x20);
	write_cmos_sensor_8(0x5599, 0x0);
	write_cmos_sensor_8(0x559a, 0x20);
	write_cmos_sensor_8(0x559b, 0x0);
	write_cmos_sensor_8(0x559c, 0x0);
	write_cmos_sensor_8(0x559d, 0x0);
	write_cmos_sensor_8(0x559e, 0x0);
	write_cmos_sensor_8(0x559f, 0x0);
	write_cmos_sensor_8(0x55a0, 0x0);
	write_cmos_sensor_8(0x55a1, 0x0);
	write_cmos_sensor_8(0x55a2, 0x0);
	write_cmos_sensor_8(0x55a3, 0x0);
	write_cmos_sensor_8(0x55a4, 0x0);
	write_cmos_sensor_8(0x55a5, 0x0);
	write_cmos_sensor_8(0x55a6, 0x0);
	write_cmos_sensor_8(0x55a7, 0x0);
	write_cmos_sensor_8(0x55a8, 0x0);
	write_cmos_sensor_8(0x55a9, 0x20);
	write_cmos_sensor_8(0x55aa, 0x0);
	write_cmos_sensor_8(0x55ab, 0x20);
	write_cmos_sensor_8(0x55ac, 0x0);
	write_cmos_sensor_8(0x55ad, 0x0);
	write_cmos_sensor_8(0x55ae, 0x0);
	write_cmos_sensor_8(0x55af, 0x0);
	write_cmos_sensor_8(0x55b0, 0x0);
	write_cmos_sensor_8(0x55b1, 0x0);
	write_cmos_sensor_8(0x55b2, 0x0);
	write_cmos_sensor_8(0x55b3, 0x0);
	write_cmos_sensor_8(0x55b4, 0x0);
	write_cmos_sensor_8(0x55b5, 0x0);
	write_cmos_sensor_8(0x55b6, 0x0);
	write_cmos_sensor_8(0x55b7, 0x0);
	write_cmos_sensor_8(0x55b8, 0x0);
	write_cmos_sensor_8(0x55b9, 0x20);
	write_cmos_sensor_8(0x55ba, 0x0);
	write_cmos_sensor_8(0x55bb, 0x20);
	write_cmos_sensor_8(0x55bc, 0x0);
	write_cmos_sensor_8(0x55bd, 0x0);
	write_cmos_sensor_8(0x55be, 0x0);
	write_cmos_sensor_8(0x55bf, 0x0);
	write_cmos_sensor_8(0x55c0, 0x0);
	write_cmos_sensor_8(0x55c1, 0x0);
	write_cmos_sensor_8(0x55c2, 0x0);
	write_cmos_sensor_8(0x55c3, 0x0);
	write_cmos_sensor_8(0x55c4, 0x0);
	write_cmos_sensor_8(0x55c5, 0x0);
	write_cmos_sensor_8(0x55c6, 0x0);
	write_cmos_sensor_8(0x55c7, 0x0);
	write_cmos_sensor_8(0x55c8, 0x20);
	write_cmos_sensor_8(0x55c9, 0x0);
	write_cmos_sensor_8(0x55ca, 0x20);
	write_cmos_sensor_8(0x55cb, 0x0);
	write_cmos_sensor_8(0x55cc, 0x0);
	write_cmos_sensor_8(0x55cd, 0x0);
	write_cmos_sensor_8(0x55ce, 0x0);
	write_cmos_sensor_8(0x55cf, 0x0);
	write_cmos_sensor_8(0x55d0, 0x0);
	write_cmos_sensor_8(0x55d1, 0x0);
	write_cmos_sensor_8(0x55d2, 0x0);
	write_cmos_sensor_8(0x55d3, 0x0);
	write_cmos_sensor_8(0x55d4, 0x0);
	write_cmos_sensor_8(0x55d5, 0x0);
	write_cmos_sensor_8(0x55d6, 0x0);
	write_cmos_sensor_8(0x55d7, 0x0);
	write_cmos_sensor_8(0x55d8, 0x20);
	write_cmos_sensor_8(0x55d9, 0x0);
	write_cmos_sensor_8(0x55da, 0x20);
	write_cmos_sensor_8(0x55db, 0x0);
	write_cmos_sensor_8(0x55dc, 0x0);
	write_cmos_sensor_8(0x55dd, 0x0);
	write_cmos_sensor_8(0x55de, 0x0);
	write_cmos_sensor_8(0x55df, 0x0);
	write_cmos_sensor_8(0x55e0, 0x0);
	write_cmos_sensor_8(0x55e1, 0x0);
	write_cmos_sensor_8(0x55e2, 0x0);
	write_cmos_sensor_8(0x55e3, 0x0);
	write_cmos_sensor_8(0x55e4, 0x0);
	write_cmos_sensor_8(0x55e5, 0x0);
	write_cmos_sensor_8(0x55e6, 0x0);
	write_cmos_sensor_8(0x55e7, 0x0);
	write_cmos_sensor_8(0x55e8, 0x0);
	write_cmos_sensor_8(0x55e9, 0x20);
	write_cmos_sensor_8(0x55ea, 0x0);
	write_cmos_sensor_8(0x55eb, 0x20);
	write_cmos_sensor_8(0x55ec, 0x0);
	write_cmos_sensor_8(0x55ed, 0x0);
	write_cmos_sensor_8(0x55ee, 0x0);
	write_cmos_sensor_8(0x55ef, 0x0);
	write_cmos_sensor_8(0x55f0, 0x0);
	write_cmos_sensor_8(0x55f1, 0x0);
	write_cmos_sensor_8(0x55f2, 0x0);
	write_cmos_sensor_8(0x55f3, 0x0);
	write_cmos_sensor_8(0x55f4, 0x0);
	write_cmos_sensor_8(0x55f5, 0x0);
	write_cmos_sensor_8(0x55f6, 0x0);
	write_cmos_sensor_8(0x55f7, 0x0);
	write_cmos_sensor_8(0x55f8, 0x0);
	write_cmos_sensor_8(0x55f9, 0x20);
	write_cmos_sensor_8(0x55fa, 0x0);
	write_cmos_sensor_8(0x55fb, 0x20);
	write_cmos_sensor_8(0x55fc, 0x0);
	write_cmos_sensor_8(0x55fd, 0x0);
	write_cmos_sensor_8(0x55fe, 0x0);
	write_cmos_sensor_8(0x55ff, 0x0);
	write_cmos_sensor_8(0x5022, 0x93);
	write_cmos_sensor_8(0x0302, 0x21);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4640, 0x0);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4643, 0x8);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4809, 0x2b);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5d29, 0x0);

	
	sensor_write_alpha_parm();
	

	write_cmos_sensor_8(0x0100, 0x1);
        write_cmos_sensor_8(0x4202, 0x00);
}

static void hyperlapse_setting(void)
{
	
	LOG_INF("[sensor_pick]%s :E setting\n",__func__);
	normal_video_setting(30);
}

static void sensor_init(void)
{
	LOG_INF("[sensor_pick]%s :E setting\n",__func__);

	
	write_cmos_sensor_8(0x301e, 0x00);
	write_cmos_sensor_8(0x0103, 0x01);
	write_cmos_sensor_8(0x0300, 0x0);
	write_cmos_sensor_8(0x0302, 0x3d);
	write_cmos_sensor_8(0x0303, 0x0);
	write_cmos_sensor_8(0x0304, 0x7);
	write_cmos_sensor_8(0x030e, 0x2);
	write_cmos_sensor_8(0x030f, 0x3);
	write_cmos_sensor_8(0x0312, 0x3);
	write_cmos_sensor_8(0x0313, 0x3);
	write_cmos_sensor_8(0x031e, 0x9);
	write_cmos_sensor_8(0x3002, 0x0);
	write_cmos_sensor_8(0x3009, 0x6);
	write_cmos_sensor_8(0x3010, 0x0);
	write_cmos_sensor_8(0x3011, 0x4);
	write_cmos_sensor_8(0x3012, 0x41);
	write_cmos_sensor_8(0x3013, 0xcc);
	write_cmos_sensor_8(0x3017, 0xf0);
	write_cmos_sensor_8(0x3018, 0xda);
	write_cmos_sensor_8(0x3019, 0xf1);
	write_cmos_sensor_8(0x301a, 0xf2);
	write_cmos_sensor_8(0x301b, 0x96);
	write_cmos_sensor_8(0x301d, 0x2);
	write_cmos_sensor_8(0x3022, 0xe);
	write_cmos_sensor_8(0x3023, 0xb0);
	write_cmos_sensor_8(0x3028, 0xc3);
	write_cmos_sensor_8(0x3031, 0x68);
	write_cmos_sensor_8(0x3400, 0x8);
	write_cmos_sensor_8(0x3501, 0xe);
	write_cmos_sensor_8(0x3502, 0xea);
        write_cmos_sensor_8(0x3503, 0x33);
	write_cmos_sensor_8(0x3507, 0x2);
	write_cmos_sensor_8(0x3508, 0x0);
	write_cmos_sensor_8(0x3509, 0x12);
	write_cmos_sensor_8(0x350a, 0x0);
	write_cmos_sensor_8(0x350b, 0x40);
	write_cmos_sensor_8(0x3549, 0x12);
	write_cmos_sensor_8(0x354e, 0x0);
	write_cmos_sensor_8(0x354f, 0x10);
	write_cmos_sensor_8(0x3600, 0x12);
	write_cmos_sensor_8(0x3603, 0xc0);
	write_cmos_sensor_8(0x3604, 0x2c);
	write_cmos_sensor_8(0x3606, 0x88);
	write_cmos_sensor_8(0x3607, 0x8);
	write_cmos_sensor_8(0x360a, 0x6d);
	write_cmos_sensor_8(0x360d, 0x5);
	write_cmos_sensor_8(0x360e, 0x7);
	write_cmos_sensor_8(0x360f, 0x44);
	write_cmos_sensor_8(0x3622, 0x75);
	write_cmos_sensor_8(0x3623, 0x57);
	write_cmos_sensor_8(0x3624, 0x50);
	write_cmos_sensor_8(0x362b, 0x77);
	write_cmos_sensor_8(0x3641, 0x0);
	write_cmos_sensor_8(0x3660, 0xc0);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x3661, 0xf);
	write_cmos_sensor_8(0x3662, 0x0);
	write_cmos_sensor_8(0x3663, 0x20);
	write_cmos_sensor_8(0x3664, 0x8);
	write_cmos_sensor_8(0x3667, 0x0);
	write_cmos_sensor_8(0x366a, 0x54);
	write_cmos_sensor_8(0x366c, 0x10);
	write_cmos_sensor_8(0x3708, 0x43);
	write_cmos_sensor_8(0x3709, 0x1c);
	write_cmos_sensor_8(0x3718, 0x8c);
	write_cmos_sensor_8(0x3719, 0x0);
	write_cmos_sensor_8(0x371a, 0x4);
	write_cmos_sensor_8(0x3725, 0xf0);
	write_cmos_sensor_8(0x3726, 0x0);
	write_cmos_sensor_8(0x3727, 0x22);
	write_cmos_sensor_8(0x3728, 0x22);
	write_cmos_sensor_8(0x3729, 0x0);
	write_cmos_sensor_8(0x372a, 0x20);
	write_cmos_sensor_8(0x372b, 0x40);
	write_cmos_sensor_8(0x372f, 0x92);
	write_cmos_sensor_8(0x3730, 0x0);
	write_cmos_sensor_8(0x3731, 0x0);
	write_cmos_sensor_8(0x3732, 0x0);
	write_cmos_sensor_8(0x3733, 0x0);
	write_cmos_sensor_8(0x3748, 0x0);
	write_cmos_sensor_8(0x3760, 0x92);
	write_cmos_sensor_8(0x3767, 0x31);
	write_cmos_sensor_8(0x3772, 0x0);
	write_cmos_sensor_8(0x3773, 0x0);
	write_cmos_sensor_8(0x3774, 0x40);
	write_cmos_sensor_8(0x3775, 0x81);
	write_cmos_sensor_8(0x3776, 0x31);
	write_cmos_sensor_8(0x3777, 0x6);
	write_cmos_sensor_8(0x3778, 0xa0);
	write_cmos_sensor_8(0x377f, 0x31);
	write_cmos_sensor_8(0x378d, 0x39);
	write_cmos_sensor_8(0x3790, 0xcc);
	write_cmos_sensor_8(0x3791, 0xcc);
	write_cmos_sensor_8(0x379c, 0x2);
	write_cmos_sensor_8(0x379d, 0x0);
	write_cmos_sensor_8(0x379e, 0x0);
	write_cmos_sensor_8(0x379f, 0x1e);
	write_cmos_sensor_8(0x37a0, 0x22);
	write_cmos_sensor_8(0x37a1, 0x2);
	write_cmos_sensor_8(0x37a5, 0x96);
	write_cmos_sensor_8(0x37a2, 0x4);
	write_cmos_sensor_8(0x37a3, 0x23);
	write_cmos_sensor_8(0x37a4, 0x0);
	write_cmos_sensor_8(0x37b0, 0x48);
	write_cmos_sensor_8(0x37b3, 0x62);
	write_cmos_sensor_8(0x37b6, 0x22);
	write_cmos_sensor_8(0x37b9, 0x23);
	write_cmos_sensor_8(0x37c3, 0x7);
	write_cmos_sensor_8(0x37c6, 0x35);
	write_cmos_sensor_8(0x37c8, 0x0);
	write_cmos_sensor_8(0x37c9, 0x0);
	write_cmos_sensor_8(0x37cc, 0x93);
	write_cmos_sensor_8(0x3800, 0x0);
	write_cmos_sensor_8(0x3801, 0x0);
	write_cmos_sensor_8(0x3802, 0x0);
	write_cmos_sensor_8(0x3803, 0x8);
	write_cmos_sensor_8(0x3804, 0x12);
	write_cmos_sensor_8(0x3805, 0x5f);
	write_cmos_sensor_8(0x3806, 0xd);
	write_cmos_sensor_8(0x3807, 0xc7);
	write_cmos_sensor_8(0x3808, 0x12);
	write_cmos_sensor_8(0x3809, 0x40);
	write_cmos_sensor_8(0x380a, 0xd);
	write_cmos_sensor_8(0x380b, 0xb0);
	write_cmos_sensor_8(0x380c, 0x14);
	write_cmos_sensor_8(0x380d, 0x4);
	write_cmos_sensor_8(0x380e, 0xe);
	write_cmos_sensor_8(0x380f, 0xf0);
	write_cmos_sensor_8(0x3810, 0x0);
	write_cmos_sensor_8(0x3811, 0x10);
	write_cmos_sensor_8(0x3812, 0x0);
	write_cmos_sensor_8(0x3813, 0x8);
	write_cmos_sensor_8(0x3814, 0x11);
	write_cmos_sensor_8(0x3815, 0x11);
	write_cmos_sensor_8(0x3820, 0xc4);
	write_cmos_sensor_8(0x3821, 0x0);
	write_cmos_sensor_8(0x3834, 0x0);
	write_cmos_sensor_8(0x3836, 0x14);
	write_cmos_sensor_8(0x3837, 0x2);
	write_cmos_sensor_8(0x3841, 0x4);
	write_cmos_sensor_8(0x3d85, 0x17);
	write_cmos_sensor_8(0x3d8c, 0x79);
	write_cmos_sensor_8(0x3d8d, 0x7f);
	write_cmos_sensor_8(0x3f00, 0x50);
	write_cmos_sensor_8(0x3f85, 0x0);
	write_cmos_sensor_8(0x3f86, 0x0);
	write_cmos_sensor_8(0x3f87, 0x5);
	write_cmos_sensor_8(0x3f9e, 0x0);
	write_cmos_sensor_8(0x3f9f, 0x0);
	write_cmos_sensor_8(0x4000, 0x57);
	write_cmos_sensor_8(0x4001, 0x60);
	write_cmos_sensor_8(0x4003, 0x40);
	write_cmos_sensor_8(0x4006, 0x0);
	write_cmos_sensor_8(0x4007, 0x0);
	write_cmos_sensor_8(0x4008, 0x0);
	write_cmos_sensor_8(0x4009, 0xf);
	write_cmos_sensor_8(0x400f, 0x9);
	write_cmos_sensor_8(0x4010, 0xf0);
	write_cmos_sensor_8(0x4011, 0xf0);
	write_cmos_sensor_8(0x4013, 0x2);
	write_cmos_sensor_8(0x4018, 0x4);
	write_cmos_sensor_8(0x4019, 0x4);
	write_cmos_sensor_8(0x401a, 0x48);
	write_cmos_sensor_8(0x4020, 0x8);
	write_cmos_sensor_8(0x4022, 0x8);
	write_cmos_sensor_8(0x4024, 0x8);
	write_cmos_sensor_8(0x4026, 0x8);
	write_cmos_sensor_8(0x4028, 0x8);
	write_cmos_sensor_8(0x402a, 0x8);
	write_cmos_sensor_8(0x402c, 0x8);
	write_cmos_sensor_8(0x402e, 0x8);
	write_cmos_sensor_8(0x4030, 0x8);
	write_cmos_sensor_8(0x4032, 0x8);
	write_cmos_sensor_8(0x4034, 0x8);
	write_cmos_sensor_8(0x4036, 0x8);
	write_cmos_sensor_8(0x4038, 0x8);
	write_cmos_sensor_8(0x403a, 0x8);
	write_cmos_sensor_8(0x403c, 0x8);
	write_cmos_sensor_8(0x403e, 0x8);
	write_cmos_sensor_8(0x4040, 0x0);
	write_cmos_sensor_8(0x4041, 0x0);
	write_cmos_sensor_8(0x4042, 0x0);
	write_cmos_sensor_8(0x4043, 0x0);
	write_cmos_sensor_8(0x4044, 0x0);
	write_cmos_sensor_8(0x4045, 0x0);
	write_cmos_sensor_8(0x4046, 0x0);
	write_cmos_sensor_8(0x4047, 0x0);
	write_cmos_sensor_8(0x4050, 0x4);
	write_cmos_sensor_8(0x4051, 0xf);
	write_cmos_sensor_8(0x40b0, 0x0);
	write_cmos_sensor_8(0x40b1, 0x0);
	write_cmos_sensor_8(0x40b2, 0x0);
	write_cmos_sensor_8(0x40b3, 0x0);
	write_cmos_sensor_8(0x40b4, 0x0);
	write_cmos_sensor_8(0x40b5, 0x0);
	write_cmos_sensor_8(0x40b6, 0x0);
	write_cmos_sensor_8(0x40b7, 0x0);
	write_cmos_sensor_8(0x4052, 0x0);
	write_cmos_sensor_8(0x4053, 0x89);
	write_cmos_sensor_8(0x4054, 0x0);
	write_cmos_sensor_8(0x4055, 0xa7);
	write_cmos_sensor_8(0x4056, 0x0);
	write_cmos_sensor_8(0x4057, 0x90);
	write_cmos_sensor_8(0x4058, 0x0);
	write_cmos_sensor_8(0x4059, 0x94);
	write_cmos_sensor_8(0x4202, 0x0);
	write_cmos_sensor_8(0x4203, 0x0);
	write_cmos_sensor_8(0x4066, 0x2);
	write_cmos_sensor_8(0x4501, 0x38);
	write_cmos_sensor_8(0x4509, 0x7);
	write_cmos_sensor_8(0x4605, 0x3);
	write_cmos_sensor_8(0x4641, 0x23);
	write_cmos_sensor_8(0x4645, 0x4);
	write_cmos_sensor_8(0x4800, 0x4);
	write_cmos_sensor_8(0x4809, 0x2b);
	write_cmos_sensor_8(0x4812, 0x2b);
	write_cmos_sensor_8(0x4813, 0x98);
	write_cmos_sensor_8(0x4833, 0x18);
	write_cmos_sensor_8(0x4837, 0xb);
	write_cmos_sensor_8(0x484b, 0x1);
	write_cmos_sensor_8(0x4850, 0x7c);
	write_cmos_sensor_8(0x4852, 0x6);
	write_cmos_sensor_8(0x4856, 0x58);
	write_cmos_sensor_8(0x4857, 0xaa);
	write_cmos_sensor_8(0x4862, 0xa);
	write_cmos_sensor_8(0x4867, 0x2);
	write_cmos_sensor_8(0x4869, 0x18);
	write_cmos_sensor_8(0x486a, 0x6a);
	write_cmos_sensor_8(0x486e, 0x7);
	write_cmos_sensor_8(0x486f, 0x55);
	write_cmos_sensor_8(0x4875, 0xf0);
	write_cmos_sensor_8(0x4b05, 0x93);
	write_cmos_sensor_8(0x4b06, 0x0);
	write_cmos_sensor_8(0x4c01, 0x5f);
	write_cmos_sensor_8(0x4d00, 0x4);
	write_cmos_sensor_8(0x4d01, 0x22);
	write_cmos_sensor_8(0x4d02, 0xb7);
	write_cmos_sensor_8(0x4d03, 0xca);
	write_cmos_sensor_8(0x4d04, 0x30);
	write_cmos_sensor_8(0x4d05, 0x1d);
	write_cmos_sensor_8(0x4f00, 0x1c);
	write_cmos_sensor_8(0x4f03, 0x50);
	write_cmos_sensor_8(0x4f04, 0x1);
	write_cmos_sensor_8(0x4f05, 0x7c);
	write_cmos_sensor_8(0x4f08, 0x0);
	write_cmos_sensor_8(0x4f09, 0x60);
	write_cmos_sensor_8(0x4f0a, 0x0);
	write_cmos_sensor_8(0x4f0b, 0x30);
	write_cmos_sensor_8(0x4f14, 0xf0);
	write_cmos_sensor_8(0x4f15, 0xf0);
	write_cmos_sensor_8(0x4f16, 0xf0);
	write_cmos_sensor_8(0x4f17, 0xf0);
	write_cmos_sensor_8(0x4f18, 0xf0);
	write_cmos_sensor_8(0x4f19, 0x0);
	write_cmos_sensor_8(0x4f1a, 0x0);
	write_cmos_sensor_8(0x4f20, 0x7);
	write_cmos_sensor_8(0x4f21, 0xd0);
	write_cmos_sensor_8(0x5000, 0x9b);
	write_cmos_sensor_8(0x5001, 0x4a);
	write_cmos_sensor_8(0x5002, 0x1c);
	write_cmos_sensor_8(0x501d, 0x0);
	write_cmos_sensor_8(0x5020, 0x3);
	write_cmos_sensor_8(0x5022, 0x91);
	write_cmos_sensor_8(0x5023, 0x0);
	write_cmos_sensor_8(0x5030, 0x0);
	write_cmos_sensor_8(0x5056, 0x0);
	write_cmos_sensor_8(0x5063, 0x0);
	write_cmos_sensor_8(0x5200, 0x2);
	write_cmos_sensor_8(0x5201, 0x71);
	write_cmos_sensor_8(0x5202, 0x0);
	write_cmos_sensor_8(0x5203, 0x10);
	write_cmos_sensor_8(0x5204, 0x0);
	write_cmos_sensor_8(0x5205, 0x90);
	write_cmos_sensor_8(0x5209, 0x81);
	write_cmos_sensor_8(0x520e, 0x31);
	write_cmos_sensor_8(0x5280, 0x0);
	write_cmos_sensor_8(0x5400, 0x1);
	write_cmos_sensor_8(0x5401, 0x0);
	write_cmos_sensor_8(0x5600, 0x30);
	write_cmos_sensor_8(0x560f, 0xfc);
	write_cmos_sensor_8(0x5610, 0xf0);
	write_cmos_sensor_8(0x5611, 0x10);
	write_cmos_sensor_8(0x562f, 0xfc);
	write_cmos_sensor_8(0x5630, 0xf0);
	write_cmos_sensor_8(0x5631, 0x10);
	write_cmos_sensor_8(0x564f, 0xfc);
	write_cmos_sensor_8(0x5650, 0xf0);
	write_cmos_sensor_8(0x5651, 0x10);
	write_cmos_sensor_8(0x566f, 0xfc);
	write_cmos_sensor_8(0x5670, 0xf0);
	write_cmos_sensor_8(0x5671, 0x10);
	write_cmos_sensor_8(0x5690, 0x0);
	write_cmos_sensor_8(0x5691, 0x0);
	write_cmos_sensor_8(0x5692, 0x0);
	write_cmos_sensor_8(0x5693, 0x8);
	write_cmos_sensor_8(0x5694, 0xc0);
	write_cmos_sensor_8(0x5695, 0x0);
	write_cmos_sensor_8(0x5696, 0x0);
	write_cmos_sensor_8(0x5697, 0x8);
	write_cmos_sensor_8(0x5698, 0x0);
	write_cmos_sensor_8(0x5699, 0x70);
	write_cmos_sensor_8(0x569a, 0x11);
	write_cmos_sensor_8(0x569b, 0xf0);
	write_cmos_sensor_8(0x569c, 0x0);
	write_cmos_sensor_8(0x569d, 0x68);
	write_cmos_sensor_8(0x569e, 0xd);
	write_cmos_sensor_8(0x569f, 0x68);
	write_cmos_sensor_8(0x56a1, 0xff);
	write_cmos_sensor_8(0x5bd9, 0x1);
	write_cmos_sensor_8(0x5d04, 0x1);
	write_cmos_sensor_8(0x5d05, 0x0);
	write_cmos_sensor_8(0x5d06, 0x1);
	write_cmos_sensor_8(0x5d07, 0x0);
	write_cmos_sensor_8(0x5d12, 0x38);
	write_cmos_sensor_8(0x5d13, 0x38);
	write_cmos_sensor_8(0x5d14, 0x38);
	write_cmos_sensor_8(0x5d15, 0x38);
	write_cmos_sensor_8(0x5d16, 0x38);
	write_cmos_sensor_8(0x5d17, 0x38);
	write_cmos_sensor_8(0x5d18, 0x38);
	write_cmos_sensor_8(0x5d19, 0x38);
	write_cmos_sensor_8(0x5d1a, 0x38);
	write_cmos_sensor_8(0x5d1b, 0x38);
	write_cmos_sensor_8(0x5d1c, 0x0);
	write_cmos_sensor_8(0x5d1d, 0x0);
	write_cmos_sensor_8(0x5d1e, 0x0);
	write_cmos_sensor_8(0x5d1f, 0x0);
	write_cmos_sensor_8(0x5d20, 0x16);
	write_cmos_sensor_8(0x5d21, 0x20);
	write_cmos_sensor_8(0x5d22, 0x10);
	write_cmos_sensor_8(0x5d23, 0xa0);
	write_cmos_sensor_8(0x5d24, 0x0);
	write_cmos_sensor_8(0x5d25, 0x0);
	write_cmos_sensor_8(0x5d26, 0x0);
	write_cmos_sensor_8(0x5d27, 0x8);
	write_cmos_sensor_8(0x5d28, 0xc0);
	write_cmos_sensor_8(0x5d29, 0x0);
	write_cmos_sensor_8(0x5d2a, 0x0);
	write_cmos_sensor_8(0x5d2b, 0x70);
	write_cmos_sensor_8(0x5d2c, 0x11);
	write_cmos_sensor_8(0x5d2d, 0xf0);
	write_cmos_sensor_8(0x5d2e, 0x0);
	write_cmos_sensor_8(0x5d2f, 0x68);
	write_cmos_sensor_8(0x5d30, 0xd);
	write_cmos_sensor_8(0x5d31, 0x68);
	write_cmos_sensor_8(0x5d32, 0x0);
	write_cmos_sensor_8(0x5d38, 0x70);
	write_cmos_sensor_8(0x5d3a, 0x58);
	write_cmos_sensor_8(0x5c80, 0x6);
	write_cmos_sensor_8(0x5c81, 0x90);
	write_cmos_sensor_8(0x5c82, 0x9);
	write_cmos_sensor_8(0x5c83, 0x5f);
	write_cmos_sensor_8(0x5c85, 0x6c);
	write_cmos_sensor_8(0x5601, 0x4);
	write_cmos_sensor_8(0x5602, 0x2);
	write_cmos_sensor_8(0x5603, 0x1);
	write_cmos_sensor_8(0x5604, 0x4);
	write_cmos_sensor_8(0x5605, 0x2);
	write_cmos_sensor_8(0x5606, 0x1);
	write_cmos_sensor_8(0x5b80, 0x0);
	write_cmos_sensor_8(0x5b81, 0x4);
	write_cmos_sensor_8(0x5b82, 0x0);
	write_cmos_sensor_8(0x5b83, 0x8);
	write_cmos_sensor_8(0x5b84, 0x0);
	write_cmos_sensor_8(0x5b85, 0x10);
	write_cmos_sensor_8(0x5b86, 0x0);
	write_cmos_sensor_8(0x5b87, 0x20);
	write_cmos_sensor_8(0x5b88, 0x0);
	write_cmos_sensor_8(0x5b89, 0x40);
	write_cmos_sensor_8(0x5b8a, 0x0);
	write_cmos_sensor_8(0x5b8b, 0x80);
	write_cmos_sensor_8(0x5b8c, 0x1a);
	write_cmos_sensor_8(0x5b8d, 0x1a);
	write_cmos_sensor_8(0x5b8e, 0x1a);
	write_cmos_sensor_8(0x5b8f, 0x18);
	write_cmos_sensor_8(0x5b90, 0x18);
	write_cmos_sensor_8(0x5b91, 0x18);
	write_cmos_sensor_8(0x5b92, 0x1a);
	write_cmos_sensor_8(0x5b93, 0x1a);
	write_cmos_sensor_8(0x5b94, 0x1a);
	write_cmos_sensor_8(0x5b95, 0x18);
	write_cmos_sensor_8(0x5b96, 0x18);
	write_cmos_sensor_8(0x5b97, 0x18);
	write_cmos_sensor_8(0x3708, 0x43);
	write_cmos_sensor_8(0x3767, 0x31);
	write_cmos_sensor_8(0x030a, 0x0);
	write_cmos_sensor_8(0x0311, 0x0);
	write_cmos_sensor_8(0x3606, 0x88);
	write_cmos_sensor_8(0x3607, 0x8);
	write_cmos_sensor_8(0x360b, 0x48);
	write_cmos_sensor_8(0x360c, 0x18);
	write_cmos_sensor_8(0x3720, 0xaa);
	write_cmos_sensor_8(0x4202, 0x0f);
	write_cmos_sensor_8(0x0100, 0x01);

}	

extern bool LC898123AF_readFlash(BYTE* data);
extern bool read_otp_pdaf_data( kal_uint16 addr, BYTE* data, kal_uint32 size);

#define OTP_DATA_SIZE 32
BYTE ov16880_otp_data[OTP_DATA_SIZE];
static int read_ov16880_otp_flag = 0;
static int read_ov16880_otp(void){
	int ret = 0;
	unsigned short inf_val;
	unsigned short mac_val;

	LOG_INF("==========read_ov16880_otp_flag =%d=======\n",read_ov16880_otp_flag);
	if(read_ov16880_otp_flag !=0){
		LOG_INF("==========ov16880 otp readed=======\n");
		return 0;
	}

	memset(ov16880_otp_data, 0x0, OTP_DATA_SIZE);
       
	ret = LC898123AF_readFlash(ov16880_otp_data);

	LOG_INF("OTP Module Vendor = 0x%x\n",                    ov16880_otp_data[0x00]);
	LOG_INF("OTP Lens Vendor = 0x%x\n",                      ov16880_otp_data[0x01]);
	LOG_INF("OTP Lens Rev = 0x%x\n",                         ov16880_otp_data[0x02]);
	LOG_INF("OTP Sensor Vendor = 0x%x\n",                    ov16880_otp_data[0x03]);
	LOG_INF("OTP Sensor Rev = 0x%x\n",                       ov16880_otp_data[0x04]);
	LOG_INF("OTP Drive IC Vendor = 0x%x\n",                    ov16880_otp_data[0x05]);
	LOG_INF("OTP Drive IC FW Rev = 0x%x\n",                     ov16880_otp_data[0x06]);
	LOG_INF("OTP Actuator Vendor = 0x%x\n",                    ov16880_otp_data[0x07]);
	LOG_INF("OTP Actuator Rev = 0x%x\n",                     ov16880_otp_data[0x08]);
	LOG_INF("OTP PCB Rev = 0x%x\n",                    ov16880_otp_data[0x09]);
	LOG_INF("OTP Module ID = 0x%x 0x%x 0x%x 0x%x\n",
		ov16880_otp_data[0x0B], ov16880_otp_data[0x0C], ov16880_otp_data[0x0D], ov16880_otp_data[0x0E]);

       LOG_INF("OTP INFO CHECKSUM = 0x%x\n",                    ov16880_otp_data[0x0F]);

       
	inf_val = (ov16880_otp_data[0x10]<<8 | ov16880_otp_data[0x11]);
	mac_val = (ov16880_otp_data[0x12]<<8 | ov16880_otp_data[0x13]);
       ov16880_otp_data[0x10] = ((inf_val >> 1) & 0xFF00) >> 8;
       ov16880_otp_data[0x11] = (inf_val >> 1) & 0xFF;
       ov16880_otp_data[0x12] = ((mac_val >> 1) & 0xFF00) >> 8;
       ov16880_otp_data[0x13] = (mac_val >> 1) & 0xFF;

	LOG_INF("OTP AF Infinity Position (High byte) = 0x%x\n", ov16880_otp_data[0x10]);
	LOG_INF("OTP AF Infinity Position (Low byte) = 0x%x\n",  ov16880_otp_data[0x11]);
	LOG_INF("OTP AF Macro Position (High byte) = 0x%x\n",    ov16880_otp_data[0x12]);
	LOG_INF("OTP AF Macro Position (Low byte) = 0x%x\n",     ov16880_otp_data[0x13]);

       LOG_INF("OTP AF POS CHECKSUM = 0x%x\n",                    ov16880_otp_data[0x14]);

	
	sensor_init();
	mDELAY(10);
	sensor_read_alpha_parm();
	

	if(ret == true) {
		spin_lock(&imgsensor_drv_lock);
		read_ov16880_otp_flag = 1;

		spin_unlock(&imgsensor_drv_lock);

	}

	LOG_INF("==========exit ov16880 read_otp=======\n");
	return ret;
}


static BYTE alpha_value[4];

static void sensor_read_alpha_parm(void)
{
	memset(alpha_value, 0x0, 4);
	alpha_value[0] = read_cmos_sensor_8(0x798F);
	alpha_value[1] = read_cmos_sensor_8(0x7991);
	alpha_value[2] = read_cmos_sensor_8(0x7993);
	alpha_value[3] = read_cmos_sensor_8(0x7995);
	LOG_INF("%s : Sensor OTP alpha_value = 0x%x, 0x%x, 0x%x, 0x%x\n", __func__, alpha_value[0], alpha_value[1], alpha_value[2], alpha_value[3]);
}

static void sensor_write_alpha_parm(void)
{
	if(read_ov16880_otp_flag == 0) {
		LOG_INF("Sensor OTP data is not read !\n");
		return;
	}

	write_cmos_sensor_8(0x4023, alpha_value[0]);
	write_cmos_sensor_8(0x4033, alpha_value[0]);
	write_cmos_sensor_8(0x4021, (alpha_value[1] - 0x0A));
	write_cmos_sensor_8(0x4031, (alpha_value[1] - 0x0A));
	write_cmos_sensor_8(0x4027, (alpha_value[2] - 0x0E));
	write_cmos_sensor_8(0x4037, (alpha_value[2] - 0x0E));
	write_cmos_sensor_8(0x4025, alpha_value[3]);
	write_cmos_sensor_8(0x4035, alpha_value[3]);
	LOG_INF("%s : Sensor OTP alpha_value = 0x%x, 0x%x, 0x%x, 0x%x\n", __func__, alpha_value[0], alpha_value[1], alpha_value[2], alpha_value[3]);
}


BYTE pdaf_cal_data_dummy[2048];
kal_uint16 offset_dummy = 0x100;
bool read_ov16880_pdaf_data(void){
	return read_otp_pdaf_data(offset_dummy, pdaf_cal_data_dummy, sizeof(pdaf_cal_data_dummy));
}

static ssize_t sensor_otp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned short inf_val;
	unsigned short mac_val;

	LOG_INF("OTP AF Infinity Position (High byte) = 0x%x\n", ov16880_otp_data[0x10]);
	LOG_INF("OTP AF Infinity Position (Low byte) = 0x%x\n", ov16880_otp_data[0x11]);
	LOG_INF("OTP AF Macro Position (High byte) = 0x%x\n",    ov16880_otp_data[0x12]);
	LOG_INF("OTP AF Macro Position (Low byte) = 0x%x\n",    ov16880_otp_data[0x13]);
	inf_val = (ov16880_otp_data[0x10]<<8 | ov16880_otp_data[0x11]);
	mac_val = (ov16880_otp_data[0x12]<<8 | ov16880_otp_data[0x13]);
	sprintf(buf, "lenc:%u:%u\n",inf_val,mac_val);
	ret = strlen(buf) + 1;

	return ret;
}


static const char *ov16880Vendor = "OmniVision";
static const char *ov16880NAME = "ov16880_pd2";
static const char *ov16880Size = "16.0M";

static ssize_t sensor_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", ov16880Vendor, ov16880NAME, ov16880Size);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor_vendor_show, NULL);
static DEVICE_ATTR(otp, 0444, sensor_otp_show, NULL);

static struct kobject *android_ov16880;
static int first = true;

static int ov16880_sysfs_init(void)
{
	int ret ;
	if(first){
		LOG_INF("kobject creat and add\n");

		android_ov16880 = kobject_create_and_add("android_camera", NULL);
		
		if (android_ov16880 == NULL) {
			LOG_INF(" subsystem_register " \
			"failed\n");
			ret = -ENOMEM;
			return ret ;
		}
		LOG_INF("sysfs_create_file\n");
		ret = sysfs_create_file(android_ov16880, &dev_attr_sensor.attr);
		if (ret) {
			LOG_INF("sysfs_create_file " \
			"failed\n");
			kobject_del(android_ov16880);
		}else
			first = false;

		ret = sysfs_create_file(android_ov16880, &dev_attr_otp.attr);
		if (ret) {
			LOG_INF("sysfs_create_file " \
			"failed\n");
			kobject_del(android_ov16880);
		}
	}

	return 0 ;
}


extern int htc_ois_FWupdate(void);


static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {	
			*sensor_id = (read_cmos_sensor_8(0x300A) << 16) | (read_cmos_sensor_8(0x300B)<<8) | read_cmos_sensor_8(0x300C);
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
                            
                            ov16880_sysfs_init();
                            read_ov16880_otp();
                            read_ov16880_pdaf_data();
                            htc_ois_FWupdate();
                            
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id,*sensor_id);
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
			sensor_id = (read_cmos_sensor_8(0x300A) << 16) | (read_cmos_sensor_8(0x300B)<<8) | read_cmos_sensor_8(0x300C);
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id, sensor_id);
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id:0x%x ,sensor Id:0x%x\n", imgsensor.i2c_write_id,sensor_id);
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
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
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
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) 
    {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	else if(imgsensor.current_fps == 240)
    {
		if (imgsensor.current_fps != imgsensor_info.cap1.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	else 
    {
		if (imgsensor.current_fps != imgsensor_info.cap2.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n",imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);
    

	return ERROR_NONE;
}	
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

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
	LOG_INF("E");

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
	LOG_INF("E");

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
	video_call_setting(15);
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
	hyperlapse_setting();
	return ERROR_NONE;
}   


static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E");
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

	sensor_resolution->SensorCustom2Width  = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	return ERROR_NONE;
}	

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);


	
	
	

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
	LOG_INF("scenario_id = %d", scenario_id);

	Is_FirstAE = true; 
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
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
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
			if(framerate==300)
			{
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			}
			else
			{
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		
		
		write_cmos_sensor_8(0x5280, 0x80);
		write_cmos_sensor_8(0x5000, 0x00);
	} else {
		
		
		write_cmos_sensor_8(0x5280, 0x00);
		write_cmos_sensor_8(0x5000, 0x08);
	}
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
	SET_PD_BLOCK_INFO_T *PDAFinfo;
	SENSOR_VC_INFO_STRUCT *pvcinfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d", feature_id);
	switch (feature_id) 
	{
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
			Is_OIStest = false;
			Is_Capture = *(feature_data+1);
			if(Is_Capture || Is_FirstAE)
				LOG_INF("Is_Capture(%d) Is_FirstAE(%d), SENSOR_FEATURE_SET_ESHUTTER", Is_Capture, Is_FirstAE);
			
			write_cmos_sensor_8(0x3208, 0x00);   
			
			set_shutter(*feature_data);
			if(!Is_Capture && !Is_FirstAE){
				write_cmos_sensor_8(0x3208, 0x10);   
				write_cmos_sensor_8(0x3208, 0xA0);   
			}
			break;
		case SENSOR_FEATURE_SET_ESHUTTER_OIS_EXT:
			Is_OIStest = true;
			
			write_cmos_sensor_8(0x3208, 0x00);   
			
			set_shutter(*feature_data);
			write_cmos_sensor_8(0x3208, 0x10);   
			write_cmos_sensor_8(0x3208, 0xA0);   
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			break;
		case SENSOR_FEATURE_SET_GAIN:
			if(!Is_Capture && !Is_FirstAE)
				write_cmos_sensor_8(0x3208, 0x00);   
			set_gain((UINT16) *feature_data);
			
			write_cmos_sensor_8(0x3208, 0x10);   
			write_cmos_sensor_8(0x3208, 0xA0);   
			

			if(Is_FirstAE){
				LOG_INF("Is_FirstAE(%d) finish, SENSOR_FEATURE_SET_GAIN", Is_FirstAE);
				Is_FirstAE = false;
			}
			break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			if((sensor_reg_data->RegData>>8)>0)
			write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
		case SENSOR_FEATURE_GET_PDAF_DATA:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
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
			
			LOG_INF("Warning! Not Support IHDR Feature");
			spin_lock(&imgsensor_drv_lock);
			
			imgsensor.ihdr_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
			wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

			switch (*feature_data_32) 
			{
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
			LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n", (UINT32)*feature_data);
			PDAFinfo= (SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

			switch (*feature_data) 
			{
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(SET_PD_BLOCK_INFO_T));
				break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				default:
				memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(SET_PD_BLOCK_INFO_T));
				break;
			}
			break;
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n", *feature_data);
			switch (*feature_data) 
			{
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
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			}
			break;
		case SENSOR_FEATURE_SET_PDAF:
			LOG_INF("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode= *feature_data_16;
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

UINT32 OV16880_PD2_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	
