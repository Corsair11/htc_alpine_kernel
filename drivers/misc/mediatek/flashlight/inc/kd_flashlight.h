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

#ifndef _KD_FLASHLIGHT_H
#define _KD_FLASHLIGHT_H

#include <linux/ioctl.h>

#ifndef _MEDIA_TYPES_H
typedef unsigned char MUINT8;
typedef unsigned short MUINT16;
typedef unsigned int MUINT32;
typedef signed char MINT8;
typedef signed short MINT16;
typedef signed int MINT32;
#endif
typedef unsigned char u8;
typedef unsigned short u16;


enum {
	e_Max_Strobe_Num_Per_Dev = 2,
	e_Max_Part_Num_Per_Dev = 2,
	e_Max_Sensor_Dev_Num = 3,
};

#define KD_STROBE_HIGH_CURRENT_WIDTH 0xFF

#define FLASH_LIGHT_WDT_TIMEOUT_MS 300



#define KD_DEFAULT_FLASHLIGHT_INDEX    0
#define KD_CUSTOM_FLASHLIGHT_INDEX    1


#define KD_DEFAULT_FLASHLIGHT_ID    0
#define KD_DUMMY_FLASHLIGHT_ID      1
#define KD_PEAK_FLASHLIGHT_ID       2
#define KD_TORCH_FLASHLIGHT_ID      3
#define KD_CONSTANT_FLASHLIGHT_ID   4


typedef enum {
	e_CAMERA_NONE_SENSOR = 0,
	e_CAMERA_MAIN_SENSOR = 1,
	e_CAMERA_SUB_SENSOR = 2,
	e_CAMERA_MAIN_2_SENSOR = 4,
	
	e_CAMERA_MAIN_SECOND_SENSOR = 4,
	
	e_CAMERA_SENSOR_MAX
} eFlashSensorId;

typedef enum {
	e_FLASH_DRIVER_6332 = 0,

} eDrvierNameId;

typedef struct {
	int (*flashlight_open)(void *pArg);
	int (*flashlight_release)(void *pArg);
	int (*flashlight_ioctl)(unsigned int cmd, unsigned long arg);
} FLASHLIGHT_FUNCTION_STRUCT, *PFLASHLIGHT_FUNCTION_STRUCT;



typedef struct {
	int sensorDev;
	int strobeId;
	int arg;

} kdStrobeDrvArg;

typedef struct {
	MUINT32(*flashlightInit) (PFLASHLIGHT_FUNCTION_STRUCT * pfFunc);
} KD_FLASHLIGHT_INIT_FUNCTION_STRUCT, *pKD_FLASHLIGHT_INIT_FUNCTION_STRUCT;

typedef enum {
	FLASHLIGHTDRV_STATE_PREVIEW,
	FLASHLIGHTDRV_STATE_STILL,
} eFlashlightState;

typedef enum {
	FLASHLIGHT_NONE = 0,
	FLASHLIGHT_LED_ONOFF,	
	FLASHLIGHT_LED_CONSTANT,	
	FLASHLIGHT_LED_PEAK,	
	FLASHLIGHT_LED_TORCH,	
	FLASHLIGHT_XENON_SCR,	
	FLASHLIGHT_XENON_IGBT	
} FLASHLIGHT_TYPE_ENUM;

typedef enum {
	FLASHLIGHT_PIN_HWEN,	
	FLASHLIGHT_PIN_TORCH,	
	FLASHLIGHT_PIN_FLASH,	
	MAIN_FLASHLIGHT_ENM_PIN, 
	MAIN_FLASHLIGHT_ENF_PIN, 
} FLASHLIGHT_GPIO_PIN_ENUM;

typedef enum {
	STATE_LOW,
	STATE_HIGH
} FLASHLIGHT_GPIO_STATE_ENUM;

#define FLASHLIGHT_MAGIC 'S'

#define FLASHLIGHTIOC_T_ENABLE _IOW(FLASHLIGHT_MAGIC, 5, int)

#define FLASHLIGHTIOC_T_LEVEL _IOW(FLASHLIGHT_MAGIC, 10, int)

#define FLASHLIGHTIOC_T_FLASHTIME _IOW(FLASHLIGHT_MAGIC, 15, int)

#define FLASHLIGHTIOC_T_STATE _IOW(FLASHLIGHT_MAGIC, 20, int)

#define FLASHLIGHTIOC_G_FLASHTYPE _IOR(FLASHLIGHT_MAGIC, 25, int)

#define FLASHLIGHTIOC_X_SET_DRIVER _IOWR(FLASHLIGHT_MAGIC, 30, int)

#define FLASHLIGHTIOC_T_DELAY _IOW(FLASHLIGHT_MAGIC, 35, int)


#define FLASH_IOC_SET_TIME_OUT_TIME_MS  _IOR(FLASHLIGHT_MAGIC, 100, int)
#define FLASH_IOC_SET_STEP		        _IOR(FLASHLIGHT_MAGIC, 105, int)
#define FLASH_IOC_SET_DUTY				_IOR(FLASHLIGHT_MAGIC, 110, int)
#define FLASH_IOC_SET_ONOFF		_IOR(FLASHLIGHT_MAGIC, 115, int)
#define FLASH_IOC_UNINIT		_IOR(FLASHLIGHT_MAGIC, 120, int)

#define FLASH_IOC_PRE_ON		_IOR(FLASHLIGHT_MAGIC, 125, int)
#define FLASH_IOC_GET_PRE_ON_TIME_MS		_IOR(FLASHLIGHT_MAGIC, 130, int)
#define FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY		_IOR(FLASHLIGHT_MAGIC, 131, int)

#define FLASH_IOC_SET_REG_ADR  _IOR(FLASHLIGHT_MAGIC, 135, int)
#define FLASH_IOC_SET_REG_VAL  _IOR(FLASHLIGHT_MAGIC, 140, int)
#define FLASH_IOC_SET_REG  _IOR(FLASHLIGHT_MAGIC, 145, int)
#define FLASH_IOC_GET_REG  _IOR(FLASHLIGHT_MAGIC, 150, int)


#define FLASH_IOC_GET_MAIN_PART_ID		_IOR(FLASHLIGHT_MAGIC, 155, int)
#define FLASH_IOC_GET_SUB_PART_ID		_IOR(FLASHLIGHT_MAGIC, 160, int)
#define FLASH_IOC_GET_MAIN2_PART_ID		_IOR(FLASHLIGHT_MAGIC, 165, int)
#define FLASH_IOC_GET_PART_ID		_IOR(FLASHLIGHT_MAGIC, 166, int)


#define FLASH_IOC_HAS_LOW_POWER_DETECT _IOR(FLASHLIGHT_MAGIC, 170, int)
#define FLASH_IOC_LOW_POWER_DETECT_START _IOR(FLASHLIGHT_MAGIC, 175, int)
#define FLASH_IOC_LOW_POWER_DETECT_END _IOR(FLASHLIGHT_MAGIC, 180, int)
#define FLASH_IOC_IS_LOW_POWER _IOR(FLASHLIGHT_MAGIC, 182, int)


#define FLASH_IOC_GET_ERR _IOR(FLASHLIGHT_MAGIC, 185, int)
#define FLASH_IOC_GET_PROTOCOL_VERSION _IOR(FLASHLIGHT_MAGIC, 190, int)	

#define FLASH_IOC_IS_CHARGER_IN _IOR(FLASHLIGHT_MAGIC, 195, int)
#define FLASH_IOC_IS_OTG_USE _IOR(FLASHLIGHT_MAGIC, 200, int)
#define FLASH_IOC_GET_FLASH_DRIVER_NAME_ID _IOR(FLASHLIGHT_MAGIC, 205, int)

#define FLASH_IOC_SET_DISPLAY_BACKLIGHT   _IOR(FLASHLIGHT_MAGIC, 210, int)
#define FLASH_IOC_RESUME_DISPLAY_BACKLIGHT   _IOR(FLASHLIGHT_MAGIC, 211, int)


typedef struct {
	int sensorDev;
	int arg;

} StrobeDrvArg;


int checkAndRelease(void);
int strobe_getPartId(int sensorDev, int strobeId);
MUINT32 strobeInit_dummy(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 constantFlashlightInit(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_main_sid1_part2(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_main_sid2_part1(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_main_sid2_part2(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 subStrobeInit(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_sub_sid1_part2(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_sub_sid2_part1(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_sub_sid2_part2(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);
MUINT32 strobeInit_main_sid1_part1(FLASHLIGHT_FUNCTION_STRUCT **pfFunc);

MUINT32 dummyFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 defaultFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 peakFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 torchFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 subStrobeInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 strobeInit_sub_sid1_part2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 strobeInit_main_sid1_part2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);

MUINT32 subStrobeInit_2ndPart_2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 mainStrobeInit_2ndPart_2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);

int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

bool mtk_is_host_mode(void);

int mt6332_OpenBoost4Flash(void);
int mt6332_CloseBoost4Flash(void);

int flashlight_gpio_init(struct platform_device *pdev);
int flashlight_gpio_set(int pin, int state);
int flashlight_gpio_hwen_high(void);
int flashlight_gpio_hwen_low(void);
#endif
