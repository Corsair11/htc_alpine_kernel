/*******************************************************************************
Copyright ?2014, STMicroelectronics International N.V.
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




#ifndef _VL6180x_DEF
#define _VL6180x_DEF

#define VL6180x_API_REV_MAJOR   3
#define VL6180x_API_REV_MINOR   0
#define VL6180x_API_REV_SUB     1

#define VL6180X_STR_HELPER(x) #x
#define VL6180X_STR(x) VL6180X_STR_HELPER(x)

#include "vl6180x_cfg.h"
#include "vl6180x_types.h"


#ifndef VL6180x_UPSCALE_SUPPORT
#error "VL6180x_UPSCALE_SUPPORT not defined"
#endif

#ifndef VL6180x_ALS_SUPPORT
#error "VL6180x_ALS_SUPPORT not defined"
#endif

#ifndef VL6180x_HAVE_DMAX_RANGING
#error "VL6180x_HAVE_DMAX_RANGING not defined"
#define VL6180x_HAVE_DMAX_RANGING   0
#endif

#ifndef VL6180x_EXTENDED_RANGE
#define VL6180x_EXTENDED_RANGE   0
#endif

#ifndef  VL6180x_WRAP_AROUND_FILTER_SUPPORT
#error "VL6180x_WRAP_AROUND_FILTER_SUPPORT not defined ?"
#define VL6180x_WRAP_AROUND_FILTER_SUPPORT 0
#endif





#define VL6180x_MAX_I2C_XFER_SIZE   8 

#if VL6180x_UPSCALE_SUPPORT < 0
#define VL6180x_HAVE_UPSCALE_DATA 
#endif

#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
#define  VL6180x_HAVE_WRAP_AROUND_DATA
#endif

#if VL6180x_ALS_SUPPORT != 0
#define VL6180x_HAVE_ALS_DATA
#endif


#if VL6180x_WRAP_AROUND_FILTER_SUPPORT || VL6180x_HAVE_DMAX_RANGING
	#define	VL6180x_HAVE_RATE_DATA
#endif

enum VL6180x_ErrCode_t{
	API_NO_ERROR        = 0,
    CALIBRATION_WARNING = 1,  
    MIN_CLIPED          = 2,  
    NOT_GUARANTEED      = 3,  
    NOT_READY           = 4,  

    API_ERROR      = -1,    
    INVALID_PARAMS = -2,    
    NOT_SUPPORTED  = -3,    
    RANGE_ERROR    = -4,    
    TIME_OUT       = -5,    
};

typedef struct RangeFilterResult_tag {
    uint16_t range_mm;      
    uint16_t rawRange_mm;   
} RangeFilterResult_t;

typedef uint8_t  FilterType1_t;

#define FILTER_NBOF_SAMPLES             10
struct FilterData_t {
    uint32_t MeasurementIndex;                      
    uint16_t LastTrueRange[FILTER_NBOF_SAMPLES];    
    uint32_t LastReturnRates[FILTER_NBOF_SAMPLES];  
    uint16_t StdFilteredReads;                      
    FilterType1_t Default_ZeroVal;                  
    FilterType1_t Default_VAVGVal;                  
    FilterType1_t NoDelay_ZeroVal;                  
    FilterType1_t NoDelay_VAVGVal;                  
    FilterType1_t Previous_VAVGDiff;                
};

#if  VL6180x_HAVE_DMAX_RANGING
typedef int32_t DMaxFix_t;
struct DMaxData_t {
    uint32_t ambTuningWindowFactor_K; 

    DMaxFix_t retSignalAt400mm;  
    
    
    
    int32_t snrLimit_K;         
    uint16_t ClipSnrLimit;      
    
    
};
#endif

struct VL6180xDevData_t {

    uint32_t Part2PartAmbNVM;  
    uint32_t XTalkCompRate_KCps; 

    uint16_t EceFactorM;        
    uint16_t EceFactorD;        

#ifdef VL6180x_HAVE_ALS_DATA
    uint16_t IntegrationPeriod; 
    uint16_t AlsGainCode;       
    uint16_t AlsScaler;         
#endif

#ifdef VL6180x_HAVE_UPSCALE_DATA
    uint8_t UpscaleFactor;      
#endif

#ifdef  VL6180x_HAVE_WRAP_AROUND_DATA
    uint8_t WrapAroundFilterActive; 
    struct FilterData_t FilterData; 
#endif

#if  VL6180x_HAVE_DMAX_RANGING
    struct DMaxData_t DMaxData;
    uint8_t DMaxEnable;
#endif
    int8_t  Part2PartOffsetNVM;     
};

#ifdef VL6180x_SINGLE_DEVICE_DRIVER
extern  struct VL6180xDevData_t SingleVL6180xDevData;
#define VL6180xDevDataGet(dev, field) (SingleVL6180xDevData.field)
#define VL6180xDevDataSet(dev, field, data) (SingleVL6180xDevData.field)=(data)
#endif


typedef struct {
    int32_t range_mm;          
    int32_t signalRate_mcps;   
    uint32_t errorStatus;      


#ifdef VL6180x_HAVE_RATE_DATA
    uint32_t rtnAmbRate;    
    uint32_t rtnRate;       
    uint32_t rtnConvTime;   
    uint32_t refConvTime;   
#endif


#if  VL6180x_HAVE_DMAX_RANGING
    uint32_t DMax;              
#endif

#ifdef  VL6180x_HAVE_WRAP_AROUND_DATA
    RangeFilterResult_t FilteredData; 
#endif
}VL6180x_RangeData_t;


typedef uint16_t FixPoint97_t;

typedef uint32_t lux_t;

typedef struct VL6180x_AlsData_st{
    lux_t lux;                 
    uint32_t errorStatus;      
}VL6180x_AlsData_t;

typedef enum {
    NoError_=0,                
    VCSEL_Continuity_Test,     
    VCSEL_Watchdog_Test,       
    VCSEL_Watchdog,            
    PLL1_Lock,                 
    PLL2_Lock,                 
    Early_Convergence_Estimate,
    Max_Convergence,           
    No_Target_Ignore,          
    Not_used_9,                
    Not_used_10,               
    Max_Signal_To_Noise_Ratio, 
    Raw_Ranging_Algo_Underflow,
    Raw_Ranging_Algo_Overflow, 
    Ranging_Algo_Underflow,    
    Ranging_Algo_Overflow,     

    
    RangingFiltered =0x10,     

} RangeError_u;



 

#define IDENTIFICATION_MODEL_ID                 0x000
#define IDENTIFICATION_MODULE_REV_MAJOR         0x003
#define IDENTIFICATION_MODULE_REV_MINOR         0x004


#define SYSTEM_MODE_GPIO0                       0x010
#define SYSTEM_MODE_GPIO1                       0x011
    
    #define GPIOx_POLARITY_SELECT_MASK              0x20
    
    #define GPIOx_FUNCTIONALITY_SELECT_SHIFT          1
    
    #define GPIOx_FUNCTIONALITY_SELECT_MASK          (0xF<<GPIOx_FUNCTIONALITY_SELECT_SHIFT)
    
    #define GPIOx_SELECT_OFF                        0x00
    
    #define GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT      0x08
    
    #define GPIOx_MODE_SELECT_RANGING               0x00
    
    #define GPIOx_MODE_SELECT_ALS                   0x01


#define SYSTEM_INTERRUPT_CONFIG_GPIO           0x014
    
    #define CONFIG_GPIO_RANGE_SHIFT            0
    
    #define CONFIG_GPIO_RANGE_MASK             (0x7<<CONFIG_GPIO_RANGE_SHIFT)
    
    #define CONFIG_GPIO_ALS_SHIFT              3
    
    #define CONFIG_GPIO_ALS_MASK               (0x7<<CONFIG_GPIO_ALS_SHIFT)
    
    #define CONFIG_GPIO_INTERRUPT_DISABLED         0x00
    
    #define CONFIG_GPIO_INTERRUPT_LEVEL_LOW        0x01
    
    #define CONFIG_GPIO_INTERRUPT_LEVEL_HIGH       0x02
    
    #define CONFIG_GPIO_INTERRUPT_OUT_OF_WINDOW    0x03
    
    #define CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY 0x04

#define SYSTEM_INTERRUPT_CLEAR                0x015
    
    #define INTERRUPT_CLEAR_RANGING                0x01
    
    #define INTERRUPT_CLEAR_ALS                    0x02
    
    #define INTERRUPT_CLEAR_ERROR                  0x04

#define SYSTEM_FRESH_OUT_OF_RESET             0x016

#define SYSTEM_GROUPED_PARAMETER_HOLD         0x017


#define SYSRANGE_START                        0x018
    
    #define MODE_MASK          0x03
    
    #define MODE_START_STOP    0x01
    
    #define MODE_CONTINUOUS    0x02
    
    #define MODE_SINGLESHOT    0x00

#define SYSRANGE_THRESH_HIGH                  0x019

#define SYSRANGE_THRESH_LOW                   0x01A

#define SYSRANGE_INTERMEASUREMENT_PERIOD      0x01B

#define SYSRANGE_MAX_CONVERGENCE_TIME         0x01C
#define SYSRANGE_CROSSTALK_COMPENSATION_RATE  0x01E
#define SYSRANGE_CROSSTALK_VALID_HEIGHT       0x021
#define SYSRANGE_EARLY_CONVERGENCE_ESTIMATE   0x022
#define SYSRANGE_PART_TO_PART_RANGE_OFFSET    0x024
#define SYSRANGE_RANGE_IGNORE_VALID_HEIGHT    0x025
#define SYSRANGE_RANGE_IGNORE_THRESHOLD       0x026
#define SYSRANGE_EMITTER_BLOCK_THRESHOLD      0x028
#define SYSRANGE_MAX_AMBIENT_LEVEL_THRESH     0x02A
#define SYSRANGE_MAX_AMBIENT_LEVEL_MULT       0x02C
#define SYSRANGE_RANGE_CHECK_ENABLES          0x02D
    #define RANGE_CHECK_ECE_ENABLE_MASK      0x01
    #define RANGE_CHECK_RANGE_ENABLE_MASK    0x02
    #define RANGE_CHECK_SNR_ENABLKE          0x10

#define SYSRANGE_VHV_RECALIBRATE              0x02E
#define SYSRANGE_VHV_REPEAT_RATE              0x031

#define SYSALS_START                          0x038

#define SYSALS_THRESH_HIGH                    0x03A
#define SYSALS_THRESH_LOW                     0x03C
#define SYSALS_INTERMEASUREMENT_PERIOD        0x03E
#define SYSALS_ANALOGUE_GAIN                  0x03F
#define SYSALS_INTEGRATION_PERIOD             0x040

#define RESULT_RANGE_STATUS                   0x04D
    
    #define RANGE_DEVICE_READY_MASK       0x01
    
    #define RANGE_ERROR_CODE_MASK         0xF0 
    
    #define RANGE_ERROR_CODE_SHIFT        4

#define RESULT_ALS_STATUS                     0x4E
    
   #define ALS_DEVICE_READY_MASK       0x01

#define RESULT_ALS_VAL                        0x50

#define FW_ALS_RESULT_SCALER                  0x120


typedef union IntrStatus_u{
    uint8_t val;           
    struct  {
        unsigned Range     :3; 
        unsigned Als       :3; 
        unsigned Error     :2; 
     } status;                 
} IntrStatus_t;

#define RESULT_INTERRUPT_STATUS_GPIO          0x4F
    
    #define RES_INT_RANGE_SHIFT  0
    
    #define RES_INT_ALS_SHIFT    3
    
    #define RES_INT_ERROR_SHIFT  6
    
    #define RES_INT_RANGE_MASK (0x7<<RES_INT_RANGE_SHIFT)
    
    #define RES_INT_ALS_MASK   (0x7<<RES_INT_ALS_SHIFT)

    
    #define RES_INT_STAT_GPIO_LOW_LEVEL_THRESHOLD  0x01
    
    #define RES_INT_STAT_GPIO_HIGH_LEVEL_THRESHOLD 0x02
    
    #define RES_INT_STAT_GPIO_OUT_OF_WINDOW        0x03
    
    #define RES_INT_STAT_GPIO_NEW_SAMPLE_READY     0x04
    
    #define RES_INT_ERROR_MASK (0x3<<RES_INT_ERROR_SHIFT)
        
        #define RES_INT_ERROR_LASER_SAFETY  1
        
        #define RES_INT_ERROR_PLL           2

#define RESULT_RANGE_VAL                        0x062

#define RESULT_RANGE_RAW                        0x064

#define RESULT_RANGE_SIGNAL_RATE                0x066

#define RESULT_RANGE_RETURN_SIGNAL_COUNT        0x06C

#define RESULT_RANGE_REFERENCE_SIGNAL_COUNT     0x070

#define RESULT_RANGE_RETURN_AMB_COUNT           0x074

#define RESULT_RANGE_REFERENCE_AMB_COUNT        0x078

#define RESULT_RANGE_RETURN_CONV_TIME           0x07C

#define RESULT_RANGE_REFERENCE_CONV_TIME        0x080


#define RANGE_SCALER                            0x096

#define READOUT_AVERAGING_SAMPLE_PERIOD     0x10A

#define I2C_SLAVE_DEVICE_ADDRESS               0x212

#endif 
