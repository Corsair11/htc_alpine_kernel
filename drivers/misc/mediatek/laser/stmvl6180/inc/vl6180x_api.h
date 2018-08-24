/*******************************************************************************
Copyright 2014, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED. 
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/



#ifndef VL6180x_API_H_
#define VL6180x_API_H_

#include "vl6180x_def.h"
#include "vl6180x_platform.h"

#ifdef __cplusplus
extern "C" {
#endif


 

#ifndef  VL6180x_SINGLE_DEVICE_DRIVER
#error "VL6180x_SINGLE_DEVICE_DRIVER not defined"
#endif


#ifndef  VL6180x_RANGE_STATUS_ERRSTRING
#warning "VL6180x_RANGE_STATUS_ERRSTRING not defined ?"
#define VL6180x_RANGE_STATUS_ERRSTRING  0
#endif

#ifndef  VL6180X_SAFE_POLLING_ENTER
#warning "VL6180X_SAFE_POLLING_ENTER not defined, likely old vl6180x_cfg.h file ?"
#define VL6180X_SAFE_POLLING_ENTER 0 
#endif

#ifndef VL6180X_LOG_ENABLE
#define VL6180X_LOG_ENABLE  0
#endif

#if VL6180x_RANGE_STATUS_ERRSTRING
#define  VL6180x_HAVE_RANGE_STATUS_ERRSTRING
#endif


#define VL6180x_ApiRevInt  ((VL6180x_API_REV_MAJOR<<24)+(VL6180x_API_REV_MINOR<<16)+VL6180x_API_REV_SUB)

#define VL6180x_ApiRevStr  VL6180X_STR(VL6180x_API_REV_MAJOR) "." VL6180X_STR(VL6180x_API_REV_MINOR) "." VL6180X_STR(VL6180x_API_REV_SUB)

int VL6180x_WaitDeviceBooted(VL6180xDev_t dev);

int VL6180x_InitData(VL6180xDev_t dev );

int VL6180x_SetupGPIO1(VL6180xDev_t dev, uint8_t IntFunction, int ActiveHigh);

 int VL6180x_Prepare(VL6180xDev_t dev);
 
 
 

 
int VL6180x_RangeStartContinuousMode(VL6180xDev_t dev);

int VL6180x_RangeStartSingleShot(VL6180xDev_t dev);

int VL6180x_RangeSetMaxConvergenceTime(VL6180xDev_t dev, uint8_t  MaxConTime_msec);
 
int VL6180x_RangePollMeasurement(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);

int VL6180x_RangeGetMeasurementIfReady(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);

int VL6180x_RangeGetMeasurement(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);

int VL6180x_RangeGetResult(VL6180xDev_t dev, int32_t *pRange_mm);

int VL6180x_RangeConfigInterrupt(VL6180xDev_t dev, uint8_t ConfigGpioInt);


#define VL6180x_RangeClearInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_RANGING)

int VL6180x_RangeGetInterruptStatus(VL6180xDev_t dev, uint8_t *pIntStatus);

#if VL6180x_RANGE_STATUS_ERRSTRING

extern const char * ROMABLE_DATA VL6180x_RangeStatusErrString[];
const char * VL6180x_RangeGetStatusErrString(uint8_t RangeErrCode);
#else
#define VL6180x_RangeGetStatusErrString(...) NULL
#endif


#if VL6180x_ALS_SUPPORT


int VL6180x_AlsPollMeasurement(VL6180xDev_t dev, VL6180x_AlsData_t *pAlsData);


int VL6180x_AlsGetMeasurement(VL6180xDev_t dev, VL6180x_AlsData_t *pAlsData);

int VL6180x_AlsConfigInterrupt(VL6180xDev_t dev, uint8_t ConfigGpioInt);


int VL6180x_AlsSetIntegrationPeriod(VL6180xDev_t dev, uint16_t period_ms);

int VL6180x_AlsSetInterMeasurementPeriod(VL6180xDev_t dev,  uint16_t intermeasurement_period_ms);

 
int VL6180x_AlsSetAnalogueGain(VL6180xDev_t dev, uint8_t gain);
int VL6180x_AlsSetThresholds(VL6180xDev_t dev, uint8_t low, uint8_t high);

 #define VL6180x_AlsClearInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_ALS)
 
int VL6180x_AlsGetInterruptStatus(VL6180xDev_t dev, uint8_t *pIntStatus);
 
#endif

 
int VL6180x_StaticInit(VL6180xDev_t dev);
 
 
 

int VL6180x_RangeWaitDeviceReady(VL6180xDev_t dev, int MaxLoop );

int VL6180x_RangeSetInterMeasPeriod(VL6180xDev_t dev, uint32_t  InterMeasTime_msec);


int VL6180x_UpscaleSetScaling(VL6180xDev_t dev, uint8_t scaling);

int VL6180x_UpscaleGetScaling(VL6180xDev_t dev);


#define VL6180x_RangeIsFilteredMeasurement(pRangeData) ((pRangeData)->errorStatus == RangingFiltered)

uint16_t VL6180x_GetUpperLimit(VL6180xDev_t dev);

int VL6180x_RangeSetThresholds(VL6180xDev_t dev, uint16_t low, uint16_t high, int SafeHold);

int VL6180x_RangeGetThresholds(VL6180xDev_t dev, uint16_t *low, uint16_t *high);

int VL6180x_RangeSetRawThresholds(VL6180xDev_t dev, uint8_t low, uint8_t high);

int VL6180x_RangeSetEceFactor(VL6180xDev_t dev, uint16_t  FactorM, uint16_t FactorD);

int VL6180x_RangeSetEceState(VL6180xDev_t dev, int enable );

int VL6180x_FilterSetState(VL6180xDev_t dev, int state);

int VL6180x_FilterGetState(VL6180xDev_t dev);


int VL6180x_DMaxSetState(VL6180xDev_t dev, int state);

int VL6180x_DMaxGetState(VL6180xDev_t dev);


int VL6180x_RangeSetSystemMode(VL6180xDev_t dev, uint8_t mode);
 
 

int8_t VL6180x_GetOffsetCalibrationData(VL6180xDev_t dev);

int VL6180x_SetOffsetCalibrationData(VL6180xDev_t dev, int8_t offset);

int  VL6180x_SetXTalkCompensationRate(VL6180xDev_t dev, FixPoint97_t Rate);




#if VL6180x_ALS_SUPPORT

int VL6180x_AlsWaitDeviceReady(VL6180xDev_t dev, int MaxLoop );

int VL6180x_AlsSetSystemMode(VL6180xDev_t dev, uint8_t mode); 

 
#endif 


int VL6180x_SetGroupParamHold(VL6180xDev_t dev, int Hold);

int VL6180x_SetI2CAddress(VL6180xDev_t dev, uint8_t NewAddr);

int VL6180x_SetupGPIOx(VL6180xDev_t dev, int pin, uint8_t IntFunction, int ActiveHigh);


int VL6180x_SetGPIOxPolarity(VL6180xDev_t dev, int pin, int active_high);

int VL6180x_SetGPIOxFunctionality(VL6180xDev_t dev, int pin, uint8_t functionality);

int VL6180x_DisableGPIOxOut(VL6180xDev_t dev, int pin);

#define msec_2_i2cloop( time_ms, i2c_khz )  (((time_ms)*(i2c_khz)/49)+1)




typedef enum  {
    INTR_POL_LOW        =0, 
    INTR_POL_HIGH       =1, 
}IntrPol_e;


int VL6180x_GetInterruptStatus(VL6180xDev_t dev, uint8_t *status);

int VL6180x_ClearInterrupt(VL6180xDev_t dev, uint8_t IntClear );

 #define VL6180x_ClearErrorInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_ERROR)

#define VL6180x_ClearAllInterrupt(dev) VL6180x_ClearInterrupt(dev, INTERRUPT_CLEAR_ERROR|INTERRUPT_CLEAR_RANGING|INTERRUPT_CLEAR_ALS)




int VL6180x_WrByte(VL6180xDev_t dev, uint16_t index, uint8_t data);
int VL6180x_UpdateByte(VL6180xDev_t dev, uint16_t index, uint8_t AndData, uint8_t OrData);
int VL6180x_WrWord(VL6180xDev_t dev, uint16_t index, uint16_t data);
int VL6180x_WrDWord(VL6180xDev_t dev, uint16_t index, uint32_t data);

int VL6180x_RdByte(VL6180xDev_t dev, uint16_t index, uint8_t *data);

int VL6180x_RdWord(VL6180xDev_t dev, uint16_t index, uint16_t *data);

int VL6180x_RdDWord(VL6180xDev_t dev, uint16_t index, uint32_t *data);





#ifdef __cplusplus
}
#endif

#endif 
