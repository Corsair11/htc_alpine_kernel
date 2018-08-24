/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2624.c
**
** Description:
**     DRV2624 chip driver
**
** =============================================================================
*/
#ifdef CONFIG_VIB_TRIGGERS
#include <linux/vibtrig.h>
#include <linux/slab.h>
#endif
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include "drv2624.h"

#define	AUTOCALIBRATION_ENABLE
static unsigned char cali_data[3] = {0,0,0};
static uint32_t cali_node_data;
static bool cali_flag = 0;


static struct drv2624_platform_data  drv2624_plat_data;
static struct drv2624_data *g_DRV2624data = NULL;


#ifdef CONFIG_OF
static const struct of_device_id vibrator_of_match[] = {
	{.compatible = "htc,vibrator"},
	{},
};
static const struct of_device_id drv2624_mttable[] = {
	{.compatible = "htc_vibrator"},
	{},
};
#endif

static struct pinctrl *vibrator_pinctrl;
static struct pinctrl_state *vibrator_init;

static int drv2624_reg_write(struct drv2624_data *pDrv2624data,
    unsigned char reg, unsigned char val);
static int drv2624_set_bits(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned char mask, unsigned char val);
#define VIB_CALI_NVRAM_FILE_NAME "/data/nvram/APCFG/APRDCL/HTC_VIBDRV2604_CALIBRATION"


struct vib_cali_gain {
	int v1;
	int v2;
};

static int get_data_from_nvram(char *filename, char *buf, ssize_t len, int offset)
{
	struct file *fd;
	int retLen = -1;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	pr_err("[VIB] %s:  offset =%d!!\n", __FUNCTION__, offset);

	fd = filp_open(filename, O_RDONLY, 0644);
	if (IS_ERR(fd)) {
		pr_err("[VIB] %s: failed to open!!\n", __FUNCTION__);
		return -1;
	}

	do {
		if ((fd->f_op == NULL) || (fd->f_op->read == NULL)) {
			pr_err("[VIB]  %s: file can not be read!!\n", __FUNCTION__);
			break;
		}

		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					pr_err("[VIB] %s: failed to seek!!\n", __FUNCTION__);
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		retLen = fd->f_op->read(fd, buf, len, &fd->f_pos);
	} while (0);

	filp_close(fd, NULL);
	set_fs(old_fs);

	return retLen;
}


void htc_load_vib_cali_data(void)
{
	unsigned char nvram_v1[4];
	int ret;
	struct drv2624_data *pDrv2624data = g_DRV2624data;

	
	ret = get_data_from_nvram(VIB_CALI_NVRAM_FILE_NAME,
								(char *)nvram_v1,
								sizeof(int),
								((unsigned long)&(((struct vib_cali_gain *)0)->v1)));
	if (ret >= 0) {
		pr_info("[VIB] %s: nvram_v1 = 0x%x, 0x%x, 0x%x,0x%x, \n", __FUNCTION__, nvram_v1[0],nvram_v1[1], nvram_v1[2],nvram_v1[3] );
		if(nvram_v1[2] >0)
			drv2624_reg_write(pDrv2624data, DRV2624_REG_CAL_COMP, nvram_v1[2]);
		if(nvram_v1[1] >0)
			drv2624_reg_write(pDrv2624data, DRV2624_REG_CAL_BEMF, nvram_v1[1] );
		if(nvram_v1[0] >0)
			drv2624_set_bits(pDrv2624data, DRV2624_REG_LOOP_CONTROL, BEMFGAIN_MASK , nvram_v1[0]);
	}
}

static ssize_t attrs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	return ret;
}

static ssize_t attr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long res;
	int rc;

	rc = kstrtoul(buf, 10, &res);
	if (rc) {
		pr_err("[VIB] invalid parameter, %s %d\n", buf, rc);
		count = -EINVAL;
		goto err_out;
	}

	
	if ( res == 1)
		htc_load_vib_cali_data();

err_out:
	return count;
}

static DEVICE_ATTR(vib_cali_read, S_IRUGO | S_IWUSR, attrs_show, attr_store);

int vibrator_gpio_init(struct platform_device *pdev)
{
	int err = 0;
    vibrator_pinctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(vibrator_pinctrl)) {
        printk("Cannot find vibrator pinctrl!");
        err = PTR_ERR(vibrator_pinctrl);
    }
    
    vibrator_init = pinctrl_lookup_state(vibrator_pinctrl, "vib_init");
    if (IS_ERR(vibrator_init)) {
        err = PTR_ERR(vibrator_init);
          printk("%s : init err, vibrator_init\n", __func__);
    }
    pinctrl_select_state(vibrator_pinctrl, vibrator_init);

		return err;
}

#ifdef CONFIG_VIB_TRIGGERS
struct mtk_vib {
	struct timed_output_dev timed_dev;
	struct vib_trigger_enabler enabler;
};
#endif

static int drv2624_reg_read(struct drv2624_data *pDrv2624data, unsigned char reg)
{
	int ret;
	unsigned int val;

	ret = regmap_read(pDrv2624data->mpRegmap, reg, &val);

	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0x%x error %d\n", __FUNCTION__, reg, ret);
		return ret;
	}else{
		dev_dbg(pDrv2624data->dev, "%s, Reg[0x%x]=0x%x\n",
			__FUNCTION__, reg, val);
		return val;
	}
}

static int drv2624_reg_write(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned char val)
{
	int ret;

	ret = regmap_write(pDrv2624data->mpRegmap, reg, val);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0x%x, value=0%x error %d\n",
			__FUNCTION__, reg, val, ret);
	}else{
		dev_dbg(pDrv2624data->dev, "%s, Reg[0x%x]=0x%x\n",
			__FUNCTION__, reg, val);
	}

	return ret;
}

static int drv2624_bulk_read(struct drv2624_data *pDrv2624data,
	unsigned char reg, u8 *buf, unsigned int count)
{
	int ret;
	ret = regmap_bulk_read(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0%x, count=%d error %d\n",
			__FUNCTION__, reg, count, ret);
	}

	return ret;
}

static int drv2624_bulk_write(struct drv2624_data *pDrv2624data,
	unsigned char reg, const u8 *buf, unsigned int count)
{
	int ret, i;
	ret = regmap_bulk_write(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0%x, count=%d error %d\n",
			__FUNCTION__, reg, count, ret);
	}else{
		for(i = 0; i < count; i++)
			dev_dbg(pDrv2624data->dev, "%s, Reg[0x%x]=0x%x\n",
				__FUNCTION__,reg+i, buf[i]);
	}
	return ret;
}

static int drv2624_set_bits(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned char mask, unsigned char val)
{
	int ret;
	ret = regmap_update_bits(pDrv2624data->mpRegmap, reg, mask, val);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=%x, mask=0x%x, value=0x%x error %d\n",
			__FUNCTION__, reg, mask, val, ret);
	}else{
		dev_dbg(pDrv2624data->dev, "%s, Reg[0x%x]:M=0x%x, V=0x%x\n",
			__FUNCTION__, reg, mask, val);
	}

	return ret;
}

static void drv2624_enableIRQ(struct drv2624_data *pDrv2624data, unsigned char bRTP)
{
#ifdef CONFIG_VIB_INT
	unsigned char mask = INT_ENABLE_CRITICAL;

	if(bRTP == 0){
		mask = INT_ENABLE_ALL;
	}

	drv2624_reg_read(pDrv2624data,DRV2624_REG_STATUS);
	drv2624_reg_write(pDrv2624data, DRV2624_REG_INT_ENABLE, mask);
	enable_irq(pDrv2624data->mnIRQ);
#endif
}

static void drv2624_disableIRQ(struct drv2624_data *pDrv2624data)
{
#ifdef CONFIG_VIB_INT
	disable_irq(pDrv2624data->mnIRQ);
	drv2624_reg_write(pDrv2624data, DRV2624_REG_INT_ENABLE, INT_MASK_ALL);
#endif
}

static int drv2624_set_go_bit(struct drv2624_data *pDrv2624data, unsigned char val)
{
	int ret = 0, value =0;
	int retry = 10; 

	val &= 0x01;
	ret = drv2624_reg_write(pDrv2624data, DRV2624_REG_GO, val);
	if(ret >= 0 ){
		if(val == 1){
			mdelay(1);
			value = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO);
			if(value < 0){
				ret = value;
			}else if(value != 1){
				ret = -1;
				dev_warn(pDrv2624data->dev,
					"%s, GO fail, stop action\n", __FUNCTION__);
			}
		}else{
			while(retry > 0){
				value = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO);
				if(value < 0){
					ret = value;
					break;
				}

				if(value==0) break;
				mdelay(10);
				retry--;
			}

			if(retry == 0){
				dev_err(pDrv2624data->dev,
					"%s, ERROR: clear GO fail\n", __FUNCTION__);
			}
		}
	}

	return ret;
}

static void drv2624_change_mode(struct drv2624_data *pDrv2624data, unsigned char work_mode)
{
	drv2624_set_bits(pDrv2624data, DRV2624_REG_MODE, DRV2624MODE_MASK , work_mode);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2624_data *pDrv2624data = container_of(dev, struct drv2624_data, to_dev);

    if (hrtimer_active(&pDrv2624data->timer)) {
        ktime_t r = hrtimer_get_remaining(&pDrv2624data->timer);
        return ktime_to_ms(r);
    }

    return 0;
}

static void drv2624_stop(struct drv2624_data *pDrv2624data)
{
	if(pDrv2624data->mnVibratorPlaying == YES){
		dev_dbg(pDrv2624data->dev, "%s\n", __FUNCTION__);
		drv2624_disableIRQ(pDrv2624data);
		hrtimer_cancel(&pDrv2624data->timer);
		drv2624_set_go_bit(pDrv2624data, STOP);
		pDrv2624data->mnVibratorPlaying = NO;
		wake_unlock(&pDrv2624data->wklock);
	}
}

static void vibrator_enable( struct timed_output_dev *dev, int value)
{
	int ret = 0;
	struct drv2624_data *pDrv2624data =
		container_of(dev, struct drv2624_data, to_dev);

	dev_dbg(pDrv2624data->dev,
		"%s, value=%d\n", __FUNCTION__, value);

    mutex_lock(&pDrv2624data->lock);

	pDrv2624data->mnWorkMode = WORK_IDLE;
	dev_dbg(pDrv2624data->dev,
		"%s, afer mnWorkMode=0x%x\n",
		__FUNCTION__, pDrv2624data->mnWorkMode);

	drv2624_stop(pDrv2624data);

    if (value > 0) {
		wake_lock(&pDrv2624data->wklock);

		drv2624_change_mode(pDrv2624data, MODE_RTP);
		pDrv2624data->mnVibratorPlaying = YES;
		drv2624_enableIRQ(pDrv2624data, YES);
		ret = drv2624_set_go_bit(pDrv2624data, GO);
		if(ret <0){
			dev_warn(pDrv2624data->dev, "Start RTP failed\n");
			wake_unlock(&pDrv2624data->wklock);
			pDrv2624data->mnVibratorPlaying = NO;
			drv2624_disableIRQ(pDrv2624data);
		}else{
			value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
			hrtimer_start(&pDrv2624data->timer,
				ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
    }

	mutex_unlock(&pDrv2624data->lock);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2624_data *pDrv2624data =
		container_of(timer, struct drv2624_data, timer);

	dev_dbg(pDrv2624data->dev, "%s\n", __FUNCTION__);
	pDrv2624data->mnWorkMode |= WORK_VIBRATOR;
	schedule_work(&pDrv2624data->vibrator_work);

    return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2624_data *pDrv2624data =
		container_of(work, struct drv2624_data, vibrator_work);
	unsigned char mode = MODE_RTP;
	unsigned char status;

	mutex_lock(&pDrv2624data->lock);

	dev_dbg(pDrv2624data->dev,
		"%s, afer mnWorkMode=0x%x\n",
		__FUNCTION__, pDrv2624data->mnWorkMode);

	if(pDrv2624data->mnWorkMode & WORK_IRQ){
		pDrv2624data->mnIntStatus = drv2624_reg_read(pDrv2624data,DRV2624_REG_STATUS);
		drv2624_disableIRQ(pDrv2624data);
		status = pDrv2624data->mnIntStatus;
		dev_dbg(pDrv2624data->dev,
			"%s, status=0x%x\n",
			__FUNCTION__, pDrv2624data->mnIntStatus);

		if(status & OVERCURRENT_MASK){
			dev_err(pDrv2624data->dev,
				"ERROR, Over Current detected!!\n");
		}

		if(status & OVERTEMPRATURE_MASK){
			dev_err(pDrv2624data->dev,
				"ERROR, Over Temperature detected!!\n");
		}

		if(status & ULVO_MASK){
			dev_err(pDrv2624data->dev,
				"ERROR, VDD drop observed!!\n");
		}

		if(status & PRG_ERR_MASK){
			dev_err(pDrv2624data->dev,
				"ERROR, PRG error!!\n");
		}

		if(status & PROCESS_DONE_MASK){
			mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & DRV2624MODE_MASK;
			if(mode == MODE_CALIBRATION){
				pDrv2624data->mAutoCalResult.mnResult = status;
				if((status&DIAG_MASK) != DIAG_SUCCESS){
					dev_err(pDrv2624data->dev, "Calibration fail\n");
				}else{
					pDrv2624data->mAutoCalResult.mnCalComp =
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
					pDrv2624data->mAutoCalResult.mnCalBemf =
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
					pDrv2624data->mAutoCalResult.mnCalGain =
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP) & BEMFGAIN_MASK;
					dev_dbg(pDrv2624data->dev,
						"AutoCal : Comp=0x%x, Bemf=0x%x, Gain=0x%x\n",
						pDrv2624data->mAutoCalResult.mnCalComp,
						pDrv2624data->mAutoCalResult.mnCalBemf,
						pDrv2624data->mAutoCalResult.mnCalGain);
				}
			}else if(mode == MODE_DIAGNOSTIC){
				pDrv2624data->mDiagResult.mnResult = status;
				if((status&DIAG_MASK) != DIAG_SUCCESS){
					dev_err(pDrv2624data->dev, "Diagnostic fail\n");
				}else{
					pDrv2624data->mDiagResult.mnDiagZ =
						drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_Z);
					pDrv2624data->mDiagResult.mnDiagK =
						drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_K);
					dev_dbg(pDrv2624data->dev,
						"Diag : ZResult=0x%x, CurrentK=0x%x\n",
						pDrv2624data->mDiagResult.mnDiagZ,
						pDrv2624data->mDiagResult.mnDiagK);
				}
			}else if(mode == MODE_WAVEFORM_SEQUENCER){
				dev_dbg(pDrv2624data->dev,
					"Waveform Sequencer Playback finished\n");
			}else if(mode == MODE_RTP){
				dev_dbg(pDrv2624data->dev,
					"RTP IRQ\n");
			}
		}
		if((mode != MODE_RTP)&&
			(pDrv2624data->mnVibratorPlaying == YES)){
			dev_info(pDrv2624data->dev, "release wklock\n");
			pDrv2624data->mnVibratorPlaying = NO;
			wake_unlock(&pDrv2624data->wklock);
		}

		pDrv2624data->mnWorkMode &= ~WORK_IRQ;
	}

	if(pDrv2624data->mnWorkMode & WORK_VIBRATOR){
		drv2624_stop(pDrv2624data);
		pDrv2624data->mnWorkMode &= ~WORK_VIBRATOR;
	}

	mutex_unlock(&pDrv2624data->lock);
}

static int dev_auto_calibrate(struct drv2624_data *pDrv2624data)
{
	int ret = 0;

	dev_info(pDrv2624data->dev, "%s\n", __FUNCTION__);
	wake_lock(&pDrv2624data->wklock);
	pDrv2624data->mnVibratorPlaying = YES;
	drv2624_change_mode(pDrv2624data, MODE_CALIBRATION);
	ret = drv2624_set_go_bit(pDrv2624data, GO);
	if(ret < 0){
		dev_warn(pDrv2624data->dev, "calibration start fail\n");
		wake_unlock(&pDrv2624data->wklock);
		pDrv2624data->mnVibratorPlaying = NO;
	}else{
		dev_dbg(pDrv2624data->dev, "calibration start\n");
	}
	return ret;
}

static int dev_run_diagnostics(struct drv2624_data *pDrv2624data)
{
	int ret = 0;

	dev_info(pDrv2624data->dev, "%s\n", __FUNCTION__);
	wake_lock(&pDrv2624data->wklock);
	pDrv2624data->mnVibratorPlaying = YES;
	drv2624_change_mode(pDrv2624data, MODE_DIAGNOSTIC);
	ret = drv2624_set_go_bit(pDrv2624data, GO);
	if(ret < 0){
		dev_warn(pDrv2624data->dev, "Diag start fail\n");
		wake_unlock(&pDrv2624data->wklock);
		pDrv2624data->mnVibratorPlaying = NO;
	}else{
		dev_dbg(pDrv2624data->dev, "Diag start\n");
	}

	return ret;
}

static int drv2624_playEffect(struct drv2624_data *pDrv2624data)
{
	int ret = 0;
	dev_info(pDrv2624data->dev, "%s\n", __FUNCTION__);
	wake_lock(&pDrv2624data->wklock);
	pDrv2624data->mnVibratorPlaying = YES;
	drv2624_change_mode(pDrv2624data, MODE_WAVEFORM_SEQUENCER);
	ret = drv2624_set_go_bit(pDrv2624data, GO);
	if(ret < 0){
		dev_warn(pDrv2624data->dev, "effects start fail\n");
		wake_unlock(&pDrv2624data->wklock);
		pDrv2624data->mnVibratorPlaying = NO;
	}else{
		dev_dbg(pDrv2624data->dev, "effects start\n");
	}

	return ret;
}

static int drv2624_config_waveform(struct drv2624_data *pDrv2624data,
	struct drv2624_wave_setting *psetting)
{
	int ret = 0;
	int value = 0;

	ret = drv2624_reg_write(pDrv2624data,
			DRV2624_REG_MAIN_LOOP, psetting->mnLoop&0x07);
	if(ret >= 0){
		value |= ((psetting->mnInterval&0x01)<<INTERVAL_SHIFT);
		value |= (psetting->mnScale&0x03);
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_CONTROL2, INTERVAL_MASK|SCALE_MASK, value);
	}
	return ret;
}

static int drv2624_set_waveform(struct drv2624_data *pDrv2624data,
	struct drv2624_waveform_sequencer *pSequencer)
{
	int ret = 0;
	int i = 0;
	unsigned char loop[2] = {0};
	unsigned char effects[DRV2624_SEQUENCER_SIZE] = {0};
	unsigned char len = 0;

	for(i = 0; i < DRV2624_SEQUENCER_SIZE; i++){
		len++;
		if(pSequencer->msWaveform[i].mnEffect != 0){
			if(i < 4)
				loop[0] |=
						(pSequencer->msWaveform[i].mnLoop << (2*i));
			else
				loop[1] |=
						(pSequencer->msWaveform[i].mnLoop << (2*(i-4)));

			effects[i] = pSequencer->msWaveform[i].mnEffect;
		}else{
			break;
		}
	}

	if(len == 1)
		ret = drv2624_reg_write(pDrv2624data, DRV2624_REG_SEQUENCER_1, 0);
	else
		ret = drv2624_bulk_write(pDrv2624data, DRV2624_REG_SEQUENCER_1, effects, len);

	if(ret < 0){
		dev_err(pDrv2624data->dev, "sequence error\n");
	}

	if(ret >= 0){
		if(len > 1){
			if((len-1) <= 4)
				drv2624_reg_write(pDrv2624data, DRV2624_REG_SEQ_LOOP_1, loop[0]);
			else
				drv2624_bulk_write(pDrv2624data, DRV2624_REG_SEQ_LOOP_1, loop, 2);
		}
	}

	return ret;
}

static int drv2624_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE)) return -ENODEV;

	file->private_data = (void*)g_DRV2624data;
	return 0;
}

static int drv2624_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void*)NULL;
	module_put(THIS_MODULE);
	return 0;
}

static long drv2624_file_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drv2624_data *pDrv2624data = file->private_data;
	
	int ret = 0;

	mutex_lock(&pDrv2624data->lock);

	dev_dbg(pDrv2624data->dev, "ioctl 0x%x\n", cmd);

	switch (cmd) {

	}

	mutex_unlock(&pDrv2624data->lock);

	return ret;
}

static ssize_t drv2624_file_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)filp->private_data;
	int ret = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pDrv2624data->lock);

	switch(pDrv2624data->mnFileCmd)
	{
		case HAPTIC_CMDID_REG_READ:
		{
			if(length == 1){
				ret = drv2624_reg_read(pDrv2624data, pDrv2624data->mnCurrentReg);
				if( 0 > ret) {
					dev_err(pDrv2624data->dev, "dev read fail %d\n", ret);
				}else{
					value = ret;
					ret = copy_to_user(buff, &value, 1);
					if (0 != ret) {
						
						dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
					}
				}
			}else if(length > 1){
				p_kBuf = (unsigned char *)kzalloc(length, GFP_KERNEL);
				if(p_kBuf != NULL){
					ret = drv2624_bulk_read(pDrv2624data,
						pDrv2624data->mnCurrentReg, p_kBuf, length);
					if( 0 > ret) {
						dev_err(pDrv2624data->dev, "dev bulk read fail %d\n", ret);
					}else{
						ret = copy_to_user(buff, p_kBuf, length);
						if (0 != ret) {
							
							dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
						}
					}

					kfree(p_kBuf);
				}else{
					dev_err(pDrv2624data->dev, "read no mem\n");
					ret = -ENOMEM;
				}
			}
			break;
		}

		case HAPTIC_CMDID_RUN_DIAG:
		{
			if(pDrv2624data->mnVibratorPlaying){
				length = 0;
			}else{
				unsigned char buf[3] ;
				buf[0] = pDrv2624data->mDiagResult.mnResult;
				buf[1] = pDrv2624data->mDiagResult.mnDiagZ;
				buf[2] = pDrv2624data->mDiagResult.mnDiagK;
				ret = copy_to_user(buff, buf, 3);
				if (0 != ret) {
					
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
				}
			}
			break;
		}

		case HAPTIC_CMDID_RUN_CALIBRATION:
		{
			if(pDrv2624data->mnVibratorPlaying){
				length = 0;
			}else{
				unsigned char buf[4] ;
				buf[0] = pDrv2624data->mAutoCalResult.mnResult;
				buf[1] = pDrv2624data->mAutoCalResult.mnCalComp;
				buf[2] = pDrv2624data->mAutoCalResult.mnCalBemf;
				buf[3] = pDrv2624data->mAutoCalResult.mnCalGain;
				ret = copy_to_user(buff, buf, 4);
				if (0 != ret) {
					
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
				}
			}
			break;
		}

		case HAPTIC_CMDID_CONFIG_WAVEFORM:
		{
			if(length == sizeof(struct drv2624_wave_setting)){
				struct drv2624_wave_setting wavesetting;
				value = drv2624_reg_read(pDrv2624data, DRV2624_REG_CONTROL2);
				wavesetting.mnLoop =
					drv2624_reg_read(pDrv2624data, DRV2624_REG_MAIN_LOOP)&0x07;
				wavesetting.mnInterval = ((value&INTERVAL_MASK)>>INTERVAL_SHIFT);
				wavesetting.mnScale = (value&SCALE_MASK);
				ret = copy_to_user(buff, &wavesetting, length);
				if (0 != ret) {
					
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
				}
			}

			break;
		}

		case HAPTIC_CMDID_SET_SEQUENCER:
		{
			if(length == sizeof(struct drv2624_waveform_sequencer)){
				struct drv2624_waveform_sequencer sequencer;
				unsigned char effects[DRV2624_SEQUENCER_SIZE] = {0};
				unsigned char loop[2] = {0};
				int i = 0;
				ret = drv2624_bulk_read(pDrv2624data,
						DRV2624_REG_SEQUENCER_1,
						effects, DRV2624_SEQUENCER_SIZE);
				if(ret < 0){
					dev_err(pDrv2624data->dev, "bulk read error %d\n", ret);
					break;
				}

				ret = drv2624_bulk_read(pDrv2624data,
						DRV2624_REG_SEQ_LOOP_1,
						loop, 2);

				for(i=0; i < DRV2624_SEQUENCER_SIZE; i++){
					sequencer.msWaveform[i].mnEffect = effects[i];
					if(i < 4)
						sequencer.msWaveform[i].mnLoop = ((loop[0]>>(2*i))&0x03);
					else
						sequencer.msWaveform[i].mnLoop = ((loop[1]>>(2*(i-4)))&0x03);
				}

				ret = copy_to_user(buff, &sequencer, length);
				if (0 != ret) {
					
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
				}
			}

			break;
		}

		default:
			pDrv2624data->mnFileCmd = 0;
			break;
	}

	mutex_unlock(&pDrv2624data->lock);

    return length;
}

static ssize_t drv2624_file_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	struct drv2624_data *pDrv2624data =
		(struct drv2624_data *)filp->private_data;
	unsigned char *p_kBuf = NULL;
	int ret = 0;

    mutex_lock(&pDrv2624data->lock);

	p_kBuf = (unsigned char *)kzalloc(len, GFP_KERNEL);
	if(p_kBuf == NULL) {
		dev_err(pDrv2624data->dev, "write no mem\n");
		goto err;
	}

	ret = copy_from_user(p_kBuf, buff, len);
	if (0 != ret) {
		dev_err(pDrv2624data->dev,"copy_from_user failed.\n");
		goto err;
	}

	pDrv2624data->mnFileCmd = p_kBuf[0];

    switch(pDrv2624data->mnFileCmd)
    {
		case HAPTIC_CMDID_REG_READ:
		{
			if(len == 2){
				pDrv2624data->mnCurrentReg = p_kBuf[1];
			}else{
				dev_err(pDrv2624data->dev,
					" read cmd len %d err\n", (int)len);
			}
			break;
		}

		case HAPTIC_CMDID_REG_WRITE:
		{
			if((len-1) == 2){
				drv2624_reg_write(pDrv2624data, p_kBuf[1], p_kBuf[2]);
			}else if((len-1)>2){
				drv2624_bulk_write(pDrv2624data, p_kBuf[1], &p_kBuf[2], len-2);
			}else{
				dev_err(pDrv2624data->dev,
					"%s, reg_write len %d error\n", __FUNCTION__, (int)len);
			}
			break;
		}

		case HAPTIC_CMDID_REG_SETBIT:
		{
			if(len == 4){
				drv2624_set_bits(pDrv2624data, p_kBuf[1], p_kBuf[2], p_kBuf[3]);
			}else{
				dev_err(pDrv2624data->dev,
					"setbit len %d error\n", (int)len);
			}
			break;
		}

		case HAPTIC_CMDID_RUN_DIAG:
		{
			drv2624_stop(pDrv2624data);
			drv2624_enableIRQ(pDrv2624data, NO);
			ret = dev_run_diagnostics(pDrv2624data);
			if(ret < 0)
			drv2624_disableIRQ(pDrv2624data);
			break;
		}

		case HAPTIC_CMDID_RUN_CALIBRATION:
		{
			drv2624_stop(pDrv2624data);
			drv2624_enableIRQ(pDrv2624data, NO);
			ret = dev_auto_calibrate(pDrv2624data);
			if(ret < 0)
			drv2624_disableIRQ(pDrv2624data);
			break;
		}

		case HAPTIC_CMDID_CONFIG_WAVEFORM:
		{
			if(len == (1+ sizeof(struct drv2624_wave_setting))){
				struct drv2624_wave_setting wavesetting;
				memcpy(&wavesetting, &p_kBuf[1],
					sizeof(struct drv2624_wave_setting));
				ret = drv2624_config_waveform(pDrv2624data, &wavesetting);
			}else{
				dev_dbg(pDrv2624data->dev, "pass cmd, prepare for read\n");
			}
		}
		break;

		case HAPTIC_CMDID_SET_SEQUENCER:
		{
			if(len == (1+ sizeof(struct drv2624_waveform_sequencer))){
				struct drv2624_waveform_sequencer sequencer;
				memcpy(&sequencer, &p_kBuf[1],
					sizeof(struct drv2624_waveform_sequencer));
				ret = drv2624_set_waveform(pDrv2624data, &sequencer);
			}else{
				dev_dbg(pDrv2624data->dev, "pass cmd, prepare for read\n");
			}
		}
		break;

		case HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE:
		{
			drv2624_stop(pDrv2624data);
			drv2624_enableIRQ(pDrv2624data, NO);
			ret = drv2624_playEffect(pDrv2624data);
			if(ret < 0)
			drv2624_disableIRQ(pDrv2624data);
			break;
		}

		default:
			dev_err(pDrv2624data->dev,
			"%s, unknown cmd\n", __FUNCTION__);
			break;
    }

err:
	if(p_kBuf != NULL)
		kfree(p_kBuf);

    mutex_unlock(&pDrv2624data->lock);

    return len;
}

#ifdef CONFIG_VIB_TRIGGERS
static void mtk_vib_trigger_enable(struct vib_trigger_enabler *enabler, int value)
{
	struct mtk_vib *vib;
	struct timed_output_dev *dev;

	vib = enabler->trigger_data;
	dev = &vib->timed_dev;

	printk(KERN_INFO "[VIB] vib_trigger=%d.\r\n",value);
	vibrator_enable(&g_DRV2624data->to_dev, value);
	
}
#endif

#ifdef	AUTOCALIBRATION_ENABLE
void drv2624_cali(void)
{
	struct drv2624_data *pDrv2624data = g_DRV2624data;
    struct drv2624_autocal_result autocalResult;
	unsigned char mode,go;
	int ret = 0;

    mutex_lock(&pDrv2624data->lock);
    memset(&autocalResult, 0, sizeof(struct drv2624_autocal_result));
	drv2624_stop(pDrv2624data);
	drv2624_enableIRQ(pDrv2624data,NO);
	ret = dev_auto_calibrate(pDrv2624data);
	if(ret < 0)
	drv2624_disableIRQ(pDrv2624data);

    mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & DRV2624MODE_MASK;
    if(mode != MODE_CALIBRATION){
        autocalResult.mnFinished = -EFAULT;
		dev_dbg(pDrv2624data->dev,"%s, error = 0x%x\n",__FUNCTION__, autocalResult.mnFinished);
    }

	mdelay(1000);
	go = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO) & 0x01;
    if(go){
        autocalResult.mnFinished = NO;
		cali_flag = 0;
		dev_dbg(pDrv2624data->dev,
			"AutoCal not finished  :GO = 0x%x\n",go);
    }else{
		cali_flag = 1;
        autocalResult.mnFinished = YES;
        autocalResult.mnResult =
            ((pDrv2624data->mnIntStatus & DIAG_MASK) >> DIAG_SHIFT);
        autocalResult.mnCalComp = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
        autocalResult.mnCalBemf = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
        autocalResult.mnCalGain =
            drv2624_reg_read(pDrv2624data, DRV2624_REG_LOOP_CONTROL) & BEMFGAIN_MASK;
		dev_dbg(pDrv2624data->dev,
			"AutoCal :Comp=0x%x, Bemf=0x%x, Gain=0x%x\n",
			autocalResult.mnCalComp,autocalResult.mnCalBemf,autocalResult.mnCalGain);

		cali_data[0] = autocalResult.mnCalComp;
		cali_data[1] = autocalResult.mnCalBemf;
		cali_data[2] = autocalResult.mnCalGain;
		cali_node_data = ((cali_data[0] << 16) | (cali_data[1] << 8) | cali_data[2]);
		dev_info(pDrv2624data->dev,"[VIB] calibration success, cali_node_data=0x%x\n",cali_node_data);

		}

		mutex_unlock(&pDrv2624data->lock);

}

static ssize_t vib_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%x\n", cali_flag);
}

static ssize_t vib_calibration_store(struct device *dev,
	struct device_attribute *attr,const char *buf, size_t count)
{
	int input;
	input = simple_strtoul(buf, NULL, 16);

	cali_flag = 0;
	if (input == 1)
		drv2624_cali();

	return count;
}

static DEVICE_ATTR(auto_calibration, 0644, vib_calibration_show,
	vib_calibration_store);

static ssize_t vib_cali_data_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct drv2624_data *pDrv2624data = g_DRV2624data;
    if (!cali_node_data) {
        cali_data[0] = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
        cali_data[1] = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
        cali_data[2] = drv2624_reg_read(pDrv2624data, DRV2624_REG_LOOP_CONTROL) & BEMFGAIN_MASK;
        cali_node_data = ((cali_data[0] << 16) | (cali_data[1] << 8) | cali_data[2]);
    }
    return sprintf(buf, "%x\n", cali_node_data);
}

static ssize_t vib_cali_data_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{

    uint32_t val;
    sscanf(buf, "%x", &val);
    cali_data[0] = (val & (0xff << 16)) >> 16;
    cali_data[1] = (val & (0xff << 8)) >> 8;
    cali_data[2] = (val & (0xff));
	if ((cali_data[0] > 0) && (cali_data[0] < 0xff) && (cali_data[1] > 0) && (cali_data[1] < 0xff)
		&& (cali_data[2] > 0) && (cali_data[2] < 0xff)) {
		cali_flag = 1;
	} else {
		cali_flag = 0;
	}

    printk ("[VIB] REG21:%x   REG22:%x    REG23:%x\n", cali_data[0], cali_data[1], cali_data[2]);

    cali_node_data = val;
    return count;
}

static DEVICE_ATTR(cali_data, 0644, vib_cali_data_show,
        vib_cali_data_store);

#endif

static ssize_t vib_settings_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char vib_setting_data[49];  
	struct drv2624_data *pDrv2624data = g_DRV2624data;
	uint16_t i = 0;
	uint32_t buf_size = 0;

#ifdef AUTOCALIBRATION_ENABLE
	printk(KERN_INFO"[VIB] cali_data:  %x  %x  %x\n", cali_data[0], cali_data[1], cali_data[2]);
#endif

	for ( i = 0x0; i <= 0x30; i++)
		vib_setting_data[i] = 0;

	for ( i = 0x0; i <= 0x30; i++) {
		vib_setting_data[i] = drv2624_reg_read(pDrv2624data, i );
		snprintf(buf+(i*10), 10, "REG%x=%x\n", i, vib_setting_data[i]);
		buf_size += 10;  
		printk("[VIB] I2C READ 0x%x from DRV2624 REG 0x%2x\n", vib_setting_data[i], i);
	}
	return buf_size;

}

static ssize_t vib_settings_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{

    uint32_t reg = 0,val = 0;
	struct drv2624_data *pDrv2624data = g_DRV2624data;
	int ret = 0;

    sscanf(buf, "%x %x", &reg, &val);
	ret = drv2624_reg_write(pDrv2624data, reg, val);
	if (ret < 0)
		printk ("[VIB] vib_settings_store REG%x failed\n", reg);
	else
		printk ("[VIB] vib_settings_store REG%x = %x\n", reg, drv2624_reg_read(pDrv2624data, reg));

	return count;
}

static DEVICE_ATTR(settings, 0644, vib_settings_show, vib_settings_store);

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read = drv2624_file_read,
	.write = drv2624_file_write,
	.unlocked_ioctl = drv2624_file_unlocked_ioctl,
	.open = drv2624_file_open,
	.release = drv2624_file_release,
};

static struct miscdevice drv2624_misc =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = HAPTICS_DEVICE_NAME,
	.fops = &fops,
};

static int Haptics_init(struct drv2624_data *pDrv2624data)
{
    int ret = 0;

	pDrv2624data->to_dev.name = "vibrator";
	pDrv2624data->to_dev.get_time = vibrator_get_time;
	pDrv2624data->to_dev.enable = vibrator_enable;

	ret = timed_output_dev_register(&(pDrv2624data->to_dev));
    if ( ret < 0){
        dev_err(pDrv2624data->dev,
			"drv2624: fail to create timed output dev\n");
        return ret;
    }

	ret = misc_register(&drv2624_misc);
	if (ret) {
		dev_err(pDrv2624data->dev,
			"drv2624 misc fail: %d\n", ret);
		return ret;
	}

	ret = device_create_file(pDrv2624data->to_dev.dev, &dev_attr_auto_calibration);
	if (ret < 0) {
		dev_err(pDrv2624data->dev,"%s: failed on create attr calibration \n", __FUNCTION__);
	}

	ret = device_create_file(pDrv2624data->to_dev.dev, &dev_attr_cali_data);
	if (ret < 0) {
		dev_err(pDrv2624data->dev,"%s: failed on create attr cali_data \n", __FUNCTION__);
	}

	ret = device_create_file(pDrv2624data->to_dev.dev, &dev_attr_settings);
	if (ret < 0) {
		dev_err(pDrv2624data->dev,"%s: failed on create attr settings \n", __FUNCTION__);
	}

	ret = device_create_file(pDrv2624data->to_dev.dev, &dev_attr_vib_cali_read);
	if (ret < 0) {
		dev_err(pDrv2624data->dev,"%s: failed on create attr vib_cali_read \n", __FUNCTION__);
	}

    hrtimer_init(&pDrv2624data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    pDrv2624data->timer.function = vibrator_timer_func;
    INIT_WORK(&pDrv2624data->vibrator_work, vibrator_work_routine);

    wake_lock_init(&pDrv2624data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
    mutex_init(&pDrv2624data->lock);

    return 0;
}

static void dev_init_platform_data(struct drv2624_data *pDrv2624data)
{
	struct drv2624_platform_data *pDrv2624Platdata = &pDrv2624data->msPlatData;
	struct actuator_data actuator = pDrv2624Platdata->msActuator;
	unsigned char value_temp = 0;
	unsigned char mask_temp = 0;

	drv2624_set_bits(pDrv2624data,
		DRV2624_REG_MODE, PINFUNC_MASK, (PINFUNC_INT<<PINFUNC_SHIFT));

	if((actuator.mnActuatorType == ERM)||
		(actuator.mnActuatorType == LRA)){
		mask_temp |= ACTUATOR_MASK;
		value_temp |= (actuator.mnActuatorType << ACTUATOR_SHIFT);
	}

	if((pDrv2624Platdata->mnLoop == CLOSE_LOOP)||
		(pDrv2624Platdata->mnLoop == OPEN_LOOP)){
		mask_temp |= LOOP_MASK;
		value_temp |= (pDrv2624Platdata->mnLoop << LOOP_SHIFT);
	}

	if(value_temp != 0){
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_CONTROL1,
			mask_temp|AUTOBRK_OK_MASK, value_temp|AUTOBRK_OK_ENABLE);
	}

	value_temp = 0;
	if(actuator.mnActuatorType == ERM)
		value_temp = LIB_ERM;
	else if(actuator.mnActuatorType == LRA)
		value_temp = LIB_LRA;
	if(value_temp != 0){
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_CONTROL2, LIB_MASK, value_temp<<LIB_SHIFT);
	}

	if(actuator.mnRatedVoltage != 0){
		drv2624_reg_write(pDrv2624data,
			DRV2624_REG_RATED_VOLTAGE, actuator.mnRatedVoltage);
	}else{
		dev_err(pDrv2624data->dev,
			"%s, ERROR Rated ZERO\n", __FUNCTION__);
	}

	if(actuator.mnOverDriveClampVoltage != 0){
		drv2624_reg_write(pDrv2624data,
			DRV2624_REG_OVERDRIVE_CLAMP, actuator.mnOverDriveClampVoltage);
	}else{
		dev_err(pDrv2624data->dev,
			"%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
	}

	drv2624_reg_write(pDrv2624data,
		DRV2624_REG_OD_CLAMP_TIME, actuator.mnOverDriveClampTime);

	if(actuator.mnActuatorType == LRA){
		unsigned char DriveTime = 5*(1000 - actuator.mnLRAFreq)/actuator.mnLRAFreq;
		unsigned short openLoopPeriod =
			(unsigned short)((unsigned int)1000000000 / (24619 * actuator.mnLRAFreq));

		if(actuator.mnLRAFreq < 125)
			DriveTime |= (MINFREQ_SEL_45HZ << MINFREQ_SEL_SHIFT);
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_DRIVE_TIME,
			DRIVE_TIME_MASK | MINFREQ_SEL_MASK, DriveTime);
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_OL_PERIOD_H, 0x03, (openLoopPeriod&0x0300)>>8);
		drv2624_reg_write(pDrv2624data,
			DRV2624_REG_OL_PERIOD_L, (openLoopPeriod&0x00ff));

		dev_info(pDrv2624data->dev,
			"%s, LRA = %d, DriveTime=0x%x\n",
			__FUNCTION__, actuator.mnLRAFreq, DriveTime);
	}
}

#ifdef CONFIG_VIB_INT
static irqreturn_t drv2624_irq_handler(int irq, void *dev_id)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)dev_id;

	pDrv2624data->mnWorkMode |= WORK_IRQ;
	schedule_work(&pDrv2624data->vibrator_work);
	return IRQ_HANDLED;
}
#endif

static int drv2624_parse_dt(struct device_node* np)
{
	struct drv2624_platform_data *pPlatData = &drv2624_plat_data;
	int rc= 0, ret = 0;
	unsigned int value;

	
	pPlatData->mnGpioNRST = of_get_named_gpio(np, "vib-reset-gpio_ioexp", 0);
	if (pPlatData->mnGpioNRST < 0) {
		printk("%s Looking up %s property in node %s failed %d\n",
			"vib-reset-gpio_ioexp",__func__, np->full_name,
			pPlatData->mnGpioNRST);
		ret = -1;
	}else{
		printk("%s vib-reset-gpio_ioexp=%d\n",__func__, pPlatData->mnGpioNRST);
	}

	if(ret >=0){
		rc = of_property_read_u32(np, "vib-smart-loop", &value);
		if (rc) {
			printk("Looking up %s property in node %s failed %d, value %d\n",
				"vib-smart-loop", np->full_name, rc, value);
			ret = -2;
		}else{
			pPlatData->mnLoop = value&0x01;
		printk("vib-smart-loop = %d\n", pPlatData->mnLoop);
		}
	}


	if(ret >=0){
		rc = of_property_read_u32(np, "vib-actuator", &value);
		if (rc) {
			printk("Looking up %s property in node %s failed %d\n",
				"vib-actuator", np->full_name, rc);
			ret = -2;
		}else{
			pPlatData->msActuator.mnActuatorType = value&0x01;
			printk(
				"vib-actuator=%d\n", pPlatData->msActuator.mnActuatorType);
		}
	}

	if(ret >=0){
		rc = of_property_read_u32(np, "vib-rated-voltage", &value);
		if (rc) {
			printk("Looking up %s property in node %s failed %d\n",
				"vib-rated-voltage", np->full_name, rc);
			ret = -2;
		}else{
			pPlatData->msActuator.mnRatedVoltage = value;
			printk(
				"vib-rated-voltage=0x%x\n", pPlatData->msActuator.mnRatedVoltage);
		}
	}

	if(ret >=0){
		rc = of_property_read_u32(np, "vib-odclamp-voltage", &value);
		if (rc) {
			printk( "Looking up %s property in node %s failed %d\n",
				"vib-odclamp-voltage", np->full_name, rc);
			ret = -2;
		}else{
			pPlatData->msActuator.mnOverDriveClampVoltage = value;
			printk(
				"vib-odclamp-voltage=0x%x\n",
				pPlatData->msActuator.mnOverDriveClampVoltage);
		}
	}
	if(ret >=0){
		rc = of_property_read_u32(np, "vib-odclamp-time", &value);
		if (rc) {
			printk( "Looking up %s property in node %s failed %d\n",
				"vib-odclamp-time", np->full_name, rc);
			ret = -2;
			pPlatData->msActuator.mnOverDriveClampTime = OD_CLAMP_TIME_DEFAUTL;
		}else{
			pPlatData->msActuator.mnOverDriveClampTime = value;
			printk(
				"vib-odclamp-time=0x%x\n",
				pPlatData->msActuator.mnOverDriveClampTime);
		}
	}

	if((ret >=0)&&(pPlatData->msActuator.mnActuatorType == LRA)){
		rc = of_property_read_u32(np, "vib-lra-frequency", &value);
		if (rc) {
			printk( "Looking up %s property in node %s failed %d\n",
				"vib-lra-frequency", np->full_name, rc);
			ret = -3;
		}else{
			if((value >= 45) &&(value <= 300)){
				pPlatData->msActuator.mnLRAFreq = value;
				printk(
					"vib-lra-frequency=%d\n",
					pPlatData->msActuator.mnLRAFreq);
			}else{
				ret = -1;
				printk(
					"ERROR, vib-lra-frequency=%d, out of range\n",
					pPlatData->msActuator.mnLRAFreq);
			}
		}
	}

#ifdef CONFIG_VIB_INT
	if(ret >=0){
		 of_property_read_u32(np, "htc,irq-gpio", &pPlatData->mnGpioINT);
		if (pPlatData->mnGpioINT < 0) {
			printk("Looking up %s property in node %s failed %d\n",
				"htc,irq-gpio", np->full_name,
				pPlatData->mnGpioINT);
			ret = -1;
		}else{
			printk("htc,irq-gpio=%d\n", pPlatData->mnGpioINT);
		}
	}
#endif
	return ret;
}

static struct regmap_config drv2624_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int drv2624_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2624_data *pDrv2624data;
	struct drv2624_platform_data *pDrv2624Platdata = &drv2624_plat_data;
	int err = 0;
#ifdef CONFIG_VIB_TRIGGERS
	struct vib_trigger_enabler *enabler;
#endif

	dev_info(&client->dev, "%s enter\n", __FUNCTION__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		dev_err(&client->dev, "%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	pDrv2624data = devm_kzalloc(&client->dev, sizeof(struct drv2624_data), GFP_KERNEL);
	if (pDrv2624data == NULL){
		dev_err(&client->dev, "%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pDrv2624data->dev = &client->dev;
	i2c_set_clientdata(client,pDrv2624data);
	dev_set_drvdata(&client->dev, pDrv2624data);

	pDrv2624data->mpRegmap = devm_regmap_init_i2c(client, &drv2624_i2c_regmap);
	if (IS_ERR(pDrv2624data->mpRegmap)) {
		err = PTR_ERR(pDrv2624data->mpRegmap);
		dev_err(pDrv2624data->dev,
			"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	memcpy(&pDrv2624data->msPlatData, pDrv2624Platdata, sizeof(struct drv2624_platform_data));
	if((err < 0)
		||(pDrv2624data->msPlatData.mnGpioNRST <= 0)){
		dev_err(pDrv2624data->dev,
			"%s: platform data error\n",__FUNCTION__);
		return -1;
	}

	if(pDrv2624data->msPlatData.mnGpioNRST){
		err = gpio_request(pDrv2624data->msPlatData.mnGpioNRST,"DRV2624-NRST");
		if(err < 0){
			dev_err(pDrv2624data->dev,
				"%s: GPIO %d request NRST error\n",
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioNRST);
			return err;
		}

		gpio_set_value(pDrv2624data->msPlatData.mnGpioNRST, 0);
		mdelay(5);
		gpio_set_value(pDrv2624data->msPlatData.mnGpioNRST, 1);
		mdelay(2);
	}
	err = drv2624_reg_read(pDrv2624data, DRV2624_REG_ID);
	if(err < 0){
		dev_err(pDrv2624data->dev,
			"%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed1;
	}else{
		dev_info(pDrv2624data->dev,
			"%s, ID status (0x%x)\n", __FUNCTION__, err);
		pDrv2624data->mnDeviceID = err;
	}

	if((pDrv2624data->mnDeviceID&0xf0) != DRV2624_ID){
		dev_err(pDrv2624data->dev,
			"%s, device_id(0x%x) fail\n",
			__FUNCTION__, pDrv2624data->mnDeviceID);
		goto exit_gpio_request_failed1;
	}

#ifdef CONFIG_VIB_TRIGGERS
		enabler = kzalloc(sizeof(*enabler), GFP_KERNEL);
		if(!enabler)
			return -ENOMEM;
#endif

	dev_init_platform_data(pDrv2624data);
#ifdef CONFIG_VIB_INT
	if(pDrv2624data->msPlatData.mnGpioINT){
		err = gpio_request(pDrv2624data->msPlatData.mnGpioINT, "DRV2624-IRQ");
		if(err < 0){
			dev_err(pDrv2624data->dev,
				"%s: GPIO %d request INT error\n",
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioINT);
			goto exit_gpio_request_failed1;
		}

		gpio_direction_input(pDrv2624data->msPlatData.mnGpioINT);

		pDrv2624data->mnIRQ = gpio_to_irq(pDrv2624data->msPlatData.mnGpioINT);
		dev_dbg(pDrv2624data->dev, "irq = %d \n", pDrv2624data->mnIRQ);

		err = request_threaded_irq(pDrv2624data->mnIRQ, drv2624_irq_handler,
				NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name, pDrv2624data);
		if (err < 0) {
			dev_err(pDrv2624data->dev,
				"request_irq failed, %d\n", err);
			goto exit_gpio_request_failed2;
		}
		drv2624_disableIRQ(pDrv2624data);
	}
#endif
	g_DRV2624data = pDrv2624data;

    Haptics_init(pDrv2624data);

#ifdef AUTOCALIBRATION_ENABLE
#if 0
	drv2624_enableIRQ(pDrv2624data, NO);
	err = dev_auto_calibrate(pDrv2624data);
	if(err < 0){
		dev_err(pDrv2624data->dev,
			"%s, ERROR, calibration fail\n",
			__FUNCTION__);
	}
#endif
#endif

#ifdef CONFIG_VIB_TRIGGERS
	enabler->name = "mtk-vibrator";
	enabler->default_trigger = "vibrator";
	enabler->enable = mtk_vib_trigger_enable;
	vib_trigger_enabler_register(enabler);
#endif
    dev_info(pDrv2624data->dev,
		"drv2624 probe succeeded\n");

    return 0;
#ifdef CONFIG_VIB_INT
exit_gpio_request_failed2:

	if(pDrv2624data->msPlatData.mnGpioINT > 0){
		gpio_free(pDrv2624data->msPlatData.mnGpioINT);
	}
#endif
exit_gpio_request_failed1:
	if(pDrv2624data->msPlatData.mnGpioNRST > 0){
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);
	}

    dev_err(pDrv2624data->dev,
		"%s failed, err=%d\n",
		__FUNCTION__, err);
	return err;
}

static int drv2624_remove(struct i2c_client* client)
{
	struct drv2624_data *pDrv2624data = i2c_get_clientdata(client);

	if(pDrv2624data->msPlatData.mnGpioNRST)
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);
#ifdef CONFIG_VIB_INT
	if(pDrv2624data->msPlatData.mnGpioINT)
		gpio_free(pDrv2624data->msPlatData.mnGpioINT);
#endif
	misc_deregister(&drv2624_misc);

    return 0;
}

static struct i2c_device_id drv2624_id_table[] =
{
    { HAPTICS_DEVICE_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, drv2624_id_table);

static struct i2c_driver drv2624_driver =
{
    .driver = {
        .name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = drv2624_mttable,
#endif
    },
    .id_table = drv2624_id_table,
    .probe = drv2624_probe,
    .remove = drv2624_remove,
};

static int vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk("[vibrator_probe]~\n");
	ret = drv2624_parse_dt(pdev->dev.of_node);
	if(ret < 0){
		printk("%s, ERROR, drv2624 parse dts fail\n",
			__FUNCTION__);
		return -1;
	}
	ret = i2c_add_driver(&drv2624_driver);
	if(ret == 0) {
		printk("[vibrator_probe] i2c_add_driver OK\n");
		return 0;
	}
		

	return 0;
}

static struct platform_driver vibrator_platform_driver = {
	.probe = vibrator_probe,
	.driver = {
		   .name = "vibrator",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = vibrator_of_match,
#endif
		   },
};


static int __init drv2624_init(void)
{
	int ret;
	ret = platform_driver_register(&vibrator_platform_driver);
	if (ret) {
		printk("[vibrator_init] platform_driver_register failed ~");
		return ret;
	}else{
	printk("[vibrator_init] platform_driver_register ok ~\n");
	}

	return 0;
}

static void __exit drv2624_exit(void)
{
	i2c_del_driver(&drv2624_driver);
}

late_initcall(drv2624_init);
module_exit(drv2624_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
