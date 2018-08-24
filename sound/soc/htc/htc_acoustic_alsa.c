/* sound/soc/htc/htc_acoustic_alsa.c
 *
 * Copyright (C) 2012 HTC Corporation
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

#include <linux/device.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <sound/htc_acoustic_alsa.h>

#if HTC_MSM8994_BRINGUP_OPTION
#include <sound/htc_headset_mgr.h>
#endif

#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/htc_devices_dtb.h>
#include <linux/delay.h>
#ifdef CONFIG_SWITCH_FSA3030
#include <linux/fsa3030.h>
#endif

int mcu_audio_gpio_output(u8 v_gpio_group, u8 v_gpio_ping, bool v_hilo);
#endif

struct device_info {
	unsigned pcb_id;
	unsigned sku_id;
};

#define D(fmt, args...) printk(KERN_ERR "[AUD] htc-acoustic: "fmt, ##args)
#define E(fmt, args...) printk(KERN_ERR "[AUD] htc-acoustic: "fmt, ##args)

static DEFINE_MUTEX(api_lock);
static struct acoustic_ops default_acoustic_ops;
static struct acoustic_ops *the_ops = &default_acoustic_ops;
static struct switch_dev sdev_effect_icon;
static struct switch_dev sdev_dq;
static struct switch_dev sdev_listen_notification;

static DEFINE_MUTEX(hs_amp_lock);
static struct amp_register_s hs_amp = {NULL, NULL};

static DEFINE_MUTEX(spk_amp_lock);
static struct amp_register_s spk_amp[SPK_AMP_MAX] = {{NULL}};

static struct amp_power_ops default_amp_power_ops = {NULL};
static struct amp_power_ops *the_amp_power_ops = &default_amp_power_ops;

static struct wake_lock htc_acoustic_wakelock;
static struct wake_lock htc_acoustic_wakelock_timeout;
static struct wake_lock htc_acoustic_dummy_wakelock;
static struct hs_notify_t hs_plug_nt[HS_N_MAX] = {{0,NULL,NULL}};
static DEFINE_MUTEX(hs_nt_lock);

static int hs_amp_open(struct inode *inode, struct file *file);
static int hs_amp_release(struct inode *inode, struct file *file);
static long hs_amp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations hs_def_fops = {
	.owner = THIS_MODULE,
	.open = hs_amp_open,
	.release = hs_amp_release,
	.unlocked_ioctl = hs_amp_ioctl,
};

static struct miscdevice hs_amp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aud_hs_amp",
	.fops = &hs_def_fops,
};

struct hw_component HTC_AUD_HW_LIST[AUD_HW_NUM] = {
	{
		.name = "TPA2051",
		.id = HTC_AUDIO_TPA2051,
	},
	{
		.name = "TPA2026",
		.id = HTC_AUDIO_TPA2026,
	},
	{
		.name = "TPA2028",
		.id = HTC_AUDIO_TPA2028,
	},
	{
		.name = "A1028",
		.id = HTC_AUDIO_A1028,
	},
	{
		.name = "TPA6185",
		.id = HTC_AUDIO_TPA6185,
	},
	{
		.name = "RT5501",
		.id = HTC_AUDIO_RT5501,
	},
	{
		.name = "TFA9887",
		.id = HTC_AUDIO_TFA9887,
	},
	{
		.name = "TFA9887L",
		.id = HTC_AUDIO_TFA9887L,
	},
};
EXPORT_SYMBOL(HTC_AUD_HW_LIST);

unsigned int system_rev = 0;
void htc_acoustic_register_spk_amp(enum SPK_AMP_TYPE type,int (*aud_spk_amp_f)(int, int), struct file_operations* ops)
{
	mutex_lock(&spk_amp_lock);

	if(type < SPK_AMP_MAX) {
		spk_amp[type].aud_amp_f = aud_spk_amp_f;
		spk_amp[type].fops = ops;
	}

	mutex_unlock(&spk_amp_lock);
}

int htc_acoustic_spk_amp_ctrl(enum SPK_AMP_TYPE type,int on, int dsp)
{
	int ret = 0;
	mutex_lock(&spk_amp_lock);

	if(type < SPK_AMP_MAX) {
		if(spk_amp[type].aud_amp_f)
			ret = spk_amp[type].aud_amp_f(on,dsp);
	}

	mutex_unlock(&spk_amp_lock);

	return ret;
}

int htc_acoustic_hs_amp_ctrl(int on, int dsp)
{
	int ret = 0;
	mutex_lock(&hs_amp_lock);
	if(hs_amp.aud_amp_f) {
		ret = hs_amp.aud_amp_f(on,dsp);
	}
	mutex_unlock(&hs_amp_lock);
	return ret;
}

void htc_acoustic_register_hs_amp(int (*aud_hs_amp_f)(int, int), struct file_operations* ops)
{
	mutex_lock(&hs_amp_lock);
	hs_amp.aud_amp_f = aud_hs_amp_f;
	hs_amp.fops = ops;
	mutex_unlock(&hs_amp_lock);
}

void htc_acoustic_register_hs_notify(enum HS_NOTIFY_TYPE type, struct hs_notify_t *notify)
{
	if(notify == NULL)
		return;

	mutex_lock(&hs_nt_lock);
	if(hs_plug_nt[type].used) {
		pr_err("%s: hs notification %d is reigstered\n",__func__,(int)type);
	} else {
		hs_plug_nt[type].private_data = notify->private_data;
		hs_plug_nt[type].callback_f = notify->callback_f;
		hs_plug_nt[type].used = 1;
	}
	mutex_unlock(&hs_nt_lock);
}

void htc_acoustic_register_ops(struct acoustic_ops *ops)
{
        D("acoustic_register_ops \n");
	mutex_lock(&api_lock);
	the_ops = ops;
	mutex_unlock(&api_lock);
}

int htc_acoustic_query_feature(enum HTC_FEATURE feature)
{
	int ret = -1;
	mutex_lock(&api_lock);
	switch(feature) {
		case HTC_Q6_EFFECT:
			if(the_ops && the_ops->get_q6_effect)
				ret = the_ops->get_q6_effect();
			break;
		case HTC_AUD_24BIT:
			if(the_ops && the_ops->enable_24b_audio)
				ret = the_ops->enable_24b_audio();
			break;
		default:
			break;

	};
	mutex_unlock(&api_lock);
	return ret;
}

static int acoustic_open(struct inode *inode, struct file *file)
{
	D("open\n");
	return 0;
}

static int acoustic_release(struct inode *inode, struct file *file)
{
	D("release\n");
	return 0;
}

static long
acoustic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	int hw_rev = 0;
	int mode = -1;
	char *mid = NULL;
	mutex_lock(&api_lock);
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(ACOUSTIC_SET_Q6_EFFECT): {
		if (copy_from_user(&mode, (void *)arg, sizeof(mode))) {
			rc = -EFAULT;
			break;
		}
		D("Set Q6 Effect : %d\n", mode);
		if (mode < -1 || mode > 1) {
			E("unsupported Q6 mode: %d\n", mode);
			rc = -EINVAL;
			break;
		}
		if (the_ops->set_q6_effect)
			the_ops->set_q6_effect(mode);
		break;
	}
	case _IOC_NR(ACOUSTIC_GET_HTC_REVISION):
		if (the_ops->get_htc_revision)
			hw_rev = the_ops->get_htc_revision();
		else
			hw_rev = 1;

		D("Audio HW revision:  %u\n", hw_rev);
		if(copy_to_user((void *)arg, &hw_rev, sizeof(hw_rev))) {
			E("acoustic_ioctl: copy to user failed\n");
			rc = -EINVAL;
		}
		break;
	case _IOC_NR(ACOUSTIC_GET_HW_COMPONENT):
		if (the_ops->get_hw_component)
			rc = the_ops->get_hw_component();

		D("support components: 0x%x\n", rc);
		if(copy_to_user((void *)arg, &rc, sizeof(rc))) {
			E("acoustic_ioctl: copy to user failed\n");
			rc = -EINVAL;
		}
		break;
	case _IOC_NR(ACOUSTIC_GET_DMIC_INFO):
		if (the_ops->enable_digital_mic)
			rc = the_ops->enable_digital_mic();

		D("support components: 0x%x\n", rc);
		if(copy_to_user((void *)arg, &rc, sizeof(rc))) {
			E("acoustic_ioctl: copy to user failed\n");
			rc = -EINVAL;
		}
		break;
	case _IOC_NR(ACOUSTIC_GET_MID):
		if (the_ops->get_mid)
			mid = the_ops->get_mid();

		D("get mid: %s\n", mid);
		if (mid) {
			if(copy_to_user((void *)arg, mid, strlen(mid))) {
				E("acoustic_ioctl: copy to user failed\n");
				rc = -EINVAL;
			}
		} else {
			E("acoustic_ioctl: mid is NULL\n");
			rc = -EINVAL;
		}
		break;
	case _IOC_NR(ACOUSTIC_UPDATE_EFFECT_ICON): {
		int new_state = -1;

		if (copy_from_user(&new_state, (void *)arg, sizeof(new_state))) {
			rc = -EFAULT;
			break;
		}
		D("Update Effect Icon : %d\n", new_state);
		if (new_state < -1 || new_state > 1) {
			E("Invalid Effect Icon update");
			rc = -EINVAL;
			break;
		}

		sdev_effect_icon.state = -1;
		switch_set_state(&sdev_effect_icon, new_state);
		break;
	}
	case _IOC_NR(ACOUSTIC_UPDATE_DQ_STATUS): {
		int new_state = -1;

		if (copy_from_user(&new_state, (void *)arg, sizeof(new_state))) {
			rc = -EFAULT;
			break;
		}
		D("Update DQ Status : %d\n", new_state);
		if (new_state < -1 || new_state > 1) {
			E("Invalid Beats status update");
			rc = -EINVAL;
			break;
		}

		sdev_dq.state = -1;
		switch_set_state(&sdev_dq, new_state);
		break;
	}
	case _IOC_NR(ACOUSTIC_CONTROL_WAKELOCK): {
		int new_state = -1;

		if (copy_from_user(&new_state, (void *)arg, sizeof(new_state))) {
			rc = -EFAULT;
			break;
		}
		D("control wakelock : %d\n", new_state);
		if (new_state == 1) {
			wake_lock_timeout(&htc_acoustic_wakelock, 60*HZ);
		} else {
			wake_lock_timeout(&htc_acoustic_wakelock_timeout, 1*HZ);
			wake_unlock(&htc_acoustic_wakelock);
		}
		break;
	}
	case _IOC_NR(ACOUSTIC_DUMMY_WAKELOCK): {
		wake_lock_timeout(&htc_acoustic_dummy_wakelock, 5*HZ);
		break;
	}
	case _IOC_NR(ACOUSTIC_UPDATE_LISTEN_NOTIFICATION): {
		int new_state = -1;

		if (copy_from_user(&new_state, (void *)arg, sizeof(new_state))) {
			rc = -EFAULT;
			break;
		}
		D("Update listen notification : %d\n", new_state);
		if (new_state < -1 || new_state > 1) {
			E("Invalid listen notification state");
			rc = -EINVAL;
			break;
		}

		sdev_listen_notification.state = -1;
		switch_set_state(&sdev_listen_notification, new_state);
		break;
	}
       case _IOC_NR(ACOUSTIC_RAMDUMP):
#if 0
		pr_err("trigger ramdump by user space\n");
		if (copy_from_user(&mode, (void *)arg, sizeof(mode))) {
			rc = -EFAULT;
			break;
		}

		if (mode >= 4100 && mode <= 4800) {
			dump_stack();
			pr_err("msgid = %d\n", mode);
			BUG();
		}
#endif
                break;
       case _IOC_NR(ACOUSTIC_GET_HW_REVISION):
		{
			struct device_info info;
			info.pcb_id = system_rev;
			info.sku_id = 0;
			D("acoustic pcb_id: 0x%x, sku_id: 0x%x\n", info.pcb_id, info.sku_id);
			if(copy_to_user((void *)arg, &info, sizeof(info))) {
				E("acoustic_ioctl: copy to user failed\n");
				rc = -EINVAL;
			}
		}
	        break;
	case _IOC_NR(ACOUSTIC_AMP_CTRL):
		{
			struct amp_ctrl ampctrl;
			int i;

			if (copy_from_user(&ampctrl, (void __user *)arg, sizeof(ampctrl)))
				return -EFAULT;

			if(ampctrl.type == AMP_HEADPONE) {
				mutex_lock(&hs_amp_lock);
				if(hs_amp.fops && hs_amp.fops->unlocked_ioctl) {
					hs_amp.fops->unlocked_ioctl(file, cmd, arg);
				}
				mutex_unlock(&hs_amp_lock);
			} else if (ampctrl.type == AMP_SPEAKER) {
				mutex_lock(&spk_amp_lock);
				for(i=0; i<SPK_AMP_MAX; i++) {
					if(spk_amp[i].fops && spk_amp[i].fops->unlocked_ioctl) {
						spk_amp[i].fops->unlocked_ioctl(file, cmd, arg);
					}
				}
				mutex_unlock(&spk_amp_lock);
			}
		}
		break;
	case _IOC_NR(ACOUSTIC_KILL_PID):
		{
			int pid = 0;
			struct pid *pid_struct = NULL;

			if (copy_from_user(&pid, (void *)arg, sizeof(pid))) {
				rc = -EFAULT;
				break;
			}

			D("ACOUSTIC_KILL_PID: %d\n", pid);

			if (pid <= 0)
				break;

			pid_struct = find_get_pid(pid);
			if (pid_struct) {
				kill_pid(pid_struct, SIGKILL, 1);
				D("kill pid: %d", pid);
			}
		}
		break;
#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
	case _IOC_NR(ACOUSTIC_GET_HEADSET_TYPE):
		{
			int headset_type = htc_usb_headset_type_get();

			D("ACOUSTIC_GET_HEADSET_TYPE:  %u\n", headset_type);
			if(copy_to_user((void *)arg, &headset_type, sizeof(headset_type))) {
				E("acoustic_ioctl: copy to user failed\n");
				rc = -EINVAL;
			}
		}
		break;
#endif
	default:
		rc = -EINVAL;
	}
	mutex_unlock(&api_lock);
	return rc;
}


static int hs_amp_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	const struct file_operations *old_fops = NULL;

	mutex_lock(&hs_amp_lock);

	if(hs_amp.fops) {
		old_fops = file->f_op;

		file->f_op = fops_get(hs_amp.fops);

		if (file->f_op == NULL) {
			file->f_op = old_fops;
			ret = -ENODEV;
		}
	}
	mutex_unlock(&hs_amp_lock);

	if(ret >= 0) {

		if (file->f_op->open) {
			ret = file->f_op->open(inode, file);
			if (ret) {
				fops_put(file->f_op);
				if(old_fops)
					file->f_op = fops_get(old_fops);
				return ret;
			}
		}

		if(old_fops)
			fops_put(old_fops);
	}

	return ret;
}

static int hs_amp_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long hs_amp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

void htc_amp_power_register_ops(struct amp_power_ops *ops)
{
	D("%s", __func__);
	the_amp_power_ops = ops;
}

void htc_amp_power_enable(bool enable)
{
	D("%s", __func__);
	if(the_amp_power_ops && the_amp_power_ops->set_amp_power_enable)
		the_amp_power_ops->set_amp_power_enable(enable);
}

#if HTC_MSM8994_BRINGUP_OPTION
static int htc_acoustic_hsnotify(int on)
{
	int i = 0;
	mutex_lock(&hs_nt_lock);
	for(i=0; i<HS_N_MAX; i++) {
		if(hs_plug_nt[i].used && hs_plug_nt[i].callback_f)
			hs_plug_nt[i].callback_f(hs_plug_nt[i].private_data, on);
	}
	mutex_unlock(&hs_nt_lock);

	return 0;
}
#endif

#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
enum {
MICLR_FLIP_EN_HIGH,
MICLR_FLIP_EN_LOW,
AUD_SWITCH_EN_HIGH,
AUD_HPMIC_AGND_FLIP_EN_S0_HIGH,
AUD_HPMIC_AGND_FLIP_EN_S0_LOW,
AUD_HPMIC_AGND_FLIP_EN_S1_HIGH,
AUD_HPMIC_AGND_FLIP_EN_S1_LOW,
AUD_HSMIC_2V85_EN_HIGH,
AUD_HSMIC_2V85_EN_LOW,

AUD_USB_POSITION,
AUD_ADAPTOR_MIC_DET,
AUD_TYPEC_ID1,
AUD_TYPEC_ID2,

AUD_HP_DET_HIGH,
AUD_HP_DET_LOW,
PINCTRL_MAX
};

static char *pinctrl_name[PINCTRL_MAX] = {
"aud_miclr_flip_en_high",
"aud_miclr_flip_en_low",
"v_audio_switch_en_high",
"aud_hpmic_agnd_flip_en_s0_high",
"aud_hpmic_agnd_flip_en_s0_low",
"aud_hpmic_agnd_flip_en_s1_high",
"aud_hpmic_agnd_flip_en_s1_low",
"aud_hsmic_2v85_en_high",
"aud_hsmic_2v85_en_low",
"aud_usb_position_default",
"aud_adaptor_mic_det_default",
"aud_typec_id1_default",
"aud_typec_id2_default",
"aud_hp_det_high",
"aud_hp_det_low"
};

static int aud_hsmic_2v85_en;
static int aud_hpmic_agnd_flip_en_s0;
static int aud_hpmic_agnd_flip_en_s1;
static int miclr_flip_en;
static int aud_miclr_dgnd_sw_en;
static int as2_en;

static int usb_position_gpio = -1;
static int adaptor_mic_det_gpio = -1;
static int typec_id1_gpio = -1;
static int typec_id2_gpio = -1;

static int mfg_id1 = -1;
static int mfg_id2 = -1;

#ifdef CONFIG_SWITCH_FSA3030
static int usb_hs_L_switch_handle = 0;
static int usb_hs_R_switch_handle = 0;
#endif

static struct pinctrl *pinctrl_htc = NULL;
static struct pinctrl_state *pinctrl_states[PINCTRL_MAX] = {0};

static int pinctrl_select(int index)
{
	if (pinctrl_htc == NULL)
	{
		pr_err("%s: pinctrl is null!\n", __func__);
		return -1;
	}

	if (index < 0 && index >= PINCTRL_MAX)
	{
		pr_err("%s: invalid index %d\n", __func__, index);
		return -1;
	}

	if (pinctrl_states[index])
	{
		int ret = pinctrl_select_state(pinctrl_htc, pinctrl_states[index]);
		if (ret) {
			pr_err("%s: set %s state fail: %d\n",
				__func__, pinctrl_name[index], ret);
			return ret;
		}
	}
	else
	{
		pr_err("%s: pinctrl_stats[%d] is null !\n", __func__, index);
		return -1;
	}
	return 0;
}

void htc_aud_hp_det_set(int value)
{
	pr_info("%s: value = %d\n", __func__, value);
	if (value == 0)
	{
		pinctrl_select (AUD_HP_DET_LOW);
	}
	else
	{
		pinctrl_select (AUD_HP_DET_HIGH);
	}
}

int htc_typec_usb_accessory_detection_get(void)
{
	if (pinctrl_htc == NULL)
	{
		pr_info("%s: disabled\n", __func__);
		return 0;
	}
	pr_info("%s: enabled\n", __func__);
	return 1;
}

void htc_miclr_flip_en_set(int value)
{
	pr_info("%s: value = %d\n", __func__, value);
	if (value == 0)
	{
		pinctrl_select (MICLR_FLIP_EN_LOW);
	}
	else
	{
		pinctrl_select (MICLR_FLIP_EN_HIGH);
	}
	miclr_flip_en = value;
}

int htc_miclr_flip_en_get()
{
	return miclr_flip_en;
}

int htc_usb_position_get(void)
{
	int val = 0;
	if (usb_position_gpio < 0)
		return -1;

	if (gpio_is_valid(usb_position_gpio))
	{
		val = gpio_get_value(usb_position_gpio);
		pr_info("%s: usb_position_gpio value = %d\n", __func__, val);
	}
	else
	{
		pr_err("%s: gpio is invalid\n", __func__);
		val = -1;
	}
	return val;
}

int htc_adaptor_mic_det_get(void)
{
	int val = 0;
	if (adaptor_mic_det_gpio < 0)
		return -1;

	if (gpio_is_valid(adaptor_mic_det_gpio))
	{
		val = gpio_get_value(adaptor_mic_det_gpio);
		pr_info("%s: adaptor_mic_det_gpio value = %d\n", __func__,  val);
	}
	else
	{
		pr_err("%s: gpio is invalid\n", __func__);
		val = -1;
	}
	return val;
}

int htc_typec_id1_get(void)
{
	int val = 0;
	if (typec_id1_gpio < 0)
		return -1;

	if (gpio_is_valid(typec_id1_gpio))
	{
		val = gpio_get_value(typec_id1_gpio);
		pr_info("%s: typec_id1_gpio value = %d\n", __func__,  val);
	}
	else
	{
		pr_err("%s: gpio is invalid\n", __func__);
		val = -1;
	}
	return val;
}

int htc_typec_id2_get(void)
{
	int val = 0;
	if (typec_id2_gpio < 0)
		return -1;

	if (gpio_is_valid(typec_id2_gpio))
	{
		val = gpio_get_value(typec_id2_gpio);
		pr_info("%s: typec_id2_gpio value = %d\n", __func__,  val);
	}
	else
	{
		pr_err("%s: gpio is invalid\n", __func__);
		val = -1;
	}
	return val;
}

void htc_aud_hpmic_agnd_flip_en_s0_set(int value)
{
	pr_info("%s: value = %d\n", __func__, value);
	if (value == 0)
	{
		pinctrl_select (AUD_HPMIC_AGND_FLIP_EN_S0_LOW);
	}
	else
	{
		pinctrl_select (AUD_HPMIC_AGND_FLIP_EN_S0_HIGH);
	}
	aud_hpmic_agnd_flip_en_s0 = value;
}

int htc_aud_hpmic_agnd_flip_en_s0_get()
{
	return aud_hpmic_agnd_flip_en_s0;
}

void htc_aud_hpmic_agnd_flip_en_s1_set(int value)
{
	pr_info("%s: value = %d\n", __func__, value);
	if (value == 0)
	{
		pinctrl_select (AUD_HPMIC_AGND_FLIP_EN_S1_LOW);
	}
	else
	{
		pinctrl_select (AUD_HPMIC_AGND_FLIP_EN_S1_HIGH);
	}
	aud_hpmic_agnd_flip_en_s1 = value;
}

int htc_aud_hpmic_agnd_flip_en_s1_get()
{
	return aud_hpmic_agnd_flip_en_s1;
}

void htc_aud_hsmic_2v85_en (int value)
{
	pr_info("%s: value = %d\n", __func__, value);
	if (value == 0)
	{
		pinctrl_select (AUD_HSMIC_2V85_EN_LOW);
	}
	else
	{
		pinctrl_select (AUD_HSMIC_2V85_EN_HIGH);
	}
	aud_hsmic_2v85_en = value;
}

int htc_aud_hsmic_2v85_en_get ()
{
	return aud_hsmic_2v85_en;
}

void htc_aud_miclr_dgnd_sw_en (bool value)
{
	pr_info("%s: value = %d\n", __func__, value);
#ifdef CONFIG_INPUT_CWSTM32_V2
	if ((strcmp(htc_get_bootmode(), "charger")))
		mcu_audio_gpio_output(0, 1, value);
#endif
	aud_miclr_dgnd_sw_en = value;
}

bool htc_aud_miclr_dgnd_sw_en_get ()
{
	return aud_miclr_dgnd_sw_en;
}

void htc_as2_en (bool value)
{
	pr_info("%s: value = %d\n", __func__, value);
#ifdef CONFIG_INPUT_CWSTM32_V2
	if ((strcmp(htc_get_bootmode(), "charger")))
		mcu_audio_gpio_output(0, 5, value);
#endif
	as2_en = value;
}

bool htc_as2_en_get ()
{
	return as2_en;
}

static void htc_id1_id2_test(void)
{
	htc_aud_hsmic_2v85_en(1);
	htc_aud_miclr_dgnd_sw_en(MICLR_DGND_TO_MICLR);
	msleep(100);

	mfg_id1 = htc_typec_id1_get();
	mfg_id2 = htc_typec_id2_get();
	htc_aud_miclr_dgnd_sw_en(MICLR_DGND_TO_GND);
	htc_aud_hsmic_2v85_en(0);
}

int htc_mfg_id1_get()
{
	return mfg_id1;
}

int htc_mfg_id2_get()
{
	return mfg_id2;
}

static int init_pinctrl(struct pinctrl* pinctrl)
{
	int i;
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("%s(): Pinctrl not defined", __func__);
		return -1;
	}

	for(i = 0;i < PINCTRL_MAX; i ++)
	{
		pinctrl_states[i] = pinctrl_lookup_state(pinctrl, pinctrl_name[i]);
		if (IS_ERR_OR_NULL(pinctrl_states[i]))
		{
			pr_err("%s(): pinctrl %s is not found!\n", __func__, pinctrl_name[i]);
			return -1;
		}
		pr_info("%s: lookup pinctrl %s ok\n", __func__, pinctrl_name[i]);
	}

	
	pinctrl_select(AUD_USB_POSITION);
	pinctrl_select(AUD_ADAPTOR_MIC_DET);
	pinctrl_select(AUD_TYPEC_ID1);
	pinctrl_select(AUD_TYPEC_ID2);
	pinctrl_select(AUD_SWITCH_EN_HIGH);

	
	htc_aud_hsmic_2v85_en(0);
	htc_miclr_flip_en_set(MICLR_NON_FLIP);
	htc_aud_hpmic_agnd_flip_en_s0_set(HSMIC_AGND_NON_FLIP);
	htc_aud_hpmic_agnd_flip_en_s1_set(HSMIC_AGND_NON_FLIP);
	htc_aud_miclr_dgnd_sw_en(MICLR_DGND_TO_GND);
	htc_as2_en(AS2_TO_MAIN_MIC);
	htc_aud_hp_det_set(0);

	htc_usb_position_get();
	htc_adaptor_mic_det_get();
	htc_typec_id1_get();
	htc_typec_id2_get();

	
	htc_id1_id2_test();
	return 0;
}

void htc_aud_hs_switch(bool isL, bool isOn)
{
	pr_info("%s isL = %d isOn = %d\n", __func__, isL, isOn);
#ifdef CONFIG_SWITCH_FSA3030
	if (isL == HEADSET_L) {
		if (usb_hs_L_switch_handle)
			fsa3030_switch_set(usb_hs_L_switch_handle, FSA3030_SWITCH_TYPE_AUDIO, isOn);
	} else {
		if (usb_hs_R_switch_handle)
			fsa3030_switch_set(usb_hs_R_switch_handle, FSA3030_SWITCH_TYPE_AUDIO, isOn);
	}
#endif
}

static int audio_pinctrl_probe(struct platform_device *pdev)
{
	int ret=0;
	struct pinctrl *pinctrl = devm_pinctrl_get(&pdev->dev);
	struct device_node *np = pdev->dev.of_node;
	struct property *prop;

	pr_info("%s enter\n", __func__);
	prop = of_find_property(np, "aud_usb_position_gpio", NULL);
	if (prop) {
		of_property_read_u32(np, "aud_usb_position_gpio", &usb_position_gpio);
		pr_info("%s: usb_position_gpio = %d\n", __func__, usb_position_gpio);
	} else {
		pr_warn("%s: usb_position_gpio not found\n", __func__);
		usb_position_gpio = -1;
	}

	if (usb_position_gpio < 0)
		return usb_position_gpio;

	if (!gpio_is_valid(usb_position_gpio))
	{
		pr_err("%s: gpio is invalid %d", __func__, usb_position_gpio);
		return -1;
	}

	ret = devm_gpio_request_one(&pdev->dev, usb_position_gpio, GPIOF_DIR_IN, "aud_usb_position");
	if (ret < 0) {
		pr_err("DEBUG: gpio request fail: %d  ret = %d\n", usb_position_gpio, ret);
		return ret;
	}
	

	prop = of_find_property(np, "aud_adaptor_mic_det_gpio", NULL);
	if (prop) {
		of_property_read_u32(np, "aud_adaptor_mic_det_gpio", &adaptor_mic_det_gpio);
		pr_info("%s: adaptor_mic_det_gpio = %d\n", __func__, adaptor_mic_det_gpio);
	} else {
		pr_warn("%s: adaptor_mic_det_gpio not found\n", __func__);
		adaptor_mic_det_gpio = -1;
	}

	if (adaptor_mic_det_gpio < 0)
		return adaptor_mic_det_gpio;

	if (!gpio_is_valid(adaptor_mic_det_gpio))
	{
		pr_err("%s: gpio is invalid %d", __func__, adaptor_mic_det_gpio);
		return -1;
	}

	ret = devm_gpio_request_one(&pdev->dev, adaptor_mic_det_gpio, GPIOF_DIR_IN, "aud_adaptor_mic_det");
	if (ret < 0) {
		pr_err("DEBUG: gpio request fail: %d  ret = %d\n", adaptor_mic_det_gpio, ret);
		return ret;
	}
	

	prop = of_find_property(np, "aud_typec_id1_gpio", NULL);
	if (prop) {
		of_property_read_u32(np, "aud_typec_id1_gpio", &typec_id1_gpio);
		pr_info("%s: typec_id1_gpio = %d\n", __func__, typec_id1_gpio);
	} else {
		pr_warn("%s: typec_id1_gpio not found\n", __func__);
		typec_id1_gpio = -1;
	}

	if (typec_id1_gpio < 0)
		return typec_id1_gpio;

	if (!gpio_is_valid(typec_id1_gpio))
	{
		pr_err("%s: gpio is invalid %d", __func__, typec_id1_gpio);
		return -1;
	}

	ret = devm_gpio_request_one(&pdev->dev, typec_id1_gpio, GPIOF_DIR_IN, "aud_typec_id1");
	if (ret < 0) {
		pr_err("DEBUG: gpio request fail: %d  ret = %d\n", typec_id1_gpio, ret);
		return ret;
	}
	

	prop = of_find_property(np, "aud_typec_id2_gpio", NULL);
	if (prop) {
		of_property_read_u32(np, "aud_typec_id2_gpio", &typec_id2_gpio);
		pr_info("%s: typec_id2_gpio = %d\n", __func__, typec_id2_gpio);
	} else {
		pr_warn("%s: typec_id2_gpio not found\n", __func__);
		typec_id2_gpio = -1;
	}

	if (typec_id2_gpio < 0)
		return typec_id2_gpio;

	if (!gpio_is_valid(typec_id2_gpio))
	{
		pr_err("%s: gpio is invalid %d", __func__, typec_id2_gpio);
		return -1;
	}

	ret = devm_gpio_request_one(&pdev->dev, typec_id2_gpio, GPIOF_DIR_IN, "aud_typec_id2");
	if (ret < 0) {
		pr_err("DEBUG: gpio request fail: %d  ret = %d\n", typec_id2_gpio, ret);
		return ret;
	}
	

	pinctrl_htc = pinctrl;
	ret = init_pinctrl(pinctrl);
	if (ret < 0)
	{
		pinctrl_htc = NULL;
		pr_err ("init_pinctrl failed !!");
	}

#ifdef CONFIG_SWITCH_FSA3030
	usb_hs_L_switch_handle = fsa3030_switch_register(FSA3030_SWITCH_TYPE_AUDIO);
	usb_hs_R_switch_handle = fsa3030_switch_register(FSA3030_SWITCH_TYPE_AUDIO);
#endif

	pr_info("%s exit\n", __func__);

	return 0;
}

static int audio_pinctrl_remove(struct platform_device *pdev)
{
	if (gpio_is_valid(usb_position_gpio))
		devm_gpio_free(&pdev->dev, usb_position_gpio);
	if (gpio_is_valid(adaptor_mic_det_gpio))
		devm_gpio_free(&pdev->dev, adaptor_mic_det_gpio);
	if (gpio_is_valid(typec_id1_gpio))
		devm_gpio_free(&pdev->dev, typec_id1_gpio);
	if (gpio_is_valid(typec_id2_gpio))
		devm_gpio_free(&pdev->dev, typec_id2_gpio);
	usb_position_gpio = adaptor_mic_det_gpio = typec_id1_gpio = typec_id2_gpio = -1;
	pinctrl_htc = NULL;
	return 0;
}
#endif

static ssize_t acoustic_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	return ret;
}

static ssize_t acoustic_write(struct file *f, const char __user *buf, size_t count, loff_t *offset)
{
	int ret = 0;
	return ret;
}

static struct file_operations acoustic_fops = {
	.owner = THIS_MODULE,
	.open = acoustic_open,
	.read = acoustic_read,
	.write = acoustic_write,
	.release = acoustic_release,
	.unlocked_ioctl = acoustic_ioctl,
	.compat_ioctl = acoustic_ioctl,
};

static struct miscdevice acoustic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "htc-acoustic",
	.fops = &acoustic_fops,
};

#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
static const struct of_device_id audio_pinctrl_of_match[] = {
	{ .compatible = "htc,audio_pinctrl", },
	{ },
};

static struct platform_driver audio_pinctrl_driver = {
	.driver = {
		.name = "htc_audio_pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = audio_pinctrl_of_match,
	},
	.probe = audio_pinctrl_probe,
	.remove = audio_pinctrl_remove,
};
#endif

static int __init acoustic_init(void)
{
	int ret = 0;

#if HTC_MSM8994_BRINGUP_OPTION
	struct headset_notifier notifier;
#endif

#ifdef CONFIG_HTC_TYPEC_HEADSET_DETECT
	ret = platform_driver_register(&audio_pinctrl_driver);

	if (ret < 0) {
		pr_err("failed to register pinctrl driver!\n");
		return ret;
	}
#endif

	ret = misc_register(&acoustic_misc);
	wake_lock_init(&htc_acoustic_wakelock, WAKE_LOCK_SUSPEND, "htc_acoustic");
	wake_lock_init(&htc_acoustic_wakelock_timeout, WAKE_LOCK_SUSPEND, "htc_acoustic_timeout");
	wake_lock_init(&htc_acoustic_dummy_wakelock, WAKE_LOCK_SUSPEND, "htc_acoustic_dummy");

	if (ret < 0) {
		pr_err("failed to register misc device!\n");
		return ret;
	}

	ret = misc_register(&hs_amp_misc);
	if (ret < 0) {
		pr_err("failed to register aud hs amp device!\n");
		return ret;
	}

	sdev_effect_icon.name = "effect_icon";

	ret = switch_dev_register(&sdev_effect_icon);
	if (ret < 0) {
		pr_err("failed to register Effect icon switch device!\n");
		return ret;
	}

	sdev_dq.name = "DQ";

	ret = switch_dev_register(&sdev_dq);
	if (ret < 0) {
		pr_err("failed to register DQ switch device!\n");
		return ret;
	}

	sdev_listen_notification.name = "Listen_notification";

	ret = switch_dev_register(&sdev_listen_notification);
	if (ret < 0) {
		pr_err("failed to register listen_notification switch device!\n");
		return ret;
	}

#if HTC_MSM8994_BRINGUP_OPTION
	notifier.id = HEADSET_REG_HS_INSERT;
	notifier.func = htc_acoustic_hsnotify;
	headset_notifier_register(&notifier);
#endif

	return 0;
}

static void __exit acoustic_exit(void)
{
	misc_deregister(&acoustic_misc);
}

module_init(acoustic_init);
module_exit(acoustic_exit);

