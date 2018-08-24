/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */





#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#include "mt_clkbuf_ctl.h"
#include <mach/mt_pmic.h>
#include <mt-plat/mt_chip.h>
#ifdef _VOW_ENABLE
#include <mt-plat/vow_api.h>
#endif
#include "mt_soc_afe_control.h"
#include <mt-plat/upmu_common.h>

#ifdef CONFIG_MTK_SPEAKER
#include "mt_soc_codec_speaker_63xx.h"
#endif

#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Gpio.h"

#define AW8736_MODE_CTRL 

#ifdef CONFIG_MTK_SPKGPIO_REWORK	
#include "../../../../drivers/misc/mediatek/auxadc/mt_auxadc.h"
#endif

#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
#include <sound/htc_acoustic_alsa.h>
#endif

static bool AudioPreAmp1_Sel(int Mul_Sel);
static bool GetAdcStatus(void);
static void Apply_Speaker_Gain(void);
static bool TurnOnVOWDigitalHW(bool enable);
static void TurnOffDacPower(void);
static void TurnOnDacPower(void);
static void SetDcCompenSation(void);
static void Voice_Amp_Change(bool enable);
static void Speaker_Amp_Change(bool enable);
static bool TurnOnVOWADcPowerACC(int MicType, bool enable);

static mt6331_Codec_Data_Priv *mCodec_data;
static uint32 mBlockSampleRate[AUDIO_ANALOG_DEVICE_INOUT_MAX] = { 48000, 48000, 48000 };

static DEFINE_MUTEX(Ana_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_buf_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_Clk_Mutex);
static DEFINE_MUTEX(Ana_Power_Mutex);
static DEFINE_MUTEX(AudAna_lock);

static int mAudio_Analog_Mic1_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic2_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic3_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic4_mode = AUDIO_ANALOGUL_MODE_ACC;

static int mAudio_Vow_Analog_Func_Enable;
static int mAudio_Vow_Digital_Func_Enable;

static int TrimOffset = 2048;
static const int DC1unit_in_uv = 19184;	
	
static const int DC1devider = 8;	

#ifdef EFUSE_HP_TRIM
static unsigned int RG_AUDHPLTRIM_VAUDP15, RG_AUDHPRTRIM_VAUDP15, RG_AUDHPLFINETRIM_VAUDP15,
	RG_AUDHPRFINETRIM_VAUDP15, RG_AUDHPLTRIM_VAUDP15_SPKHP, RG_AUDHPRTRIM_VAUDP15_SPKHP,
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP, RG_AUDHPRFINETRIM_VAUDP15_SPKHP;
#endif

static unsigned int pin_extspkamp, pin_extspkamp_2, pin_vowclk, pin_audmiso, pin_rcvspkswitch,
		    pin_hpswitchtoground;
static unsigned int pin_mode_extspkamp, pin_mode_extspkamp_2, pin_mode_vowclk, pin_mode_audmiso,
	pin_mode_rcvspkswitch, pin_mode_hpswitchtoground;


#ifdef CONFIG_MTK_SPEAKER
static int Speaker_mode = AUDIO_SPEAKER_MODE_AB;
static unsigned int Speaker_pga_gain = 1;	
static bool mSpeaker_Ocflag;
#endif
static int mAdc_Power_Mode;
static unsigned int dAuxAdcChannel = 16;
static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec;
static bool mClkBufferfromPMIC;
static uint32 MicbiasRef, GetMicbias;

static int reg_AFE_VOW_CFG0 = 0x0000;	
static int reg_AFE_VOW_CFG1 = 0x0000;	
static int reg_AFE_VOW_CFG2 = 0x2222;	
static int reg_AFE_VOW_CFG3 = 0x8767;	
static int reg_AFE_VOW_CFG4 = 0x006E;	
static int reg_AFE_VOW_CFG5 = 0x0001;	
static bool mIsVOWOn;

typedef enum {
	AUDIO_VOW_MIC_TYPE_Handset_AMIC = 0,
	AUDIO_VOW_MIC_TYPE_Headset_MIC,
	AUDIO_VOW_MIC_TYPE_Handset_DMIC,	
	AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K,	
	AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC,	
	AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC,
	AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM,	
	AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM	
} AUDIO_VOW_MIC_TYPE;

static int mAudio_VOW_Mic_type = AUDIO_VOW_MIC_TYPE_Handset_AMIC;
static void Audio_Amp_Change(int channels, bool enable);
static void SavePowerState(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		mCodec_data->mAudio_BackUpAna_DevicePower[i] =
		    mCodec_data->mAudio_Ana_DevicePower[i];
	}
}

static void RestorePowerState(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		mCodec_data->mAudio_Ana_DevicePower[i] =
		    mCodec_data->mAudio_BackUpAna_DevicePower[i];
	}
}

static bool GetDLStatus(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}

static bool mAnaSuspend;
void SetAnalogSuspend(bool bEnable)
{
	pr_warn("%s bEnable ==%d mAnaSuspend = %d\n", __func__, bEnable, mAnaSuspend);
	if ((bEnable == true) && (mAnaSuspend == false)) {
		
		SavePowerState();
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
			    false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
			    false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
			    false;
			Voice_Amp_Change(false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
			    false;
			Speaker_Amp_Change(false);
		}
		
		mAnaSuspend = true;
	} else if ((bEnable == false) && (mAnaSuspend == true)) {
		
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
			    true;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
			    false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] ==
		    true) {
			Voice_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
			    false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] ==
		    true) {
			Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
			    false;
		}
		RestorePowerState();
		
		mAnaSuspend = false;
	}
}

static int audck_buf_Count;
void audckbufEnable(bool enable)
{
	
	mutex_lock(&Ana_buf_Ctrl_Mutex);
	if (enable) {
		if (audck_buf_Count == 0) {
			
#ifndef CONFIG_FPGA_EARLY_PORTING
			if (mClkBufferfromPMIC) {
				Ana_Set_Reg(DCXO_CW01, 0x8000, 0x8000);
				pr_warn("-PMIC DCXO XO_AUDIO_EN_M enable\n");
			} else {
				clk_buf_ctrl(CLK_BUF_AUDIO, true);
				pr_warn("-RF clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
			}
#endif
		}
		audck_buf_Count++;
	} else {
		audck_buf_Count--;
		if (audck_buf_Count == 0) {
			
#ifndef CONFIG_FPGA_EARLY_PORTING
			if (mClkBufferfromPMIC) {
				Ana_Set_Reg(DCXO_CW01, 0x0000, 0x8000);
				pr_warn("-PMIC DCXO XO_AUDIO_EN_M disable\n");
			} else {
				clk_buf_ctrl(CLK_BUF_AUDIO, false);
				pr_warn("-RF clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
			}
#endif
		}
		if (audck_buf_Count < 0) {
			pr_warn("audck_buf_Count count <0\n");
			audck_buf_Count = 0;
		}
	}
	mutex_unlock(&Ana_buf_Ctrl_Mutex);
}

static int ClsqCount;
static void ClsqEnable(bool enable)
{
	
	mutex_lock(&AudAna_lock);
	if (enable) {
		if (ClsqCount == 0) {
			Ana_Set_Reg(TOP_CLKSQ, 0x0001, 0x0001);
			
			Ana_Set_Reg(TOP_CLKSQ_SET, 0x0001, 0x0001);
			
		}
		ClsqCount++;
	} else {
		ClsqCount--;
		if (ClsqCount < 0) {
			pr_warn("ClsqEnable count <0\n");
			ClsqCount = 0;
		}
		if (ClsqCount == 0) {
			Ana_Set_Reg(TOP_CLKSQ_CLR, 0x0001, 0x0001);
			
			Ana_Set_Reg(TOP_CLKSQ, 0x0000, 0x0001);
			
		}
	}
	mutex_unlock(&AudAna_lock);
}

static int TopCkCount;
static void Topck_Enable(bool enable)
{
	
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (TopCkCount == 0) {
			Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0xf000, 0xf000);
			
			
			
		}
		TopCkCount++;
	} else {
		TopCkCount--;
		if (TopCkCount == 0) {
			Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0xf000, 0xf000);
			
			
		}

		if (TopCkCount <= 0) {
			
			TopCkCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static int NvRegCount;
static void NvregEnable(bool enable)
{
	
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (NvRegCount == 0) {
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0x1000);
			
		}
		NvRegCount++;
	} else {
		NvRegCount--;
		if (NvRegCount == 0) {
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1000, 0x1000);
			
		}
		if (NvRegCount < 0) {
			pr_warn("NvRegCount <0 =%d\n ", NvRegCount);
			NvRegCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static void HP_Ana_Switch_to_On(void)
{
#if defined(CONFIG_MTK_HP_ANASWITCH)
#if defined(CONFIG_MTK_LEGACY)
	int ret;

	ret = GetGPIO_Info(12, &pin_hpswitchtoground, &pin_mode_hpswitchtoground);
	if (ret < 0) {
		pr_debug("Headphone swtich to ground GetGPIO_Info FAIL!!!\n");
		return;
	}

	mt_set_gpio_mode(pin_hpswitchtoground, GPIO_MODE_00);	
	mt_set_gpio_dir(pin_hpswitchtoground, GPIO_DIR_OUT);	
	mt_set_gpio_out(pin_hpswitchtoground, GPIO_OUT_ZERO);	
#else
	AudDrv_GPIO_HPDEPOP_Select(true);
#endif

	usleep_range(500, 800);
#endif
}

static void HP_Ana_Switch_to_Release(void)
{
#if defined(CONFIG_MTK_HP_ANASWITCH)
	usleep_range(500, 800);

#if defined(CONFIG_MTK_LEGACY)
	int ret;

	ret = GetGPIO_Info(12, &pin_hpswitchtoground, &pin_mode_hpswitchtoground);
	if (ret < 0) {
		pr_debug("Headphone swtich to ground GetGPIO_Info FAIL!!!\n");
		return;
	}

	mt_set_gpio_mode(pin_hpswitchtoground, GPIO_MODE_00);	
	mt_set_gpio_dir(pin_hpswitchtoground, GPIO_DIR_OUT);
	mt_set_gpio_out(pin_hpswitchtoground, GPIO_OUT_ONE);	
#else
	AudDrv_GPIO_HPDEPOP_Select(false);
#endif
#endif
}

#ifdef _VOW_ENABLE
static int VOW12MCKCount;
#endif

#ifdef _VOW_ENABLE

static void VOW12MCK_Enable(bool enable)
{

	pr_warn("VOW12MCK_Enable VOW12MCKCount == %d enable = %d\n", VOW12MCKCount, enable);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (VOW12MCKCount == 0)
			Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x8000, 0x8000);
		
		VOW12MCKCount++;
	} else {
		VOW12MCKCount--;
		if (VOW12MCKCount == 0)
			Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x8000, 0x8000);
		

		if (VOW12MCKCount < 0) {
			pr_warn("VOW12MCKCount <0 =%d\n ", VOW12MCKCount);
			VOW12MCKCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}
#endif


void vow_irq_handler(void)
{
#ifdef _VOW_ENABLE

	pr_warn("vow_irq_handler,audio irq event....\n");
	
	
#if defined(VOW_TONE_TEST)
	EnableSideGenHw(Soc_Aud_InterConnectionOutput_O03, Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT,
			true);
#endif
	
#endif
}


void Auddrv_Read_Efuse_HPOffset(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef EFUSE_HP_TRIM
	unsigned int ret = 0;
	unsigned int reg_val = 0;
	int i = 0, j = 0;
	unsigned int efusevalue[3];

	pr_warn("Auddrv_Read_Efuse_HPOffset(+)\n");

	
	ret = pmic_config_interface(0x026C, 0x0040, 0xFFFF, 0);
	ret = pmic_config_interface(0x024E, 0x0004, 0xFFFF, 0);

	
	ret = pmic_config_interface(0x0C16, 0x1, 0x1, 0);



	for (i = 0xe; i <= 0x10; i++) {
		
		ret = pmic_config_interface(0x0C00, i, 0x1F, 1);

		
		ret = pmic_read_interface(0xC10, &reg_val, 0x1, 0);

		if (reg_val == 0)
			ret = pmic_config_interface(0xC10, 1, 0x1, 0);
		else
			ret = pmic_config_interface(0xC10, 0, 0x1, 0);


		
		reg_val = 1;
		while (reg_val == 1) {
			ret = pmic_read_interface(0xC1A, &reg_val, 0x1, 0);
			pr_warn("Auddrv_Read_Efuse_HPOffset polling 0xC1A=0x%x\n", reg_val);
		}

		udelay(1000);	

		
		efusevalue[j] = upmu_get_reg_value(0x0C18);
		pr_warn("HPoffset : efuse[%d]=0x%x\n", j, efusevalue[j]);
		j++;
	}

	
	ret = pmic_config_interface(0x024C, 0x0004, 0xFFFF, 0);
	ret = pmic_config_interface(0x026A, 0x0040, 0xFFFF, 0);

	RG_AUDHPLTRIM_VAUDP15 = (efusevalue[0] >> 10) & 0xf;
	RG_AUDHPRTRIM_VAUDP15 = ((efusevalue[0] >> 14) & 0x3) + ((efusevalue[1] & 0x3) << 2);
	RG_AUDHPLFINETRIM_VAUDP15 = (efusevalue[1] >> 3) & 0x3;
	RG_AUDHPRFINETRIM_VAUDP15 = (efusevalue[1] >> 5) & 0x3;
	RG_AUDHPLTRIM_VAUDP15_SPKHP = (efusevalue[1] >> 7) & 0xF;
	RG_AUDHPRTRIM_VAUDP15_SPKHP = (efusevalue[1] >> 11) & 0xF;
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP =
	    ((efusevalue[1] >> 15) & 0x1) + ((efusevalue[2] & 0x1) << 1);
	RG_AUDHPRFINETRIM_VAUDP15_SPKHP = ((efusevalue[2] >> 1) & 0x3);

	pr_warn("RG_AUDHPLTRIM_VAUDP15 = %x\n", RG_AUDHPLTRIM_VAUDP15);
	pr_warn("RG_AUDHPRTRIM_VAUDP15 = %x\n", RG_AUDHPRTRIM_VAUDP15);
	pr_warn("RG_AUDHPLFINETRIM_VAUDP15 = %x\n", RG_AUDHPLFINETRIM_VAUDP15);
	pr_warn("RG_AUDHPRFINETRIM_VAUDP15 = %x\n", RG_AUDHPRFINETRIM_VAUDP15);
	pr_warn("RG_AUDHPLTRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPLTRIM_VAUDP15_SPKHP);
	pr_warn("RG_AUDHPRTRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPRTRIM_VAUDP15_SPKHP);
	pr_warn("RG_AUDHPLFINETRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPLFINETRIM_VAUDP15_SPKHP);
	pr_warn("RG_AUDHPRFINETRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPRFINETRIM_VAUDP15_SPKHP);
#endif
#endif
	pr_warn("Auddrv_Read_Efuse_HPOffset(-)\n");
}
EXPORT_SYMBOL(Auddrv_Read_Efuse_HPOffset);

#ifdef CONFIG_MTK_SPEAKER
static void Apply_Speaker_Gain(void)
{
	pr_warn("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);

	Ana_Set_Reg(SPK_ANA_CON0, Speaker_pga_gain << 11, 0x7800);
}
#else
static void Apply_Speaker_Gain(void)
{
	Ana_Set_Reg(ZCD_CON1,
		    (mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR] << 7) |
		    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL],
		    0x0f9f);
}
#endif

void setOffsetTrimMux(unsigned int Mux)
{
	
	Ana_Set_Reg(AUDDEC_ANA_CON4, Mux << 8, 0xf << 8);	
}

void setOffsetTrimBufferGain(unsigned int gain)
{
	Ana_Set_Reg(AUDDEC_ANA_CON4, gain << 12, 0x3 << 12);	
}

static int mHplTrimOffset = 2048;
static int mHprTrimOffset = 2048;

void SetHplTrimOffset(int Offset)
{
	pr_warn("%s Offset = %d\n", __func__, Offset);
	mHplTrimOffset = Offset;
	if ((Offset > 2098) || (Offset < 1998)) {
		mHplTrimOffset = 2048;
		pr_err("SetHplTrimOffset offset may be invalid offset = %d\n", Offset);
	}
}

void SetHprTrimOffset(int Offset)
{
	pr_warn("%s Offset = %d\n", __func__, Offset);
	mHprTrimOffset = Offset;
	if ((Offset > 2098) || (Offset < 1998)) {
		mHprTrimOffset = 2048;
		pr_err("SetHprTrimOffset offset may be invalid offset = %d\n", Offset);
	}
}

void EnableTrimbuffer(bool benable)
{
	if (benable == true) {
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0080, 0x0080);
		
	} else {
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0000, 0x0080);
		
	}
}

void OpenTrimBufferHardware(bool enable)
{
	
	if (enable) {
		pr_warn("%s true\n", __func__);
		TurnOnDacPower();

		
		Ana_Set_Reg(0x0EAA, 0x0200, 0x0200);

		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0700, 0xffff);
		
		Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
		
		Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
		

		udelay(50);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE08F, 0xffff);
		

		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0300, 0xffff);
		
	} else {
		pr_warn("%s false\n", __func__);
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
		
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
		
		TurnOffDacPower();
	}
}

void OpenAnalogTrimHardware(bool enable)
{
	if (enable) {
		pr_warn("%s true\n", __func__);
		TurnOnDacPower();
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		
		Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
		
		Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
		

		udelay(50);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
		

		
		Ana_Set_Reg(ZCD_CON2, 0x0489, 0xffff);
		
		

		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF49F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4FF, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4EF, 0xffff);
		
	} else {
		pr_warn("%s false\n", __func__);
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
		
		TurnOffDacPower();
	}
}

void OpenAnalogHeadphone(bool bEnable)
{
	pr_warn("OpenAnalogHeadphone bEnable = %d", bEnable);
	if (bEnable) {
		SetHplTrimOffset(2048);
		SetHprTrimOffset(2048);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 44100;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = true;
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
}

bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
	
	if (GetDLStatus() == true)
		return false;

	if (bEnable == true) {
		TurnOnDacPower();
		
#if 0				
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);	
		
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0001, 0x0001);	
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, 0xf000, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, 0xf000, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, 0x0001, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, 0x7f00, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, 0x7f00, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, 0x0001, 0xffff);
#endif

		HP_Ana_Switch_to_On();

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0000, 0x2000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x4000, 0x4000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE089, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON5, 0x0009, 0xffff);
		

		
		HP_Ana_Switch_to_Release();

	} else {
		HP_Ana_Switch_to_On();

		Ana_Set_Reg(AUDDEC_ANA_CON5, 0x0000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0000, 0x4000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0x2000);
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);

		
		HP_Ana_Switch_to_Release();

		TurnOffDacPower();
	}
	return true;
}

void setHpGainZero(void)
{
	Ana_Set_Reg(ZCD_CON2, 0x8 << 7, 0x0f80);
	Ana_Set_Reg(ZCD_CON2, 0x8, 0x001f);
}

void SetSdmLevel(unsigned int level)
{
	Ana_Set_Reg(AFE_DL_SDM_CON1, level, 0xffffffff);
}


static void SetHprOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	DCoffsetValue = (OffsetTrimming * 11250 + 2048) / 4096;
	
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, RegValue, 0xffff);
}

static void SetHplOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	DCoffsetValue = (OffsetTrimming * 11250 + 2048) / 4096;
	
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, RegValue, 0xffff);
}

static void EnableDcCompensation(bool bEnable)
{
#ifndef EFUSE_HP_TRIM
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, bEnable, 0x1);
#endif
}

static void SetHprOffsetTrim(void)
{
	int OffsetTrimming = mHprTrimOffset - TrimOffset;

	SetHprOffset(OffsetTrimming);
}

static void SetHpLOffsetTrim(void)
{
	int OffsetTrimming = mHplTrimOffset - TrimOffset;

	SetHplOffset(OffsetTrimming);
}

static void SetDcCompenSation(void)
{
#ifndef EFUSE_HP_TRIM
	SetHprOffsetTrim();
	SetHpLOffsetTrim();
	EnableDcCompensation(true);
#else				
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0800, 0x0800);	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLTRIM_VAUDP15 << 3, 0x0078);	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRTRIM_VAUDP15 << 7, 0x0780);	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLFINETRIM_VAUDP15 << 12, 0x3000);	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRFINETRIM_VAUDP15 << 14, 0xC000);	
#endif
}

#ifdef CONFIG_MTK_SPEAKER
static void SetDcCompenSation_SPKHP(void)
{
#ifdef EFUSE_HP_TRIM		
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0800, 0x0800);	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLTRIM_VAUDP15_SPKHP << 3, 0x0078);
	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRTRIM_VAUDP15_SPKHP << 7, 0x0780);
	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLFINETRIM_VAUDP15_SPKHP << 12, 0x3000);
	
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRFINETRIM_VAUDP15_SPKHP << 14, 0xC000);
	
#endif
}
#endif

static void SetDCcoupleNP(int MicBias, int mode)
{
	
	switch (mode) {
	case AUDIO_ANALOGUL_MODE_ACC:
	case AUDIO_ANALOGUL_MODE_DCC:
	case AUDIO_ANALOGUL_MODE_DMIC:{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x000E);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS2)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0E00);
		}
		break;
	case AUDIO_ANALOGUL_MODE_DCCECMDIFF:{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x000E, 0x000E);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS2)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0E00, 0x0E00);
		}
		break;
	case AUDIO_ANALOGUL_MODE_DCCECMSINGLE:{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0002, 0x000E);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS2)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0200, 0x0E00);
		}
		break;
	default:
		break;
	}
}

uint32 GetULFrequency(uint32 frequency)
{
	uint32 Reg_value = 0;

	pr_warn("%s frequency =%d\n", __func__, frequency);
	switch (frequency) {
	case 8000:
	case 16000:
	case 32000:
		Reg_value = 0x0;
		break;
	case 48000:
		Reg_value = 0x1;
	default:
		break;

	}
	return Reg_value;
}


uint32 ULSampleRateTransform(uint32 SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return 0;
	case 16000:
		return 1;
	case 32000:
		return 2;
	case 48000:
		return 3;
	default:
		break;
	}
	return 0;
}


static int mt63xx_codec_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && substream->runtime->rate) {
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && substream->runtime->rate) {
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	
	return 0;
}

static int mt63xx_codec_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_warn("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
			substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	return 0;
}

static int mt6323_codec_trigger(struct snd_pcm_substream *substream, int command,
				struct snd_soc_dai *Daiport)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}

	
	return 0;
}

static const struct snd_soc_dai_ops mt6323_aif1_dai_ops = {
	.startup = mt63xx_codec_startup,
	.prepare = mt63xx_codec_prepare,
	.trigger = mt6323_codec_trigger,
};

static struct snd_soc_dai_driver mtk_6331_dai_codecs[] = {
	{
	 .name = MT_SOC_CODEC_TXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_DL1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_RXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .capture = {
		     .stream_name = MT_SOC_UL1_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_TDMRX_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .capture = {
		     .stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
		     .channels_min = 2,
		     .channels_max = 8,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8 |
				 SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE |
				 SNDRV_PCM_FMTBIT_U16_BE | SNDRV_PCM_FMTBIT_S16_BE |
				 SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_S24_LE |
				 SNDRV_PCM_FMTBIT_U24_BE | SNDRV_PCM_FMTBIT_S24_BE |
				 SNDRV_PCM_FMTBIT_U24_3LE | SNDRV_PCM_FMTBIT_S24_3LE |
				 SNDRV_PCM_FMTBIT_U24_3BE | SNDRV_PCM_FMTBIT_S24_3BE |
				 SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_S32_LE |
				 SNDRV_PCM_FMTBIT_U32_BE | SNDRV_PCM_FMTBIT_S32_BE),
		     },
	 },
	{
	 .name = MT_SOC_CODEC_I2S0TXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rate_min = 8000,
		      .rate_max = 192000,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      }
	 },
	{
	 .name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_VOICE_MD2DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
		.name = MT_SOC_CODEC_VOICE_USBDAI_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			   .stream_name = MT_SOC_VOICE_USB_STREAM_NAME,
			   .channels_min = 1,
			   .channels_max = 2,
			   .rates = SNDRV_PCM_RATE_8000_192000,
			   .formats = SND_SOC_ADV_MT_FMTS,
			   },
		.capture = {
			  .stream_name = MT_SOC_VOICE_USB_STREAM_NAME,
			  .channels_min = 1,
			  .channels_max = 2,
			  .rates = SNDRV_PCM_RATE_8000_192000,
			  .formats = SND_SOC_ADV_MT_FMTS,
			  },
	},
	{
		.name = MT_SOC_CODEC_VOICE_USB_ECHOREF_DAI_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			   .stream_name = MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME,
			   .channels_min = 1,
			   .channels_max = 2,
			   .rates = SNDRV_PCM_RATE_8000_192000,
			   .formats = SND_SOC_ADV_MT_FMTS,
			   },
		.capture = {
			  .stream_name = MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME,
			  .channels_min = 1,
			  .channels_max = 2,
			  .rates = SNDRV_PCM_RATE_8000_192000,
			  .formats = SND_SOC_ADV_MT_FMTS,
			  },
	},
	{
	 .name = MT_SOC_CODEC_FMI2S2RXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_FM_I2S2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_FM_I2S2_RECORD_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_ULDLLOOPBACK_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_STUB_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_ROUTING_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_RXDAI2_NAME,
	 .capture = {
		     .stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_MRGRX_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_MRGRX_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_MRGRX_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_HP_IMPEDANCE_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_FM_I2S_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_TXDAI2_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_DL2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
};


uint32 GetDLNewIFFrequency(unsigned int frequency)
{
	uint32 Reg_value = 0;
	
	switch (frequency) {
	case 8000:
		Reg_value = 0;
		break;
	case 11025:
		Reg_value = 1;
		break;
	case 12000:
		Reg_value = 2;
		break;
	case 16000:
		Reg_value = 3;
		break;
	case 22050:
		Reg_value = 4;
		break;
	case 24000:
		Reg_value = 5;
		break;
	case 32000:
		Reg_value = 6;
		break;
	case 44100:
		Reg_value = 7;
		break;
	case 48000:
		Reg_value = 8;
		break;
	default:
		pr_warn("ApplyDLNewIFFrequency with frequency = %d", frequency);
	}
	return Reg_value;
}

static void TurnOnDacPower(void)
{
	pr_warn("TurnOnDacPower\n");
	audckbufEnable(true);
	NvregEnable(true);	
	ClsqEnable(true);	
	Topck_Enable(true);	
	
	udelay(250);

	if (GetAdcStatus() == false)
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0xffff);	
	else
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);	

	
	Ana_Set_Reg(AFE_NCP_CFG1, 0x1515, 0xffff);
	
	Ana_Set_Reg(AFE_NCP_CFG0, 0x8C01, 0xffff);
	

	udelay(250);

	
	
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xC3A1, 0xffff);
	
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000B, 0xffff);
	
	Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001E, 0xffff);
	
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
	

	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0,
		    ((GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12) |
		     0x330), 0xffff);
	Ana_Set_Reg(AFE_DL_SRC2_CON0_H,
		    ((GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12) |
		     0x300), 0xffff);

	Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0001, 0xffff);
	
	Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
	
}

static void TurnOffDacPower(void)
{
	pr_warn("TurnOffDacPower\n");

	Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0000, 0xffff);	
	if (GetAdcStatus() == false)
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);	

	udelay(250);

	Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0x0040);	
	Ana_Set_Reg(AFE_NCP_CFG0, 0x4600, 0xffff);		

#if 0
	Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0000, 0xffff);	
	Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0000, 0xffff);	
	Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0001, 0xffff);	
#endif
	
	NvregEnable(false);
	ClsqEnable(false);
	Topck_Enable(false);
	audckbufEnable(false);
}

static void HeadsetVoloumeRestore(void)
{
	int index = 0, oldindex = 0, offset = 0, count = 1;

	
	index = 8;

	oldindex = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	if (index > oldindex) {
		pr_warn("%s index = %d oldindex = %d\n", __func__, index, oldindex);
		offset = index - oldindex;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2, (((oldindex + count) << 7) | (oldindex + count)),
				    0xf9f);
			offset--;
			count++;
			udelay(100);
		}
	} else {
		pr_warn("%s index = %d oldindex = %d\n", __func__, index, oldindex);
		offset = oldindex - index;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2, (((oldindex - count) << 7) | (oldindex - count)),
				    0xf9f);
			offset--;
			count++;
			udelay(100);
		}
	}
	Ana_Set_Reg(ZCD_CON2, 0x0489, 0xf9f);
}

static void HeadsetVoloumeSet(void)
{
	int index = 0, oldindex = 0, offset = 0, count = 1;

	
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	oldindex = 8;
	if (index > oldindex) {
		
		offset = index - oldindex;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2, (((oldindex + count) << 7) | (oldindex + count)),
				    0xf9f);
			offset--;
			count++;
			udelay(200);
		}
	} else {
		
		offset = oldindex - index;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2, (((oldindex - count) << 7) | (oldindex - count)),
				    0xf9f);
			offset--;
			count++;
			udelay(200);
		}
	}
	Ana_Set_Reg(ZCD_CON2, (index << 7) | (index), 0xf9f);
}

static void Audio_Amp_Change(int channels, bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();

		
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false
		    && mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    false) {
			pr_warn("%s\n", __func__);

			
			HP_Ana_Switch_to_On();

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
			
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
			
			Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0700, 0xffff);
			
			Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
			
			Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
			
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
			

			udelay(50);

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
			

			
			setHpGainZero();
			SetDcCompenSation();

			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF49F, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4FF, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4EF, 0xffff);
			

			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0300, 0xffff);
			

			
			HP_Ana_Switch_to_Release();

			
			HeadsetVoloumeSet();
		}

	} else {

		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false
		    && mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    false) {
			

			HeadsetVoloumeRestore();
			

			
			
			setHpGainZero();

			
			HP_Ana_Switch_to_On();

			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF40F, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE00F, 0xffff);
			

			EnableDcCompensation(false);

			
			HP_Ana_Switch_to_Release();
		}

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
			

			TurnOffDacPower();
		}
	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpL_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
		htc_aud_hs_switch(HEADSET_L, true);
#endif
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] ==
		       true)) {
#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
		htc_aud_hs_switch(HEADSET_L, false);
#endif
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR];
	return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
		htc_aud_hs_switch(HEADSET_R, true);
#endif
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		       true)) {
#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
		htc_aud_hs_switch(HEADSET_R, false);
#endif
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int PMIC_REG_CLEAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
#if 0
	Ana_Set_Reg(ABB_AFE_CON2, 0, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON4, 0, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON5, 0x28, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON6, 0x218, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON7, 0x204, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON8, 0x0, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON9, 0x0, 0xffff);
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG1, 0x18, 0xffff);
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG3, 0xf872, 0xffff);
#endif
	return 0;
}

static int PMIC_REG_CLEAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s(), not support\n", __func__);

	return 0;
}
#if 0				
static void SetVoiceAmpVolume(void)
{
	int index;

	pr_warn("%s\n", __func__);
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	Ana_Set_Reg(ZCD_CON3, index, 0x001f);
}
#endif

static void Voice_Amp_Change(bool enable)
{
	if (enable) {
		pr_warn("%s\n", __func__);
		if (GetDLStatus() == false) {
			TurnOnDacPower();
			pr_warn("Voice_Amp_Change on amp\n");

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
			
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
			
			Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
			

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0400, 0xffff);
			
			Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0100, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE089, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE109, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE119, 0xffff);
			
#if 1 
			Ana_Set_Reg(ZCD_CON3, 0x001f & mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL], 0xffff);
#else
			Ana_Set_Reg(ZCD_CON3, 0x0009, 0xffff);
#endif 
			
		}
	} else {
		pr_warn("Voice_Amp_Change off amp\n");
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE109, 0xffff);
		

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			

			TurnOffDacPower();
		}
	}
}

static int Voice_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Voice_Amp_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL];
	return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == false)) {
		Voice_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
		Voice_Amp_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();

		pr_warn("%s\n", __func__);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4028, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0800, 0xffff);
		
		Ana_Set_Reg(ZCD_CON1, 0x0F9F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE009, 0xffff);
		

		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4234, 0xffff);
		
		Ana_Set_Reg(ZCD_CON1, 0x0F89, 0xffff);
		

		Apply_Speaker_Gain();
	} else {
		
#ifdef CONFIG_MTK_SPEAKER
#if 0				
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_close();
#endif
#endif
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			

			TurnOffDacPower();
		}
	}
}

static int Speaker_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL];
	return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == false)) {
		Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
		    ucontrol->value.integer.value[0];
		Speaker_Amp_Change(false);
	}
	return 0;
}


#define GAP (2)			
#if defined(CONFIG_MTK_LEGACY)
#define AW8736_MODE3  \
	do { \
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE); \
		udelay(GAP); \
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO); \
		udelay(GAP); \
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE); \
		udelay(GAP); \
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO); \
		udelay(GAP); \
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE); \
	} while (0)
#endif

#define NULL_PIN_DEFINITION    (-1)
static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (35)	
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
	int ret;

#ifndef CONFIG_MTK_SPKGPIO_REWORK
	ret = GetGPIO_Info(5, &pin_extspkamp, &pin_mode_extspkamp);
	if (ret < 0) {
		pr_err("Ext_Speaker_Amp_Change GetGPIO_Info FAIL!!!\n");
		return;
	}
#endif
#endif
	if (enable) {
		pr_debug("Ext_Speaker_Amp_Change ON+\n");
#ifndef CONFIG_MTK_SPEAKER
#if defined(CONFIG_MTK_LEGACY)

		ret = GetGPIO_Info(10, &pin_extspkamp_2, &pin_mode_extspkamp_2);
		
		mt_set_gpio_mode(pin_extspkamp, GPIO_MODE_00);	
		mt_set_gpio_pull_enable(pin_extspkamp, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO);	
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION) {
			mt_set_gpio_mode(pin_extspkamp_2, GPIO_MODE_00);	
			mt_set_gpio_pull_enable(pin_extspkamp_2, GPIO_PULL_ENABLE);
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ZERO);	
		}
#else
#ifndef CONFIG_MTK_SPKGPIO_REWORK
		AudDrv_GPIO_EXTAMP_Select(false, 3);
#else
		if (pin_extspkamp == 0)
			AudDrv_GPIO_EXTAMP_Select(false, 3);
		else
			AudDrv_GPIO_EXTAMP2_Select(false, 3);
#endif
#endif				

		
		usleep_range(1*1000, 1.5*1000);
#if defined(CONFIG_MTK_LEGACY)
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION)
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	

#ifdef AW8736_MODE_CTRL
		AW8736_MODE3;
#else
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE);	
#endif 
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION)
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ONE);	
#else
#ifndef CONFIG_MTK_SPKGPIO_REWORK
		AudDrv_GPIO_EXTAMP_Select(true, 3);
#else
		if (pin_extspkamp == 0)
			AudDrv_GPIO_EXTAMP_Select(true, 3);
		else
			AudDrv_GPIO_EXTAMP2_Select(true, 3);
#endif
#endif 
		msleep(SPK_WARM_UP_TIME);
#endif
		
	} else {
		pr_debug("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER
#if defined(CONFIG_MTK_LEGACY)
		ret = GetGPIO_Info(10, &pin_extspkamp_2, &pin_mode_extspkamp_2);
		
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO);	
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION) {
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ZERO);	
		}
#else
#ifndef CONFIG_MTK_SPKGPIO_REWORK
		AudDrv_GPIO_EXTAMP_Select(false, 3);
#else
		if (pin_extspkamp == 0)
			AudDrv_GPIO_EXTAMP_Select(false, 3);
		else
			AudDrv_GPIO_EXTAMP2_Select(false, 3);
#endif
#endif
		
#endif
		
	}
#endif
}


static int Ext_Speaker_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP];
	return 0;
}

static int Ext_Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

	
	if (ucontrol->value.integer.value[0]) {
		Ext_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] =
		    ucontrol->value.integer.value[0];
		Ext_Speaker_Amp_Change(false);
	}
	return 0;
}

static void Receiver_Speaker_Switch_Change(bool enable)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
#if defined(CONFIG_MTK_LEGACY)

	int ret;

	pr_debug("%s\n", __func__);

	if (enable) {
		pr_debug("switch to receiver\n");
		ret = GetGPIO_Info(11, &pin_rcvspkswitch, &pin_mode_rcvspkswitch);
		if (ret < 0) {
			pr_err("Receiver_Speaker_Switch_Change GetGPIO_Info FAIL!!!\n");
			return;
		}
		mt_set_gpio_mode(pin_rcvspkswitch, GPIO_MODE_00);
		mt_set_gpio_pull_enable(pin_rcvspkswitch, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(pin_rcvspkswitch, GPIO_DIR_OUT);	
		mt_set_gpio_out(pin_rcvspkswitch, GPIO_OUT_ZERO);	

	} else {
		pr_debug("switch to speaker\n");
		ret = GetGPIO_Info(11, &pin_rcvspkswitch, &pin_mode_rcvspkswitch);
		if (ret < 0) {
			pr_err("Receiver_Speaker_Switch_Change GetGPIO_Info FAIL!!!\n");
			return;
		}
		mt_set_gpio_mode(pin_rcvspkswitch, GPIO_MODE_00);
		mt_set_gpio_pull_enable(pin_rcvspkswitch, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(pin_rcvspkswitch, GPIO_DIR_OUT);	
		mt_set_gpio_out(pin_rcvspkswitch, GPIO_OUT_ONE);	

	}
#else
	pr_debug("%s\n", __func__);

	if (enable)
		AudDrv_GPIO_RCVSPK_Select(true);
	else
		AudDrv_GPIO_RCVSPK_Select(false);
#endif
#endif
#endif
}

static int Receiver_Speaker_Switch_Get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() : %d\n", __func__,
		 mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH];
	return 0;
}

static int Receiver_Speaker_Switch_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] ==
		false)) {
		Receiver_Speaker_Switch_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->
		       mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] =
		    ucontrol->value.integer.value[0];
		Receiver_Speaker_Switch_Change(false);
	}
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();

		pr_warn("%s\n", __func__);

		
		HP_Ana_Switch_to_On();

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4028, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0F00, 0xffff);
		
		Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
		
		Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		
		Ana_Set_Reg(ZCD_CON1, 0x0F9F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
		

		udelay(50);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
		

		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4234, 0xffff);
		

		
		setHpGainZero();
		SetDcCompenSation();
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEA9F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEAFF, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEAEF, 0xffff);
		

		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0B00, 0xffff);
		

		
		HP_Ana_Switch_to_Release();

		
		HeadsetVoloumeSet();
		Apply_Speaker_Gain();
	} else {
		HeadsetVoloumeRestore();
		
		setHpGainZero();

		HP_Ana_Switch_to_On();
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEA0F, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE00F, 0xffff);
		
		HP_Ana_Switch_to_Release();
		

		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			

			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
			

			TurnOffDacPower();
		}
		EnableDcCompensation(false);
	}
}


static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R];
	return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	

	
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] ==
		false)) {
		Headset_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->
		       mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] == true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] =
		    ucontrol->value.integer.value[0];
		Headset_Speaker_Amp_Change(false);
	}
	return 0;
}

#ifdef CONFIG_MTK_SPEAKER
static const char *const speaker_amp_function[] = { "CALSSD", "CLASSAB", "RECEIVER" };

static const char *const speaker_PGA_function[] = {
	"4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};
static const char *const speaker_OC_function[] = { "Off", "On" };
static const char *const speaker_CS_function[] = { "Off", "On" };
static const char *const speaker_CSPeakDetecReset_function[] = { "Off", "On" };

static int Audio_Speaker_Class_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	Speaker_mode = ucontrol->value.integer.value[0];
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_Speaker_Class_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = Speaker_mode;
	return 0;
}

static int Audio_Speaker_Pga_Gain_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	Speaker_pga_gain = ucontrol->value.integer.value[0];

	pr_warn("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);
	Ana_Set_Reg(SPK_ANA_CON0, Speaker_pga_gain << 11, 0x7800);
	return 0;
}

static int Audio_Speaker_OcFlag_Get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	mSpeaker_Ocflag = GetSpeakerOcFlag();
	ucontrol->value.integer.value[0] = mSpeaker_Ocflag;
	return 0;
}

static int Audio_Speaker_OcFlag_Set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s is not support setting\n", __func__);
	return 0;
}

static int Audio_Speaker_Pga_Gain_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = Speaker_pga_gain;
	return 0;
}

static int Audio_Speaker_Current_Sensing_Set(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{

	if (ucontrol->value.integer.value[0])
		Ana_Set_Reg(SPK_CON12, 0x9300, 0xff00);
	else
		Ana_Set_Reg(SPK_CON12, 0x1300, 0xff00);

	return 0;
}

static int Audio_Speaker_Current_Sensing_Get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = (Ana_Get_Reg(SPK_CON12) >> 15) & 0x01;
	return 0;
}

static int Audio_Speaker_Current_Sensing_Peak_Detector_Set(struct snd_kcontrol *kcontrol,
							   struct snd_ctl_elem_value *ucontrol)
{

	if (ucontrol->value.integer.value[0])
		Ana_Set_Reg(SPK_CON12, 1 << 14, 1 << 14);
	else
		Ana_Set_Reg(SPK_CON12, 0, 1 << 14);

	return 0;
}

static int Audio_Speaker_Current_Sensing_Peak_Detector_Get(struct snd_kcontrol *kcontrol,
							   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = (Ana_Get_Reg(SPK_CON12) >> 14) & 0x01;
	return 0;
}


static const struct soc_enum Audio_Speaker_Enum[] = {
	
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_amp_function),
			    speaker_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_PGA_function),
			    speaker_PGA_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_OC_function),
			    speaker_OC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CS_function),
			    speaker_CS_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CSPeakDetecReset_function),
			    speaker_CSPeakDetecReset_function),
};

static const struct snd_kcontrol_new mt6331_snd_Speaker_controls[] = {
	SOC_ENUM_EXT("Audio_Speaker_class_Switch", Audio_Speaker_Enum[0],
		     Audio_Speaker_Class_Get,
		     Audio_Speaker_Class_Set),
	SOC_ENUM_EXT("Audio_Speaker_PGA_gain", Audio_Speaker_Enum[1],
		     Audio_Speaker_Pga_Gain_Get,
		     Audio_Speaker_Pga_Gain_Set),
	SOC_ENUM_EXT("Audio_Speaker_OC_Falg", Audio_Speaker_Enum[2],
		     Audio_Speaker_OcFlag_Get,
		     Audio_Speaker_OcFlag_Set),
	SOC_ENUM_EXT("Audio_Speaker_CurrentSensing", Audio_Speaker_Enum[3],
		     Audio_Speaker_Current_Sensing_Get,
		     Audio_Speaker_Current_Sensing_Set),
	SOC_ENUM_EXT("Audio_Speaker_CurrentPeakDetector",
		     Audio_Speaker_Enum[4],
		     Audio_Speaker_Current_Sensing_Peak_Detector_Get,
		     Audio_Speaker_Current_Sensing_Peak_Detector_Set),
};

int Audio_AuxAdcData_Get_ext(void)
{
	int dRetValue = PMIC_IMM_GetOneChannelValue(AUX_ICLASSAB_AP, 1, 0);

	pr_warn("%s dRetValue 0x%x\n", __func__, dRetValue);
	return dRetValue;
}

#endif

static int Audio_AuxAdcData_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

#ifdef CONFIG_MTK_SPEAKER
	ucontrol->value.integer.value[0] = Audio_AuxAdcData_Get_ext();
	
#else
	ucontrol->value.integer.value[0] = 0;
#endif
	pr_warn("%s dMax = 0x%lx\n", __func__, ucontrol->value.integer.value[0]);
	return 0;

}

static int Audio_AuxAdcData_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	dAuxAdcChannel = ucontrol->value.integer.value[0];
	pr_warn("%s dAuxAdcChannel = 0x%x\n", __func__, dAuxAdcChannel);
	return 0;
}


static const struct snd_kcontrol_new Audio_snd_auxadc_controls[] = {
	SOC_SINGLE_EXT("Audio AUXADC Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_AuxAdcData_Get,
		       Audio_AuxAdcData_Set),
};


static const char *const amp_function[] = { "Off", "On" };
static const char *const aud_clk_buf_function[] = { "Off", "On" };

static const char *const DAC_DL_PGA_Headset_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db",
	"-1Db", "-2Db", "-3Db",
	"-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

static const char *const DAC_DL_PGA_Handset_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db",
	"-1Db", "-2Db", "-3Db",
	"-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

static const char *const DAC_DL_PGA_Speaker_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db",
	"-1Db", "-2Db", "-3Db",
	"-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};


static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL];
	return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];

	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON1, index, 0x001f);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL] = index;
	return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR];
	return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];

	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON1, index << 7, 0x0f80);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR] = index;
	return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	int index = 0;

	

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];

	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON3, index, 0x001f);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	int index = 0;


	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];

	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON2, index, 0x001f);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	return 0;
}


static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	int index = 0;

	

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];

	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON2, index << 7, 0x0f80);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static uint32 mHp_Impedance = 32;

static int Audio_Hp_Impedance_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_Hp_Impedance_Get = %d\n", mHp_Impedance);
	ucontrol->value.integer.value[0] = mHp_Impedance;
	return 0;

}

static int Audio_Hp_Impedance_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mHp_Impedance = ucontrol->value.integer.value[0];
	pr_warn("%s Audio_Hp_Impedance_Set = 0x%x\n", __func__, mHp_Impedance);
	return 0;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("\%s n", __func__);
	ucontrol->value.integer.value[0] = audck_buf_Count;
	return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	

	if (ucontrol->value.integer.value[0])
		audckbufEnable(true);
	else
		audckbufEnable(false);

	return 0;
}


static const struct soc_enum Audio_DL_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN),
			    DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN),
			    DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN),
			    DAC_DL_PGA_Handset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN),
			    DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN),
			    DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aud_clk_buf_function),
			    aud_clk_buf_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
};

static const struct snd_kcontrol_new mt6331_snd_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_R_Switch", Audio_DL_Enum[0], Audio_AmpR_Get,
		     Audio_AmpR_Set),
	SOC_ENUM_EXT("Audio_Amp_L_Switch", Audio_DL_Enum[1], Audio_AmpL_Get,
		     Audio_AmpL_Set),
	SOC_ENUM_EXT("Voice_Amp_Switch", Audio_DL_Enum[2], Voice_Amp_Get,
		     Voice_Amp_Set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", Audio_DL_Enum[3],
		     Speaker_Amp_Get, Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", Audio_DL_Enum[4],
		     Headset_Speaker_Amp_Get,
		     Headset_Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_PGAL_GAIN", Audio_DL_Enum[5],
		     Headset_PGAL_Get, Headset_PGAL_Set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", Audio_DL_Enum[6],
		     Headset_PGAR_Get, Headset_PGAR_Set),
	SOC_ENUM_EXT("Handset_PGA_GAIN", Audio_DL_Enum[7], Handset_PGA_Get,
		     Handset_PGA_Set),
	SOC_ENUM_EXT("Lineout_PGAR_GAIN", Audio_DL_Enum[8],
		     Lineout_PGAR_Get, Lineout_PGAR_Set),
	SOC_ENUM_EXT("Lineout_PGAL_GAIN", Audio_DL_Enum[9],
		     Lineout_PGAL_Get, Lineout_PGAL_Set),
	SOC_ENUM_EXT("AUD_CLK_BUF_Switch", Audio_DL_Enum[10],
		     Aud_Clk_Buf_Get, Aud_Clk_Buf_Set),
	SOC_ENUM_EXT("Ext_Speaker_Amp_Switch", Audio_DL_Enum[11],
		     Ext_Speaker_Amp_Get,
		     Ext_Speaker_Amp_Set),
	SOC_ENUM_EXT("Receiver_Speaker_Switch", Audio_DL_Enum[11],
		     Receiver_Speaker_Switch_Get,
		     Receiver_Speaker_Switch_Set),
	SOC_SINGLE_EXT("Audio HP Impedance", SND_SOC_NOPM, 0, 512, 0,
		       Audio_Hp_Impedance_Get,
		       Audio_Hp_Impedance_Set),
	SOC_ENUM_EXT("PMIC_REG_CLEAR", Audio_DL_Enum[12], PMIC_REG_CLEAR_Get, PMIC_REG_CLEAR_Set),

};

static const struct snd_kcontrol_new mt6331_Voice_Switch[] = {
	
};

void SetMicPGAGain(void)
{
	int index = 0;

	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	
	Ana_Set_Reg(AUDENC_ANA_CON0, index << 8, 0x0700);
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);

}

static bool GetAdcStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		if ((mCodec_data->mAudio_Ana_DevicePower[i] == true)
		    && (i != AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH))
			return true;
	}
	return false;
}

static bool GetDacStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_OUT_EARPIECER; i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}


static bool TurnOnADcPowerACC(int ADCType, bool enable)
{
	bool refmic_using_ADC_L;

	refmic_using_ADC_L = false;

	pr_warn("%s ADCType = %d enable = %d, refmic_using_ADC_L=%d\n", __func__, ADCType,
	       enable, refmic_using_ADC_L);

	if (enable) {
		
		uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
		

		if (GetAdcStatus() == false) {
			audckbufEnable(true);

			Ana_Set_Reg(LDO_VUSB33_CON0, 0x1A62, 0x000A);
			Ana_Set_Reg(LDO_VA18_CON0, 0x1A62, 0x000A);

			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400);
			
			NvregEnable(true);
			
			
			
			ClsqEnable(true);
			

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x0070);
			
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000C);
			
		}

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {	
			pr_warn("%s  AUDIO_ANALOG_DEVICE_IN_ADC1 mux =%d\n",
				__func__, mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic1_mode);
				
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00f1);
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				if (GetMicbias == 0) {
					MicbiasRef = Ana_Get_Reg(AUDENC_ANA_CON10) & 0x0070;
					
					pr_warn("ACCDET Micbias1Ref=0x%x\n", MicbiasRef);
					GetMicbias = 1;
				}
				
				SetDCcoupleNP(AUDIO_MIC_BIAS1, mAudio_Analog_Mic1_mode);
				
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0001, 0x0081);
				
			}
			
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0300, 0x0f00);	

		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {	
			pr_warn
			    ("%s  AUDIO_ANALOG_DEVICE_IN_ADC2 refmic_using_ADC_L =%d\n",
			     __func__, refmic_using_ADC_L);
			SetDCcoupleNP(AUDIO_MIC_BIAS2, mAudio_Analog_Mic2_mode);
			
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x2100, 0xf100);
			

			if (refmic_using_ADC_L == false) {
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0300, 0x0f00);
				
			} else {
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0300, 0x0f00);
				
			}
		}

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {	

			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0311, 0x003f);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5311, 0xf000);
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0221, 0x003f);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5221, 0xf000);
				

			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			

			if (refmic_using_ADC_L == false) {
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0331, 0x003f);
				
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x5331, 0xf000);
				
			} else {
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0331, 0x003f);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5331, 0xf000);
				
			}
		}

		SetMicPGAGain();

		if (GetAdcStatus() == false) {
			
			Topck_Enable(true);
			
			if (GetDacStatus() == false) {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0xffff);
				
			} else {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
				
			}
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
			
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
			

			Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1)
							 << 1), 0x000E);
			
			Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0001, 0xffff);
			
		}
	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0x0001);
			
			Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0x0020);
			
		}
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0311, 0xf000);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0221, 0xf000);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				
				
			}

			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x00ff);
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0001);
				
				GetMicbias = 0;
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {	
			if (refmic_using_ADC_L == false) {
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0331, 0xf000);
				
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0fff);
				
				
			} else {
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0331, 0xf000);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				
				
			}

			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xff00);
			
		}
		if (GetAdcStatus() == false) {
			
			
			
			

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000f);
			
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0x0070);
			

			if (GetDLStatus() == false) {
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x00C4, 0x00C4);
				
			}

			
			Topck_Enable(false);
			
			ClsqEnable(false);
			NvregEnable(false);
			audckbufEnable(false);
		}

	}
	return true;
}

static bool TurnOnADcPowerDmic(int ADCType, bool enable)
{
	pr_warn("%s ADCType = %d enable = %d\n", __func__, ADCType, enable);
	if (enable) {
		uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
		uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
		

		if (GetAdcStatus() == false) {
			audckbufEnable(true);
			Ana_Set_Reg(LDO_VUSB33_CON0, 0x1A62, 0x000A);
			Ana_Set_Reg(LDO_VA18_CON0, 0x1A62, 0x000A);

			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400);
			
			NvregEnable(true);
			
			ClsqEnable(true);

			SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic1_mode);
			
			
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00ff);
			
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0005, 0x0007);
			

			
			Topck_Enable(true);
			
			if (GetDacStatus() == false) {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0xffff);
				
				
			} else {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
				
			}
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, (ULIndex << 7) | (ULIndex << 6), 0xffff);
			
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
			

			Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1)
							 << 1), 0x000E);
			
			Ana_Set_Reg(AFE_UL_SRC0_CON0_H, 0x00E0, 0xfff0);
			
			Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0003, 0xffff);
			
		}
	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0xffff);
			
			Ana_Set_Reg(AFE_UL_SRC0_CON0_H, 0x0000, 0xfff0);
			
			Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0x0020);
			

			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0004, 0x0007);
			

			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x00ff);
			
			if (GetDLStatus() == false) {
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x00C4, 0x00C4);
				
			}

			
			Topck_Enable(false);
			
			ClsqEnable(false);
			NvregEnable(false);
			audckbufEnable(false);
		}
	}
	return true;
}

static bool TurnOnADcPowerDCC(int ADCType, bool enable, int ECMmode)
{
	pr_warn("%s ADCType = %d enable = %d\n", __func__, ADCType, enable);
	if (enable) {
		
		uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
		

		if (GetAdcStatus() == false) {
			audckbufEnable(true);

			Ana_Set_Reg(LDO_VUSB33_CON0, 0x1A62, 0x000A);
			Ana_Set_Reg(LDO_VA18_CON0, 0x1A62, 0x000A);

			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400);
			
			NvregEnable(true);
			
			
			
			ClsqEnable(true);
			

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x0070);
			
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000C);
			

			
			Ana_Set_Reg(TOP_CKPDN_CON0, 0x0000, 0x6000);
			
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
			
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);
			

			
			
		}
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {	
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic1_mode);
				
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00f1);
				
#if 0
				if (ECMmode == 1) {	
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x000E, 0x000E);
					

				} else if (ECMmode == 2) {	
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x0002, 0x000E);
					
				} else {	
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x000E);
					
				}
#endif
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0317, 0x073f);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5317, 0xf000);
				
				udelay(100);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5313, 0x0004);
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				
				if (GetMicbias == 0) {
					MicbiasRef = Ana_Get_Reg(AUDENC_ANA_CON10) & 0x0070;
					
					pr_warn("ACCDET Micbias1Ref=0x%x\n", MicbiasRef);
					GetMicbias = 1;
				}
				SetDCcoupleNP(AUDIO_MIC_BIAS1, mAudio_Analog_Mic1_mode);
				
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0001, 0x0081);
				
#if 0
				if (ECMmode == 1) {	
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
					
				} else if (ECMmode == 2) {	
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
					
				} else {	
					
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0006);
					
				}
#endif
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0327, 0x073f);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5327, 0xf000);
				
				udelay(100);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5323, 0x0004);
				
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {	
			SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic2_mode);
			
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x2100, 0xf100);
			
#if 0
			if (ECMmode == 1) {	
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0717, 0xff1f);
				

			} else if (ECMmode == 2) {	
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0713, 0xff1f);
				
			} else {	
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0711, 0xff1f);
				
			}
#endif
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0300, 0x0f00);
			
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0337, 0x003f);
			
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x5337, 0xf000);
			
			udelay(100);
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x5333, 0x0004);
			
		}

		SetMicPGAGain();

		if (GetAdcStatus() == false) {
			
			Topck_Enable(true);
			

			
			if (GetDacStatus() == false) {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0xffff);
				
			} else {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
				
			}

			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
			
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
			

			Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (ULSampleRateTransform(SampleRate_VUL1)
							 << 1), 0x000E);
			
			Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0001, 0xffff);	
		}
#if 0
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);	
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
#endif
	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0xffff);
			
			Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0x0020);
			
		}
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0313, 0xf000);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0323, 0xf000);
				
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				
				
			}

			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x00ff);
				
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0001);
				
				GetMicbias = 0;
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0333, 0xf000);
			
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0fff);
			
			

			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xff00);
			
		}

		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0x0001);
			
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x0fe2, 0xffff);
			

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000f);
			
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0x0070);
			

			if (GetDLStatus() == false) {
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x00C4, 0x00C4);
				
			}

			
			Topck_Enable(false);
			
			ClsqEnable(false);
			NvregEnable(false);
			audckbufEnable(false);
		}
	}
	return true;
}


static bool TurnOnVOWDigitalHW(bool enable)
{
	return false;
}

static bool TurnOnVOWADcPowerACC(int MicType, bool enable)
{
	return false;
}

#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
void EnableMicbias1(bool enabled)
{
	int32 old_value;
	uint32 reg_value;

	mutex_lock(&Ana_Power_Mutex);

	old_value = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = 1; 

	reg_value = Ana_Get_Reg(AUDDEC_ANA_CON1);
	if ((reg_value & 0x2000) == 0x0)
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000|reg_value, 0xffff); 
	TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, enabled, 0);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = old_value;

	mutex_unlock(&Ana_Power_Mutex);
}

int isHeadsetAmpOn(bool isL)
{
	int isOn;
	if (isL == HEADSET_L) {
		isOn = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL];
	} else {
		isOn = mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR];
	}
	return isOn;
}
#endif

static const char *const ADC_function[] = { "Off", "On" };
static const char *const ADC_power_mode[] = { "normal", "lowpower" };
static const char *const PreAmp_Mux_function[] = { "OPEN", "IN_ADC1", "IN_ADC2", "IN_ADC3" };

static const char *const ADC_UL_PGA_GAIN[] = { "0Db", "6Db", "12Db", "18Db", "24Db", "30Db" };
static const char *const Pmic_Digital_Mux[] = { "ADC1", "ADC2", "ADC3", "ADC4" };
static const char *const Adc_Input_Sel[] = { "idle", "AIN", "Preamp" };

static const char *const Audio_AnalogMic_Mode[] = {
	"ACCMODE", "DCCMODE", "DMIC", "DCCECMDIFFMODE", "DCCECMSINGLEMODE"
};
static const char *const Audio_VOW_ADC_Function[] = { "Off", "On" };
static const char *const Audio_VOW_Digital_Function[] = { "Off", "On" };

static const char *const Audio_VOW_MIC_Type[] = {
	"HandsetAMIC", "HeadsetMIC", "HandsetDMIC", "HandsetDMIC_800K", "HandsetAMIC_DCC",
	"HeadsetMIC_DCC", "HandsetAMIC_DCCECM", "HeadsetMIC_DCCECM" };

static const char * const Pmic_Test_function[] = { "Off", "On" };
static const char * const Pmic_LPBK_function[] = { "Off", "LPBK3_DL44_UL48", "LPBK3_DL48_UL48" };

static const struct soc_enum Audio_UL_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function), PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_power_mode), ADC_power_mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_ADC_Function), Audio_VOW_ADC_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function), PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_Digital_Function), Audio_VOW_Digital_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_MIC_Type), Audio_VOW_MIC_Type),
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_ADC1_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1];
	return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true, 0);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true, 1);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true, 2);

		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false, 0);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false, 1);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false, 2);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_ADC2_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2];
	return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true, 0);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true, 1);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true, 2);

		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false, 0);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false, 1);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false, 2);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC1_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1];
	return 0;
}

static int Audio_ADC1_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x00 << 13), 0x6000);	
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x01 << 13), 0x6000);	
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x02 << 13), 0x6000);	
	else
		pr_warn("%s() warning\n ", __func__);

	pr_warn("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC2_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2];
	return 0;
}

static int Audio_ADC2_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x00 << 13), 0x6000);	
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x01 << 13), 0x6000);	
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x02 << 13), 0x6000);	
	else
		pr_warn("%s() warning\n ", __func__);

	pr_warn("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC3_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC3_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC4_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_ADC4_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}


static bool AudioPreAmp1_Sel(int Mul_Sel)
{
	
	if (Mul_Sel == 0)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0030);	
	else if (Mul_Sel == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0010, 0x0030);	
	else if (Mul_Sel == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0020, 0x0030);	
	else if (Mul_Sel == 3)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0030, 0x0030);	
	else
		pr_warn("AudioPreAmp1_Sel warning");

	return true;
}


static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]; = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
	return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	
	return 0;
}

static bool AudioPreAmp2_Sel(int Mul_Sel)
{
	

	if (Mul_Sel == 0)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0030);	
	else if (Mul_Sel == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0010, 0x0030);	
	else if (Mul_Sel == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0020, 0x0030);	 
	else if (Mul_Sel == 3)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0030, 0x0030);	
	else
		pr_warn("AudioPreAmp1_Sel warning");

	return true;
}


static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]; = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2];
	return 0;
}

static int Audio_PreAmp2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp2_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	
	return 0;
}

static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n",
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON0, (index << 8), 0x0700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_PGA2_Get = %d\n",
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON1, (index << 8), 0x0700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_PGA3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_PGA4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_PGA4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_MicSource1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_MicSource1_Get = %d\n",
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
	return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	int index = 0;

	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_warn("%s() index = %d done\n", __func__, index);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_MicSource2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_MicSource2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_MicSource3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_MicSource3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}


static int Audio_MicSource4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_MicSource4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	return 0;
}

static int Audio_Mic1_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() mAudio_Analog_Mic1_mode = %d\n", __func__, mAudio_Analog_Mic1_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
	return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic1_mode = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Mic2_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Analog_Mic2_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
	return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic2_mode = ucontrol->value.integer.value[0];
	return 0;
}


static int Audio_Mic3_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Analog_Mic3_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
	return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic3_mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Analog_Mic3_mode = %d\n", __func__, mAudio_Analog_Mic3_mode);
	return 0;
}

static int Audio_Mic4_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Analog_Mic4_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
	return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Analog_Mic4_mode = %d\n", __func__, mAudio_Analog_Mic4_mode);
	return 0;
}

static int Audio_Adc_Power_Mode_Get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAdc_Power_Mode);
	ucontrol->value.integer.value[0] = mAdc_Power_Mode;
	return 0;
}

static int Audio_Adc_Power_Mode_Set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_power_mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAdc_Power_Mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAdc_Power_Mode = %d\n", __func__, mAdc_Power_Mode);
	return 0;
}


static int Audio_Vow_ADC_Func_Switch_Get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Vow_Analog_Func_Enable);
	ucontrol->value.integer.value[0] = mAudio_Vow_Analog_Func_Enable;
	return 0;
}

static int Audio_Vow_ADC_Func_Switch_Set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_ADC_Function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWADcPowerACC(mAudio_VOW_Mic_type, true);
	else
		TurnOnVOWADcPowerACC(mAudio_VOW_Mic_type, false);


	mAudio_Vow_Analog_Func_Enable = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Vow_Analog_Func_Enable = %d\n", __func__,
	       mAudio_Vow_Analog_Func_Enable);
	return 0;
}

static int Audio_Vow_Digital_Func_Switch_Get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Vow_Digital_Func_Enable);
	ucontrol->value.integer.value[0] = mAudio_Vow_Digital_Func_Enable;
	return 0;
}

static int Audio_Vow_Digital_Func_Switch_Set(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_Digital_Function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWDigitalHW(true);
	else
		TurnOnVOWDigitalHW(false);


	mAudio_Vow_Digital_Func_Enable = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Vow_Digital_Func_Enable = %d\n", __func__,
	       mAudio_Vow_Digital_Func_Enable);
	return 0;
}


static int Audio_Vow_MIC_Type_Select_Get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_VOW_Mic_type);
	ucontrol->value.integer.value[0] = mAudio_VOW_Mic_type;
	return 0;
}

static int Audio_Vow_MIC_Type_Select_Set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_MIC_Type)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_VOW_Mic_type = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_VOW_Mic_type = %d\n", __func__, mAudio_VOW_Mic_type);
	return 0;
}


static int Audio_Vow_Cfg0_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value =  reg_AFE_VOW_CFG0;

	
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg0_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, (int)(ucontrol->value.integer.value[0]));
	
	reg_AFE_VOW_CFG0 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value =  reg_AFE_VOW_CFG1;

	
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	
	reg_AFE_VOW_CFG1 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value =  reg_AFE_VOW_CFG2;

	
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	
	reg_AFE_VOW_CFG2 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value =  reg_AFE_VOW_CFG3;

	
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	
	reg_AFE_VOW_CFG3 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value =  reg_AFE_VOW_CFG4;

	
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	
	reg_AFE_VOW_CFG4 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg5_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value =  reg_AFE_VOW_CFG5;

	
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg5_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	
	reg_AFE_VOW_CFG5 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_State_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = mIsVOWOn;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_State_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	
	return 0;
}

static bool SineTable_DAC_HP_flag;
static bool SineTable_UL2_flag;

static int SineTable_UL2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);	
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x2);	
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0000, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	}
	return 0;
}

static int32 Pmic_Loopback_Type;

static int Pmic_Loopback_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Pmic_Loopback_Type;
	return 0;
}

static int Pmic_Loopback_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_LPBK_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0) {
		
		Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0000, 0x0005);
		
		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0000, 0x0001);
		
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
		
		Topck_Enable(false);
		ClsqEnable(false);
		audckbufEnable(false);
	} else if (ucontrol->value.integer.value[0] > 0) {
		
		audckbufEnable(true);
		ClsqEnable(true);
		Topck_Enable(true);

		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
		

		
		Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffffffff);
		
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffffffff);
		

		if (ucontrol->value.integer.value[0] == 1) {
			
			Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0, 0x7330, 0xffffffff);
			
			Ana_Set_Reg(AFE_DL_SRC2_CON0_H, 0x7300, 0xffffffff);
			
		} else if (ucontrol->value.integer.value[0] == 2) {
			
			Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0, 0x8330, 0xffffffff);
			
			Ana_Set_Reg(AFE_DL_SRC2_CON0_H, 0x8300, 0xffffffff);
			
		}

		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0001, 0x0001);
		
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x0001);
		

		
		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x0c00, 0x0c00);
		
		Ana_Set_Reg(AFE_UL_SRC0_CON0_H, (0x3 << 3 | 0x3 << 1) , 0x001E);
		
		Ana_Set_Reg(AFE_UL_SRC0_CON0_L, 0x0005, 0xffff);
		
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x0002);
		
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);
		
	}

	pr_warn("%s() done\n", __func__);
	Pmic_Loopback_Type = ucontrol->value.integer.value[0];
	return 0;
}
static int SineTable_UL2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_UL2_flag;
	return 0;
}

static int SineTable_DAC_HP_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_DAC_HP_flag;
	return 0;
}

static int SineTable_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	
	pr_warn("%s()\n", __func__);
	return 0;
}

static void ADC_LOOP_DAC_Func(int command)
{
	
}

static bool DAC_LOOP_DAC_HS_flag;
static int ADC_LOOP_DAC_HS_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HS_flag;
	return 0;
}

static int ADC_LOOP_DAC_HS_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		DAC_LOOP_DAC_HS_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON);
	} else {
		DAC_LOOP_DAC_HS_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HS_OFF);
	}
	return 0;
}

static bool DAC_LOOP_DAC_HP_flag;
static int ADC_LOOP_DAC_HP_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HP_flag;
	return 0;
}

static int ADC_LOOP_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

	pr_warn("%s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		DAC_LOOP_DAC_HP_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON);
	} else {
		DAC_LOOP_DAC_HP_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HP_OFF);
	}
	return 0;
}

static bool Voice_Call_DAC_DAC_HS_flag;
static int Voice_Call_DAC_DAC_HS_Get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Voice_Call_DAC_DAC_HS_flag;
	return 0;
}

static int Voice_Call_DAC_DAC_HS_Set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	
	pr_warn("%s()\n", __func__);
	return 0;
}

static const struct soc_enum Pmic_Test_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_LPBK_function), Pmic_LPBK_function),
};

static const struct snd_kcontrol_new mt6331_pmic_Test_controls[] = {
	SOC_ENUM_EXT("SineTable_DAC_HP", Pmic_Test_Enum[0], SineTable_DAC_HP_Get,
		     SineTable_DAC_HP_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HS", Pmic_Test_Enum[1], ADC_LOOP_DAC_HS_Get,
		     ADC_LOOP_DAC_HS_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HP", Pmic_Test_Enum[2], ADC_LOOP_DAC_HP_Get,
		     ADC_LOOP_DAC_HP_Set),
	SOC_ENUM_EXT("Voice_Call_DAC_DAC_HS", Pmic_Test_Enum[3], Voice_Call_DAC_DAC_HS_Get,
		     Voice_Call_DAC_DAC_HS_Set),
	SOC_ENUM_EXT("SineTable_UL2", Pmic_Test_Enum[4], SineTable_UL2_Get, SineTable_UL2_Set),
	SOC_ENUM_EXT("Pmic_Loopback", Pmic_Test_Enum[5], Pmic_Loopback_Get, Pmic_Loopback_Set),
};

static const struct snd_kcontrol_new mt6331_UL_Codec_controls[] = {
	SOC_ENUM_EXT("Audio_ADC_1_Switch", Audio_UL_Enum[0], Audio_ADC1_Get,
		     Audio_ADC1_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", Audio_UL_Enum[1], Audio_ADC2_Get,
		     Audio_ADC2_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Switch", Audio_UL_Enum[2], Audio_ADC3_Get,
		     Audio_ADC3_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Switch", Audio_UL_Enum[3], Audio_ADC4_Get,
		     Audio_ADC4_Set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", Audio_UL_Enum[4],
		     Audio_PreAmp1_Get,
		     Audio_PreAmp1_Set),
	SOC_ENUM_EXT("Audio_ADC_1_Sel", Audio_UL_Enum[5],
		     Audio_ADC1_Sel_Get, Audio_ADC1_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Sel", Audio_UL_Enum[6],
		     Audio_ADC2_Sel_Get, Audio_ADC2_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Sel", Audio_UL_Enum[7],
		     Audio_ADC3_Sel_Get, Audio_ADC3_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Sel", Audio_UL_Enum[8],
		     Audio_ADC4_Sel_Get, Audio_ADC4_Sel_Set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", Audio_UL_Enum[9], Audio_PGA1_Get,
		     Audio_PGA1_Set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", Audio_UL_Enum[10],
		     Audio_PGA2_Get, Audio_PGA2_Set),
	SOC_ENUM_EXT("Audio_PGA3_Setting", Audio_UL_Enum[11],
		     Audio_PGA3_Get, Audio_PGA3_Set),
	SOC_ENUM_EXT("Audio_PGA4_Setting", Audio_UL_Enum[12],
		     Audio_PGA4_Get, Audio_PGA4_Set),
	SOC_ENUM_EXT("Audio_MicSource1_Setting", Audio_UL_Enum[13],
		     Audio_MicSource1_Get,
		     Audio_MicSource1_Set),
	SOC_ENUM_EXT("Audio_MicSource2_Setting", Audio_UL_Enum[14],
		     Audio_MicSource2_Get,
		     Audio_MicSource2_Set),
	SOC_ENUM_EXT("Audio_MicSource3_Setting", Audio_UL_Enum[15],
		     Audio_MicSource3_Get,
		     Audio_MicSource3_Set),
	SOC_ENUM_EXT("Audio_MicSource4_Setting", Audio_UL_Enum[16],
		     Audio_MicSource4_Get,
		     Audio_MicSource4_Set),
	SOC_ENUM_EXT("Audio_MIC1_Mode_Select", Audio_UL_Enum[17],
		     Audio_Mic1_Mode_Select_Get,
		     Audio_Mic1_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC2_Mode_Select", Audio_UL_Enum[18],
		     Audio_Mic2_Mode_Select_Get,
		     Audio_Mic2_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC3_Mode_Select", Audio_UL_Enum[19],
		     Audio_Mic3_Mode_Select_Get,
		     Audio_Mic3_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC4_Mode_Select", Audio_UL_Enum[20],
		     Audio_Mic4_Mode_Select_Get,
		     Audio_Mic4_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_Mic_Power_Mode", Audio_UL_Enum[21],
		     Audio_Adc_Power_Mode_Get,
		     Audio_Adc_Power_Mode_Set),
	SOC_ENUM_EXT("Audio_Vow_ADC_Func_Switch", Audio_UL_Enum[22],
		     Audio_Vow_ADC_Func_Switch_Get,
		     Audio_Vow_ADC_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", Audio_UL_Enum[23],
		     Audio_PreAmp2_Get,
		     Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_Vow_Digital_Func_Switch", Audio_UL_Enum[24],
		     Audio_Vow_Digital_Func_Switch_Get,
		     Audio_Vow_Digital_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Vow_MIC_Type_Select", Audio_UL_Enum[25],
		     Audio_Vow_MIC_Type_Select_Get,
		     Audio_Vow_MIC_Type_Select_Set),
	SOC_SINGLE_EXT("Audio VOWCFG0 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg0_Get,
		       Audio_Vow_Cfg0_Set),
	SOC_SINGLE_EXT("Audio VOWCFG1 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg1_Get,
		       Audio_Vow_Cfg1_Set),
	SOC_SINGLE_EXT("Audio VOWCFG2 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg2_Get,
		       Audio_Vow_Cfg2_Set),
	SOC_SINGLE_EXT("Audio VOWCFG3 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg3_Get,
		       Audio_Vow_Cfg3_Set),
	SOC_SINGLE_EXT("Audio VOWCFG4 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg4_Get,
		       Audio_Vow_Cfg4_Set),
	SOC_SINGLE_EXT("Audio VOWCFG5 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg5_Get,
		       Audio_Vow_Cfg5_Set),
	SOC_SINGLE_EXT("Audio_VOW_State", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_State_Get,
		       Audio_Vow_State_Set),
};

static const struct snd_soc_dapm_widget mt6331_dapm_widgets[] = {
	
	SND_SOC_DAPM_OUTPUT("EARPIECE"),
	SND_SOC_DAPM_OUTPUT("HEADSET"),
	SND_SOC_DAPM_OUTPUT("SPEAKER"),

};

static const struct snd_soc_dapm_route mtk_audio_map[] = {
	{"VOICE_Mux_E", "Voice Mux", "SPEAKER PGA"},
};

static void mt6331_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_warn("%s\n", __func__);

	audckbufEnable(true);
	Ana_Set_Reg(TOP_CLKSQ, 0x0, 0x0001);
	
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1000, 0x1000);
	
	Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x7000, 0x7000);
	
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0xe000, 0xe000);
	
	Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
	
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x8000, 0x8000);
	
	Ana_Set_Reg(DRV_CON2, 0x00e0, 0x00f0);
	
	audckbufEnable(false);
}

void InitCodecDefault(void)
{
	pr_warn("%s\n", __func__);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;

	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
}

static void InitGlobalVarDefault(void)
{
	mCodec_data = NULL;
	mAudio_Vow_Analog_Func_Enable = false;
	mAudio_Vow_Digital_Func_Enable = false;
	mIsVOWOn = false;
	mAdc_Power_Mode = 0;
	mInitCodec = false;
	mAnaSuspend = false;
	audck_buf_Count = 0;
	ClsqCount = 0;
	TopCkCount = 0;
	NvRegCount = 0;

#ifdef CONFIG_MTK_SPEAKER
	mSpeaker_Ocflag = false;
#endif

}

static int mt6331_codec_probe(struct snd_soc_codec *codec)
{
#ifdef CONFIG_MTK_SPKGPIO_REWORK		
	int data[4];
	int rawdata;
#endif
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_warn("%s()\n", __func__);

	if (mInitCodec == true)
		return 0;

	pin_extspkamp = pin_extspkamp_2 = pin_vowclk = pin_audmiso = pin_rcvspkswitch = 0;
	pin_mode_extspkamp = pin_mode_extspkamp_2 = pin_mode_vowclk =
	    pin_mode_audmiso = pin_mode_rcvspkswitch = 0;

	pin_hpswitchtoground = pin_mode_hpswitchtoground = 0;

	snd_soc_dapm_new_controls(dapm, mt6331_dapm_widgets, ARRAY_SIZE(mt6331_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, mtk_audio_map, ARRAY_SIZE(mtk_audio_map));

	
	snd_soc_add_codec_controls(codec, mt6331_snd_controls, ARRAY_SIZE(mt6331_snd_controls));
	snd_soc_add_codec_controls(codec, mt6331_UL_Codec_controls,
				   ARRAY_SIZE(mt6331_UL_Codec_controls));
	snd_soc_add_codec_controls(codec, mt6331_Voice_Switch, ARRAY_SIZE(mt6331_Voice_Switch));
	snd_soc_add_codec_controls(codec, mt6331_pmic_Test_controls,
				   ARRAY_SIZE(mt6331_pmic_Test_controls));

#ifdef CONFIG_MTK_SPEAKER
	snd_soc_add_codec_controls(codec, mt6331_snd_Speaker_controls,
				   ARRAY_SIZE(mt6331_snd_Speaker_controls));
#endif

	snd_soc_add_codec_controls(codec, Audio_snd_auxadc_controls,
				   ARRAY_SIZE(Audio_snd_auxadc_controls));

	
	mCodec_data = kzalloc(sizeof(mt6331_Codec_Data_Priv), GFP_KERNEL);
	if (!mCodec_data) {
		
		return -ENOMEM;
	}
	snd_soc_codec_set_drvdata(codec, mCodec_data);

	memset((void *)mCodec_data, 0, sizeof(mt6331_Codec_Data_Priv));
	mt6331_codec_init_reg(codec);
	InitCodecDefault();
	mInitCodec = true;

	mClkBufferfromPMIC = is_clk_buf_from_pmic();
	pr_debug("%s Clk_buf_from_pmic is %d\n", __func__, mClkBufferfromPMIC);

#ifdef CONFIG_MTK_SPKGPIO_REWORK		
	
	IMM_GetOneChannelValue(12, data, &rawdata);
	pr_warn("PCB_ID: voltage: %d.%d\n", data[0], data[1]);

	if ((data[0] == 0) && (data[1] > 40 && data[1] < 54)) {
		
		pin_extspkamp = -1;
		pr_warn("AW8736 Rework version\n");
	} else {
		pin_extspkamp = 0;
		pr_warn("AW8736 Normal version\n");
	}
#endif
	return 0;
}

static int mt6331_codec_remove(struct snd_soc_codec *codec)
{
	pr_warn("%s()\n", __func__);
	return 0;
}

static unsigned int mt6331_read(struct snd_soc_codec *codec, unsigned int reg)
{
	pr_warn("mt6331_read reg = 0x%x", reg);
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6331_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	pr_warn("mt6331_write reg = 0x%x  value= 0x%x\n", reg, value);
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}


static struct snd_soc_codec_driver soc_mtk_codec = {
	.probe = mt6331_codec_probe,
	.remove = mt6331_codec_remove,

	.read = mt6331_read,
	.write = mt6331_write,


	
	
	

	.dapm_widgets = mt6331_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6331_dapm_widgets),
	.dapm_routes = mtk_audio_map,
	.num_dapm_routes = ARRAY_SIZE(mtk_audio_map),

};

static int mtk_mt6331_codec_dev_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;


	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);


	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev,
				      &soc_mtk_codec, mtk_6331_dai_codecs,
				      ARRAY_SIZE(mtk_6331_dai_codecs));
}

static int mtk_mt6331_codec_dev_remove(struct platform_device *pdev)
{
	pr_warn("%s:\n", __func__);

	snd_soc_unregister_codec(&pdev->dev);
	return 0;

}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_63xx_of_ids[] = {
	{.compatible = "mediatek,mt_soc_codec_63xx",},
	{}
};
#endif

static struct platform_driver mtk_codec_6331_driver = {
	.driver = {
		   .name = MT_SOC_CODEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_codec_63xx_of_ids,
#endif
		   },
	.probe = mtk_mt6331_codec_dev_probe,
	.remove = mtk_mt6331_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6331_dev;
#endif

static int __init mtk_mt6331_codec_init(void)
{
	pr_warn("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret = 0;

	soc_mtk_codec6331_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);

	if (!soc_mtk_codec6331_dev)
		return -ENOMEM;


	ret = platform_device_add(soc_mtk_codec6331_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec6331_dev);
		return ret;
	}
#endif
	InitGlobalVarDefault();

	return platform_driver_register(&mtk_codec_6331_driver);
}

module_init(mtk_mt6331_codec_init);

static void __exit mtk_mt6331_codec_exit(void)
{
	pr_warn("%s:\n", __func__);

	platform_driver_unregister(&mtk_codec_6331_driver);
}

module_exit(mtk_mt6331_codec_exit);

MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");
