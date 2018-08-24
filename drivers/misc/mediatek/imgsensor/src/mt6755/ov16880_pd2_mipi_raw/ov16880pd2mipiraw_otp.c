#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include <linux/proc_fs.h>


#include <linux/dma-mapping.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#define PFX "OV16880_pdafotp"
#define LOG_INF(fmt, args...)   pr_info(PFX "[%s] " fmt, __FUNCTION__, ##args)

#define USHORT             	unsigned short
#define BYTE                unsigned char
#define Sleep(ms)           mdelay(ms)

static bool get_done = false;
#define PDAF_CAL_DATA_REAL_SIZE 1372 
#define PDAF_CAL_DATA_MAX_SIZE 2048
static BYTE pdaf_cal_data_cache[PDAF_CAL_DATA_MAX_SIZE];

extern bool LC898123AF_readFlash_PDdata(BYTE* data);

bool read_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size)
{
   
   
    LOG_INF("read ov16880 pdaf data from flash, addr = %d; size = %d\n", addr, size);
    size = 496+876;
    LC898123AF_readFlash_PDdata(pdaf_cal_data_cache);
    memcpy(data, pdaf_cal_data_cache + 4, size); 

#if 0 
    for(i = 0; i < size; i = i + 4){
	LOG_INF("[OTP PD data] addr 0x%x = %x %x %x %x\n", pd_addr, data[i], data[i+1], data[i+2], data[i+3]);
	pd_addr++;
    }
#endif
    get_done = true;

  return true;
}

bool read_otp_pdaf_data( kal_uint16 addr, BYTE* data, kal_uint32 size){
	LOG_INF("read_otp_pdaf_data enter");

	if(!get_done) {
		if(!read_eeprom(addr, data, size)){
			get_done = 0;
			LOG_INF("read_otp_pdaf_data fail");
			return false;
		}
		LOG_INF("read_otp_pdaf_data from flash success");
	}
	else {
		LOG_INF("Read pdaf data from cache !");
		if (size >= PDAF_CAL_DATA_REAL_SIZE) {
			memcpy(data, pdaf_cal_data_cache + 4, PDAF_CAL_DATA_REAL_SIZE);
			LOG_INF("Got pdaf_cal_data_cache success!");
		}
	}
	LOG_INF("read_otp_pdaf_data end");
    return true;
}





