#ifndef _CUST_CHARGING_H_
#define _CUST_CHARGING_H_

#define HTC_AICL_START_VOL 4200
#define HTC_AICL_VBUS_DROP_RATIO 140
#define BAD_CABLE_DPM 4200
#define BAD_CABLE_R_THRESHOLD 700
#define BAD_CABLE_SLEEP_TIME 1000
#define BAD_CABLE_CURRENT_GAP_R1 186 
#define BAD_CABLE_CURRENT_GAP_R2 94 
#define BAD_CABLE_MAIN_BD_R 50
#define AC_IBAT_CHARGER_CURRENT CHARGE_CURRENT_1300_00_MA

#define USB_CHARGER_CURRENT_SUSPEND			0
#define USB_CHARGER_CURRENT_UNCONFIGURED	CHARGE_CURRENT_70_00_MA
#define USB_CHARGER_CURRENT_CONFIGURED		CHARGE_CURRENT_500_00_MA

#define USB_CHARGER_CURRENT					CHARGE_CURRENT_500_00_MA	
#define AC_CHARGER_CURRENT					CHARGE_CURRENT_2000_00_MA
#define AC_CHARGER_INPUT_CURRENT				CHARGE_CURRENT_2000_00_MA
#define NON_STD_AC_CHARGER_CURRENT			CHARGE_CURRENT_1000_00_MA
#define CHARGING_HOST_CHARGER_CURRENT       CHARGE_CURRENT_650_00_MA
#define APPLE_0_5A_CHARGER_CURRENT          CHARGE_CURRENT_500_00_MA
#define APPLE_1_0A_CHARGER_CURRENT          CHARGE_CURRENT_650_00_MA
#define APPLE_2_1A_CHARGER_CURRENT          CHARGE_CURRENT_800_00_MA

#define V_CHARGER_MAX 6500				
#define V_CHARGER_MIN 4400				
#define BAT_LOW_TEMP_PROTECT_ENABLE         
#define CAR_TUNE_VALUE	118 
#define LEVEL100_THRESHOLD 400

#define SHUTDOWN_SYSTEM_VOLTAGE 3400
#endif
