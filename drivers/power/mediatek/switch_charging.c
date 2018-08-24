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

#include <linux/types.h>
#include <linux/kernel.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mach/mt_charging.h>
#include <mt-plat/mt_boot.h>
#include "cust_charging.h"

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#if !defined(TA_AC_CHARGING_CURRENT)
#include <mach/mt_pe.h>
#endif
#endif

#ifdef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
#include <mt-plat/diso.h>
#include <mach/mt_diso.h>
#endif

#include <mt-plat/htc_battery.h>
extern bool g_flag_keep_charge_on;
extern bool g_flag_force_ac_chg;
extern bool g_flag_ats_limit_chg;
#ifdef CONFIG_HTC_BATT_PCN0010
extern bool g_is_limit_IUSB;
#endif
extern kal_bool g_cable_out_happened_in_aicl_checking;

#define POST_CHARGING_TIME (30*60)	
#define FULL_CHECK_TIMES 6

unsigned int g_bcct_flag = 0;
unsigned int g_bcct_value = 0;
#ifdef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
unsigned int g_bcct_input_flag = 0;
unsigned int g_bcct_input_value = 0;
#endif
unsigned int g_full_check_count = 0;
CHR_CURRENT_ENUM g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
CHR_CURRENT_ENUM g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
unsigned int g_usb_state = USB_UNCONFIGURED;
static bool usb_unlimited;
#if defined(CONFIG_MTK_HAFG_20)
#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
BATTERY_VOLTAGE_ENUM g_cv_voltage = BATTERY_VOLT_04_400000_V;
#else
BATTERY_VOLTAGE_ENUM g_cv_voltage = BATTERY_VOLT_04_200000_V;
#endif
unsigned int get_cv_voltage(void)
{
	return g_cv_voltage;
}
#endif

#ifdef HTC_ENABLE_AICL
kal_bool g_aicl_fail_at_2A = KAL_FALSE;
extern void msleep(unsigned int msecs);
#endif
#ifdef CONFIG_HTC_BATT_PCN0001
extern bool g_charging_full_to_close_charger;
#endif
extern int g_chg_limit_reason;

 
 
 
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
struct wake_lock TA_charger_suspend_lock;
kal_bool ta_check_chr_type = KAL_TRUE;
kal_bool ta_cable_out_occur = KAL_FALSE;
kal_bool is_ta_connect = KAL_FALSE;
kal_bool ta_vchr_tuning = KAL_TRUE;
#if defined(PUMPEX_PLUS_RECHG)
kal_bool pep_det_rechg = KAL_FALSE;
#endif
int ta_v_chr_org = 0;
#endif

 
 
 
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
int g_temp_status = TEMP_POS_10_TO_POS_45;
kal_bool temp_error_recovery_chr_flag = KAL_TRUE;
#endif




void BATTERY_SetUSBState(int usb_state_value)
{
#if defined(CONFIG_POWER_EXT)
	battery_log(BAT_LOG_CRTI, "[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
	if ((usb_state_value < USB_SUSPEND) || ((usb_state_value > USB_CONFIGURED))) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		battery_log(BAT_LOG_CRTI, "[BATTERY] BAT_SetUSBState Success! Set %d\r\n",
			    usb_state_value);
		g_usb_state = usb_state_value;
	}
#endif
}


unsigned int get_charging_setting_current(void)
{
	return g_temp_CC_value;
}

#if defined(CONFIG_MTK_DYNAMIC_BAT_CV_SUPPORT)
static unsigned int get_constant_voltage(void)
{
	unsigned int cv;
#ifdef CONFIG_MTK_BIF_SUPPORT
	unsigned int vbat_bif;
	unsigned int vbat_auxadc;
	unsigned int vbat, bif_ok;
	int i;
#endif
	
	cv = V_CC2TOPOFF_THRES;
#ifdef CONFIG_MTK_BIF_SUPPORT
	
	i = 0;
	do {
		battery_charging_control(CHARGING_CMD_GET_BIF_VBAT, &vbat_bif);
		vbat_auxadc = battery_meter_get_battery_voltage(KAL_TRUE);
		if (vbat_bif < vbat_auxadc && vbat_bif != 0) {
			vbat = vbat_bif;
			bif_ok = 1;
			battery_log(BAT_LOG_FULL, "[BIF]using vbat_bif=%d\n with dV=%dmV", vbat,
				    (vbat_bif - vbat_auxadc));
		} else {
			vbat = vbat_auxadc;
			if (i < 5)
				i++;
			else {
				battery_log(BAT_LOG_FULL,
					    "[BIF]using vbat_auxadc=%d, check vbat_bif=%d\n", vbat,
					    vbat_bif);
				bif_ok = 0;
				break;
			}
		}
	} while (vbat_bif > vbat_auxadc || vbat_bif == 0);


	
	if (bif_ok == 1) {
#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
		int vbat1 = 4250;
		int vbat2 = 4300;
		int cv1 = 4450;
#else
		int vbat1 = 4100;
		int vbat2 = 4150;
		int cv1 = 4350;
#endif
		if (vbat >= 3400 && vbat < vbat1)
			cv = 4608;
		else if (vbat >= vbat1 && vbat < vbat2)
			cv = cv1;
		else
			cv = V_CC2TOPOFF_THRES;

		battery_log(BAT_LOG_FULL, "[BIF]dynamic CV=%dmV\n", cv);
	}
#endif
	return cv;
}
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static void switch_charger_set_vindpm(unsigned int chr_v)
{
	
	unsigned int vindpm = 0;

	
	

	if (chr_v > 11000)
		vindpm = SWITCH_CHR_VINDPM_12V;
	else if (chr_v > 8000)
		vindpm = SWITCH_CHR_VINDPM_9V;
	else if (chr_v > 6000)
		vindpm = SWITCH_CHR_VINDPM_7V;
	else
		vindpm = SWITCH_CHR_VINDPM_5V;

	battery_charging_control(CHARGING_CMD_SET_VINDPM, &vindpm);
	battery_log(BAT_LOG_CRTI,
		    "[PE+] switch charger set VINDPM=%dmV with charger volatge=%dmV\n",
		    vindpm * 100 + 2600, chr_v);
}
#endif
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static DEFINE_MUTEX(ta_mutex);

static void set_ta_charging_current(void)
{
	int real_v_chrA = 0;

	real_v_chrA = battery_meter_get_charger_voltage();
#if defined(TA_AC_12V_INPUT_CURRENT)
	if ((real_v_chrA - ta_v_chr_org) > 6000) {
		g_temp_input_CC_value = TA_AC_12V_INPUT_CURRENT;	
		g_temp_CC_value = batt_cust_data.ta_ac_charging_current;
	} else
#endif
	if ((real_v_chrA - ta_v_chr_org) > 3000) {
		g_temp_input_CC_value = batt_cust_data.ta_ac_9v_input_current;	
		g_temp_CC_value = batt_cust_data.ta_ac_charging_current;
	} else if ((real_v_chrA - ta_v_chr_org) > 1000) {
		g_temp_input_CC_value = batt_cust_data.ta_ac_7v_input_current;	
		g_temp_CC_value = batt_cust_data.ta_ac_charging_current;
	}
	battery_log(BAT_LOG_CRTI, "[PE+]set Ichg=%dmA with Iinlim=%dmA, chrA=%d, chrB=%d\n",
		    g_temp_CC_value / 100, g_temp_input_CC_value / 100, ta_v_chr_org, real_v_chrA);
}

static void mtk_ta_reset_vchr(void)
{
	CHR_CURRENT_ENUM chr_current = CHARGE_CURRENT_70_00_MA;

	battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &chr_current);
	msleep(250);		

	battery_log(BAT_LOG_CRTI, "[PE+]mtk_ta_reset_vchr(): reset Vchr to 5V\n");
}

static void mtk_ta_increase(void)
{
	kal_bool ta_current_pattern = KAL_TRUE;	

	if (ta_cable_out_occur == KAL_FALSE) {
		battery_charging_control(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_current_pattern);
	} else {
		ta_check_chr_type = KAL_TRUE;
		battery_log(BAT_LOG_CRTI, "[PE+]mtk_ta_increase() Cable out\n");
	}
}

static kal_bool mtk_ta_retry_increase(void)
{
	int real_v_chrA;
	int real_v_chrB;
	kal_bool retransmit = KAL_TRUE;
	unsigned int retransmit_count = 0;

	do {
		real_v_chrA = battery_meter_get_charger_voltage();
		mtk_ta_increase();	
		real_v_chrB = battery_meter_get_charger_voltage();

		if (real_v_chrB - real_v_chrA >= 1000) 
			retransmit = KAL_FALSE;
		else {
			retransmit_count++;
			battery_log(BAT_LOG_CRTI,
				    "[PE+]Communicated with adapter:retransmit_count =%d, chrA=%d, chrB=%d\n",
				    retransmit_count, real_v_chrA, real_v_chrB);
		}

		if ((retransmit_count == 3) || (BMT_status.charger_exist == KAL_FALSE))
			retransmit = KAL_FALSE;


	} while ((retransmit == KAL_TRUE) && (ta_cable_out_occur == KAL_FALSE));

	battery_log(BAT_LOG_CRTI,
		    "[PE+]Finished communicating with adapter real_v_chrA=%d, real_v_chrB=%d, retry=%d\n",
		    real_v_chrA, real_v_chrB, retransmit_count);

	if (retransmit_count == 3)
		return KAL_FALSE;

		return KAL_TRUE;
}

static void mtk_ta_detector(void)
{
	int real_v_chrB = 0;
#if defined(CONFIG_MTK_BQ25896_SUPPORT)
	
	unsigned int vindpm;
	unsigned int hw_ovp_en;

	
	hw_ovp_en = 0;
	battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN, &hw_ovp_en);
	batt_cust_data.v_charger_max = 15000;
#endif
	battery_log(BAT_LOG_CRTI, "[PE+]starting PE+ adapter detection\n");

	ta_v_chr_org = battery_meter_get_charger_voltage();
	mtk_ta_retry_increase();
	real_v_chrB = battery_meter_get_charger_voltage();

	if (real_v_chrB - ta_v_chr_org >= 1000)
		is_ta_connect = KAL_TRUE;
	else {
		is_ta_connect = KAL_FALSE;
#if defined(CONFIG_MTK_BQ25896_SUPPORT)
		hw_ovp_en = 1;
		battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN, &hw_ovp_en);
		batt_cust_data.v_charger_max = V_CHARGER_MAX;

		
		vindpm = SWITCH_CHR_VINDPM_5V;
		battery_charging_control(CHARGING_CMD_SET_VINDPM, &vindpm);
#endif
	}

	battery_log(BAT_LOG_CRTI, "[PE+]End of PE+ adapter detection, is_ta_connect=%d\n",
		    is_ta_connect);
}

static void mtk_ta_init(void)
{
	is_ta_connect = KAL_FALSE;
	ta_cable_out_occur = KAL_FALSE;

	if (batt_cust_data.ta_9v_support || batt_cust_data.ta_12v_support)
		ta_vchr_tuning = KAL_FALSE;

	battery_charging_control(CHARGING_CMD_INIT, NULL);
}

static void battery_pump_express_charger_check(void)
{
	if (KAL_TRUE == ta_check_chr_type &&
	    STANDARD_CHARGER == BMT_status.charger_type &&
	    BMT_status.SOC >= batt_cust_data.ta_start_battery_soc &&
	    BMT_status.SOC < batt_cust_data.ta_stop_battery_soc) {
		battery_log(BAT_LOG_CRTI, "[PE+]Starting PE Adaptor detection\n");

		mutex_lock(&ta_mutex);
		wake_lock(&TA_charger_suspend_lock);

		mtk_ta_reset_vchr();

		mtk_ta_init();
		mtk_ta_detector();

		
		if (KAL_TRUE == ta_cable_out_occur)
			ta_check_chr_type = KAL_TRUE;
		else
			ta_check_chr_type = KAL_FALSE;
#if defined(PUMPEX_PLUS_RECHG)
		
		pep_det_rechg = KAL_FALSE;
#endif
		wake_unlock(&TA_charger_suspend_lock);
		mutex_unlock(&ta_mutex);
	} else {
		battery_log(BAT_LOG_CRTI,
			    "[PE+]Stop battery_pump_express_charger_check, SOC=%d, ta_check_chr_type = %d, charger_type = %d\n",
			    BMT_status.SOC, ta_check_chr_type, BMT_status.charger_type);
	}
}

static void battery_pump_express_algorithm_start(void)
{
	signed int charger_vol;
	unsigned int charging_enable = KAL_FALSE;

#if defined(CONFIG_MTK_DYNAMIC_BAT_CV_SUPPORT)
	unsigned int cv;
	unsigned int vbat;
#endif
#if defined(TA_12V_SUPPORT)
	kal_bool pumped_volt;
	unsigned int chr_ovp_en;

	battery_log(BAT_LOG_FULL, "[PE+][battery_pump_express_algorithm_start]start PEP...");
#endif

	mutex_lock(&ta_mutex);
	wake_lock(&TA_charger_suspend_lock);

	if (KAL_TRUE == is_ta_connect) {
		
		charger_vol = battery_meter_get_charger_voltage();
		batt_cust_data.v_charger_max = 15000;
#if defined(CONFIG_MTK_DYNAMIC_BAT_CV_SUPPORT)
		cv = get_constant_voltage();
		vbat = battery_meter_get_battery_voltage(KAL_TRUE);
#endif
		if (KAL_FALSE == ta_vchr_tuning) {
#ifndef TA_12V_SUPPORT
			mtk_ta_retry_increase();
			charger_vol = battery_meter_get_charger_voltage();

#else
			

			
			pumped_volt = mtk_ta_retry_increase();

			if (pumped_volt == KAL_FALSE) {
				battery_log(BAT_LOG_CRTI,
					    "[PE+]adaptor failed to output 9V, Please check adaptor");
				
				
				
				
			}
#endif
#if defined(TA_12V_SUPPORT)
			else {
				
				chr_ovp_en = 0;
				battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN, &chr_ovp_en);
				batt_cust_data.v_charger_max = 15000;

				
				
				

				
				pumped_volt = mtk_ta_retry_increase();	
				if (pumped_volt == KAL_FALSE) {
					
					chr_ovp_en = 0;
					battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN,
								 &chr_ovp_en);

					
					
					

					battery_log(BAT_LOG_CRTI,
						    "[PE+]adaptor failed to output 12V, Please check adaptor.");
				} else
					battery_log(BAT_LOG_FULL,
						    "[PE+]adaptor successed to output 12V.");
				charger_vol = battery_meter_get_charger_voltage();
			}
#endif

			ta_vchr_tuning = KAL_TRUE;
		} else if (BMT_status.SOC > batt_cust_data.ta_stop_battery_soc) {
			
			battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
			mtk_ta_reset_vchr();	
			charger_vol = battery_meter_get_charger_voltage();
			if (abs(charger_vol - ta_v_chr_org) <= 1000)	
				is_ta_connect = KAL_FALSE;

			battery_log(BAT_LOG_CRTI,
				    "[PE+]Stop battery_pump_express_algorithm, SOC=%d is_ta_connect =%d, TA_STOP_BATTERY_SOC: %d\n",
				    BMT_status.SOC, is_ta_connect, batt_cust_data.ta_stop_battery_soc);
		}
#if defined(CONFIG_MTK_DYNAMIC_BAT_CV_SUPPORT)
		else if (vbat >= cv) {
			
			battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
			mtk_ta_reset_vchr();	
			charger_vol = battery_meter_get_charger_voltage();
			if (abs(charger_vol - ta_v_chr_org) <= 1000)	
				is_ta_connect = KAL_FALSE;

			
			chr_ovp_en = 1;
			battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN, &chr_ovp_en);

			
			batt_cust_data.v_charger_max = V_CHARGER_MAX;
			battery_log(BAT_LOG_CRTI,
				    "[PE+]CV=%d reached. Stopping PE+, is_ta_connect =%d\n", cv,
				    is_ta_connect);
		} else if (0) {
			
			if (abs(charger_vol - ta_v_chr_org) < 1000 && BMT_status.bat_charging_state == CHR_CC) {
				ta_check_chr_type = KAL_TRUE;
				battery_log(BAT_LOG_CRTI, "[PE+] abnormal TA chager voltage, rechecking PE+ adapter\n");
			}
		}
		
		switch_charger_set_vindpm(charger_vol);

#endif
		battery_log(BAT_LOG_CRTI,
			    "[PE+]check cable impedance, VA(%d) VB(%d) delta(%d).\n",
			    ta_v_chr_org, charger_vol, charger_vol - ta_v_chr_org);

		battery_log(BAT_LOG_CRTI, "[PE+]mtk_ta_algorithm() end\n");
	} else {
		battery_log(BAT_LOG_FULL, "[PE+]Not a TA charger, bypass TA algorithm\n");
#if defined(TA_12V_SUPPORT)
		batt_cust_data.v_charger_max = V_CHARGER_MAX;
		chr_ovp_en = 1;
		battery_charging_control(CHARGING_CMD_SET_VBUS_OVP_EN, &chr_ovp_en);
#endif
	}

	wake_unlock(&TA_charger_suspend_lock);
	mutex_unlock(&ta_mutex);
}
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

static BATTERY_VOLTAGE_ENUM select_jeita_cv(void)
{
	BATTERY_VOLTAGE_ENUM cv_voltage;

	if (g_temp_status == TEMP_ABOVE_POS_60) {
		cv_voltage = JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_45_TO_POS_60) {
		cv_voltage = JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_10_TO_POS_45) {
		if (batt_cust_data.high_battery_voltage_support)
			cv_voltage = BATTERY_VOLT_04_400000_V;
		else
			cv_voltage = JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE;

		#ifdef ALPINE_BATT 
		if (fgauge_get_batt_id() == 1) {
			cv_voltage = BATTERY_VOLT_04_350000_V;
		}
		#endif

	} else if (g_temp_status == TEMP_POS_0_TO_POS_10) {
		cv_voltage = JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		cv_voltage = JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_BELOW_NEG_10) {
		cv_voltage = JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE;
	} else {
		cv_voltage = BATTERY_VOLT_04_200000_V;
	}

	return cv_voltage;
}

PMU_STATUS do_jeita_state_machine(void)
{
	BATTERY_VOLTAGE_ENUM cv_voltage;
	PMU_STATUS jeita_status = PMU_STATUS_OK;

	

	if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery Over high Temperature(%d) !!\n\r",
			    TEMP_POS_60_THRESHOLD);

		g_temp_status = TEMP_ABOVE_POS_60;

		return PMU_STATUS_FAIL;
	} else if (BMT_status.temperature > TEMP_POS_45_THRESHOLD) {	
		if ((g_temp_status == TEMP_ABOVE_POS_60)
		    && (BMT_status.temperature >= TEMP_POS_60_THRES_MINUS_X_DEGREE)) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_POS_60_THRES_MINUS_X_DEGREE, TEMP_POS_60_THRESHOLD);

			jeita_status = PMU_STATUS_FAIL;
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_45_THRESHOLD, TEMP_POS_60_THRESHOLD);

			g_temp_status = TEMP_POS_45_TO_POS_60;
		}
	} else if (BMT_status.temperature >= TEMP_POS_10_THRESHOLD) {
		if (((g_temp_status == TEMP_POS_45_TO_POS_60)
		     && (BMT_status.temperature >= TEMP_POS_45_THRES_MINUS_X_DEGREE))
		    || ((g_temp_status == TEMP_POS_0_TO_POS_10)
			&& (BMT_status.temperature <= TEMP_POS_10_THRES_PLUS_X_DEGREE))) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");
		} else {
			battery_log(BAT_LOG_FULL,
				    "[BATTERY] Battery Normal Temperature between %d and %d !!\n\r",
				    TEMP_POS_10_THRESHOLD, TEMP_POS_45_THRESHOLD);
			g_temp_status = TEMP_POS_10_TO_POS_45;
		}
	} else if (BMT_status.temperature >= TEMP_POS_0_THRESHOLD) {
		if ((g_temp_status == TEMP_NEG_10_TO_POS_0 || g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_POS_0_THRES_PLUS_X_DEGREE)) {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
					    TEMP_POS_0_THRES_PLUS_X_DEGREE, TEMP_POS_10_THRESHOLD);
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
					    TEMP_POS_0_THRESHOLD, TEMP_POS_0_THRES_PLUS_X_DEGREE);
				return PMU_STATUS_FAIL;
			}
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_0_THRESHOLD, TEMP_POS_10_THRESHOLD);

			g_temp_status = TEMP_POS_0_TO_POS_10;
		}
	} else if (BMT_status.temperature >= TEMP_NEG_10_THRESHOLD) {
		if ((g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_NEG_10_THRES_PLUS_X_DEGREE)) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_NEG_10_THRESHOLD, TEMP_NEG_10_THRES_PLUS_X_DEGREE);

			jeita_status = PMU_STATUS_FAIL;
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_NEG_10_THRESHOLD, TEMP_POS_0_THRESHOLD);

			g_temp_status = TEMP_NEG_10_TO_POS_0;
		}
	} else {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery below low Temperature(%d) !!\n\r",
			    TEMP_NEG_10_THRESHOLD);
		g_temp_status = TEMP_BELOW_NEG_10;

		jeita_status = PMU_STATUS_FAIL;
	}

	

	cv_voltage = select_jeita_cv();
	battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);

#if defined(CONFIG_MTK_HAFG_20)
	g_cv_voltage = cv_voltage;
#endif

	return jeita_status;
}


static void set_jeita_charging_current(void)
{
#ifdef CONFIG_USB_IF
	if (BMT_status.charger_type == STANDARD_HOST)
		return;
#endif

	if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		g_temp_CC_value = CHARGE_CURRENT_350_00_MA;
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		battery_log(BAT_LOG_CRTI, "[BATTERY] JEITA set charging current : %d\r\n",
			    g_temp_CC_value);
	}
}

#endif

#ifdef HTC_ENABLE_AICL
unsigned int temp_input_current[] = {CHARGE_CURRENT_1050_00_MA, CHARGE_CURRENT_1500_00_MA, CHARGE_CURRENT_2100_00_MA};
unsigned int temp_current[] = {CHARGE_CURRENT_2450_00_MA, CHARGE_CURRENT_2450_00_MA, CHARGE_CURRENT_2450_00_MA}; 
unsigned int temp_vin_dpm[] = {4300, 4300, 4000};
unsigned int temp_avg_voltage[] = {0, 0, 0, 0, 0, 0};
extern int htc_charging_avg_voltage_check(void);
#endif

#ifdef HTC_ENABLE_AICL
extern bool mtkfb_is_suspend(void);
static CHR_CURRENT_ENUM current_after_BTM_configuration(void)
{
#if defined(ALPINE_BATT)
	static bool is_limit_ibat = false;
#endif
	g_temp_CC_value = BMT_status.htc_acil_state == AICL_DONE ? CHARGE_CURRENT_2575_00_MA :
				g_aicl_fail_at_2A ? temp_current[1] : temp_current[0];
	if(g_flag_keep_charge_on)
		return g_temp_CC_value;
#if defined(ALPINE_BATT)
	if(BMT_status.bat_vol < 4250) {
		if ((BMT_status.temperature <= 48) && (BMT_status.temperature > 10))
			g_temp_CC_value = CHARGE_CURRENT_2450_00_MA; 
		else
			g_temp_CC_value = CHARGE_CURRENT_1675_00_MA; 
	} else {
		if ((BMT_status.temperature <= 48) && (BMT_status.temperature > 10))
			g_temp_CC_value = CHARGE_CURRENT_1675_00_MA; 
		else
			g_temp_CC_value = CHARGE_CURRENT_1225_00_MA; 
	}

	if(!mtkfb_is_suspend()) {
		if (BMT_status.temperature >= 39) {
			g_temp_CC_value = CHARGE_CURRENT_1050_00_MA;
			is_limit_ibat = true;
		} else {
			if ((is_limit_ibat == true) && (BMT_status.temperature > 37)) {
				g_temp_CC_value = CHARGE_CURRENT_1050_00_MA;
				is_limit_ibat = true;
			} else {
				is_limit_ibat = false;
			}
		}
	} else {
		is_limit_ibat = false;
	}

	if (g_chg_limit_reason & HTC_BATT_CHG_LIMIT_BIT_TALK) 
		g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
#else
	g_temp_CC_value = g_temp_input_CC_value;
#endif
	
	if (g_flag_ats_limit_chg)
		g_temp_CC_value = CHARGE_CURRENT_1000_00_MA;

	return g_temp_CC_value;
}
#endif


bool get_usb_current_unlimited(void)
{
	if (BMT_status.charger_type == STANDARD_HOST || BMT_status.charger_type == CHARGING_HOST)
		return usb_unlimited;

		return false;
}

void set_usb_current_unlimited(bool enable)
{
	unsigned int en;

	usb_unlimited = enable;
	if (enable == true)
		en = 0;
	else
		en = 1;

#ifndef CONFIG_MTK_BQ24296_SUPPORT
	battery_charging_control(CHARGING_CMD_ENABLE_SAFETY_TIMER, &en);
#endif
}

void select_charging_current_bcct(void)
{
#ifndef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
	if ((BMT_status.charger_type == STANDARD_HOST) ||
	    (BMT_status.charger_type == NONSTANDARD_CHARGER)) {
		if (g_bcct_value < 100)
			g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (g_bcct_value < 500)
			g_temp_input_CC_value = CHARGE_CURRENT_100_00_MA;
		else if (g_bcct_value < 800)
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		else if (g_bcct_value == 800)
			g_temp_input_CC_value = CHARGE_CURRENT_800_00_MA;
		else
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	} else if ((BMT_status.charger_type == STANDARD_CHARGER) ||
		   (BMT_status.charger_type == CHARGING_HOST)) {
		g_temp_input_CC_value = CHARGE_CURRENT_MAX;

		
		
		if (g_bcct_value < 550)
			g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (g_bcct_value < 650)
			g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
		else if (g_bcct_value < 750)
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		else if (g_bcct_value < 850)
			g_temp_CC_value = CHARGE_CURRENT_750_00_MA;
		else if (g_bcct_value < 950)
			g_temp_CC_value = CHARGE_CURRENT_850_00_MA;
		else if (g_bcct_value < 1050)
			g_temp_CC_value = CHARGE_CURRENT_950_00_MA;
		else if (g_bcct_value < 1150)
			g_temp_CC_value = CHARGE_CURRENT_1050_00_MA;
		else if (g_bcct_value < 1250)
			g_temp_CC_value = CHARGE_CURRENT_1150_00_MA;
		else if (g_bcct_value == 1250)
			g_temp_CC_value = CHARGE_CURRENT_1250_00_MA;
		else
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		

	} else {
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	}
#else
	if (g_bcct_flag == 1)
		g_temp_CC_value = g_bcct_value * 100;
	if (g_bcct_input_flag == 1)
		g_temp_input_CC_value = g_bcct_input_value * 100;
#endif
}


unsigned int set_chr_input_current_limit(int current_limit)
{
#ifdef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
	if (current_limit != -1) {
		g_bcct_input_flag = 1;

		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == NONSTANDARD_CHARGER)) {
				g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
				g_bcct_input_value = CHARGE_CURRENT_500_00_MA / 100;
		} else if ((BMT_status.charger_type == STANDARD_CHARGER) ||
			(BMT_status.charger_type == CHARGING_HOST)) {

			if (current_limit < 650) {
				g_temp_input_CC_value = CHARGE_CURRENT_650_00_MA;
				g_bcct_input_value = CHARGE_CURRENT_650_00_MA / 100;
			} else {
				g_temp_input_CC_value = current_limit * 100;
				g_bcct_input_value = current_limit;
			}
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
			g_bcct_input_value = CHARGE_CURRENT_500_00_MA / 100;
		}

		battery_log(BAT_LOG_CRTI, "[BATTERY] set_chr_input_current_limit (%d)\r\n",
		current_limit);
	} else {
		
		g_bcct_input_flag = 0;
}

	
	
	
	return g_bcct_input_flag;
#else
	battery_log(BAT_LOG_CRTI, "[BATTERY] set_chr_input_current_limit _NOT_ supported\n");
	return 0;
#endif
}

static void pchr_turn_on_charging(void);
unsigned int set_bat_charging_current_limit(int current_limit)
{
	if (g_flag_keep_charge_on) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] set 6 4, do not limit current\r\n");
		g_bcct_flag = 0;
		return g_bcct_flag;
	}

	battery_log(BAT_LOG_CRTI, "[BATTERY] set_bat_charging_current_limit (%d)\r\n",
		    current_limit);

	if (current_limit != -1) {
		g_bcct_flag = 1;
		g_bcct_value = current_limit;
#ifdef CONFIG_MTK_THERMAL_TEST_SUPPORT
		g_temp_CC_value = current_limit * 100;
#else
		if (current_limit < 70)
			g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (current_limit < 200)
			g_temp_CC_value = CHARGE_CURRENT_70_00_MA;
		else if (current_limit < 300)
			g_temp_CC_value = CHARGE_CURRENT_200_00_MA;
		else if (current_limit < 400)
			g_temp_CC_value = CHARGE_CURRENT_300_00_MA;
		else if (current_limit < 450)
			g_temp_CC_value = CHARGE_CURRENT_400_00_MA;
		else if (current_limit < 550)
			g_temp_CC_value = CHARGE_CURRENT_450_00_MA;
		else if (current_limit < 650)
			g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
		else if (current_limit < 700)
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		else if (current_limit < 800)
			g_temp_CC_value = CHARGE_CURRENT_700_00_MA;
		else if (current_limit < 900)
			g_temp_CC_value = CHARGE_CURRENT_800_00_MA;
		else if (current_limit < 1000)
			g_temp_CC_value = CHARGE_CURRENT_900_00_MA;
		else if (current_limit < 1100)
			g_temp_CC_value = CHARGE_CURRENT_1000_00_MA;
		else if (current_limit < 1200)
			g_temp_CC_value = CHARGE_CURRENT_1100_00_MA;
		else if (current_limit < 1300)
			g_temp_CC_value = CHARGE_CURRENT_1200_00_MA;
		else if (current_limit < 1400)
			g_temp_CC_value = CHARGE_CURRENT_1300_00_MA;
		else if (current_limit < 1500)
			g_temp_CC_value = CHARGE_CURRENT_1400_00_MA;
		else if (current_limit < 1600)
			g_temp_CC_value = CHARGE_CURRENT_1500_00_MA;
		else if (current_limit == 1600)
			g_temp_CC_value = CHARGE_CURRENT_1600_00_MA;
		else
			g_temp_CC_value = CHARGE_CURRENT_450_00_MA;
#endif
	} else {
		
		g_bcct_flag = 0;
	}

	
	pchr_turn_on_charging();

	return g_bcct_flag;
}


void select_charging_current(void)
{
	if (g_ftm_battery_flag) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] FTM charging : %d\r\n",
			    charging_level_data[0]);
		g_temp_CC_value = charging_level_data[0];

		if (g_temp_CC_value == CHARGE_CURRENT_450_00_MA) {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_MAX;
			g_temp_CC_value = batt_cust_data.ac_charger_current;

			battery_log(BAT_LOG_CRTI, "[BATTERY] set_ac_current \r\n");
		}
	} else {
		if (BMT_status.charger_type == STANDARD_HOST) {
#ifdef CONFIG_USB_IF
			{
				g_temp_input_CC_value = CHARGE_CURRENT_MAX;
				if (g_usb_state == USB_SUSPEND)
					g_temp_CC_value = USB_CHARGER_CURRENT_SUSPEND;
				else if (g_usb_state == USB_UNCONFIGURED)
					g_temp_CC_value = batt_cust_data.usb_charger_current_unconfigured;
				else if (g_usb_state == USB_CONFIGURED)
					g_temp_CC_value = batt_cust_data.usb_charger_current_configured;
				else
					g_temp_CC_value = batt_cust_data.usb_charger_current_unconfigured;

				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n",
					    g_temp_CC_value, g_usb_state);
			}
#else
			{
				g_temp_input_CC_value = batt_cust_data.usb_charger_current;
				g_temp_CC_value = batt_cust_data.usb_charger_current;
			}
#endif
#ifdef CONFIG_HTC_BATT_PCN0006
		if (g_flag_force_ac_chg) {
			g_temp_input_CC_value = batt_cust_data.ac_charger_current;
			g_temp_CC_value = batt_cust_data.ac_charger_current;
		}
#endif
		} else if (BMT_status.charger_type == NONSTANDARD_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.non_std_ac_charger_current;
			g_temp_CC_value = batt_cust_data.non_std_ac_charger_current;

#ifdef CONFIG_HTC_BATT_PCN0006
			if (g_flag_force_ac_chg) {
				g_temp_input_CC_value = batt_cust_data.ac_charger_current;
				g_temp_CC_value = batt_cust_data.ac_charger_current;
			}
#endif
		} else if (BMT_status.charger_type == STANDARD_CHARGER) {

		#ifdef HTC_ENABLE_AICL
		g_temp_input_CC_value = BMT_status.htc_acil_state == AICL_DONE ? temp_input_current[2] :
			g_aicl_fail_at_2A ? temp_input_current[1] : temp_input_current[0];

		g_temp_CC_value = current_after_BTM_configuration();
		#else
		g_temp_input_CC_value = batt_cust_data.ac_charger_input_current;
		g_temp_CC_value = batt_cust_data.ac_charger_current;
		#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
			if (is_ta_connect == KAL_TRUE)
				set_ta_charging_current();
#endif
		} else if (BMT_status.charger_type == CHARGING_HOST) {
			g_temp_input_CC_value = batt_cust_data.charging_host_charger_current;
			g_temp_CC_value = batt_cust_data.charging_host_charger_current;
		} else if (BMT_status.charger_type == APPLE_2_1A_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.apple_2_1a_charger_current;
			g_temp_CC_value = batt_cust_data.apple_2_1a_charger_current;
		} else if (BMT_status.charger_type == APPLE_1_0A_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.apple_1_0a_charger_current;
			g_temp_CC_value = batt_cust_data.apple_1_0a_charger_current;
		} else if (BMT_status.charger_type == APPLE_0_5A_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.apple_0_5a_charger_current;
			g_temp_CC_value = batt_cust_data.apple_0_5a_charger_current;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
		}

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE) {
			g_temp_input_CC_value = batt_cust_data.ac_charger_current;
			g_temp_CC_value = batt_cust_data.ac_charger_current;
		}
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		set_jeita_charging_current();
#endif
	}
}

static unsigned int charging_full_check(void)
{
	unsigned int status;

	battery_charging_control(CHARGING_CMD_GET_CHARGING_STATUS, &status);
	if (status == KAL_TRUE) {
		g_full_check_count++;
		if (g_full_check_count >= FULL_CHECK_TIMES)
			return KAL_TRUE;
		else
			return KAL_FALSE;
	} 
		g_full_check_count = 0;
		return status;
	
}

static void pchr_turn_on_charging(void)
{
#ifdef HTC_ENABLE_AICL
	int Vin_dpm;
	int DPM_status;
	#ifdef CONFIG_MTK_BQ25896_SUPPORT
	int iDPM_state;
	#endif
	int i = 0, j = 0, k = 0;
	int charger_vol_sum = 0;
	int bad_cable_input_current[] = {CHARGE_CURRENT_300_00_MA,CHARGE_CURRENT_200_00_MA,CHARGE_CURRENT_100_00_MA};
	int bad_cable_current[] = {CHARGE_CURRENT_400_00_MA,CHARGE_CURRENT_300_00_MA,CHARGE_CURRENT_150_00_MA};
	int bad_cable_charger_vol[] = {0,0,0};
	int bad_cable_charger_vol_temp[] = {0,0,0,0,0,0};
	int bad_cable_R1,bad_cable_R2;
#endif

#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	BATTERY_VOLTAGE_ENUM cv_voltage;
	kal_bool hiz_mode_enable = KAL_FALSE;
	kal_bool hiz_mode_disable = KAL_TRUE;
#endif
	unsigned int charging_enable = KAL_TRUE;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (KAL_TRUE == BMT_status.charger_exist)
		charging_enable = KAL_TRUE;
	else
		charging_enable = KAL_FALSE;
#endif

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Charger Error, turn OFF charging !\n");

		charging_enable = KAL_FALSE;
#if 0 
	} else if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = KAL_FALSE;
#endif
	} else {
		
		battery_charging_control(CHARGING_CMD_INIT, NULL);

		battery_log(BAT_LOG_FULL, "charging_hw_init\n");

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		battery_pump_express_algorithm_start();
#endif

		
		if (get_usb_current_unlimited()) {
			if (batt_cust_data.ac_charger_input_current != 0)
				g_temp_input_CC_value = batt_cust_data.ac_charger_input_current;
			else
				g_temp_input_CC_value = batt_cust_data.ac_charger_current;

			g_temp_CC_value = batt_cust_data.ac_charger_current;
			battery_log(BAT_LOG_FULL,
				    "USB_CURRENT_UNLIMITED, use batt_cust_data.ac_charger_current\n");
#ifndef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
		} else if (g_bcct_flag == 1) {
			select_charging_current_bcct();

			battery_log(BAT_LOG_FULL, "[BATTERY] select_charging_current_bcct !\n");
		} else {
			select_charging_current();

			battery_log(BAT_LOG_FULL, "[BATTERY] select_charging_current !\n");
		}
#else
		} else if (g_bcct_flag == 1 || g_bcct_input_flag == 1) {
			select_charging_current();
			select_charging_current_bcct();
			battery_log(BAT_LOG_FULL, "[BATTERY] select_charging_curret_bcct !\n");
		} else {
			select_charging_current();
			battery_log(BAT_LOG_FULL, "[BATTERY] select_charging_curret !\n");
		}
#endif
#ifndef HTC_ENABLE_AICL
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Default CC mode charging : %d, input current = %d\r\n",
			    g_temp_CC_value, g_temp_input_CC_value);
#endif
		if (g_temp_CC_value == CHARGE_CURRENT_0_00_MA
		    || g_temp_input_CC_value == CHARGE_CURRENT_0_00_MA) {

			charging_enable = KAL_FALSE;

			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] charging current is set 0mA, turn off charging !\r\n");
		} else {
#ifdef HTC_ENABLE_AICL
			if(BMT_status.charger_type == STANDARD_CHARGER &&
							BMT_status.htc_acil_state == 0){
				temp_avg_voltage[2] = BMT_status.bat_vol;
				#ifdef CONFIG_HTC_BATT_PCN0010
				if((BMT_status.bat_vol <= HTC_AICL_START_VOL) && (g_is_limit_IUSB == false)){
				#else
				if(BMT_status.bat_vol <= HTC_AICL_START_VOL){
				#endif
					BMT_status.htc_acil_state = AICL_START;
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL]AICL_START , bat_vol = %d.\n", BMT_status.bat_vol);
				}else{
					BMT_status.htc_acil_state = AICL_STOP;
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL]AICL_STOP , bat_vol = %d.\n", BMT_status.bat_vol);
				}
				#ifdef ALPINE_BATT 
				if (fgauge_get_batt_id() == 1) {
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] ALPINE battery id1, AICL_STOP.\n");
					BMT_status.htc_acil_state = AICL_STOP;
				}
				#endif
			}
			switch(BMT_status.htc_acil_state){
				case AICL_START:
					BMT_status.htc_acil_state = AICL_CHECKING;
					Vin_dpm = temp_vin_dpm[0];
					g_temp_input_CC_value = temp_input_current[0];
					g_temp_CC_value = temp_current[0];
					battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm);
					battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
					battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
					battery_log(BAT_LOG_CRTI, "[BATTERY] set default charging current before rise current done\n");
					break; 
				case AICL_CHECKING:
					#ifdef CONFIG_MTK_BQ24296_SUPPORT
					for(i = 0; i < 3; i++){
						Vin_dpm = temp_vin_dpm[i];
						g_temp_input_CC_value = temp_input_current[i];
						g_temp_CC_value = temp_current[i];
						battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm);
						battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
						battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
						if( 2 == i)break;
						if(htc_charging_avg_voltage_check())
							temp_avg_voltage[i] = BMT_status.avg_charger_vol;
					}
					battery_charging_control(CHARGINF_CMD_HTC_GET_DPM_STATUS, &DPM_status);
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] vol1= %d. vol2 = %d, PDM_status=%d\n", temp_avg_voltage[0], temp_avg_voltage[1], DPM_status);
					if(DPM_status == KAL_TRUE){
						if(htc_charging_avg_voltage_check() && (temp_avg_voltage[0] != temp_avg_voltage[1])){
							if((BMT_status.avg_charger_vol > 4350) && (((temp_avg_voltage[1] - BMT_status.avg_charger_vol) * 100 ) / (temp_avg_voltage[0]- temp_avg_voltage[1])) < HTC_AICL_VBUS_DROP_RATIO){
								BMT_status.htc_acil_state = AICL_DONE;
								battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] Done 1.5A charging, charger vol = %d.\n", BMT_status.avg_charger_vol);
								break;
							} else {
								battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] charger vol = %d to small,restore charging cc = %d.\n", BMT_status.avg_charger_vol, g_temp_input_CC_value);
								g_aicl_fail_at_2A = KAL_TRUE; 
							}
						}
					}
					#elif defined(CONFIG_MTK_BQ25896_SUPPORT)
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL]rise charging current to 2A start\n");
					for(i = 0; i < 3; i++){
						Vin_dpm = temp_vin_dpm[i];
						g_temp_input_CC_value = temp_input_current[i];
						g_temp_CC_value = temp_current[i];
						battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm);
						battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
						battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
						msleep(500);
						if(g_cable_out_happened_in_aicl_checking)
							break;
						battery_charging_control(CHARGING_CMD_DUMP_REGISTER, NULL);
						battery_charging_control(CHARGINF_CMD_HTC_GET_DPM_STATUS, &DPM_status);
						battery_charging_control(CHARGING_CMD_GET_IDPM_STATE, &iDPM_state);
						battery_log(BAT_LOG_CRTI, "[BATTERY][AICL][ongoing]vDPM_state=%d,iDPM_state=%d,VCHR=%d,ICHR=%d\n",DPM_status,iDPM_state,
							battery_meter_get_charger_voltage(),battery_meter_get_battery_current());
						if(DPM_status != 0 || iDPM_state != 1 || i == 2)
							break;
						}
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] Rise charging current to 2A end\n");
					if(i == 2 && !g_cable_out_happened_in_aicl_checking)
						{
							if(DPM_status == 0 && iDPM_state == 1)
								{
								BMT_status.htc_acil_state = AICL_DONE;
								battery_log(BAT_LOG_CRTI, "[BATTERY][AICL][finished] Done 2A charging\n");
								break;
							}
						}
					#endif
				case AICL_STOP:
					#ifdef CONFIG_MTK_BQ24296_SUPPORT
					g_temp_input_CC_value = temp_input_current[1];
					g_temp_CC_value = temp_input_current[1];
					Vin_dpm = 4520;
					battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm);  
					battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
					battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
					#elif defined(CONFIG_MTK_BQ25896_SUPPORT)
					if(i < 2) {
						g_aicl_fail_at_2A = KAL_FALSE;
						g_temp_input_CC_value = temp_input_current[0];
						g_temp_CC_value = temp_current[0];
						Vin_dpm = temp_vin_dpm[0]; 
						battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm);
						battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
						battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
						battery_log(BAT_LOG_CRTI, "[BATTERY][AICL][finished] Setback_current_to_1A\n");
					} else { 
						g_aicl_fail_at_2A = KAL_TRUE;
						
						if(g_cable_out_happened_in_aicl_checking) {
							g_temp_input_CC_value = temp_input_current[1];
							g_temp_CC_value = temp_current[1];
							Vin_dpm = temp_vin_dpm[1]; 
							battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm);
							battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
							battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
							battery_log(BAT_LOG_CRTI, "[BATTERY][AICL][finished] Setback_current_to_1.5A for 2A CableOut event!\n");
						} else {
							
							for(i = 1; i < 6; i++)
							{
								g_temp_input_CC_value = 100*(2000 - 100*i);
								battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
								battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_input_CC_value);
								msleep(1000);
								battery_charging_control(CHARGING_CMD_DUMP_REGISTER, NULL);
								battery_log(BAT_LOG_CRTI, "[BATTERY][AICL][2Adropping] VCHR=%d,ICHR=%d\n",battery_meter_get_charger_voltage(),battery_meter_get_battery_current());
							}
							battery_log(BAT_LOG_CRTI, "[BATTERY][AICL][finished] Setback_current_to_1.5A for Reg error!\n");
						}
					}
					#endif
					htc_charging_avg_voltage_check();
					battery_log(BAT_LOG_CRTI,"[BATTERY][BadCable] bat_vol=%d,charger_vol=%d,bat_curr=%d\n",
						BMT_status.bat_vol,BMT_status.avg_charger_vol,battery_meter_get_battery_current());
					if(BMT_status.bat_vol <= 4200 && BMT_status.avg_charger_vol <= 4600 && g_aicl_fail_at_2A == KAL_FALSE)
						BMT_status.htc_acil_state = AICL_PENDGIN;
					else
						BMT_status.htc_acil_state = AICL_COMPLETE;
					battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] Stop 1.5A charging, charger cc = %d. Vin_dpm = %d\n", g_temp_input_CC_value, Vin_dpm);
					break;
				case AICL_PENDGIN: 
					#ifdef CONFIG_HTC_BATT_PCN0018
					for(i = 0; i < sizeof(bad_cable_current)/sizeof(bad_cable_current[0]); i++){
						g_temp_input_CC_value = bad_cable_input_current[i];
						g_temp_CC_value = bad_cable_current[i];
						Vin_dpm = BAD_CABLE_DPM;
						charger_vol_sum = 0;
						battery_charging_control(CHARGING_CMD_HTC_SET_VDM, &Vin_dpm); 
						battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
						battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
						msleep(BAD_CABLE_SLEEP_TIME);
						battery_charging_control(CHARGING_CMD_DUMP_REGISTER,NULL);
						for(j = 0; j < 6; j++)
						{
							bad_cable_charger_vol_temp[j] = battery_meter_get_charger_voltage();
							msleep(200);
							charger_vol_sum += bad_cable_charger_vol_temp[j];
						}
						for(j = 0; j < 6; j++)
						{
							for(k = 5; k > j; k--){
							if(bad_cable_charger_vol_temp[k] < bad_cable_charger_vol_temp[k-1])
							{
								int temp = bad_cable_charger_vol_temp[k];
								bad_cable_charger_vol_temp[k] = bad_cable_charger_vol_temp[k-1];
								bad_cable_charger_vol_temp[k-1] = temp;
							}
							}
						}
						charger_vol_sum -= (bad_cable_charger_vol_temp[0]+bad_cable_charger_vol_temp[5]);
						bad_cable_charger_vol[i] = charger_vol_sum/4;
						battery_log(BAT_LOG_CRTI,"[BATTERY][BadCable] bad_cable_charger_vol[%d]=%d\n",i,bad_cable_charger_vol[i]);
					}
					bad_cable_R1 = (bad_cable_charger_vol[2] - bad_cable_charger_vol[0])*1000/(BAD_CABLE_CURRENT_GAP_R1) - BAD_CABLE_MAIN_BD_R;
					bad_cable_R2 = (bad_cable_charger_vol[2] - bad_cable_charger_vol[1])*1000/(BAD_CABLE_CURRENT_GAP_R2) - BAD_CABLE_MAIN_BD_R;
					battery_log(BAT_LOG_CRTI, "[BATTERY][BadCable] R1=%d,R2=%d\n",bad_cable_R1,bad_cable_R2);
					if((bad_cable_R1 > BAD_CABLE_R_THRESHOLD) || (bad_cable_R2 > BAD_CABLE_R_THRESHOLD))
						{
							htc_update_bad_cable(1);
							bat_update_thread_wakeup();
							battery_log(BAT_LOG_CRTI, "[BATTERY][BadCable] pop up bad cable warning message!\n");
						}
					#endif 
					BMT_status.htc_acil_state = AICL_COMPLETE;
				case AICL_COMPLETE:
				case AICL_DONE:
				default:
					battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
					battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
					break;
			}
			battery_log(BAT_LOG_CRTI, "[BATTERY][AICL] status = %d, input cc = %d(%d).\n",
				BMT_status.htc_acil_state, g_temp_input_CC_value, g_temp_CC_value);
#else
			battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
						 &g_temp_input_CC_value);
			battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);
#endif
			
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
			if (batt_cust_data.high_battery_voltage_support)
				cv_voltage = BATTERY_VOLT_04_400000_V;
			else
				cv_voltage = BATTERY_VOLT_04_200000_V;

			#ifdef CONFIG_MTK_DYNAMIC_BAT_CV_SUPPORT
			cv_voltage = get_constant_voltage() * 1000;
			battery_log(BAT_LOG_FULL, "[BATTERY][BIF] Setting CV to %d\n", cv_voltage / 1000);
			#endif
			if(BMT_status.temperature >= 48 && !g_flag_keep_charge_on)
		 		cv_voltage = BATTERY_VOLT_04_000000_V;
			battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);
			if(BMT_status.bat_vol > cv_voltage)
				battery_charging_control(CHARGING_CMD_HIZ_EN_CTRL, &hiz_mode_enable);
			else if(BMT_status.bat_vol < (cv_voltage - 100))
				battery_charging_control(CHARGING_CMD_HIZ_EN_CTRL, &hiz_mode_disable);

			#if defined(CONFIG_MTK_HAFG_20)
			g_cv_voltage = cv_voltage;
			#endif
#endif
		}
	}

	
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	battery_log(BAT_LOG_FULL, "[BATTERY] pchr_turn_on_charging(), enable =%d !\r\n",
		    charging_enable);
}


PMU_STATUS BAT_PreChargeModeAction(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] Pre-CC mode charge, timer=%d on %d !!\n\r",
		    BMT_status.PRE_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	
	pchr_turn_on_charging();
#if defined(CONFIG_MTK_HAFG_20)
	if (BMT_status.UI_SOC2 == 100 && charging_full_check()) {
#else
	if (BMT_status.UI_SOC == 100) {
#endif
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = KAL_TRUE;
		g_charging_full_reset_bat_meter = KAL_TRUE;
	} else if (BMT_status.bat_vol > V_PRE2CC_THRES) {
		BMT_status.bat_charging_state = CHR_CC;
	}



	return PMU_STATUS_OK;
}


PMU_STATUS BAT_ConstantCurrentModeAction(void)
{
	battery_log(BAT_LOG_FULL, "[BATTERY] CC mode charge, timer=%d on %d !!\n\r",
		    BMT_status.CC_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time += BAT_TASK_PERIOD;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	
	pchr_turn_on_charging();
#ifdef CONFIG_HTC_BATT_PCN0001
	if(g_charging_full_to_close_charger == true){
#else
	if (charging_full_check() == KAL_TRUE) {
#endif 
#ifdef CONFIG_HTC_BATT_PCN0001
		printk("[enter Full from CC]\n");
		g_charging_full_to_close_charger = false;
#endif 
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = KAL_TRUE;
		g_charging_full_reset_bat_meter = KAL_TRUE;
	}

	return PMU_STATUS_OK;
}


PMU_STATUS BAT_BatteryFullAction(void)
{
	battery_log(BAT_LOG_CRTI, "[BATTERY] Battery full !!SOC=%d\n\r",BMT_status.SOC);

	
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = KAL_FALSE;

#ifdef CONFIG_HTC_BATT_WA_PCN0018
	if(g_flag_keep_charge_on) {
		int charging_enable = KAL_TRUE;
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	} else {
		int charging_enable = KAL_FALSE;
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		battery_charging_control(CHARGING_CMD_HIZ_EN_CTRL, &charging_enable);
		BMT_status.bat_full = KAL_FALSE;
	}
	if (BMT_status.SOC <= 99) {
#else
	if (charging_full_check() == KAL_FALSE) {
#endif
		battery_log(BAT_LOG_CRTI, "[BATTERY] Battery Re-charging !!\n\r");

		BMT_status.bat_in_recharging_state = KAL_TRUE;
		BMT_status.bat_charging_state = CHR_CC;
#ifndef CONFIG_MTK_HAFG_20
		battery_meter_reset();
#endif
#if defined(PUMPEX_PLUS_RECHG) && defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		
		pep_det_rechg = KAL_TRUE;
#endif
	}


	return PMU_STATUS_OK;
}


PMU_STATUS BAT_BatteryHoldAction(void)
{
	unsigned int charging_enable;

	battery_log(BAT_LOG_CRTI, "[BATTERY] Hold mode !!\n\r");

	if (BMT_status.bat_vol < TALKING_RECHARGE_VOLTAGE || g_call_state == CALL_IDLE) {
		BMT_status.bat_charging_state = CHR_CC;
		battery_log(BAT_LOG_CRTI, "[BATTERY] Exit Hold mode and Enter CC mode !!\n\r");
	}

	
	charging_enable = KAL_FALSE;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}


PMU_STATUS BAT_BatteryStatusFailAction(void)
{
	unsigned int charging_enable;

	battery_log(BAT_LOG_CRTI, "[BATTERY] BAD Battery status... Charging Stop !!\n\r");

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((g_temp_status == TEMP_ABOVE_POS_60) || (g_temp_status == TEMP_BELOW_NEG_10))
		temp_error_recovery_chr_flag = KAL_FALSE;

	if ((temp_error_recovery_chr_flag == KAL_FALSE) && (g_temp_status != TEMP_ABOVE_POS_60)
	    && (g_temp_status != TEMP_BELOW_NEG_10)) {
		temp_error_recovery_chr_flag = KAL_TRUE;
		BMT_status.bat_charging_state = CHR_PRE;
	}
#endif

	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_full = KAL_FALSE;

	
	charging_enable = KAL_FALSE;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	
	if (BMT_status.pwr_en == KAL_FALSE)
		battery_charging_control(CHARGING_CMD_HIZ_EN_CTRL, &charging_enable);
	

	return PMU_STATUS_OK;
}


void mt_battery_charging_algorithm(void)
{
	battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
#if defined(PUMPEX_PLUS_RECHG)
	if (BMT_status.bat_in_recharging_state == KAL_TRUE && pep_det_rechg == KAL_TRUE)
		ta_check_chr_type = KAL_TRUE;
#endif
	battery_pump_express_charger_check();
#endif
	switch (BMT_status.bat_charging_state) {
	case CHR_PRE:
		BAT_PreChargeModeAction();
		break;

	case CHR_CC:
		BAT_ConstantCurrentModeAction();
		break;

	case CHR_BATFULL:
		BAT_BatteryFullAction();
		break;

	case CHR_HOLD:
		BAT_BatteryHoldAction();
		break;

	case CHR_ERROR:
		BAT_BatteryStatusFailAction();
		break;
	}

	battery_charging_control(CHARGING_CMD_DUMP_REGISTER, NULL);
}
