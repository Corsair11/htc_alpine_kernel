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

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#include "ov13855mipiraw_Sensor.h"
#define PFX "OV13855_camera_pdaf"

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

struct otp_pdaf_struct {
unsigned char pdaf_flag; 
unsigned char data1[496];
unsigned char data2[806];
unsigned char data3[102];
unsigned char pdaf_checksum;

};




extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
#define I2C_SPEED        400  

#define START_OFFSET     0x800

#define Delay(ms)  mdelay(ms)
#if 0
#define EEPROM_READ_ID  0xA3
#define EEPROM_WRITE_ID   0xA2
#else
#define EEPROM_READ_ID  0xA1
#define EEPROM_WRITE_ID   0xA0
#endif
#define MAX_OFFSET       0xd7b
#define DATA_SIZE 4096
static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;


static bool OV13855_selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    if(addr > MAX_OFFSET)
        return false;
	kdSetI2CSpeed(I2C_SPEED);

	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, EEPROM_WRITE_ID)<0)
		return false;
    return true;
}

bool OV13855_read_eeprom_module_info(BYTE* data)
{
	int i = 0;
	int offset = 0x00;
	unsigned long checksum_count;

	
	for(i = 0; i < 25; i++) {
		if(!OV13855_selective_read_eeprom(offset, &data[i])){
			LOG_INF("read_eeprom 0x%0x %d fail \n",offset, data[i]);
			return false;
		}
		LOG_INF("read_eeprom 0x%0x 0x%x\n",offset, data[i]);
		offset++;
	}

	
	checksum_count = 0;
	for(i = 0; i < 17; i++) {
		checksum_count += data[i];
	}
	checksum_count = checksum_count & 0xFFFF;
	LOG_INF("read_eeprom Module Info checksum_count=0x%x\n", (unsigned int)checksum_count);

	if ( (((checksum_count>>8)&0xFF) == data[17]) &&
		((checksum_count&0xFF) == data[18])
		) {
		LOG_INF("read_eeprom Module Info checksum correct\n");
	} else {
		LOG_INF("read_eeprom Module Info checksum error\n");
		return false;
	}

	
	checksum_count = 0;
	for(i = 19; i < 23; i++) {
		checksum_count += data[i];
	}
	checksum_count = checksum_count & 0xFFFF;
	LOG_INF("read_eeprom AF DAC checksum_count=0x%x\n", (unsigned int)checksum_count);

	if ( (((checksum_count>>8)&0xFF) == data[23]) &&
		((checksum_count&0xFF) == data[24])
		) {
		LOG_INF("read_eeprom AF DAC checksum correct\n");
	} else {
		LOG_INF("read_eeprom AF DAC checksum error\n");
		return false;
	}

    return true;
}

#define PDAF_CAL_DATA_REAL_SIZE 1372 
#define PDAF_CAL_DATA_MAX_SIZE 2048
static BYTE pdaf_cal_data_cache[PDAF_CAL_DATA_MAX_SIZE];

static bool OV13855_read_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size ){
	int i = 0;
	
	int offset = 0x800;
	unsigned long pdaf_checksum_count = 0;
	BYTE pdaf_checksum_data[2];

#if 0
	
	for(i = 0; i < 1372; i++) {
		if(!OV13855_selective_read_eeprom(offset, &data[i])){
			LOG_INF("read_eeprom 0x%0x %d fail \n",offset, data[i]);
			return false;
		}
		LOG_INF("read_eeprom 0x%0x 0x%x\n",offset, data[i]);
		offset++;
	}
#else
	offset = 0x0019;

	
	for(i = 0; i < PDAF_CAL_DATA_REAL_SIZE; i++) {
		if(!OV13855_selective_read_eeprom(offset, &data[i])){
			LOG_INF("read_eeprom 0x%0x %d fail \n",offset, data[i]);
			return false;
		}
		pdaf_checksum_count += data[i];
		offset++;
	}
	pdaf_checksum_count = pdaf_checksum_count & 0xFFFF;
	LOG_INF("read_eeprom pdaf_checksum_count=0x%x\n", (unsigned int)pdaf_checksum_count);

	
	for(i = 0; i < 2; i++) {
		if(!OV13855_selective_read_eeprom(offset, &pdaf_checksum_data[i])){
			LOG_INF("read_eeprom 0x%0x %d fail \n",offset, pdaf_checksum_data[i]);
			return false;
		}
		LOG_INF("read_eeprom 0x%0x 0x%x\n",offset, pdaf_checksum_data[i]);
		offset++;
	}

	if ( (((pdaf_checksum_count>>8)&0xFF) == pdaf_checksum_data[0]) &&
		((pdaf_checksum_count&0xFF) == pdaf_checksum_data[1])
		) {
		LOG_INF("read_eeprom PDAF checksum correct\n");
	} else {
		LOG_INF("read_eeprom PDAF checksum error\n");
		return false;
	}

	memset(pdaf_cal_data_cache, 0 , sizeof(pdaf_cal_data_cache));
	memcpy(pdaf_cal_data_cache, data, PDAF_CAL_DATA_REAL_SIZE);
#endif

	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}

bool read_otp_pdaf_data( kal_uint16 addr, BYTE* data, kal_uint32 size){
	LOG_INF("read_otp_pdaf_data enter");

	if(!get_done || last_size != size || last_offset != addr) {
		
		if(!OV13855_read_eeprom(addr, data, size)){
			get_done = 0;
            last_size = 0;
            last_offset = 0;
			LOG_INF("read_otp_pdaf_data fail");
			return false;
		}
	}
	else if (get_done) {
		if (size >= PDAF_CAL_DATA_REAL_SIZE) {
			memcpy(data, pdaf_cal_data_cache, PDAF_CAL_DATA_REAL_SIZE);
			LOG_INF("Use pdaf_cal_data_cache !");
		}
	}
	
	LOG_INF("read_otp_pdaf_data end");
    return true;
}

