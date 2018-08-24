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
#include <linux/slab.h>

#ifdef IMX351_PDAFOTP_DEBUG
#define PFX "IMX351_pdafotp"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iMultiReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId, u8 number);


#define USHORT             unsigned short
#define BYTE               unsigned char
#define Sleep(ms) mdelay(ms)

#define IMX351_EEPROM_READ_ID 0xA2
#define IMX351_EEPROM_WRITE_ID 0xA3
#define IMX351_I2C_SPEED        100
#define IMX351_MAX_OFFSET		0xFFFF

static bool get_done = false;
static int last_size = 0;
static int last_offset = 0;


static bool selective_read_eeprom(kal_uint16 addr, BYTE* data)
{
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    if(addr > IMX351_MAX_OFFSET)
        return false;

	kdSetI2CSpeed(IMX351_I2C_SPEED);

	if(iReadRegI2C(pu_send_cmd, 2, (u8*)data, 1, IMX351_EEPROM_READ_ID)<0)
		return false;
    return true;
}

static bool _read_imx351_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size){
	int i = 0;
	int offset = addr;

	for(i = 0; i < size; i++) {
		if(!selective_read_eeprom(offset, &data[i])){
			return false;
		}
		LOG_INF("read_eeprom 0x%0x %d\n",offset, data[i]);
		offset++;
	}
	get_done = true;
	last_size = size;
	last_offset = addr;
    return true;
}

bool read_imx351_eeprom(kal_uint16 addr, BYTE* data, kal_uint32 size)
{
	int i;
	unsigned long checksum_count;

	LOG_INF("read_imx351_eeprom, size = %d\n", size);

	if(!get_done || last_size != size || last_offset != addr) {
		if(!_read_imx351_eeprom(addr, data, size)){
			get_done = 0;
            last_size = 0;
            last_offset = 0;
			return false;
		}
	}

	
	checksum_count = 0;
	for(i = 0; i < (size-2); i++)
		checksum_count += data[i];

	checksum_count = checksum_count & 0xFFFF;
	LOG_INF("read_imx351_eeprom, checksum_count=0x%x\n", (unsigned int)checksum_count);

	if ( (((checksum_count>>8)&0xFF) == data[size-1]) &&
		((checksum_count&0xFF) == data[size-2])
		) {
		LOG_INF("read_imx351_eeprom checksum correct\n");
	} else {
		LOG_INF("read_imx351_eeprom checksum error\n");
		return false;
	}

    return true;
}

