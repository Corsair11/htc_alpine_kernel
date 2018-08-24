/* include/sound/htc_acoustic_alsa.h
 *
 * Copyright (C) 2012 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>
#include <linux/sched.h>

#ifndef _ARCH_ARM_MACH_MSM_HTC_ACOUSTIC_QCT_ALSA_H_
#define _ARCH_ARM_MACH_MSM_HTC_ACOUSTIC_QCT_ALSA_H_

#define HTC_MSM8994_BRINGUP_OPTION (0)

#define AUD_HW_NUM			8
#define HTC_AUDIO_TPA2051	0x01
#define HTC_AUDIO_TPA2026	0x02
#define HTC_AUDIO_TPA2028	0x04
#define HTC_AUDIO_A1028		0x08
#define HTC_AUDIO_TPA6185 	0x10
#define HTC_AUDIO_RT5501 	0x20
#define HTC_AUDIO_TFA9887 	0x40
#define HTC_AUDIO_TFA9887L 	0x80

#define ACOUSTIC_IOCTL_MAGIC 'p'
#define ACOUSTIC_SET_Q6_EFFECT		_IOW(ACOUSTIC_IOCTL_MAGIC, 43, unsigned)
#define ACOUSTIC_GET_HTC_REVISION	_IOW(ACOUSTIC_IOCTL_MAGIC, 44, unsigned)
#define ACOUSTIC_GET_HW_COMPONENT	_IOW(ACOUSTIC_IOCTL_MAGIC, 45, unsigned)
#define ACOUSTIC_GET_DMIC_INFO   	_IOW(ACOUSTIC_IOCTL_MAGIC, 46, unsigned)
#define ACOUSTIC_UPDATE_EFFECT_ICON	_IOW(ACOUSTIC_IOCTL_MAGIC, 47, unsigned)
#define ACOUSTIC_UPDATE_LISTEN_NOTIFICATION	_IOW(ACOUSTIC_IOCTL_MAGIC, 48, unsigned)
#define ACOUSTIC_GET_HW_REVISION	_IOR(ACOUSTIC_IOCTL_MAGIC, 49, struct device_info)
#define ACOUSTIC_CONTROL_WAKELOCK	_IOW(ACOUSTIC_IOCTL_MAGIC, 77, unsigned)
#define ACOUSTIC_DUMMY_WAKELOCK 	_IOW(ACOUSTIC_IOCTL_MAGIC, 78, unsigned)
#define ACOUSTIC_AMP_CTRL		_IOR(ACOUSTIC_IOCTL_MAGIC, 50, unsigned)
#define ACOUSTIC_GET_MID		_IOW(ACOUSTIC_IOCTL_MAGIC, 51, unsigned)
#define ACOUSTIC_RAMDUMP		_IOW(ACOUSTIC_IOCTL_MAGIC, 99, unsigned)
#define ACOUSTIC_KILL_PID		_IOW(ACOUSTIC_IOCTL_MAGIC, 88, unsigned)
#define ACOUSTIC_UPDATE_DQ_STATUS	_IOW(ACOUSTIC_IOCTL_MAGIC, 52, unsigned)
#define ACOUSTIC_GET_HEADSET_TYPE	_IOR(ACOUSTIC_IOCTL_MAGIC, 53, unsigned)

#define AUD_AMP_SLAVE_ALL	0xffff

enum HTC_FEATURE {
	HTC_Q6_EFFECT = 0,
	HTC_AUD_24BIT,
};

enum SPK_AMP_TYPE {
	SPK_AMP_RIGHT = 0,
	SPK_AMP_LEFT,
	SPK_AMP_MAX,
};

enum ACOU_AMP_CTRL {
	AMP_READ = 0,
	AMP_WRITE,
};

enum AMP_TYPE {
	AMP_HEADPONE = 0,
	AMP_SPEAKER,
	AMP_RECEIVER,
};

enum HS_NOTIFY_TYPE {
	HS_AMP_N = 0,
	HS_CODEC_N,
	HS_N_MAX,
};

struct hw_component {
	const char *name;
	const unsigned int id;
};

struct amp_register_s {
	int (*aud_amp_f)(int, int);
	struct file_operations* fops;
};

struct amp_ctrl {
	enum ACOU_AMP_CTRL ctrl;
	enum AMP_TYPE type;
	unsigned short slave;
	unsigned int reg;
	unsigned int val;
};

struct acoustic_ops {
	void (*set_q6_effect)(int mode);
	int (*get_htc_revision)(void);
	int (*get_hw_component)(void);
	char* (*get_mid)(void);
	int (*enable_digital_mic)(void);
	int (*enable_24b_audio)(void);
	int (*get_q6_effect) (void);
};

struct hs_notify_t {
	int used;
	void *private_data;
	int (*callback_f)(void*,int);
};

void htc_acoustic_register_ops(struct acoustic_ops *ops);
void htc_acoustic_register_hs_amp(int (*aud_hs_amp_f)(int, int), struct file_operations* ops);
int htc_acoustic_hs_amp_ctrl(int on, int dsp);
void htc_acoustic_register_spk_amp(enum SPK_AMP_TYPE type,int (*aud_spk_amp_f)(int, int), struct file_operations* ops);
int htc_acoustic_spk_amp_ctrl(enum SPK_AMP_TYPE type,int on, int dsp);
int htc_acoustic_query_feature(enum HTC_FEATURE feature);
void htc_acoustic_register_hs_notify(enum HS_NOTIFY_TYPE type, struct hs_notify_t *notify);

struct amp_power_ops {
	void (*set_amp_power_enable)(bool enable);
};

void htc_amp_power_register_ops(struct amp_power_ops *ops);
void htc_amp_power_enable(bool enable);


#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
#include "htc/htc_acoustic_type.h"

int htc_typec_usb_accessory_detection_get(void);
int htc_usb_headset_type_get(void);

int htc_usb_position_get(void);
int htc_adaptor_mic_det_get(void);
int htc_typec_id1_get(void);
int htc_typec_id2_get(void);
int htc_mfg_id1_get(void);
int htc_mfg_id2_get(void);

void htc_aud_hsmic_2v85_en (int value);
int htc_aud_hsmic_2v85_en_get(void);

void htc_aud_hp_det_set(int value);

#define HEADSET_L (true)
#define HEADSET_R (!HEADSET_L)
void htc_aud_hs_switch(bool isL, bool value);

#define HSMIC_AGND_FLIP		1
#define HSMIC_AGND_NON_FLIP	0
void htc_aud_hpmic_agnd_flip_en_s0_set(int value);
void htc_aud_hpmic_agnd_flip_en_s1_set(int value);
int htc_aud_hpmic_agnd_flip_en_s0_get(void);
int htc_aud_hpmic_agnd_flip_en_s1_get(void);

#define MICLR_FLIP	0
#define MICLR_NON_FLIP	1
void htc_miclr_flip_en_set(int value);
int htc_miclr_flip_en_get(void);

#define MICLR_DGND_TO_GND	false
#define MICLR_DGND_TO_MICLR	true
void htc_aud_miclr_dgnd_sw_en (bool value);
bool htc_aud_miclr_dgnd_sw_en_get(void);

#define AS2_TO_MAIN_MIC		false
#define AS2_TO_MICLR_MIC	true
void htc_as2_en (bool value);
bool htc_as2_en_get(void);

#endif 

#endif

