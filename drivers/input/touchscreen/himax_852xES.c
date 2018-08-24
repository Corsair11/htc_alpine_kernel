/* Himax Android Driver Sample Code for HMX852xES chipset
*
* Copyright (C) 2014 Himax Corporation.
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

#include <linux/himax_852xES.h>

#ifdef CONFIG_SYNC_TOUCH_STATUS
#include <linux/CwMcuSensor.h>
#endif

#define HIMAX_I2C_RETRY_TIMES		10
#define SUPPORT_FINGER_DATA_CHECKSUM	0x0F
#define TS_WAKE_LOCK_TIMEOUT		(2 * HZ)

#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
struct touch_fwu_notifier himax_tp_notifier;
static struct wake_lock flash_wake_lock;
#endif

#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
static u8 proximity_flag = 0;
static u8 g_proximity_en = 0;
#endif

#if defined(HX_AUTO_UPDATE_FW)
static unsigned char i_CTPM_FW[] = {
#include "D816_2014_11_24.i"
};
#endif

static unsigned char	IC_CHECKSUM	= 0;
static unsigned char	IC_TYPE		= 0;

static int		HX_TOUCH_INFO_POINT_CNT = 0;

static int		HX_RX_NUM	= 0;
static int		HX_TX_NUM	= 0;
static int		HX_BT_NUM	= 0;
static int		HX_X_RES	= 0;
static int		HX_Y_RES	= 0;
static int		HX_MAX_PT	= 0;
static bool		HX_XY_REVERSE	= false;
static bool		HX_INT_IS_EDGE	= false;

static unsigned int	FW_VER_MAJ_FLASH_ADDR;
static unsigned int	FW_VER_MAJ_FLASH_LENG;
static unsigned int	FW_VER_MIN_FLASH_ADDR;
static unsigned int	FW_VER_MIN_FLASH_LENG;

static unsigned int	FW_CFG_VER_FLASH_ADDR;

static unsigned int	CFG_VER_MAJ_FLASH_ADDR;
static unsigned int	CFG_VER_MAJ_FLASH_LENG;
static unsigned int	CFG_VER_MIN_FLASH_ADDR;
static unsigned int	CFG_VER_MIN_FLASH_LENG;

#ifdef HX_TP_SYS_DIAG
#ifdef HX_TP_SYS_2T2R
static int		HX_RX_NUM_2	= 0;
static int		HX_TX_NUM_2	= 0;
#endif
static int	touch_monitor_stop_flag	= 0;
static int	touch_monitor_stop_limit = 5;
#endif

static uint8_t	vk_press = 0x00;
static uint8_t	AA_press = 0x00;
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
static uint8_t	IC_STATUS_CHECK	= 0xAA;
#endif
static uint8_t	EN_NoiseFilter	= 0x00;
static uint8_t	Last_EN_NoiseFilter = 0x00;
static int	hx_point_num	= 0;		
static int	p_point_num	= 0xFFFF;
static int	tpd_key		= 0x00;
static int	tpd_key_old	= 0x00;

static bool	config_load	= false;
static struct himax_config *config_selected = NULL;

static uint8_t mfg_flag = 0;
static unsigned int tamper_flag = 0;
static int iref_number = 11;
static bool iref_found = false;

static int irq_enabled_input = 1;
static char *mfg_mode = NULL;

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
static struct kobject *android_touch_kobj = NULL;
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h);
static void himax_ts_late_resume(struct early_suspend *h);
#endif

#ifdef CONFIG_OF
#if defined(HX_LOADIN_CONFIG)
static int himax_parse_config(struct himax_ts_data *ts, struct himax_config *pdata);
#endif
#endif

static int himax_loadSensorConfig(struct i2c_client *client, struct himax_i2c_platform_data *pdata);
static void himax_read_TP_info(struct i2c_client *client);

#ifdef CONFIG_SYNC_TOUCH_STATUS
static void switch_sensor_hub(struct himax_ts_data *ts, int mode);
#endif


#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
static int himax_hand_shaking(void)	
{
	int ret, result;
	uint8_t hw_reset_check[1];
	uint8_t hw_reset_check_2[1];
	uint8_t buf0[2];

	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));
	memset(hw_reset_check_2, 0x00, sizeof(hw_reset_check_2));

	buf0[0] = 0xF2;
	if (IC_STATUS_CHECK == 0xAA) {
		buf0[1] = 0xAA;
		IC_STATUS_CHECK = 0x55;
	} else {
		buf0[1] = 0x55;
		IC_STATUS_CHECK = 0xAA;
	}

	ret = i2c_himax_master_write(private_ts->client, buf0, 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:write 0xF2 failed line: %d \n", __LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	msleep(50);

	buf0[0] = 0xF2;
	buf0[1] = 0x00;
	ret = i2c_himax_master_write(private_ts->client, buf0, 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:write 0x92 failed line: %d \n", __LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	msleep(2);

	ret = i2c_himax_read(private_ts->client, 0x90, hw_reset_check, 1, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:i2c_himax_read 0x90 failed line: %d \n", __LINE__);
		goto work_func_send_i2c_msg_fail;
	}

	if ((IC_STATUS_CHECK != hw_reset_check[0])) {
		msleep(2);
		ret = i2c_himax_read(private_ts->client, 0x90, hw_reset_check_2, 1, DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("[Himax]:i2c_himax_read 0x90 failed line: %d \n", __LINE__);
			goto work_func_send_i2c_msg_fail;
		}

		if (hw_reset_check[0] == hw_reset_check_2[0]) {
			result = 1;
		} else {
			result = 0;
		}
	} else {
		result = 0;
	}

	return result;

work_func_send_i2c_msg_fail:
	return 2;
}
#endif
static int himax_ManualMode(int enter)
{
	uint8_t cmd[2];
	cmd[0] = enter;
	if (i2c_himax_write(private_ts->client, 0x42 , &cmd[0], 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	return 0;
}

static int himax_FlashMode(int enter)
{
	uint8_t cmd[2];
	cmd[0] = enter;
	if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	return 0;
}

static int himax_lock_flash(int enable)
{
	uint8_t cmd[5];

	if (i2c_himax_write(private_ts->client, 0xAA , &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x06;
	if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	cmd[0] = 0x03;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x44 , &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	if (enable != 0) {
		cmd[0] = 0x63;
		cmd[1] = 0x02;
		cmd[2] = 0x70;
		cmd[3] = 0x03;
	} else {
		cmd[0] = 0x63;
		cmd[1] = 0x02;
		cmd[2] = 0x30;
		cmd[3] = 0x00;
	}

	if (i2c_himax_write(private_ts->client, 0x45 , &cmd[0], 4, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	if (i2c_himax_write_command(private_ts->client, 0x4A, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	msleep(50);

	if (i2c_himax_write(private_ts->client, 0xA9 , &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	return 0;
	
}


static void himax_changeIref(int selected_iref)
{

	unsigned char temp_iref[16][2] = {	{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}
	};
	uint8_t cmd[10];
	int i = 0;
	int j = 0;

	I("%s: start to check iref,iref number = %d\n", __func__, selected_iref);

	if (i2c_himax_write(private_ts->client, 0xAA , &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 2; j++) {
			if (selected_iref == 1) {
				temp_iref[i][j] = E_IrefTable_1[i][j];
			} else if (selected_iref == 2) {
				temp_iref[i][j] = E_IrefTable_2[i][j];
			} else if (selected_iref == 3) {
				temp_iref[i][j] = E_IrefTable_3[i][j];
			} else if (selected_iref == 4) {
				temp_iref[i][j] = E_IrefTable_4[i][j];
			} else if (selected_iref == 5) {
				temp_iref[i][j] = E_IrefTable_5[i][j];
			} else if (selected_iref == 6) {
				temp_iref[i][j] = E_IrefTable_6[i][j];
			} else if (selected_iref == 7) {
				temp_iref[i][j] = E_IrefTable_7[i][j];
			}
		}
	}

	if (!iref_found) {
		
		
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		cmd[2] = 0x0A;
		if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 3, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		
		cmd[0] = 0x00;
		cmd[1] = 0x00;
		cmd[2] = 0x00;
		if (i2c_himax_write(private_ts->client, 0x44 , &cmd[0], 3, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return ;
		}

		
		if (i2c_himax_write(private_ts->client, 0x46 , &cmd[0], 0, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return ;
		}

		
		if (i2c_himax_read(private_ts->client, 0x59, cmd, 4, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return ;
		}

		
		for (i = 0; i < 16; i++) {
			if ((cmd[0] == temp_iref[i][0]) &&
			    (cmd[1] == temp_iref[i][1])) {
				iref_number = i;
				iref_found = true;
				break;
			}
		}

		if (!iref_found) {
			E("%s: Can't find iref number!\n", __func__);
			return ;
		} else {
			I("%s: iref_number=%d, cmd[0]=0x%x, cmd[1]=0x%x\n", __func__, iref_number, cmd[0], cmd[1]);
		}
	}

	msleep(5);

	
	
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x06;
	if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x44 , &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	cmd[0] = temp_iref[iref_number][0];
	cmd[1] = temp_iref[iref_number][1];
	cmd[2] = 0x17;
	cmd[3] = 0x28;

	if (i2c_himax_write(private_ts->client, 0x45 , &cmd[0], 4, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	if (i2c_himax_write(private_ts->client, 0x4A , &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x0A;
	if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x44 , &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	if (i2c_himax_write(private_ts->client, 0x46 , &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	
	if (i2c_himax_read(private_ts->client, 0x59, cmd, 4, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ;
	}

	I("%s:cmd[0]=%d,cmd[1]=%d,temp_iref_1=%d,temp_iref_2=%d\n", __func__, cmd[0], cmd[1], temp_iref[iref_number][0], temp_iref[iref_number][1]);

	if (cmd[0] != temp_iref[iref_number][0] || cmd[1] != temp_iref[iref_number][1]) {
		E("%s: IREF Read Back is not match.\n", __func__);
		E("%s: Iref [0]=%d,[1]=%d\n", __func__, cmd[0], cmd[1]);
	} else {
		I("%s: IREF Pass", __func__);
	}

	if (i2c_himax_write(private_ts->client, 0xA9 , &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

}

static uint8_t himax_calculateChecksum(bool change_iref)
{

	int iref_flag = 0;
	uint8_t cmd[10];

	memset(cmd, 0x00, sizeof(cmd));

	
	if (i2c_himax_write(private_ts->client, 0x81 , &cmd[0], 0, DEFAULT_RETRY_CNT) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	msleep(120);

	while (true) {

		if (change_iref) {
			if (iref_flag == 0) {
				himax_changeIref(2); 
			} else if (iref_flag == 1) {
				himax_changeIref(5); 
			} else if (iref_flag == 2) {
				himax_changeIref(1); 
			} else {
				goto CHECK_FAIL;
			}
			iref_flag ++;
		}

		cmd[0] = 0x00;
		cmd[1] = 0x04;
		cmd[2] = 0x0A;
		cmd[3] = 0x02;

		if (i2c_himax_write(private_ts->client, 0xED , &cmd[0], 4, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		cmd[2] = 0x02;

		if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 3, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}
		cmd[0] = 0x05;
		if (i2c_himax_write(private_ts->client, 0xD2 , &cmd[0], 1, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		cmd[0] = 0x01;
		if (i2c_himax_write(private_ts->client, 0x53 , &cmd[0], 1, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		msleep(200);

		if (i2c_himax_read(private_ts->client, 0xAD, cmd, 4, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return -1;
		}

		I("%s 0xAD[0,1,2,3] = %d,%d,%d,%d \n", __func__, cmd[0], cmd[1], cmd[2], cmd[3]);

		if (cmd[0] == 0 && cmd[1] == 0 && cmd[2] == 0 && cmd[3] == 0) {
			himax_FlashMode(0);
			goto CHECK_PASS;
		} else {
			himax_FlashMode(0);
			goto CHECK_FAIL;
		}


CHECK_PASS:
		if (change_iref) {
			if (iref_flag < 3) {
				continue;
			} else {
				return 1;
			}
		} else {
			return 1;
		}

CHECK_FAIL:
		return 0;
	}
	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs(unsigned char *fw, int len, bool change_iref)
{
	unsigned char* ImageBuffer = fw;
	int fullFileLength = len;
	int i, j;
	uint8_t cmd[5], last_byte, prePage;
	int FileLength;
	uint8_t checksumResult = 0;

	I("%s: ++\n", __func__);

	
	for (j = 0; j < 3; j++) {
		FileLength = fullFileLength;

		if (i2c_himax_write(private_ts->client, 0x81 , &cmd[0], 0, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		msleep(120);

		himax_lock_flash(0);

		cmd[0] = 0x05;
		cmd[1] = 0x00;
		cmd[2] = 0x02;
		if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 3, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		if (i2c_himax_write(private_ts->client, 0x4F , &cmd[0], 0, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}
		msleep(50);

		himax_ManualMode(1);
		himax_FlashMode(1);

		FileLength = (FileLength + 3) / 4;
		for (i = 0, prePage = 0; i < FileLength; i++) {
			last_byte = 0;
			cmd[0] = i & 0x1F;
			if (cmd[0] == 0x1F || i == FileLength - 1) {
				last_byte = 1;
			}
			cmd[1] = (i >> 5) & 0x1F;
			cmd[2] = (i >> 10) & 0x1F;
			if (i2c_himax_write(private_ts->client, 0x44 , &cmd[0], 3, DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			if (prePage != cmd[1] || i == 0) {
				prePage = cmd[1];
				cmd[0] = 0x01;
				cmd[1] = 0x09;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}

				cmd[0] = 0x01;
				cmd[1] = 0x0D;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}

				cmd[0] = 0x01;
				cmd[1] = 0x09;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}
			}

			memcpy(&cmd[0], &ImageBuffer[4 * i], 4);
			if (i2c_himax_write(private_ts->client, 0x45 , &cmd[0], 4, DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x0D;
			if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x09;
			if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			if (last_byte == 1) {
				cmd[0] = 0x01;
				cmd[1] = 0x01;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}

				cmd[0] = 0x01;
				cmd[1] = 0x05;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}

				cmd[0] = 0x01;
				cmd[1] = 0x01;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}

				cmd[0] = 0x01;
				cmd[1] = 0x00;
				if (i2c_himax_write(private_ts->client, 0x43 , &cmd[0], 2, DEFAULT_RETRY_CNT) < 0) {
					E("%s: i2c access fail!\n", __func__);
					return 0;
				}

				msleep(10);
				if (i == (FileLength - 1)) {
#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
					touch_fw_update_progress(90);
#endif
					himax_FlashMode(0);
					himax_ManualMode(0);
					checksumResult = himax_calculateChecksum(change_iref);
					
					himax_lock_flash(1);

					if (checksumResult) { 
						I("%s: --\n", __func__);
						return 1;
					} else { 
						E("%s: checksumResult fail!\n", __func__);
						return 0;
					}
				}
			}
#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
			if (i == (3 * FileLength / 4))
				touch_fw_update_progress(75);
			else if (i == (2 * FileLength / 4))
				touch_fw_update_progress(45);
			else if (i == (1 * FileLength / 4))
				touch_fw_update_progress(20);
#endif
		}
	}
	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
#define HMX_FW_NAME "tp_HMX.img"
#include <linux/async.h>
#include <linux/wakelock.h>
#include <linux/string.h>
#include <linux/kthread.h>

int himax_touch_fw_update(struct firmware *vfw)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0, i = 0, tagLen = 0;
	char tag[40];
	const char name[11] = "HIMAX8527E\0";
	char *index = NULL;
	char file_ver_char_maj[3] = {0};
	char file_ver_char_min[3] = {0};
	uint8_t file_ver_maj = 0x00;
	uint8_t file_ver_min = 0x00;
	int FileLength;
	int FirmwareUpdate_Count = 0;

	ts->fw = vfw;
	if (ts->fw == NULL) {
		I("[FW] No firmware file, no firmware update\n");
		goto HMX_FW_REQUEST_FAILURE;
	}
	touch_fw_update_progress(0);

	I("[FW] chip fw version = 0x%02X,%02X\n", ts->vendor_fw_ver_H, ts->vendor_fw_ver_L);
	
	if (vfw->data[0] == 'T' && vfw->data[1] == 'P') {
		while ((tag[i] = vfw->data[i]) != '\n')
			i++;
		tag[i] = '\0';
		tagLen = i + 1;
		index = strstr(tag, name);
		I("[FW] image fw tag = %s\n", tag);

		if (index != NULL) {
			
			if (*(index + strlen(name)) == '_') {
				
				memcpy(file_ver_char_maj, index + strlen(name) + 1, 2);

				if (kstrtou8(file_ver_char_maj, 16, &file_ver_maj) != 0) {
					I("[FW] cannot get major version. Update Bypass\n");
					goto HMX_FW_TAG_FORMAT_WRONG;
				}

				if (*(index + strlen(name) + 1 + 2) == ',') {
					
					memcpy(file_ver_char_min, index + strlen(name)
					       + 1
					       + 2
					       + 1,
					       2  );
					if (kstrtou8(file_ver_char_min, 16, &file_ver_min) != 0) {
						I("[FW] cannot get minor version. Update Bypass\n");
						goto HMX_FW_TAG_FORMAT_WRONG;
					}
				}

				I("[FW] image version: 0x%02X,%02X\n", file_ver_maj, file_ver_min);
				if (file_ver_maj != ts->vendor_fw_ver_H ||
				    file_ver_min != ts->vendor_fw_ver_L) {
					I("[FW] Need Update\n");
				} else {
					I("[FW] Update Bypass\n");
					goto HMX_FW_SAME_VER_IGNORE;
				}
			} else {
				I("[FW] incorrect tag format. Update Bypass\n");
				goto HMX_FW_TAG_FORMAT_WRONG;
			}
		} else {
			I("[FW] pattern %s not found. Update Bypass\n", name);
			goto HMX_FW_TAG_FORMAT_WRONG;
		}
	} else
		I("[FW] No tag, Force Update\n");
	ts->fw_data_start = (u8*)(vfw->data + tagLen);
	ts->fw_size = vfw->size - tagLen;

	atomic_set(&ts->in_flash, 1);
	if (ts->use_irq)
		himax_int_enable(private_ts->client->irq, 0);
	else {
		hrtimer_cancel(&ts->timer);
		cancel_work_sync(&ts->work);
	}
	wake_lock_init(&flash_wake_lock, WAKE_LOCK_SUSPEND, ts->client->name);
	wake_lock(&flash_wake_lock);
	if (wake_lock_active(&flash_wake_lock))
		I("[FW] wake lock successfully acquired!\n");
	else {
		E("[FW] failed to hold wake lock, give up....\n");
		goto WAKE_LOCK_ACQUIRE_FAILED;
	}

	FileLength = ts->fw_size;
	touch_fw_update_progress(10);
doFirmwareUpdate_Retry:
#ifdef HX_RST_PIN_FUNC
	
	himax_HW_reset(false, ts->use_irq);
#endif
	if (fts_ctpm_fw_upgrade_with_sys_fs(ts->fw_data_start, FileLength, true) == 0) {
		if (FirmwareUpdate_Count < 3) {
			FirmwareUpdate_Count++;
			I("%s: TP upgrade Error, Count: %d\n", __func__, FirmwareUpdate_Count);
			goto doFirmwareUpdate_Retry;
		}
		E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
		fw_update_complete = false;
#endif
		ret = -1;	
	} else {
		I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
		fw_update_complete = true;
#endif
		ret = 0;	
	}

	touch_fw_update_progress(95);
#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, ts->use_irq);
#endif

	wake_unlock(&flash_wake_lock);
	wake_lock_destroy(&flash_wake_lock);
	atomic_set(&ts->in_flash, 0);
	if (ts->use_irq)
		himax_int_enable(private_ts->client->irq, 1);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	himax_read_TP_info(private_ts->client);
	I("[FW] firmware flash process complete!\n");
	touch_fw_update_progress(100);

	return ret;
WAKE_LOCK_ACQUIRE_FAILED:
	wake_lock_destroy(&flash_wake_lock);
	if (ts->use_irq)
		himax_int_enable(private_ts->client->irq, 1);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	atomic_set(&ts->in_flash, 0);
HMX_FW_SAME_VER_IGNORE:
HMX_FW_TAG_FORMAT_WRONG:
	return 1;	
HMX_FW_REQUEST_FAILURE:
	return -1;	
}

static int updateFirmware(void *arg)
{
	int ret = 0, i;
	struct himax_ts_data *ts = (struct himax_ts_data *)arg;

	
	I("[FW] Requesting firmware.....\n");
	for (i = 1; i < 4; i++) {
		ret = request_firmware(&ts->fw, HMX_FW_NAME, &ts->client->dev);
		if (ret) {
			I("[FW] trial #%d, Request_firmware failed, ret = %d\n", i, ret);
			continue;
		}
	}

	if (ret == 0)
		ret = himax_touch_fw_update((struct firmware *)ts->fw);

	return ret;
}

int register_himax_touch_fw_update(void)
{
	himax_tp_notifier.fwupdate = himax_touch_fw_update;
	himax_tp_notifier.flash_timeout = 200;
	scnprintf(himax_tp_notifier.fw_vendor, sizeof(himax_tp_notifier.fw_vendor), "%s", "HIMAX");
	scnprintf(himax_tp_notifier.fw_ver, sizeof(himax_tp_notifier.fw_ver), "0x%02X", private_ts->vendor_fw_ver_H);
	return register_fw_update(&himax_tp_notifier);
}
#endif

static int himax_input_register(struct himax_ts_data *ts)
{
	int ret;
	ts->input_dev = devm_input_allocate_device(ts->dev);
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}
	ts->input_dev->name = "himax-touchscreen";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);

	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
#if defined(HX_SMART_WAKEUP)||defined(HX_PALM_REPORT)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, ts->input_dev->keybit);

	
	set_bit(KEY_APPSELECT, ts->input_dev->keybit);

	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	if (ts->protocol_type == PROTOCOL_TYPE_A) {
		
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID,
				     0, 3, 0, 0);
	} else {
		set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
		input_mt_init_slots(ts->input_dev, ts->nFinger_support, 0);
	}

	I("input_set_abs_params: min_x %d, max_x %d, min_y %d, max_y %d\n",
	  ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, ts->pdata->abs_y_min, ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, ts->pdata->abs_width_min, ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);


	return input_register_device(ts->input_dev);
}

static void calcDataSize(uint8_t finger_num)
{
	struct himax_ts_data *ts_data = private_ts;
	ts_data->coord_data_size = 4 * finger_num;
	ts_data->area_data_size = ((finger_num / 4) + (finger_num % 4 ? 1 : 0)) * 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size - ts_data->area_data_size - 4 - 4 - 1;
	ts_data->raw_data_nframes = ((uint32_t)ts_data->x_channel * ts_data->y_channel +
				     ts_data->x_channel + ts_data->y_channel) / ts_data->raw_data_frame_size +
				    (((uint32_t)ts_data->x_channel * ts_data->y_channel +
				      ts_data->x_channel + ts_data->y_channel) % ts_data->raw_data_frame_size) ? 1 : 0;
	I("%s: coord_data_size: %d, area_data_size:%d, raw_data_frame_size:%d, raw_data_nframes:%d", __func__, ts_data->coord_data_size, ts_data->area_data_size, ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

static void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = HX_MAX_PT * 4 ;

	if ((HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (HX_MAX_PT / 4) * 4 ;
	else
		HX_TOUCH_INFO_POINT_CNT += ((HX_MAX_PT / 4) + 1) * 4 ;
}
#ifdef HX_LOADIN_CONFIG
static int himax_config_reg_write(struct i2c_client *client, uint8_t StartAddr, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	char cmd[12] = {0};

	cmd[0] = 0x8C;
	cmd[1] = 0x14;
	if ((i2c_himax_master_write(client, &cmd[0], 2, toRetry)) < 0)
		return -1;

	cmd[0] = 0x8B;
	cmd[1] = 0x00;
	cmd[2] = StartAddr;	
	if ((i2c_himax_master_write(client, &cmd[0], 3, toRetry)) < 0)
		return -1;

	if ((i2c_himax_master_write(client, data, length, toRetry)) < 0)
		return -1;

	cmd[0] = 0x8C;
	cmd[1] = 0x00;
	if ((i2c_himax_master_write(client, &cmd[0], 2, toRetry)) < 0)
		return -1;

	return 0;
}
#endif
void himax_touch_information(void)
{
	char data[12] = {0};

	I("%s:IC_TYPE =%d\n", __func__, IC_TYPE);

	if (IC_TYPE == HX_85XX_ES_SERIES_PWON) {
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x70;
		i2c_himax_master_write(private_ts->client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(10);
		i2c_himax_read(private_ts->client, 0x5A, data, 12, DEFAULT_RETRY_CNT);
		HX_RX_NUM = data[0];				 
		HX_TX_NUM = data[1];				 
		HX_MAX_PT = (data[2] & 0xF0) >> 4; 
#ifdef HX_EN_SEL_BUTTON
		HX_BT_NUM = (data[2] & 0x0F); 
#endif
		if ((data[4] & 0x04) == 0x04) { 
			HX_XY_REVERSE = true;
			HX_Y_RES = data[6] * 256 + data[7]; 
			HX_X_RES = data[8] * 256 + data[9]; 
		} else {
			HX_XY_REVERSE = false;
			HX_X_RES = data[6] * 256 + data[7]; 
			HX_Y_RES = data[8] * 256 + data[9]; 
		}
		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
#ifdef HX_EN_MUT_BUTTON
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x64;
		i2c_himax_master_write(private_ts->client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(10);
		i2c_himax_read(private_ts->client, 0x5A, data, 4, DEFAULT_RETRY_CNT);
		HX_BT_NUM = (data[0] & 0x03);
		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
#endif
#ifdef HX_TP_SYS_2T2R
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);

		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x96;
		i2c_himax_master_write(private_ts->client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(10);

		i2c_himax_read(private_ts->client, 0x5A, data, 10, DEFAULT_RETRY_CNT);

		HX_RX_NUM_2 = data[0];
		HX_TX_NUM_2 = data[1];

		I("%s:Touch Panel Type=%d \n", __func__, data[2]);
		if (data[2] == 0x03) 
			Is_2T2R = true;
		else
			Is_2T2R = false;

		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
#endif
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x02;
		i2c_himax_master_write(private_ts->client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(10);
		i2c_himax_read(private_ts->client, 0x5A, data, 10, DEFAULT_RETRY_CNT);
		if ((data[1] & 0x01) == 1) { 
			HX_INT_IS_EDGE = true;
		} else {
			HX_INT_IS_EDGE = false;
		}
		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(private_ts->client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);
		I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d \n", __func__, HX_RX_NUM, HX_TX_NUM, HX_MAX_PT);

		if (i2c_himax_read(private_ts->client, HX_VER_FW_CFG, data, 1, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
		}
		private_ts->vendor_config_ver = data[0];
		I("config_ver read=%x.\n", private_ts->vendor_config_ver);
	} else {
		HX_RX_NUM				= 0;
		HX_TX_NUM				= 0;
		HX_BT_NUM				= 0;
		HX_X_RES				= 0;
		HX_Y_RES				= 0;
		HX_MAX_PT				= 0;
		HX_XY_REVERSE			= false;
		HX_INT_IS_EDGE			= false;
	}
}
static uint8_t himax_read_Sensor_ID(struct i2c_client *client)
{
	uint8_t val_high[1] = {0}, val_low[1] = {0}, ID0 = 0, ID1 = 0;
	uint8_t sensor_id;
	char data[3];

	data[0] = 0x56;
	data[1] = 0x02;
	data[2] = 0x02;
	i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
	msleep(1);

	
	i2c_himax_read(client, 0x57, val_high, 1, DEFAULT_RETRY_CNT);

	data[0] = 0x56;
	data[1] = 0x01;
	data[2] = 0x01;
	i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
	msleep(1);

	
	i2c_himax_read(client, 0x57, val_low, 1, DEFAULT_RETRY_CNT);

	if ((val_high[0] & 0x01) == 0)
		ID0 = 0x02; 
	else if ((val_low[0] & 0x01) == 0)
		ID0 = 0x01; 
	else
		ID0 = 0x04; 

	if ((val_high[0] & 0x02) == 0)
		ID1 = 0x02; 
	else if ((val_low[0] & 0x02) == 0)
		ID1 = 0x01; 
	else
		ID1 = 0x04; 
	if ((ID0 == 0x04) && (ID1 != 0x04)) {
		data[0] = 0x56;
		data[1] = 0x02;
		data[2] = 0x01;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(1);

	} else if ((ID0 != 0x04) && (ID1 == 0x04)) {
		data[0] = 0x56;
		data[1] = 0x01;
		data[2] = 0x02;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(1);

	} else if ((ID0 == 0x04) && (ID1 == 0x04)) {
		data[0] = 0x56;
		data[1] = 0x02;
		data[2] = 0x02;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(1);

	}
	sensor_id = (ID1 << 4) | ID0;

	data[0] = 0xF3;
	data[1] = sensor_id;
	i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT); 
	msleep(1);

	return sensor_id;

}
static void himax_power_on_initCMD(struct i2c_client *client)
{
	I("%s:\n", __func__);
	
	if (private_ts->chosen_config_process == LOAD_ALL_CFG) {
		i2c_himax_write_command(client, 0x83, DEFAULT_RETRY_CNT);
		msleep(30);

		i2c_himax_write_command(client, 0x81, DEFAULT_RETRY_CNT);
		msleep(50);

		i2c_himax_write_command(client, 0x82, DEFAULT_RETRY_CNT);
		msleep(50);

		i2c_himax_write_command(client, 0x80, DEFAULT_RETRY_CNT);
		msleep(50);

		himax_touch_information();
	}

	i2c_himax_write_command(client, 0x83, DEFAULT_RETRY_CNT);
	msleep(30);

	i2c_himax_write_command(client, 0x81, DEFAULT_RETRY_CNT);
	msleep(50);
}

#ifdef HX_AUTO_UPDATE_FW
static int i_update_FW(void)
{
	int upgrade_times = 0;
	unsigned char* ImageBuffer = i_CTPM_FW;
	int fullFileLength = sizeof(i_CTPM_FW);

	I("IMAGE FW_VER=%x,%x.\n", i_CTPM_FW[FW_VER_MAJ_FLASH_ADDR], i_CTPM_FW[FW_VER_MIN_FLASH_ADDR]);
	I("IMAGE CFG_VER=%x.\n", i_CTPM_FW[FW_CFG_VER_FLASH_ADDR]);
	if ((private_ts->vendor_fw_ver_H < i_CTPM_FW[FW_VER_MAJ_FLASH_ADDR])
	    || (private_ts->vendor_fw_ver_L < i_CTPM_FW[FW_VER_MIN_FLASH_ADDR])
	    || (private_ts->vendor_config_ver < i_CTPM_FW[FW_CFG_VER_FLASH_ADDR])
	    || (himax_calculateChecksum(false) == 0)) {
update_retry:
#ifdef HX_RST_PIN_FUNC
		himax_HW_reset(false, true);
#endif
		if (fts_ctpm_fw_upgrade_with_sys_fs(ImageBuffer, fullFileLength, true) == 0)
			E("%s: TP upgrade error\n", __func__);
			upgrade_times++;
			if (upgrade_times < 3)
				goto update_retry;
			else
				return -1;
		else {
			I("%s: TP upgrade OK\n", __func__);
			private_ts->vendor_fw_ver_H = i_CTPM_FW[FW_VER_MAJ_FLASH_ADDR];
			private_ts->vendor_fw_ver_L = i_CTPM_FW[FW_VER_MIN_FLASH_ADDR];
			private_ts->vendor_config_ver = i_CTPM_FW[FW_CFG_VER_FLASH_ADDR];
		}
#ifdef HX_RST_PIN_FUNC
		himax_HW_reset(false, true);
#endif
		return 1;
	} else
		return 0;
}
#endif

static int himax_loadSensorConfig(struct i2c_client *client, struct himax_i2c_platform_data *pdata)
{
#if defined(HX_LOADIN_CONFIG)
	int rc = 0;
#endif
#ifndef CONFIG_OF
#if defined(HX_LOADIN_CONFIG)
	int i = 0;
#endif
#endif
#ifdef HX_ESD_WORKAROUND
	char data[12] = {0};
#endif

	if (!client) {
		E("%s: Necessary parameters client are null!\n", __func__);
		return -1;
	}

	if (config_load == false) {
		config_selected = kzalloc(sizeof(*config_selected), GFP_KERNEL);
		if (config_selected == NULL) {
			E("%s: alloc config_selected fail!\n", __func__);
			return -1;
		}
	}
#ifndef CONFIG_OF
	pdata = client->dev.platform_data;
	if (!pdata) {
		E("%s: Necessary parameters pdata are null!\n", __func__);
		return -1;
	}
#endif

#if defined(HX_LOADIN_CONFIG)
#ifdef CONFIG_OF
	I("%s, config_selected, %p\n", __func__ , config_selected);
	if (config_load == false) {
		rc = himax_parse_config(private_ts, config_selected);
		if (rc < 0) {
			E(" DT:cfg table parser FAIL. ret=%d\n", rc);
			goto HimaxErr;
		} else if (rc == 0) {
			if ((private_ts->tw_x_max) && (private_ts->tw_y_max)) {
				pdata->abs_x_min = private_ts->tw_x_min;
				pdata->abs_x_max = private_ts->tw_x_max;
				pdata->abs_y_min = private_ts->tw_y_min;
				pdata->abs_y_max = private_ts->tw_y_max;
				I(" DT-%s:config-panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
				  pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
			}
			if ((private_ts->pl_x_max) && (private_ts->pl_y_max)) {
				pdata->screenWidth = private_ts->pl_x_max;
				pdata->screenHeight = private_ts->pl_y_max;
				I(" DT-%s:config-display-coords = (%d, %d)", __func__, pdata->screenWidth,
				  pdata->screenHeight);
			}
			config_load = true;
			I(" DT parser Done\n");
		}
	}
#else
	I("pdata->hx_config_size=%x.\n", (pdata->hx_config_size + 1));
	I("config_type_size=%x.\n", sizeof(struct himax_config));

	if (pdata->hx_config) {
		for (i = 0; i < pdata->hx_config_size / sizeof(struct himax_config); ++i) {
			I("(pdata->hx_config)[%x].version=%x.\n", i, (pdata->hx_config)[i].fw_ver);
			I("(pdata->hx_config)[%x].sensor_id=%x.\n", i, (pdata->hx_config)[i].sensor_id);

			if (private_ts->vendor_fw_ver_H < (pdata->hx_config)[i].fw_ver) {
				continue;
			} else {
				if ((private_ts->vendor_sensor_id == (pdata->hx_config)[i].sensor_id)) {
					config_selected = &((pdata->hx_config)[i]);
					I("hx_config selected, %X\n", (uint32_t)config_selected);
					config_load = true;
					break;
				} else if ((pdata->hx_config)[i].default_cfg) {
					I("default_cfg detected.\n");
					config_selected = &((pdata->hx_config)[i]);
					I("hx_config selected, %X\n", (uint32_t)config_selected);
					config_load = true;
					break;
				}
			}
		}
	} else {
		E("[HimaxError] %s pdata->hx_config is not exist \n", __func__);
		goto HimaxErr;
	}
#endif
#endif
#ifdef HX_LOADIN_CONFIG
	if (config_selected) {
		if (private_ts->chosen_config_process == LOAD_SHORT_CFG) {
			
			private_ts->vendor_config_ver = config_selected->c40[1];
			
			i2c_himax_master_write(client, config_selected->c21, sizeof(config_selected->c21), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c22, sizeof(config_selected->c22), DEFAULT_RETRY_CNT);

			
			himax_config_reg_write(client, 0x00, config_selected->c40, CUSTOMER_INFO_LENGTH, DEFAULT_RETRY_CNT);

			msleep(1);
		} else if (private_ts->chosen_config_process == LOAD_ALL_CFG) {
			
			private_ts->vendor_config_ver = config_selected->c40[1];

			
			i2c_himax_master_write(client, config_selected->c1, sizeof(config_selected->c1), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c2, sizeof(config_selected->c2), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c3, sizeof(config_selected->c3), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c4, sizeof(config_selected->c4), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c5, sizeof(config_selected->c5), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c6, sizeof(config_selected->c6), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c7, sizeof(config_selected->c7), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c8, sizeof(config_selected->c8), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c9, sizeof(config_selected->c9), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c10, sizeof(config_selected->c10), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c11, sizeof(config_selected->c11), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c12, sizeof(config_selected->c12), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c13, sizeof(config_selected->c13), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c14, sizeof(config_selected->c14), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c15, sizeof(config_selected->c15), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c16, sizeof(config_selected->c16), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c17, sizeof(config_selected->c17), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c18, sizeof(config_selected->c18), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c19, sizeof(config_selected->c19), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c20, sizeof(config_selected->c20), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c21, sizeof(config_selected->c21), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c22, sizeof(config_selected->c22), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c23, sizeof(config_selected->c23), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c24, sizeof(config_selected->c24), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c25, sizeof(config_selected->c25), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c26, sizeof(config_selected->c26), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c27, sizeof(config_selected->c27), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c28, sizeof(config_selected->c28), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c29, sizeof(config_selected->c29), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c30, sizeof(config_selected->c30), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c31, sizeof(config_selected->c31), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c32, sizeof(config_selected->c32), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c33, sizeof(config_selected->c33), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c34, sizeof(config_selected->c34), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c35, sizeof(config_selected->c35), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c36, sizeof(config_selected->c36), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c37, sizeof(config_selected->c37), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c38, sizeof(config_selected->c38), DEFAULT_RETRY_CNT);
			i2c_himax_master_write(client, config_selected->c39, sizeof(config_selected->c39), DEFAULT_RETRY_CNT);

			
			himax_config_reg_write(client, 0x00, config_selected->c40, sizeof(config_selected->c40), DEFAULT_RETRY_CNT);
			himax_config_reg_write(client, 0x9E, config_selected->c41, sizeof(config_selected->c41), DEFAULT_RETRY_CNT);

			msleep(1);
		}
	} else {
		E("[HimaxError] %s config_selected is null.\n", __func__);
		goto HimaxErr;
	}
#endif
#ifdef HX_ESD_WORKAROUND
	
	i2c_himax_read(client, 0x36, data, 2, 10);
	if (data[0] != 0x0F || data[1] != 0x53) {
		
		E("[HimaxError] %s R36 Fail : R36[0]=%d,R36[1]=%d,R36 Counter=%d \n", __func__, data[0], data[1], ESD_R36_FAIL);
		return -1;
	}
#endif

	himax_power_on_initCMD(client);

	I("%s: initialization complete\n", __func__);

	return 1;
#if defined(HX_LOADIN_CONFIG)
HimaxErr:
	return -1;
#endif
}

#ifdef HX_RST_PIN_FUNC
void himax_HW_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;

	HW_RESET_ACTIVATE = 1;

	if (ts->rst_gpio) {
		if (!int_off) {
			if (ts->use_irq)
				himax_int_enable(private_ts->client->irq, 0);
			else {
				hrtimer_cancel(&ts->timer);
				cancel_work_sync(&ts->work);
			}
		}

		I("%s: Now reset the Touch chip.\n", __func__);

		himax_rst_gpio_set(ts->rst_gpio, 0);
		msleep(40);
		himax_rst_gpio_set(ts->rst_gpio, 1);
		msleep(20);

		if (loadconfig)
			himax_loadSensorConfig(private_ts->client, private_ts->pdata);

		if (!int_off) {
			if (ts->use_irq)
				himax_int_enable(private_ts->client->irq, 1);
			else
				hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		}
	}
}
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
static u8 himax_read_FW_ver(bool hw_reset)
{
	uint8_t cmd[3];

	memset(cmd, 0x00, sizeof(cmd));

#ifdef HX_RST_PIN_FUNC
	if (hw_reset) {
		himax_HW_reset(false, false);
	}
#else
	if (ts->use_irq)
		himax_int_enable(private_ts->client->irq, 0);
#endif

	msleep(120);
	if (i2c_himax_read(private_ts->client, HX_VER_FW_MAJ, cmd, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	private_ts->vendor_fw_ver_H = cmd[0];
	if (i2c_himax_read(private_ts->client, HX_VER_FW_MIN, cmd, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	private_ts->vendor_fw_ver_L = cmd[0];
	I("FW_VER : %d,%d \n", private_ts->vendor_fw_ver_H, private_ts->vendor_fw_ver_L);

	if (i2c_himax_read(private_ts->client, HX_VER_FW_CFG, cmd, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	private_ts->vendor_config_ver = cmd[0];
	I("CFG_VER : %d \n", private_ts->vendor_config_ver);

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, false);
#else
	if (ts->use_irq)
		himax_int_enable(private_ts->client->irq, 1);
#endif

	return 0;
}
#endif
static bool himax_ic_package_check(struct himax_ts_data *ts)
{
	uint8_t cmd[3];

	memset(cmd, 0x00, sizeof(cmd));

	if (i2c_himax_read(ts->client, 0xD1, cmd, 3, DEFAULT_RETRY_CNT) < 0)
		return false ;

	if (cmd[0] == 0x05 && cmd[1] == 0x85 &&
	    (cmd[2] == 0x25 || cmd[2] == 0x26 || cmd[2] == 0x27 || cmd[2] == 0x28)) {
		IC_TYPE			= HX_85XX_ES_SERIES_PWON;
		IC_CHECKSUM		= HX_TP_BIN_CHECKSUM_CRC;
		
		FW_VER_MAJ_FLASH_ADDR	= 133;	
		FW_VER_MAJ_FLASH_LENG	= 1;;
		FW_VER_MIN_FLASH_ADDR	= 134;	
		FW_VER_MIN_FLASH_LENG	= 1;
		CFG_VER_MAJ_FLASH_ADDR	= 160;	
		CFG_VER_MAJ_FLASH_LENG	= 12;
		CFG_VER_MIN_FLASH_ADDR	= 172;	
		CFG_VER_MIN_FLASH_LENG	= 12;
		FW_CFG_VER_FLASH_ADDR	= 132;	
		I("Himax IC package 852x ES\n");
	} else {
		E("Himax IC package incorrect!!PKG[0~2]=0x%02X,%02X,%02X\n", cmd[0], cmd[1], cmd[2]);
		return false ;
	}
	return true;
}

static void himax_read_TP_info(struct i2c_client *client)
{
	char data[12] = {0};

	
	if (i2c_himax_read(client, HX_VER_FW_MAJ, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	private_ts->vendor_fw_ver_H = data[0];

	if (i2c_himax_read(client, HX_VER_FW_MIN, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	private_ts->vendor_fw_ver_L = data[0];
	
	if (i2c_himax_read(client, HX_VER_FW_CFG, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	private_ts->vendor_config_ver = data[0];

	
	private_ts->vendor_sensor_id = himax_read_Sensor_ID(client);

	I("sensor_id=%x.\n", private_ts->vendor_sensor_id);
	I("fw_ver=%x,%x.\n", private_ts->vendor_fw_ver_H, private_ts->vendor_fw_ver_L);
	I("config_ver=%x.\n", private_ts->vendor_config_ver);
}

#ifdef HX_ESD_WORKAROUND
void ESD_HW_REST(void)
{
	ESD_RESET_ACTIVATE = 1;
	ESD_COUNTER = 0;
	ESD_R36_FAIL = 0;
#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
#endif
	I("START_Himax TP: ESD - Reset\n");

	while (ESD_R36_FAIL <= 3) {

		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(40);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(20);

		if (himax_loadSensorConfig(private_ts->client, private_ts->pdata) < 0)
			ESD_R36_FAIL++;
		else
			break;
	}
	I("END_Himax TP: ESD - Reset\n");
}
#endif

#ifdef HX_CHIP_STATUS_MONITOR
static void himax_chip_monitor_function(struct work_struct *work) 
{
	int ret = 0;

	I(" %s: POLLING_COUNT=%x, STATUS=%x \n", __func__, HX_CHIP_POLLING_COUNT, ret);
	if (HX_CHIP_POLLING_COUNT >= (HX_POLLING_TIMES - 1)) { 
		HX_ON_HAND_SHAKING = 1;
		ret = himax_hand_shaking(); 
		HX_ON_HAND_SHAKING = 0;
		if (ret == 2) {
			I(" %s: I2C Fail \n", __func__);
			ESD_HW_REST();
		} else if (ret == 1) {
			I(" %s: MCU Stop \n", __func__);
			ESD_HW_REST();
		}
		HX_CHIP_POLLING_COUNT = 0; 
	} else
		HX_CHIP_POLLING_COUNT++;

	queue_delayed_work(private_ts->himax_chip_monitor_wq, &private_ts->himax_chip_monitor, HX_POLLING_TIMER * HZ);

	return;
}
#endif

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
static DEFINE_MUTEX(himax_block_mutex);
static void himax_block_touch(struct himax_ts_data *ts, int enable)
{
	mutex_lock(&himax_block_mutex);
	ts->hall_block_touch_event = enable;
	mutex_unlock(&himax_block_mutex);
	I("%s: %d\n", __func__, ts->hall_block_touch_event);
}

static void himax_block_touch_work_func(struct work_struct *dummy)
{
	struct himax_ts_data *ts = private_ts;
	himax_block_touch(ts, 0);
}
static DECLARE_DELAYED_WORK(himax_block_touch_work, himax_block_touch_work_func);

static void himax_handle_block_touch(struct himax_ts_data *ts, int enable)
{
	if (ts->hall_block_touch_event) {
		cancel_delayed_work(&himax_block_touch_work);
		himax_block_touch(ts, 0);
	}
	if (enable) {
		I("[HL] %s: %d\n", __func__, ts->hall_block_touch_time);
		schedule_delayed_work(&himax_block_touch_work, HZ * (ts->hall_block_touch_time / 1000));
		himax_block_touch(ts, 1);
	}
}

static void himax_set_cover_func(unsigned int enable)
{
	uint8_t cmd[4];
	struct himax_ts_data *ts = private_ts;

	if (enable)
		cmd[0] = 0x40;
	else
		cmd[0] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x8F , &cmd[0], 1, DEFAULT_RETRY_CNT) < 0)
		E("%s i2c write fail.\n", __func__);

	if (enable == 0)
		himax_handle_block_touch(ts, 0);

	return;
}

static int hallsensor_hover_status_handler_func(struct notifier_block *this,
		unsigned long status, void *unused)
{
	int pole = 0, pole_value = 0;
	struct himax_ts_data *ts = private_ts;

	pole_value = 0x1 & status;
	pole = (0x2 & status) >> HALL_POLE_BIT;
	I("[HL] %s[%s]\n", pole ? "att_s" : "att_n", pole_value ? "Near" : "Far");

	if (pole == HALL_POLE_S) {
		if (pole_value == HALL_FAR)
			ts->cover_enable = 0;
		else {
			ts->cover_enable = 1;
			if (ts->hall_block_touch_time > 0)
				himax_handle_block_touch(ts, 1);
		}

#ifdef CONFIG_SYNC_TOUCH_STATUS
		if (!ts->i2c_to_mcu)
#endif
		{
			himax_set_cover_func(ts->cover_enable);
			I("[HL] cover_enable = %d.\n", ts->cover_enable);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block hallsensor_status_handler = {
	.notifier_call = hallsensor_hover_status_handler_func,
};
#endif	
#endif

#ifdef HX_SMART_WAKEUP
static int himax_parse_wake_event(struct himax_ts_data *ts)
{
	uint8_t buf[5];
	if (i2c_himax_read(ts->client, 0x86, buf, 4, HIMAX_I2C_RETRY_TIMES))
		E("%s: can't read data from chip!\n", __func__);

	if ((buf[0] == 0x57) && (buf[1] == 0x61) && (buf[2] == 0x6B) && (buf[3] == 0x65)) {
		I("%s: WAKE UP system!\n", __func__);
		return 1;
	} else {
		I("%s: NOT WKAE packet, SKIP!\n", __func__);
		I("buf[0]=%x, buf[1]=%x, buf[2]=%x, buf[3]=%x\n", buf[0], buf[1], buf[2], buf[3]);
		return 0;
	}
}
#endif

static void himax_ts_button_func(int tp_key_index, struct himax_ts_data *ts)
{
	uint16_t x_position = 0, y_position = 0;
	if (tp_key_index != 0x00) {
		I("virtual key index =%x\n", tp_key_index);
		if (tp_key_index == 0x01) {
			vk_press = 1;
			I("back key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[0].index) {
					x_position = (ts->button[0].x_range_min + ts->button[0].x_range_max) / 2;
					y_position = (ts->button[0].y_range_min + ts->button[0].y_range_max) / 2;
				}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0) {
#endif	
#endif
					if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
								 x_position);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
								 y_position);
						input_mt_sync(ts->input_dev);
					} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
									   1);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
								 x_position);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
								 y_position);
					}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				}
#endif	
#endif
			} else {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0)
#endif	
#endif
					input_report_key(ts->input_dev, KEY_BACK, 1);
			}
		} else if (tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[1].index) {
					x_position = (ts->button[1].x_range_min + ts->button[1].x_range_max) / 2;
					y_position = (ts->button[1].y_range_min + ts->button[1].y_range_max) / 2;
				}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0) {
#endif	
#endif
					if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
								 x_position);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
								 y_position);
						input_mt_sync(ts->input_dev);
					} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
									   1);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
								 x_position);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
								 y_position);
					}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				}
#endif	
#endif
			} else {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0)
#endif	
#endif
					input_report_key(ts->input_dev, KEY_HOME, 1);
			}
		} else if (tp_key_index == 0x04) {
			vk_press = 1;
			I("APP_switch key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[2].index) {
					x_position = (ts->button[2].x_range_min + ts->button[2].x_range_max) / 2;
					y_position = (ts->button[2].y_range_min + ts->button[2].y_range_max) / 2;
				}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0) {
#endif	
#endif
					if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
								 x_position);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
								 y_position);
						input_mt_sync(ts->input_dev);
					} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
									   1);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
								 100);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
								 x_position);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
								 y_position);
					}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				}
#endif	
#endif
			} else {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0)
#endif	
#endif
					
					input_report_key(ts->input_dev, KEY_APPSELECT, 1);
			}
		}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		if (ts->hall_block_touch_event == 0)
#endif	
#endif
			input_sync(ts->input_dev);
	} else { 
		I("virtual key released\n");
		vk_press = 0;
		if (ts->protocol_type == PROTOCOL_TYPE_A) {
			input_mt_sync(ts->input_dev);
		} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
			input_mt_slot(ts->input_dev, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		}
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		if (ts->hall_block_touch_event == 0) {
#endif	
#endif
			input_report_key(ts->input_dev, KEY_BACK, 0);
			input_report_key(ts->input_dev, KEY_HOME, 0);
			
			input_report_key(ts->input_dev, KEY_APPSELECT, 0);
			input_sync(ts->input_dev);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		}
#endif	
#endif
	}
}

inline void himax_ts_work(struct himax_ts_data *ts)
{
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	int ret = 0;
#endif
	uint8_t buf[128], finger_num, hw_reset_check[2];
	uint16_t finger_pressed;
	uint8_t finger_on = 0;
	int32_t loop_i;
	uint8_t coordInfoSize = ts->coord_data_size + ts->area_data_size + 4;
	unsigned char check_sum_cal = 0;
	int RawDataLen = 0;
	int raw_cnt_max ;
	int raw_cnt_rmd ;
	int hx_touch_info_size;
	int base, x, y, w;

#ifdef HX_TP_SYS_DIAG
	uint8_t *mutual_data;
	uint8_t *self_data;
	uint8_t diag_cmd;
	int	i;
	int 	mul_num;
	int 	self_num;
	int 	index = 0;
	int	temp1, temp2;
	
	char coordinate_char[15 + (HX_MAX_PT + 5) * 2 * 5 + 2];
	struct timeval t;
	struct tm broken;
	
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	int j = 0;
#endif
	memset(buf, 0x00, sizeof(buf));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	if (HX_ON_HAND_SHAKING) { 
		for (j = 0; j < 100; j++) {
			if (HX_ON_HAND_SHAKING == 0) { 
				I("%s:HX_ON_HAND_SHAKING OK check %d times\n", __func__, j);
				break;
			} else
				msleep(1);
		}
		if (j == 100) {
			E("%s:HX_ON_HAND_SHAKING timeout reject interrupt\n", __func__);
			return;
		}
	}
#endif
	raw_cnt_max = HX_MAX_PT / 4;
	raw_cnt_rmd = HX_MAX_PT % 4;

	if (raw_cnt_rmd != 0x00) { 
		RawDataLen = 128 - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;
		hx_touch_info_size = (HX_MAX_PT + raw_cnt_max + 2) * 4;
	} else { 
		RawDataLen = 128 - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;
		hx_touch_info_size = (HX_MAX_PT + raw_cnt_max + 1) * 4;
	}

#ifdef HX_TP_SYS_DIAG
	diag_cmd = getDiagCommand();
#ifdef HX_ESD_WORKAROUND
	if ((diag_cmd) || (ESD_RESET_ACTIVATE) || (HW_RESET_ACTIVATE))
#else
	if ((diag_cmd) || (HW_RESET_ACTIVATE))
#endif
	{
		ret = i2c_himax_read(ts->client, 0x86, buf, 128, HIMAX_I2C_RETRY_TIMES);
	} else {
		if (touch_monitor_stop_flag != 0) {
			ret = i2c_himax_read(ts->client, 0x86, buf, 128, HIMAX_I2C_RETRY_TIMES);
			touch_monitor_stop_flag-- ;
		} else {
			ret = i2c_himax_read(ts->client, 0x86, buf, hx_touch_info_size, HIMAX_I2C_RETRY_TIMES);
		}
	}
	if (ret)
#else
	if (i2c_himax_read(ts->client, 0x86, buf, hx_touch_info_size, HIMAX_I2C_RETRY_TIMES))
#endif
	{
		E("%s: can't read data from chip!\n", __func__);
		goto err_workqueue_out;
	} else {
#ifdef HX_ESD_WORKAROUND
		for (i = 0; i < hx_touch_info_size; i++) {
			if (buf[i] == 0x00) { 
				check_sum_cal = 1;
			} else if (buf[i] == 0xED) { 
				check_sum_cal = 2;
			} else {
				check_sum_cal = 0;
				i = hx_touch_info_size;
			}
		}

		
#ifdef HX_TP_SYS_DIAG
		diag_cmd = getDiagCommand();
#ifdef HX_ESD_WORKAROUND
		if (check_sum_cal != 0 && ESD_RESET_ACTIVATE == 0 && HW_RESET_ACTIVATE == 0 && diag_cmd == 0)	
#else
		if (check_sum_cal != 0 && diag_cmd == 0)
#endif
#else
#ifdef HX_ESD_WORKAROUND
		if (check_sum_cal != 0 && ESD_RESET_ACTIVATE == 0 && HW_RESET_ACTIVATE == 0)	
#else
		if (check_sum_cal != 0)
#endif
#endif
		{
			ret = himax_hand_shaking(); 
			if (ret == 2) {
				goto err_workqueue_out;
			}

			if ((ret == 1) && (check_sum_cal == 1)) {
				I("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
				ESD_HW_REST();
			} else if (check_sum_cal == 2) {
				I("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");
				ESD_HW_REST();
			}

			
			return;
		} else if (ESD_RESET_ACTIVATE) {
			ESD_RESET_ACTIVATE = 0;
			I("[HIMAX TP MSG]:%s: Back from reset, ready to serve.\n", __func__);
			return;
		} else if (HW_RESET_ACTIVATE)
#else
		if (HW_RESET_ACTIVATE)
#endif
		{
			HW_RESET_ACTIVATE = 0; 
			I("[HIMAX TP MSG] : %s : HW_RST Back from reset, ready to serve.\n", __func__);
			return;
		}

		for (loop_i = 0, check_sum_cal = 0; loop_i < hx_touch_info_size; loop_i++)
			check_sum_cal += buf[loop_i];

		if ((check_sum_cal != 0x00)) {
			I("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n", check_sum_cal);
			return;
		}
	}

	if (ts->debug_log_level & BIT(0)) {
		I("%s: raw data:\n", __func__);
		for (loop_i = 0; loop_i < hx_touch_info_size; loop_i++) {
			I("0x%2.2X ", buf[loop_i]);
			if (loop_i % 8 == 7)
				I("\n");
		}
	}

	
#ifdef HX_TP_SYS_DIAG
	diag_cmd = getDiagCommand();
	if (diag_cmd >= 1 && diag_cmd <= 6) {
		
		for (i = hx_touch_info_size, check_sum_cal = 0; i < 128; i++) {
			check_sum_cal += buf[i];
		}
		if (check_sum_cal % 0x100 != 0) {
			goto bypass_checksum_failed_packet;
		}
#ifdef HX_TP_SYS_2T2R
		if (Is_2T2R && diag_cmd == 4) {
			mutual_data = getMutualBuffer_2();
			self_data 	= getSelfBuffer();

			
			mul_num = getXChannel_2() * getYChannel_2();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel_2() + getYChannel_2() + HX_BT_NUM;
#else
			self_num = getXChannel_2() + getYChannel_2();
#endif
		} else
#endif
		{
			mutual_data = getMutualBuffer();
			self_data 	= getSelfBuffer();

			
			mul_num = getXChannel() * getYChannel();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel() + getYChannel() + HX_BT_NUM;
#else
			self_num = getXChannel() + getYChannel();
#endif
		}

		
		if (buf[hx_touch_info_size] == buf[hx_touch_info_size + 1] && buf[hx_touch_info_size + 1] == buf[hx_touch_info_size + 2]
		    && buf[hx_touch_info_size + 2] == buf[hx_touch_info_size + 3] && buf[hx_touch_info_size] > 0) {
			index = (buf[hx_touch_info_size] - 1) * RawDataLen;
			
			for (i = 0; i < RawDataLen; i++) {
				temp1 = index + i;

				if (temp1 < mul_num) {
					
					mutual_data[index + i] = buf[i + hx_touch_info_size + 4];	
				} else {
					
					temp1 = i + index;
					temp2 = self_num + mul_num;

					if (temp1 >= temp2) {
						break;
					}

					self_data[i + index - mul_num] = buf[i + hx_touch_info_size + 4];	
				}
			}
		} else {
			I("[HIMAX TP MSG]%s: header format is wrong!\n", __func__);
		}
	} else if (diag_cmd == 7) {
		memcpy(&(diag_coor[0]), &buf[0], 128);
	}
	
	if (coordinate_dump_enable == 1) {
		for (i = 0; i < (15 + (HX_MAX_PT + 5) * 2 * 5); i++) {
			coordinate_char[i] = 0x20;
		}
		coordinate_char[15 + (HX_MAX_PT + 5) * 2 * 5] = 0xD;
		coordinate_char[15 + (HX_MAX_PT + 5) * 2 * 5 + 1] = 0xA;
	}
	
bypass_checksum_failed_packet:
#endif
	EN_NoiseFilter = (buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 3);
	
	EN_NoiseFilter = EN_NoiseFilter & 0x01;
	
#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	if (ts->pdata->proximity_bytp_enable) {
		if ((buf[HX_TOUCH_INFO_POINT_CNT] & 0x0F) == 0x00 && (buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 2 & 0x01)) {
			if (proximity_flag == 0) {
				I(" %s near event trigger\n", __func__);
				touch_report_psensor_input_event(0);
				proximity_flag = 1;
			}
			wake_lock_timeout(&ts->ts_wake_lock, TS_WAKE_LOCK_TIMEOUT);
			return;
		}
	}
#endif
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	tpd_key = (buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 4);
	if (tpd_key == 0x0F) { 
		tpd_key = 0x00;
	}
	
#else
	tpd_key = 0x00;
#endif

	p_point_num = hx_point_num;

	if (buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
		hx_point_num = 0;
	else
		hx_point_num = buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;

	
	if (hx_point_num != 0) {
		if (vk_press == 0x00) {
			uint16_t old_finger = ts->pre_finger_mask;
			finger_num = buf[coordInfoSize - 4] & 0x0F;
			finger_pressed = buf[coordInfoSize - 2] << 8 | buf[coordInfoSize - 3];
			finger_on = 1;
			AA_press = 1;
			for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
				if (((finger_pressed >> loop_i) & 1) == 1) {
					base = loop_i * 4;
					x = buf[base] << 8 | buf[base + 1];
					y = (buf[base + 2] << 8 | buf[base + 3]);
					w = buf[(ts->nFinger_support * 4) + loop_i];
					finger_num--;

					if ((ts->debug_log_level & BIT(3)) > 0) {
						if ((((old_finger >> loop_i) ^ (finger_pressed >> loop_i)) & 1) == 1) {
							if (ts->useScreenRes) {
								I("status:%X, Screen:F:%02d Down, X:%d, Y:%d, W:%d, N:%d\n",
								  finger_pressed, loop_i + 1, x * ts->widthFactor >> SHIFTBITS,
								  y * ts->heightFactor >> SHIFTBITS, w, EN_NoiseFilter);
							} else {
								I("status:%X, Raw:F:%02d Down, X:%d, Y:%d, W:%d, N:%d\n",
								  finger_pressed, loop_i + 1, x, y, w, EN_NoiseFilter);
							}
						}
					}

					if (ts->protocol_type == PROTOCOL_TYPE_B) {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						if (ts->hall_block_touch_event == 0)
#endif	
#endif
							input_mt_slot(ts->input_dev, loop_i);
					}

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
					if (ts->hall_block_touch_event == 0) {
#endif	
#endif
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
					}
#endif	
#endif

					if (ts->protocol_type == PROTOCOL_TYPE_A) {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						if (ts->hall_block_touch_event == 0) {
#endif	
#endif
							input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, loop_i);
							input_mt_sync(ts->input_dev);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						}
#endif	
#endif
					} else {
						ts->last_slot = loop_i;
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						if (ts->hall_block_touch_event == 0)
#endif	
#endif
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
					}

					if (!ts->first_pressed) {
						ts->first_pressed = 1;
						I("S1@%d, %d\n", x, y);
					}

					ts->pre_finger_data[loop_i][0] = x;
					ts->pre_finger_data[loop_i][1] = y;


					if (ts->debug_log_level & BIT(1))
						I("Finger %d=> X:%d, Y:%d W:%d, Z:%d, F:%d, N:%d\n",
						  loop_i + 1, x, y, w, w, loop_i + 1, EN_NoiseFilter);
				} else {
					if (ts->protocol_type == PROTOCOL_TYPE_B) {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						if (ts->hall_block_touch_event == 0) {
#endif	
#endif
							input_mt_slot(ts->input_dev, loop_i);
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						}
#endif	
#endif
					}

					if (loop_i == 0 && ts->first_pressed == 1) {
						ts->first_pressed = 2;
						I("E1@%d, %d\n",
						  ts->pre_finger_data[0][0] , ts->pre_finger_data[0][1]);
					}
					if ((ts->debug_log_level & BIT(3)) > 0) {
						if ((((old_finger >> loop_i) ^ (finger_pressed >> loop_i)) & 1) == 1) {
							if (ts->useScreenRes) {
								I("status:%X, Screen:F:%02d Up, X:%d, Y:%d, N:%d\n",
								  finger_pressed, loop_i + 1, ts->pre_finger_data[loop_i][0] * ts->widthFactor >> SHIFTBITS,
								  ts->pre_finger_data[loop_i][1] * ts->heightFactor >> SHIFTBITS, Last_EN_NoiseFilter);
							} else {
								I("status:%X, Raw:F:%02d Up, X:%d, Y:%d, N:%d\n",
								  finger_pressed, loop_i + 1, ts->pre_finger_data[loop_i][0],
								  ts->pre_finger_data[loop_i][1], Last_EN_NoiseFilter);
							}
						}
					}
				}
			}
			ts->pre_finger_mask = finger_pressed;
		} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			
			
			
			
			himax_ts_button_func(tpd_key, ts);
			finger_on = 0;
		}
#ifdef HX_ESD_WORKAROUND
		ESD_COUNTER = 0;
#endif
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		if (ts->hall_block_touch_event == 0) {
#endif	
#endif
			input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
			input_sync(ts->input_dev);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		}
#endif	
#endif
	} else if (hx_point_num == 0) {
#if defined(HX_PALM_REPORT)
		loop_i = 0;
		base = loop_i * 4;
		x = buf[base] << 8 | buf[base + 1];
		y = (buf[base + 2] << 8 | buf[base + 3]);
		w = buf[(ts->nFinger_support * 4) + loop_i];
		I(" %s HX_PALM_REPORT_loopi=%d,base=%x,X=%x,Y=%x,W=%x \n", __func__, loop_i, base, x, y, w);

		if ((!atomic_read(&ts->suspend_mode)) && (x == 0xFA5A) && (y == 0xFA5A) && (w == 0x00)) {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
			if (ts->hall_block_touch_event == 0) {
#endif	
#endif
				I(" %s HX_PALM_REPORT KEY power event press\n", __func__);
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				msleep(100);
				I(" %s HX_PALM_REPORT KEY power event release\n", __func__);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
			}
#endif	
#endif
			return;
		}
#endif
#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
		if ((ts->pdata->proximity_bytp_enable) && (proximity_flag)) { 
			I(" %s far event trigger\n", __func__);
			touch_report_psensor_input_event(1);
			proximity_flag = 0;	
			wake_lock_timeout(&ts->ts_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		} else if (AA_press)
#else
		if (AA_press)
#endif
		{
			
			finger_on = 0;
			AA_press = 0;
			if (ts->protocol_type == PROTOCOL_TYPE_A)
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
				if (ts->hall_block_touch_event == 0)
#endif	
#endif
					input_mt_sync(ts->input_dev);

			for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
				if (((ts->pre_finger_mask >> loop_i) & 1) == 1) {
					if (ts->protocol_type == PROTOCOL_TYPE_B) {
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						if (ts->hall_block_touch_event == 0) {
#endif	
#endif
							input_mt_slot(ts->input_dev, loop_i);
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
						}
#endif	
#endif
					}
				}
			}
			if (ts->pre_finger_mask > 0) {
				for (loop_i = 0; loop_i < ts->nFinger_support && (ts->debug_log_level & BIT(3)) > 0; loop_i++) {
					if (((ts->pre_finger_mask >> loop_i) & 1) == 1) {
						if (ts->useScreenRes) {
							I("status:%X, Screen:F:%02d Up, X:%d, Y:%d, N:%d\n", 0, loop_i + 1, ts->pre_finger_data[loop_i][0] * ts->widthFactor >> SHIFTBITS,
							  ts->pre_finger_data[loop_i][1] * ts->heightFactor >> SHIFTBITS, Last_EN_NoiseFilter);
						} else {
							I("status:%X, Raw:F:%02d Up, X:%d, Y:%d, N:%d\n", 0, loop_i + 1, ts->pre_finger_data[loop_i][0], ts->pre_finger_data[loop_i][1], Last_EN_NoiseFilter);
						}
					}
				}
				ts->pre_finger_mask = 0;
			}

			if (ts->first_pressed == 1) {
				ts->first_pressed = 2;
				I("E1@%d, %d\n", ts->pre_finger_data[0][0] , ts->pre_finger_data[0][1]);
			}

			if (ts->debug_log_level & BIT(1))
				I("All Finger leave\n");

#ifdef HX_TP_SYS_DIAG
			
			if (coordinate_dump_enable == 1) {
				do_gettimeofday(&t);
				time_to_tm(t.tv_sec, 0, &broken);
				scnprintf(&coordinate_char[0], 17, "%2d:%2d:%2d:%lu,", broken.tm_hour, broken.tm_min, broken.tm_sec, t.tv_usec / 1000);
				scnprintf(&coordinate_char[15], 10, "Touch up!");
				coordinate_fn->f_op->write(coordinate_fn, &coordinate_char[0], 15 + (HX_MAX_PT + 5) * 2 * sizeof(char) * 5 + 2, &coordinate_fn->f_pos);
			}
			
#endif
		} else if (tpd_key != 0x00) {
			
			
			
			
			
			himax_ts_button_func(tpd_key, ts);
			finger_on = 1;
		} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			
			
			
			
			himax_ts_button_func(tpd_key, ts);
			finger_on = 0;
		}
#ifdef HX_ESD_WORKAROUND
		ESD_COUNTER = 0;
#endif
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		if (ts->hall_block_touch_event == 0) {
#endif	
#endif
			input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
			input_sync(ts->input_dev);
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		}
#endif	
#endif
	}
	tpd_key_old = tpd_key;
	Last_EN_NoiseFilter = EN_NoiseFilter;

workqueue_out:
	return;

err_workqueue_out:
	I("%s: Now reset the Touch chip.\n", __func__);

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, false);
#endif

	goto workqueue_out;
}

#if defined(QCT) || (defined(MTK) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)))
static irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	struct himax_ts_data *ts = ptr;
	struct timespec timeStart, timeEnd, timeDelta;

	if (irq_enabled_input % 2 == 0) {
		I("irq enabled input = %d\n", irq_enabled_input);
		return IRQ_HANDLED;
	}

	if (ts->debug_log_level & BIT(2)) {
		getnstimeofday(&timeStart);
	}
#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode) && (!FAKE_POWER_KEY_SEND) && (ts->SMWP_enable)) {
		wake_lock_timeout(&ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		if (himax_parse_wake_event((struct himax_ts_data *)ptr)) {
			I(" %s SMART WAKEUP KEY power event press\n", __func__);
			input_report_key(ts->input_dev, KEY_POWER, 1);
			input_sync(ts->input_dev);
			msleep(100);
			I(" %s SMART WAKEUP KEY power event release\n", __func__);
			input_report_key(ts->input_dev, KEY_POWER, 0);
			input_sync(ts->input_dev);
			FAKE_POWER_KEY_SEND = true;
			return IRQ_HANDLED;
		}
	}
#endif
	himax_ts_work((struct himax_ts_data *)ptr);
	if (ts->debug_log_level & BIT(2)) {
		getnstimeofday(&timeEnd);
		timeDelta.tv_nsec = (timeEnd.tv_sec * 1000000000 + timeEnd.tv_nsec)
				    - (timeStart.tv_sec * 1000000000 + timeStart.tv_nsec);
		I("Touch latency = %ld us\n", timeDelta.tv_nsec / 1000);
	}
	return IRQ_HANDLED;
}
#endif

#ifdef MTK
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
static void tpd_eint_interrupt_handler(void)
{
	
	tpd_flag = 1;
	wake_up_interruptible(&waiter);

}
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *desc)
{
	tpd_flag = 1;
	
	
	himax_int_enable(private_ts->client->irq, 0);
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

static int touch_event_handler(void *ptr)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);

	do {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		enable_irq(private_ts->client->irq);
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);

		tpd_flag = 0;

		set_current_state(TASK_RUNNING);

		himax_ts_work((struct himax_ts_data *)ptr);
		himax_int_enable(private_ts->client->irq, 1);

	} while (!kthread_should_stop());

	return 0;
}
#endif	
#endif

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

static enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;

	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

int himax_ts_register_interrupt(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;
#if defined(MTK) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	struct device_node *node = NULL;
	uint8_t i = 0, j = 0, buf[128];
	u32 ints[2] = {0, 0};

	node = of_find_compatible_node(NULL, NULL, "mediatek,TOUCH_PANEL-eint");
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);

		client->irq = irq_of_parse_and_map(node, 0);

	} else {
		I("%s: can not find touch IRQ device node!\n", __func__);
	}

	if (himax_int_gpio_read(ts->pdata->gpio_irq) == 0) {
		for (i = 0; i < 50; i++) {
			ret = i2c_himax_read(ts->client, 0x86, buf, 128, DEFAULT_RETRY_CNT);
			msleep(10);
			if (himax_int_gpio_read(ts->pdata->gpio_irq)) {
				I("%s event stack has been clear\n ", __func__);
				j++;
				if (j > 5)
					break;
			}
		}
	}
#endif

	ts->irq_enabled = 0;
	ts->use_irq = 0;
	
	if (client->irq) {	
		ts->use_irq = 1;
		if (HX_INT_IS_EDGE) {
			I("%s edge trigger falling\n ", __func__);
#ifdef QCT
			ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
						   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						   client->name, ts);
#endif
#ifdef MTK
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
			mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE);
			mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM,
						CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
			mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,
					     CUST_EINTF_TRIGGER_FALLING,
					     tpd_eint_interrupt_handler, 1);
#else
			ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
						   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						   client->name, ts);
#endif	
#endif
		} else {
			I("%s level trigger low\n ", __func__);
#ifdef QCT
			ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
						   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						   client->name, ts);
#endif
#ifdef MTK
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
			mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE);
			mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM,
						CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
			mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,
					     CUST_EINTF_TRIGGER_LOW,
					     tpd_eint_interrupt_handler, 1);
#else
			ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
						   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						   client->name, ts);
#endif	
#endif
		}
		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			I("%s: irq enabled at qpio: %d\n", __func__, client->irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(client->irq, 1);
#endif
#if defined(MTK) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
			touch_thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
			if (IS_ERR(touch_thread)) {
				ret = PTR_ERR(touch_thread);
				E(" Failed to create kernel thread: %d\n", ret);
			}

			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}

	} else {
		I("%s: client->irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) {
		ts->himax_wq = create_singlethread_workqueue("himax_touch");

		INIT_WORK(&ts->work, himax_ts_work_func);

		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}
	return ret;
}

#if defined(HX_USB_DETECT)
static void himax_cable_tp_status_handler_func(int connect_status)
{
	struct himax_ts_data *ts;
	I("Touch: cable change to %d\n", connect_status);
	ts = private_ts;
	if (ts->cable_config) {
		if (!atomic_read(&ts->suspend_mode)) {
			if ((!!connect_status) != ts->usb_connected) {
				if (!!connect_status) {
					ts->cable_config[1] = 0x01;
					ts->usb_connected = 0x01;
				} else {
					ts->cable_config[1] = 0x00;
					ts->usb_connected = 0x00;
				}

				i2c_himax_master_write(ts->client, ts->cable_config,
						       sizeof(ts->cable_config), HIMAX_I2C_RETRY_TIMES);

				I("%s: Cable status change: 0x%2.2X\n", __func__, ts->cable_config[1]);
			} else
				I("%s: Cable status is the same as previous one, ignore.\n", __func__);
		} else {
			if (connect_status)
				ts->usb_connected = 0x01;
			else
				ts->usb_connected = 0x00;
			I("%s: Cable status remembered: 0x%2.2X\n", __func__, ts->usb_connected);
		}
	}
}

static struct t_cable_status_notifier himax_cable_status_handler = {
	.name = "usb_tp_connected",
	.func = himax_cable_tp_status_handler_func,
};

#endif

#ifdef CONFIG_FB
static void himax_fb_register(struct work_struct *work)
{
	int ret = 0;
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
						work_att.work);
	I(" %s in", __func__);

	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if (ret)
		E(" Unable to register fb_notifier: %d\n", ret);
}
#endif

#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
int proximity_enable_from_ps(int on)
{
	char buf_tmp[5];
	if (on) {
		touch_report_psensor_input_event(1);
		buf_tmp[0] = 0x92;
		buf_tmp[1] = 0x01;
		g_proximity_en = 1;
		enable_irq_wake(private_ts->client->irq);
	} else {
		buf_tmp[0] = 0x92;
		buf_tmp[1] = 0x00;
		g_proximity_en = 0;
		disable_irq_wake(private_ts->client->irq);
	}

	I("Proximity=%d\n", on);
	i2c_himax_master_write(private_ts->client, buf_tmp, 2, HIMAX_I2C_RETRY_TIMES);

	return 0;
}
EXPORT_SYMBOL_GPL(proximity_enable_from_ps);
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)

static ssize_t touch_vendor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	ts_data = private_ts;

	ret += scnprintf(buf, PAGE_SIZE, "%s_FW:%#x,%x_CFG:%#x_SensorId:%#x\n", HIMAX852xes_NAME,
			 ts_data->vendor_fw_ver_H, ts_data->vendor_fw_ver_L, ts_data->vendor_config_ver, ts_data->vendor_sensor_id);

	return ret;
}

static DEVICE_ATTR(vendor, (S_IRUGO), touch_vendor_show, NULL);

static ssize_t touch_attn_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	ts_data = private_ts;

	scnprintf(buf, PAGE_SIZE, "attn = %x\n", himax_int_gpio_read(ts_data->pdata->gpio_irq));
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(attn, (S_IRUGO), touch_attn_show, NULL);

static ssize_t himax_int_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;

	count += scnprintf(buf + count, PAGE_SIZE, "%d\n", ts->irq_enabled);

	return count;
}

static ssize_t himax_int_status_store(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = private_ts;
	int value;
	int i = 0, j = 0;
	uint8_t events[128] = {0};

	if (kstrtoint(buf, 10, &value) != 0)
		return -EINVAL;

	if (value > 3 || value < 0) {
		I("Input out of range\n");
		return -EINVAL;
	}

	irq_enabled_input = value;

	switch (value) {
	case 3:
		I("%s: %s mode. Reset chip with loading config and re-enable irq handler\n",
		  __func__, mfg_mode);
#ifdef HX_RST_PIN_FUNC
		
		himax_HW_reset(true, true);
#endif
		himax_ts_register_interrupt(ts->client);

		ts->suspended = false;
		break;
	case 2:
		ts->suspended = true;
		himax_int_enable(ts->client->irq, 0);
		free_irq(ts->client->irq, ts);
		ts->irq_enabled = 0;
		I("%s: %s mode. Disable irq handler and reset chip without loading config\n",
		  __func__, mfg_mode);
#ifdef HX_RST_PIN_FUNC
		
		himax_HW_reset(false, true);
#endif

		
		if (gpio_get_value(ts->pdata->gpio_irq) == 0) {
			for (i = 0; i < 50; i++) {
				i2c_himax_read(ts->client, 0x86, events, 128, DEFAULT_RETRY_CNT);
				msleep(10);
				if (gpio_get_value(ts->pdata->gpio_irq)) {
					I("%s event stack has been clear\n ", __func__);
					j++;
					if (j > 5)
						break;
				}
			}
		}
		break;
	case 1:
		himax_ts_register_interrupt(ts->client);
		break;
	case 0:
		himax_int_enable(ts->client->irq, 0);
		free_irq(ts->client->irq, ts);
		ts->irq_enabled = 0;
		break;
	default:
		I("Input not support\n");
		break;
	}

	return count;
}

static DEVICE_ATTR(enabled, (S_IWUSR | S_IRUGO),
		   himax_int_status_show, himax_int_status_store);

static ssize_t himax_layout_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;

	count += scnprintf(buf + count, PAGE_SIZE, "%d %d %d %d\n", ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	return count;
}

static ssize_t himax_layout_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[5];
	int i = 0, j = 0, k = 0, ret;
	unsigned long value;
	int layout[4] = {0};

	for (i = 0; i < 20; i++) {
		if (buf[i] == ',' || buf[i] == '\n') {
			memset(buf_tmp, 0x0, sizeof(buf_tmp));
			if (i - j <= 5)
				memcpy(buf_tmp, buf + j, i - j);
			else {
				I("buffer size is over 5 char\n");
				return count;
			}
			j = i + 1;
			if (k < 4) {
				ret = kstrtol(buf_tmp, 10, &value);
				layout[k++] = value;
			}
		}
	}
	if (k == 4) {
		ts->pdata->abs_x_min = layout[0];
		ts->pdata->abs_x_max = layout[1];
		ts->pdata->abs_y_min = layout[2];
		ts->pdata->abs_y_max = layout[3];
		I("%d, %d, %d, %d\n",
		  ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
		input_unregister_device(ts->input_dev);
		himax_input_register(ts);
	} else
		I("ERR@%d, %d, %d, %d\n",
		  ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	return count;
}

static DEVICE_ATTR(layout, (S_IWUSR | S_IRUGO),
		   himax_layout_show, himax_layout_store);

static ssize_t himax_debug_level_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts_data;
	size_t count = 0;
	ts_data = private_ts;

	count += scnprintf(buf, PAGE_SIZE, "%d\n", ts_data->debug_log_level);

	return count;
}

static ssize_t himax_debug_level_dump(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts;
	char buf_tmp[11];
	int i;
	ts = private_ts;
	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memcpy(buf_tmp, buf, count);

	ts->debug_log_level = 0;
	for (i = 0; i < count - 1; i++) {
		if (buf_tmp[i] >= '0' && buf_tmp[i] <= '9')
			ts->debug_log_level |= (buf_tmp[i] - '0');
		else if (buf_tmp[i] >= 'A' && buf_tmp[i] <= 'F')
			ts->debug_log_level |= (buf_tmp[i] - 'A' + 10);
		else if (buf_tmp[i] >= 'a' && buf_tmp[i] <= 'f')
			ts->debug_log_level |= (buf_tmp[i] - 'a' + 10);

		if (i != count - 2)
			ts->debug_log_level <<= 4;
	}

	if (ts->debug_log_level & BIT(3)) {
		if (ts->pdata->screenWidth > 0 && ts->pdata->screenHeight > 0 &&
		    (ts->pdata->abs_x_max - ts->pdata->abs_x_min) > 0 &&
		    (ts->pdata->abs_y_max - ts->pdata->abs_y_min) > 0) {
			ts->widthFactor = (ts->pdata->screenWidth << SHIFTBITS) / (ts->pdata->abs_x_max - ts->pdata->abs_x_min);
			ts->heightFactor = (ts->pdata->screenHeight << SHIFTBITS) / (ts->pdata->abs_y_max - ts->pdata->abs_y_min);
			if (ts->widthFactor > 0 && ts->heightFactor > 0)
				ts->useScreenRes = 1;
			else {
				ts->heightFactor = 0;
				ts->widthFactor = 0;
				ts->useScreenRes = 0;
			}
		} else
			I("Enable finger debug with raw position mode!\n");
	} else {
		ts->heightFactor = 0;
		ts->widthFactor = 0;
		ts->useScreenRes = 0;
	}

	return count;
}

static DEVICE_ATTR(debug_level, (S_IWUSR | S_IRUGO),
		   himax_debug_level_show, himax_debug_level_dump);

#ifdef HX_TP_SYS_REGISTER
static ssize_t himax_register_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int base = 0;
	uint16_t loop_i, loop_j;
	uint8_t data[128];
	uint8_t outData[5];

	memset(outData, 0x00, sizeof(outData));
	memset(data, 0x00, sizeof(data));

	I("Himax multi_register_command = %d \n", multi_register_command);

	if (multi_register_command == 1) {
		base = 0;

		for (loop_i = 0; loop_i < 6; loop_i++) {
			if (multi_register[loop_i] != 0x00) {
				if (multi_cfg_bank[loop_i] == 1) {
					outData[0] = 0x14;
					i2c_himax_write(private_ts->client, 0x8C , &outData[0], 1, DEFAULT_RETRY_CNT);
					msleep(10);

					outData[0] = 0x00;
					outData[1] = multi_register[loop_i];
					i2c_himax_write(private_ts->client, 0x8B , &outData[0], 2, DEFAULT_RETRY_CNT);
					msleep(10);

					i2c_himax_read(private_ts->client, 0x5A, data, 128, DEFAULT_RETRY_CNT);

					outData[0] = 0x00;
					i2c_himax_write(private_ts->client, 0x8C , &outData[0], 1, DEFAULT_RETRY_CNT);

					for (loop_j = 0; loop_j < 128; loop_j++)
						multi_value[base++] = data[loop_j];
				} else {
					i2c_himax_read(private_ts->client, multi_register[loop_i], data, 128, DEFAULT_RETRY_CNT);

					for (loop_j = 0; loop_j < 128; loop_j++)
						multi_value[base++] = data[loop_j];
				}
			}
		}

		base = 0;
		for (loop_i = 0; loop_i < 6; loop_i++) {
			if (multi_register[loop_i] != 0x00) {
				if (multi_cfg_bank[loop_i] == 1)
					ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Register: FE(%x)\n", multi_register[loop_i]);
				else
					ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Register: %x\n", multi_register[loop_i]);

				for (loop_j = 0; loop_j < 128; loop_j++) {
					ret += scnprintf(buf + ret, PAGE_SIZE - ret, "0x%2.2X ", multi_value[base++]);
					if ((loop_j % 16) == 15)
						ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
				}
			}
		}
		return ret;
	}

	if (config_bank_reg) {
		I("%s: register_command = FE(%x)\n", __func__, register_command);

		

		outData[0] = 0x14;
		i2c_himax_write(private_ts->client, 0x8C, &outData[0], 1, DEFAULT_RETRY_CNT);

		msleep(10);

		outData[0] = 0x00;
		outData[1] = register_command;
		i2c_himax_write(private_ts->client, 0x8B, &outData[0], 2, DEFAULT_RETRY_CNT);
		msleep(10);

		i2c_himax_read(private_ts->client, 0x5A, data, 128, DEFAULT_RETRY_CNT);
		msleep(10);

		outData[0] = 0x00;
		i2c_himax_write(private_ts->client, 0x8C, &outData[0], 1, DEFAULT_RETRY_CNT);
	} else {
		if (i2c_himax_read(private_ts->client, register_command, data, 128, DEFAULT_RETRY_CNT) < 0)
			return ret;
	}

	if (config_bank_reg)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "command: FE(%x)\n", register_command);
	else
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "command: %x\n", register_command);

	for (loop_i = 0; loop_i < 128; loop_i++) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "0x%2.2X ", data[loop_i]);
		if ((loop_i % 16) == 15)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	return ret;
}

static ssize_t himax_register_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char buf_tmp[6], length	= 0;
	unsigned long result	= 0;
	uint8_t loop_i		= 0;
	uint16_t base		= 5;
	uint8_t write_da[128];
	uint8_t outData[5];

	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memset(write_da, 0x0, sizeof(write_da));
	memset(outData, 0x0, sizeof(outData));

	I("himax %s \n", buf);

	if (buf[0] == 'm' && buf[1] == 'r' && buf[2] == ':') {
		memset(multi_register, 0x00, sizeof(multi_register));
		memset(multi_cfg_bank, 0x00, sizeof(multi_cfg_bank));
		memset(multi_value, 0x00, sizeof(multi_value));

		I("himax multi register enter\n");

		multi_register_command = 1;

		base	= 2;
		loop_i	= 0;

		while (true) {
			if (buf[base] == '\n')
				break;

			if (loop_i >= 6)
				break;

			if (buf[base] == ':' && buf[base + 1] == 'x' && buf[base + 2] == 'F' &&
			    buf[base + 3] == 'E' && buf[base + 4] != ':') {
				memcpy(buf_tmp, buf + base + 4, 2);
				if (!kstrtoul(buf_tmp, 16, &result)) {
					multi_register[loop_i] = result;
					multi_cfg_bank[loop_i++] = 1;
				}
				base += 6;
			} else {
				memcpy(buf_tmp, buf + base + 2, 2);
				if (!kstrtoul(buf_tmp, 16, &result)) {
					multi_register[loop_i] = result;
					multi_cfg_bank[loop_i++] = 0;
				}
				base += 4;
			}
		}

		I("========================== \n");
		for (loop_i = 0; loop_i < 6; loop_i++)
			I("%d,%d:", multi_register[loop_i], multi_cfg_bank[loop_i]);
		I("\n");
	} else if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':') {
		multi_register_command = 0;

		if (buf[2] == 'x') {
			if (buf[3] == 'F' && buf[4] == 'E') {
				config_bank_reg = true;

				memcpy(buf_tmp, buf + 5, 2);
				if (!kstrtoul(buf_tmp, 16, &result))
					register_command = result;
				base = 7;

				I("CMD: FE(%x)\n", register_command);
			} else {
				config_bank_reg = false;

				memcpy(buf_tmp, buf + 3, 2);
				if (!kstrtoul(buf_tmp, 16, &result))
					register_command = result;
				base = 5;
				I("CMD: %x\n", register_command);
			}

			for (loop_i = 0; loop_i < 128; loop_i++) {
				if (buf[base] == '\n') {
					if (buf[0] == 'w') {
						if (config_bank_reg) {
							outData[0] = 0x14;
							i2c_himax_write(private_ts->client, 0x8C, &outData[0], 1, DEFAULT_RETRY_CNT);
							msleep(10);

							outData[0] = 0x00;
							outData[1] = register_command;
							i2c_himax_write(private_ts->client, 0x8B, &outData[0], 2, DEFAULT_RETRY_CNT);

							msleep(10);
							i2c_himax_write(private_ts->client, 0x40, &write_da[0], length, DEFAULT_RETRY_CNT);

							msleep(10);
							outData[0] = 0x00;
							i2c_himax_write(private_ts->client, 0x8C, &outData[0], 1, DEFAULT_RETRY_CNT);

							I("CMD: FE(%x), %x, %d\n", register_command, write_da[0], length);
						} else {
							i2c_himax_write(private_ts->client, register_command, &write_da[0], length, DEFAULT_RETRY_CNT);
							I("CMD: %x, %x, %d\n", register_command, write_da[0], length);
						}
					}
					I("\n");
					return count;
				}
				if (buf[base + 1] == 'x') {
					buf_tmp[4] = '\n';
					buf_tmp[5] = '\0';
					memcpy(buf_tmp, buf + base + 2, 2);
					if (!kstrtoul(buf_tmp, 16, &result)) {
						write_da[loop_i] = result;
					}
					length++;
				}
				base += 4;
			}
		}
	}
	return count;
}

static DEVICE_ATTR(register, (S_IWUSR | S_IRUGO), himax_register_show, himax_register_store);
#endif

#ifdef HX_TP_SYS_DIAG
static uint8_t *getMutualBuffer(void)
{
	return diag_mutual;
}
static uint8_t *getSelfBuffer(void)
{
	return &diag_self[0];
}
static uint8_t getXChannel(void)
{
	return x_channel;
}
static uint8_t getYChannel(void)
{
	return y_channel;
}
static uint8_t getDiagCommand(void)
{
	return diag_command;
}
static void setXChannel(uint8_t x)
{
	x_channel = x;
}
static void setYChannel(uint8_t y)
{
	y_channel = y;
}
static void setMutualBuffer(void)
{
	diag_mutual = kzalloc(x_channel * y_channel * sizeof(uint8_t), GFP_KERNEL);
}

#ifdef HX_TP_SYS_2T2R
static uint8_t *getMutualBuffer_2(void)
{
	return diag_mutual_2;
}
static uint8_t getXChannel_2(void)
{
	return x_channel_2;
}
static uint8_t getYChannel_2(void)
{
	return y_channel_2;
}
static void setXChannel_2(uint8_t x)
{
	x_channel_2 = x;
}
static void setYChannel_2(uint8_t y)
{
	y_channel_2 = y;
}
static void setMutualBuffer_2(void)
{
	diag_mutual_2 = kzalloc(x_channel_2 * y_channel_2 * sizeof(uint8_t), GFP_KERNEL);
}
#endif

#ifdef HX_MTK_DMA
#define print_cfg(reg)	\
do {											\
	count += scnprintf(buf + count, PAGE_SIZE - count,				\
			"<c%-2d>: %lu bytes\n", ++i, sizeof(config_selected->c##reg));	\
	for (loop_i = 0; loop_i < sizeof(config_selected->c##reg); ) {			\
		count += scnprintf(buf + count, PAGE_SIZE - count,			\
			"%02X ", config_selected->c##reg[loop_i++]);			\
		if (loop_i % 8 == 0)							\
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");	\
	}										\
	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");			\
} while(0);

static ssize_t loadconfig_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int loop_i = 0, count = 0, i = 0;
	print_cfg(1);	print_cfg(2);	print_cfg(3);	print_cfg(4);	print_cfg(5);
	print_cfg(6);	print_cfg(7);	print_cfg(8);	print_cfg(9);	print_cfg(10);

	print_cfg(11);	print_cfg(12);	print_cfg(13);	print_cfg(14);	print_cfg(15);
	print_cfg(16);	print_cfg(17);	print_cfg(18);	print_cfg(19);	print_cfg(20);

	print_cfg(21);	print_cfg(22);	print_cfg(23);	print_cfg(24);	print_cfg(25);
	print_cfg(26);	print_cfg(27);	print_cfg(28);	print_cfg(29);	print_cfg(30);

	print_cfg(31);	print_cfg(32);	print_cfg(33);	print_cfg(34);	print_cfg(35);
	print_cfg(36);	print_cfg(37);	print_cfg(38);	print_cfg(39);	print_cfg(40);

	print_cfg(41);

	return count;
}

static uint8_t config_idex = 0;
static DEFINE_MUTEX(himax852xES_mutex);
#define write_cfg(reg)									\
do {											\
	memcpy(config_selected->c##reg, write_da, sizeof(config_selected->c##reg));	\
	i2c_himax_master_write(client, config_selected->c##reg,				\
			sizeof(config_selected->c##reg), DEFAULT_RETRY_CNT);		\
} while (0);

static ssize_t loadconfig_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = private_ts;
	struct i2c_client *client = ts->client;
	uint8_t loop_i = 0;
	uint8_t write_da[256] = {0x00};
	const char *buff_p = NULL;
	uint8_t byte_temp[3] = {0x00};
	uint8_t byte_count = 0;
	uint8_t byte = 0x00;
#ifdef HX_ESD_WORKAROUND
	char data[2] = {0};
#endif
	mutex_lock(&himax852xES_mutex);
	if (buf[0] == 'r' || buf[0] == 'R') {		
		wake_lock(&ts->ts_flash_wake_lock);


		if (ts->rst_gpio) {
			I("%s: Now reset the Touch chip.\n", __func__);

			himax_rst_gpio_set(ts->rst_gpio, 0);
			msleep(20);
			himax_rst_gpio_set(ts->rst_gpio, 1);
			msleep(100);
		}
	} else if (buf[0] == 'p' || buf[0] == 'P') {	
		himax_power_on_initCMD(ts->client);
		himax_read_TP_info(private_ts->client);

#if defined(HX_TP_SYS_DIAG)
		setXChannel(HX_RX_NUM); 
		setYChannel(HX_TX_NUM); 

		if (getMutualBuffer() != NULL)
			kfree(diag_mutual);

		setMutualBuffer();
		if (getMutualBuffer() == NULL)
			E("%s: mutual buffer allocate fail failed\n", __func__);

#if defined(HX_TP_SYS_2T2R)
		if (Is_2T2R) {
			setXChannel_2(HX_RX_NUM_2); 
			setYChannel_2(HX_TX_NUM_2); 

			if (getMutualBuffer_2() != NULL)
				kfree(diag_mutual_2);

			setMutualBuffer_2();
			if (getMutualBuffer_2() == NULL)
				E("%s: mutual buffer 2 allocate fail failed\n", __func__);
		}
#endif
#endif

		wake_unlock(&ts->ts_flash_wake_lock);
	} else if (strncmp(buf, "0x", 2) == 0) {	
		buff_p = buf;
		do {
			buff_p += 2;
			memcpy(byte_temp, buff_p, 2);
			if (kstrtou8(byte_temp, 16, &byte) == 0) {
				write_da[byte_count++] = byte;
				memset(byte_temp, 0x00, sizeof(byte_temp));
			} else
				I("Wrong Format in the register at byte %d.\n", byte_count + 1);
		} while ((buff_p = strstr(buff_p, "0x")));

		printk("[TP] %d byte: ", byte_count);
		for (loop_i = 0; loop_i < byte_count;) {
			printk("%02X ", write_da[loop_i++]);
			if (loop_i % 8 == 0)
				printk("\n[TP] ");
		}
		printk("\n");

		if (byte_count == (sizeof(config_selected->c40) + 1) && write_da[0] == 0x00) {
			
			memcpy(config_selected->c40, write_da + 1, sizeof(config_selected->c40));
			himax_config_reg_write(client, 0x00, config_selected->c40, sizeof(config_selected->c40), DEFAULT_RETRY_CNT);
			private_ts->vendor_config_ver = config_selected->c40[1];
		} else if (byte_count == (sizeof(config_selected->c41) + 1) && write_da[0] == 0x9E) {
			
			memcpy(config_selected->c41, write_da + 1, sizeof(config_selected->c41));
			himax_config_reg_write(client, 0x9E, config_selected->c41, sizeof(config_selected->c41), DEFAULT_RETRY_CNT);
			msleep(1);

#ifdef HX_ESD_WORKAROUND
			
			i2c_himax_read(client, 0x36, data, 2, 10);
			if (data[0] != 0x0F || data[1] != 0x53) {
				
				E("[HimaxError] %s R36 Fail : R36[0]=%d,R36[1]=%d,R36 Counter=%d \n", __func__, data[0], data[1], ESD_R36_FAIL);
				return -1;
			} else
				I("Read R36 to check IC Status: OK\n");
#endif
			config_idex = 0;
		} else {
			
			config_idex++;
			switch (config_idex) {
			case 1:
				write_cfg(1);
				break;
			case 2:
				write_cfg(2);
				break;
			case 3:
				write_cfg(3);
				break;
			case 4:
				write_cfg(4);
				break;
			case 5:
				write_cfg(5);
				break;
			case 6:
				write_cfg(6);
				break;
			case 7:
				write_cfg(7);
				break;
			case 8:
				write_cfg(8);
				break;
			case 9:
				write_cfg(9);
				break;
			case 10:
				write_cfg(10);
				break;
			case 11:
				write_cfg(11);
				break;
			case 12:
				write_cfg(12);
				break;
			case 13:
				write_cfg(13);
				break;
			case 14:
				write_cfg(14);
				break;
			case 15:
				write_cfg(15);
				break;
			case 16:
				write_cfg(16);
				break;
			case 17:
				write_cfg(17);
				break;
			case 18:
				write_cfg(18);
				break;
			case 19:
				write_cfg(19);
				break;
			case 20:
				write_cfg(20);
				break;
			case 21:
				write_cfg(21);
				break;
			case 22:
				write_cfg(22);
				break;
			case 23:
				write_cfg(23);
				break;
			case 24:
				write_cfg(24);
				break;
			case 25:
				write_cfg(25);
				break;
			case 26:
				write_cfg(26);
				break;
			case 27:
				write_cfg(27);
				break;
			case 28:
				write_cfg(28);
				break;
			case 29:
				write_cfg(29);
				break;
			case 30:
				write_cfg(30);
				break;
			case 31:
				write_cfg(31);
				break;
			case 32:
				write_cfg(32);
				break;
			case 33:
				write_cfg(33);
				break;
			case 34:
				write_cfg(34);
				break;
			case 35:
				write_cfg(35);
				break;
			case 36:
				write_cfg(36);
				break;
			case 37:
				write_cfg(37);
				break;
			case 38:
				write_cfg(38);
				break;
			case 39:
				write_cfg(39);
				break;
			default:
				I("out of range, config_idex=%d\n", config_idex);
				break;
			}
		}
	} else
		I("Wrong format!\n");
	mutex_unlock(&himax852xES_mutex);

	return count;
}
static DEVICE_ATTR(loadconfig, (S_IWUSR | S_IRUGO), loadconfig_show, loadconfig_store);
#endif

static ssize_t himax_diag_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	uint32_t loop_i;
	uint16_t mutual_num, self_num, width;
#ifdef HX_TP_SYS_2T2R
	if (Is_2T2R && diag_command == 4) {
		mutual_num 	= x_channel_2 * y_channel_2;
		self_num 	= x_channel_2 + y_channel_2; 
		width 		= x_channel_2;
		count += scnprintf(buf + count, PAGE_SIZE - count, "ChannelStart: %4d, %4d\n\n", x_channel_2, y_channel_2);
	} else
#endif
	{
		mutual_num 	= x_channel * y_channel;
		self_num 	= x_channel + y_channel; 
		width 		= x_channel;
		count += scnprintf(buf + count, PAGE_SIZE - count, "ChannelStart: %4d, %4d\n\n", x_channel, y_channel);
	}

	
	if (diag_command >= 1 && diag_command <= 6) {
		if (diag_command <= 3) {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_mutual[loop_i]);
				if ((loop_i % width) == (width - 1))
					count += scnprintf(buf + count, PAGE_SIZE - count, " %3d\n", diag_self[width + loop_i / width]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			for (loop_i = 0; loop_i < width; loop_i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_self[loop_i]);
				if (((loop_i) % width) == (width - 1))
					count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			}
#ifdef HX_EN_SEL_BUTTON
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_self[HX_RX_NUM + HX_TX_NUM + loop_i]);
#endif
#ifdef HX_TP_SYS_2T2R
		} else if (Is_2T2R && diag_command == 4) {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_mutual_2[loop_i]);
				if ((loop_i % width) == (width - 1))
					count += scnprintf(buf + count, PAGE_SIZE - count, " %3d\n", diag_self[width + loop_i / width]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			for (loop_i = 0; loop_i < width; loop_i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_self[loop_i]);
				if (((loop_i) % width) == (width - 1))
					count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			}

#ifdef HX_EN_SEL_BUTTON
			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_self[HX_RX_NUM_2 + HX_TX_NUM_2 + loop_i]);
#endif
#endif
		} else if (diag_command > 4) {
			for (loop_i = 0; loop_i < self_num; loop_i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_self[loop_i]);
				if (((loop_i - mutual_num) % width) == (width - 1))
					count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			}
		} else {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_mutual[loop_i]);
				if ((loop_i % width) == (width - 1))
					count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
			}
		}
		count += scnprintf(buf + count, PAGE_SIZE - count, "ChannelEnd\n");
	} else if (diag_command == 7) {
		for (loop_i = 0; loop_i < 128 ; loop_i++) {
			if ((loop_i % 16) == 0)
				count += scnprintf(buf + count, PAGE_SIZE - count, "LineStart:");
			count += scnprintf(buf + count, PAGE_SIZE - count, "%4d", diag_coor[loop_i]);
			if ((loop_i % 16) == 15)
				count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
		}
	}
	return count;
}

static ssize_t himax_diag_dump(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const uint8_t command_ec_128_raw_flag 		= 0x02;
	const uint8_t command_ec_24_normal_flag 	= 0x00;
	uint8_t command_ec_128_raw_baseline_flag 	= 0x01;
	uint8_t command_ec_128_raw_bank_flag 			= 0x03;

	uint8_t command_F1h[2] = {0xF1, 0x00};
	uint8_t receive[1];

	memset(receive, 0x00, sizeof(receive));

	diag_command = buf[0] - '0';

	I("[Himax]diag_command=0x%x\n", diag_command);
	if (diag_command == 0x01)	{
		command_F1h[1] = command_ec_128_raw_baseline_flag;
		i2c_himax_write(private_ts->client, command_F1h[0] , &command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x02) {
		command_F1h[1] = command_ec_128_raw_flag;
		i2c_himax_write(private_ts->client, command_F1h[0] , &command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x03) {	
		command_F1h[1] = command_ec_128_raw_bank_flag;	
		i2c_himax_write(private_ts->client, command_F1h[0] , &command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x04) {  
		command_F1h[1] = 0x04; 
		i2c_himax_write(private_ts->client, command_F1h[0] , &command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x00) {
		command_F1h[1] = command_ec_24_normal_flag;
		i2c_himax_write(private_ts->client, command_F1h[0] , &command_F1h[1], 1, DEFAULT_RETRY_CNT);
		touch_monitor_stop_flag = touch_monitor_stop_limit;
	}

	
	else if (diag_command == 0x08)	{
		coordinate_fn = filp_open(DIAG_COORDINATE_FILE, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, 0666);
		if (IS_ERR(coordinate_fn)) {
			E("%s: coordinate_dump_file_create error\n", __func__);
			coordinate_dump_enable = 0;
			filp_close(coordinate_fn, NULL);
		}
		coordinate_dump_enable = 1;
	} else if (diag_command == 0x09) {
		coordinate_dump_enable = 0;

		if (!IS_ERR(coordinate_fn)) {
			filp_close(coordinate_fn, NULL);
		}
	}
	
	else {
		E("[Himax]Diag command error!diag_command=0x%x\n", diag_command);
	}
	return count;
}
static DEVICE_ATTR(diag, (S_IWUSR | S_IRUGO), himax_diag_show, himax_diag_dump);
#endif

#ifdef HX_TP_SYS_RESET
static ssize_t himax_reset_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '1')
		himax_HW_reset(true, false);
	return count;
}

static DEVICE_ATTR(reset, (S_IWUSR), NULL, himax_reset_set);
#endif

#ifdef HX_TP_SYS_DEBUG
static ssize_t himax_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	if (debug_level_cmd == 't') {
		if (fw_update_complete) {
			count += scnprintf(buf, PAGE_SIZE, "FW Update Complete ");
		} else {
			count += scnprintf(buf, PAGE_SIZE, "FW Update Fail ");
		}
	} else if (debug_level_cmd == 'h') {
		if (handshaking_result == 0) {
			count += scnprintf(buf, PAGE_SIZE, "Handshaking Result = %d (MCU Running)\n", handshaking_result);
		} else if (handshaking_result == 1) {
			count += scnprintf(buf, PAGE_SIZE, "Handshaking Result = %d (MCU Stop)\n", handshaking_result);
		} else if (handshaking_result == 2) {
			count += scnprintf(buf, PAGE_SIZE, "Handshaking Result = %d (I2C Error)\n", handshaking_result);
		} else {
			count += scnprintf(buf, PAGE_SIZE, "Handshaking Result = error \n");
		}
	} else if (debug_level_cmd == 'v') {
		count += scnprintf(buf + count, PAGE_SIZE - count, "FW_VER = 0x%2.2X, %2.2X \n", private_ts->vendor_fw_ver_H, private_ts->vendor_fw_ver_L);
		count += scnprintf(buf + count, PAGE_SIZE - count, "CONFIG_VER = 0x%2.2X \n", private_ts->vendor_config_ver);
	} else if (debug_level_cmd == 'd') {
		count += scnprintf(buf + count, PAGE_SIZE - count, "Himax Touch IC Information :\n");
		if (IC_TYPE == HX_85XX_D_SERIES_PWON) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Type : D\n");
		} else if (IC_TYPE == HX_85XX_E_SERIES_PWON) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Type : E\n");
		} else if (IC_TYPE == HX_85XX_ES_SERIES_PWON) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Type : ES\n");
		} else {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Type error.\n");
		}

		if (IC_CHECKSUM == HX_TP_BIN_CHECKSUM_SW) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Checksum : SW\n");
		} else if (IC_CHECKSUM == HX_TP_BIN_CHECKSUM_HW) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Checksum : HW\n");
		} else if (IC_CHECKSUM == HX_TP_BIN_CHECKSUM_CRC) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Checksum : CRC\n");
		} else {
			count += scnprintf(buf + count, PAGE_SIZE - count, "IC Checksum error.\n");
		}

		if (HX_INT_IS_EDGE) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "Interrupt : EDGE TIRGGER\n");
		} else {
			count += scnprintf(buf + count, PAGE_SIZE - count, "Interrupt : LEVEL TRIGGER\n");
		}

		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "RX Num : %d\nTX Num : %d\nBT Num : %d\n"
				   "X Resolution : %d\nY Resolution : %d\nMax Point : %d\n",
				   HX_RX_NUM, HX_TX_NUM, HX_BT_NUM, HX_X_RES, HX_Y_RES, HX_MAX_PT);
#ifdef HX_TP_SYS_2T2R
		if (Is_2T2R)
			count += scnprintf(buf + count, PAGE_SIZE - count, "2T2R panel\nRX Num_2 : %d\nTX Num_2 : %d\n", HX_RX_NUM_2, HX_TX_NUM_2);
#endif
	} else if (debug_level_cmd == 'i')
		count += scnprintf(buf, PAGE_SIZE, "Himax Touch Driver Version:\n%s \n", HIMAX_DRIVER_VER);

	return count;
}

static ssize_t himax_debug_dump(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct file* filp = NULL;
	mm_segment_t oldfs;
	int result = 0;
	char fileName[128];

	if (buf[0] == 'h') {  
		debug_level_cmd = buf[0];

		himax_int_enable(private_ts->client->irq, 0);

		handshaking_result = himax_hand_shaking(); 

		himax_int_enable(private_ts->client->irq, 1);

		return count;
	}

	else if (buf[0] == 'v') {  
		debug_level_cmd = buf[0];
		himax_read_FW_ver(true);
		return count;
	}

	else if (buf[0] == 'd') {  
		debug_level_cmd = buf[0];
		return count;
	}

	else if (buf[0] == 'i') {  
		debug_level_cmd = buf[0];
		return count;
	}

	else if (buf[0] == 't') {

		himax_int_enable(private_ts->client->irq, 0);
		wake_lock(&private_ts->ts_flash_wake_lock);
#ifdef HX_CHIP_STATUS_MONITOR
		HX_CHIP_POLLING_COUNT = 0;
		cancel_delayed_work_sync(&private_ts->himax_chip_monitor);
#endif

		debug_level_cmd 	= buf[0];
		fw_update_complete	= false;

		memset(fileName, 0, 128);
		
		if (sizeof(fileName) > count - 2)
			scnprintf(fileName, count - 2, "%s", &buf[2]);
		else {
			E("%s: file name is too long\n", __func__);
			goto firmware_upgrade_done;
			
		}

		I("%s: upgrade from file(%s) start!\n", __func__, fileName);
		
		filp = filp_open(fileName, O_RDONLY, 0);
		if (IS_ERR(filp)) {
			E("%s: open firmware file failed\n", __func__);
			goto firmware_upgrade_done;
			
		}
		oldfs = get_fs();
		set_fs(get_ds());

		
		result = filp->f_op->read(filp, upgrade_fw, sizeof(upgrade_fw), &filp->f_pos);
		if (result < 0) {
			E("%s: read firmware file failed\n", __func__);
			goto firmware_upgrade_done;
			
		}

		set_fs(oldfs);
		filp_close(filp, NULL);

		I("%s: upgrade start,len %d: %02X, %02X, %02X, %02X\n", __func__, result, upgrade_fw[0], upgrade_fw[1], upgrade_fw[2], upgrade_fw[3]);

		if (result > 0) {
			
			
#ifdef HX_RST_PIN_FUNC
			himax_HW_reset(false, true);
#endif
			if (fts_ctpm_fw_upgrade_with_sys_fs(upgrade_fw, result, true) == 0) {
				E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
				fw_update_complete = false;
			} else {
				I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
				fw_update_complete = true;
			}
			
			goto firmware_upgrade_done;
			
		}
	}

firmware_upgrade_done:

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, true);
#endif
	himax_read_TP_info(private_ts->client);

	wake_unlock(&private_ts->ts_flash_wake_lock);
	himax_int_enable(private_ts->client->irq, 1);
#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	queue_delayed_work(private_ts->himax_chip_monitor_wq, &private_ts->himax_chip_monitor, HX_POLLING_TIMES * HZ);
#endif
	
	
	
	return count;
}

static DEVICE_ATTR(debug, (S_IWUSR | S_IRUGO), himax_debug_show, himax_debug_dump);

#endif

#ifdef HX_TP_SYS_FLASH_DUMP

static uint8_t getFlashCommand(void)
{
	return flash_command;
}

static uint8_t getFlashDumpProgress(void)
{
	return flash_progress;
}

static uint8_t getFlashDumpComplete(void)
{
	return flash_dump_complete;
}

static uint8_t getFlashDumpFail(void)
{
	return flash_dump_fail;
}

static uint8_t getSysOperation(void)
{
	return sys_operation;
}

static uint8_t getFlashReadStep(void)
{
	return flash_read_step;
}

static uint8_t getFlashDumpSector(void)
{
	return flash_dump_sector;
}

static uint8_t getFlashDumpPage(void)
{
	return flash_dump_page;
}

static bool getFlashDumpGoing(void)
{
	return flash_dump_going;
}

static void setFlashBuffer(void)
{
	flash_buffer = kzalloc(FLASH_SIZE * sizeof(uint8_t), GFP_KERNEL);
	if(flash_buffer == NULL) {
		E("%s: flash_buffer kzalloc fail!", __func__);
	}
}

static void setSysOperation(uint8_t operation)
{
	sys_operation = operation;
}

static void setFlashDumpProgress(uint8_t progress)
{
	flash_progress = progress;
	
}

static void setFlashDumpComplete(uint8_t status)
{
	flash_dump_complete = status;
}

static void setFlashDumpFail(uint8_t fail)
{
	flash_dump_fail = fail;
}

static void setFlashCommand(uint8_t command)
{
	flash_command = command;
}

static void setFlashReadStep(uint8_t step)
{
	flash_read_step = step;
}

static void setFlashDumpSector(uint8_t sector)
{
	flash_dump_sector = sector;
}

static void setFlashDumpPage(uint8_t page)
{
	flash_dump_page = page;
}

static void setFlashDumpGoing(bool going)
{
	flash_dump_going = going;
}

static ssize_t himax_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int loop_i;
	uint8_t local_flash_read_step = 0;
	uint8_t local_flash_complete = 0;
	uint8_t local_flash_progress = 0;
	uint8_t local_flash_command = 0;
	uint8_t local_flash_fail = 0;

	local_flash_complete = getFlashDumpComplete();
	local_flash_progress = getFlashDumpProgress();
	local_flash_command = getFlashCommand();
	local_flash_fail = getFlashDumpFail();

	I("flash_progress = %d \n", local_flash_progress);

	if (local_flash_fail) {
		ret += scnprintf(buf + ret, PAGE_SIZE, "FlashStart:Fail \nFlashEnd\n");
		return ret;
	}

	if (!local_flash_complete) {
		ret += scnprintf(buf + ret, PAGE_SIZE, "FlashStart:Ongoing:0x%2.2x \nFlashEnd\n", flash_progress);
		return ret;
	}

	if (local_flash_command == 1 && local_flash_complete) {
		ret += scnprintf(buf + ret, PAGE_SIZE, "FlashStart:Complete \nFlashEnd\n");
		return ret;
	}

	if (local_flash_command == 3 && local_flash_complete) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "FlashStart: \n");
		for (loop_i = 0; loop_i < 128; loop_i++) {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "x%2.2x", flash_buffer[loop_i]);
			if ((loop_i % 16) == 15) {
				ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
			}
		}
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "FlashEnd\n");
		return ret;
	}

	
	local_flash_read_step = getFlashReadStep();

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "FlashStart:%2.2x \n", local_flash_read_step);

	for (loop_i = 0; loop_i < 1024; loop_i++) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "x%2.2X", flash_buffer[local_flash_read_step * 1024 + loop_i]);

		if ((loop_i % 16) == 15) {
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}
	}

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "FlashEnd\n");
	return ret;
}

static ssize_t himax_flash_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char buf_tmp[6];
	unsigned long result = 0;
	uint8_t loop_i = 0;
	int base = 0;

	memset(buf_tmp, 0x0, sizeof(buf_tmp));

	I("%s: buf[0] = %s\n", __func__, buf);

	if (getSysOperation() == 1) {
		E("%s: SYS is busy , return!\n", __func__);
		return count;
	}

	if (buf[0] == '0') {
		setFlashCommand(0);
		if (buf[1] == ':' && buf[2] == 'x') {
			memcpy(buf_tmp, buf + 3, 2);
			I("%s: read_Step = %s\n", __func__, buf_tmp);
			if (!kstrtoul(buf_tmp, 16, &result)) {
				I("%s: read_Step = %lu \n", __func__, result);
				setFlashReadStep(result);
			}
		}
	} else if (buf[0] == '1') {
		setSysOperation(1);
		setFlashCommand(1);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);
		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	} else if (buf[0] == '2') {
		setSysOperation(1);
		setFlashCommand(2);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	} else if (buf[0] == '3') {
		setSysOperation(1);
		setFlashCommand(3);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		memcpy(buf_tmp, buf + 3, 2);
		if (!kstrtoul(buf_tmp, 16, &result)) {
			setFlashDumpSector(result);
		}

		memcpy(buf_tmp, buf + 7, 2);
		if (!kstrtoul(buf_tmp, 16, &result)) {
			setFlashDumpPage(result);
		}

		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	} else if (buf[0] == '4') {
		I("%s: command 4 enter.\n", __func__);
		setSysOperation(1);
		setFlashCommand(4);
		setFlashDumpProgress(0);
		setFlashDumpComplete(0);
		setFlashDumpFail(0);

		memcpy(buf_tmp, buf + 3, 2);
		if (!kstrtoul(buf_tmp, 16, &result)) {
			setFlashDumpSector(result);
		} else {
			E("%s: command 4 , sector error.\n", __func__);
			return count;
		}

		memcpy(buf_tmp, buf + 7, 2);
		if (!kstrtoul(buf_tmp, 16, &result)) {
			setFlashDumpPage(result);
		} else {
			E("%s: command 4 , page error.\n", __func__);
			return count;
		}

		base = 11;

		I("=========Himax flash page buffer start=========\n");
		for (loop_i = 0; loop_i < 128; loop_i++) {
			memcpy(buf_tmp, buf + base, 2);
			if (!kstrtoul(buf_tmp, 16, &result)) {
				flash_buffer[loop_i] = result;
				I("%d ", flash_buffer[loop_i]);
				if (loop_i % 16 == 15) {
					I("\n");
				}
			}
			base += 3;
		}
		I("=========Himax flash page buffer end=========\n");

		queue_work(private_ts->flash_wq, &private_ts->flash_work);
	}
	return count;
}
static DEVICE_ATTR(flash_dump, (S_IWUSR | S_IRUGO), himax_flash_show, himax_flash_store);

static void himax_ts_flash_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, flash_work);

	uint8_t page_tmp[128];
	uint8_t x59_tmp[4] = {0, 0, 0, 0};
	int i = 0, j = 0, k = 0, l = 0, buffer_ptr = 0;
	uint8_t local_flash_command = 0;
	uint8_t sector = 0;
	uint8_t page = 0;

	
	uint8_t x81_command[2] = {0x81, 0x00};
	uint8_t x82_command[2] = {0x82, 0x00};
	uint8_t x35_command[2] = {0x35, 0x00};
	uint8_t x43_command[4] = {0x43, 0x00, 0x00, 0x00};
	uint8_t x44_command[4] = {0x44, 0x00, 0x00, 0x00};
	uint8_t x45_command[5] = {0x45, 0x00, 0x00, 0x00, 0x00};
	uint8_t x46_command[2] = {0x46, 0x00};
	
	uint8_t x4D_command[2] = {0x4D, 0x00};
	

	himax_int_enable(private_ts->client->irq, 0);
#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	cancel_delayed_work_sync(&private_ts->himax_chip_monitor);
#endif

	setFlashDumpGoing(true);

	sector = getFlashDumpSector();
	page = getFlashDumpPage();

	local_flash_command = getFlashCommand();

#ifdef HX_RST_PIN_FUNC
	if (local_flash_command < 0x0F)
		himax_HW_reset(false, false);
#endif

	
	
	
	
	
	

	if (i2c_himax_master_write(ts->client, x81_command, 1, 3) < 0) {   
		E("%s i2c write 81 fail.\n", __func__);
		goto Flash_Dump_i2c_transfer_error;
	}
	msleep(120);

	if (i2c_himax_master_write(ts->client, x82_command, 1, 3) < 0) {
		E("%s i2c write 82 fail.\n", __func__);
		goto Flash_Dump_i2c_transfer_error;
	}
	msleep(100);

	I("%s: local_flash_command = %d enter.\n", __func__, local_flash_command);

	if ((local_flash_command == 1 || local_flash_command == 2) || (local_flash_command == 0x0F)) {
		x43_command[1] = 0x01;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 1, DEFAULT_RETRY_CNT) < 0) {
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(100);

		for (i = 0 ; i < 8 ; i++) {
			for (j = 0 ; j < 32 ; j++) {
				
				
				for (k = 0; k < 128; k++) {
					page_tmp[k] = 0x00;
				}
				for (k = 0; k < 32; k++) {
					x44_command[1] = k;
					x44_command[2] = j;
					x44_command[3] = i;
					if (i2c_himax_write(ts->client, x44_command[0], &x44_command[1], 3, DEFAULT_RETRY_CNT) < 0) {
						E("%s i2c write 44 fail.\n", __func__);
						goto Flash_Dump_i2c_transfer_error;
					}

					if (i2c_himax_write_command(ts->client, x46_command[0], DEFAULT_RETRY_CNT) < 0) {
						E("%s i2c write 46 fail.\n", __func__);
						goto Flash_Dump_i2c_transfer_error;
					}
					
					if (i2c_himax_read(ts->client, 0x59, x59_tmp, 4, DEFAULT_RETRY_CNT) < 0) {
						E("%s i2c write 59 fail.\n", __func__);
						goto Flash_Dump_i2c_transfer_error;
					}
					
					for (l = 0; l < 4; l++) {
						page_tmp[k * 4 + l] = x59_tmp[l];
					}
					
				}
				

				for (k = 0; k < 128; k++) {
					flash_buffer[buffer_ptr++] = page_tmp[k];

				}
				setFlashDumpProgress(i * 32 + j);
			}
		}
	} else if (local_flash_command == 3) {
		x43_command[1] = 0x01;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 1, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(100);

		for (i = 0; i < 128; i++) {
			page_tmp[i] = 0x00;
		}

		for (i = 0; i < 32; i++) {
			x44_command[1] = i;
			x44_command[2] = page;
			x44_command[3] = sector;

			if (i2c_himax_write(ts->client, x44_command[0], &x44_command[1], 3, DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 44 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}

			if (i2c_himax_write_command(ts->client, x46_command[0], DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 46 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}
			
			if (i2c_himax_read(ts->client, 0x59, x59_tmp, 4, DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 59 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}
			
			for (j = 0; j < 4; j++) {
				page_tmp[i * 4 + j] = x59_tmp[j];
			}
			
		}
		
		for (i = 0; i < 128; i++) {
			flash_buffer[buffer_ptr++] = page_tmp[i];
		}
	} else if (local_flash_command == 4) {
		
		

		
		himax_lock_flash(0);

		msleep(50);

		
		x43_command[1] = 0x01;
		x43_command[2] = 0x00;
		x43_command[3] = 0x02;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 3, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		x44_command[1] = 0x00;
		x44_command[2] = page;
		x44_command[3] = sector;
		if (i2c_himax_write(ts->client, x44_command[0], &x44_command[1], 3, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 44 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		if (i2c_himax_write_command(ts->client, x4D_command[0], DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 4D fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(100);

		

		x35_command[1] = 0x01;
		if (i2c_himax_write(ts->client, x35_command[0], &x35_command[1], 1, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 35 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}

		msleep(100);

		
		x43_command[1] = 0x01;
		x43_command[2] = 0x00;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		
		x44_command[1] = 0x00;
		x44_command[2] = page;
		x44_command[3] = sector;
		if (i2c_himax_write(ts->client, x44_command[0], &x44_command[1], 3, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 44 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		
		x43_command[1] = 0x01;
		x43_command[2] = 0x09;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		x43_command[1] = 0x01;
		x43_command[2] = 0x0D;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		x43_command[1] = 0x01;
		x43_command[2] = 0x09;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		for (i = 0; i < 32; i++) {
			I("himax :i=%d \n", i);
			x44_command[1] = i;
			x44_command[2] = page;
			x44_command[3] = sector;
			if (i2c_himax_write(ts->client, x44_command[0], &x44_command[1], 3, DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 44 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}
			msleep(10);

			x45_command[1] = flash_buffer[i * 4 + 0];
			x45_command[2] = flash_buffer[i * 4 + 1];
			x45_command[3] = flash_buffer[i * 4 + 2];
			x45_command[4] = flash_buffer[i * 4 + 3];
			if (i2c_himax_write(ts->client, x45_command[0], &x45_command[1], 4, DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 45 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}
			msleep(10);

			// manual mode command : 48 ,data will be written into flash buffer
			x43_command[1] = 0x01;
			x43_command[2] = 0x0D;
			if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 43 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}
			msleep(10);

			x43_command[1] = 0x01;
			x43_command[2] = 0x09;
			if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
				E("%s i2c write 43 fail.\n", __func__);
				goto Flash_Dump_i2c_transfer_error;
			}
			msleep(10);
		}

		
		x43_command[1] = 0x01;
		x43_command[2] = 0x01;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		x43_command[1] = 0x01;
		x43_command[2] = 0x05;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		x43_command[1] = 0x01;
		x43_command[2] = 0x01;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		x43_command[1] = 0x01;
		x43_command[2] = 0x00;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 2, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		
		x43_command[1] = 0x00;
		if (i2c_himax_write(ts->client, x43_command[0], &x43_command[1], 1, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 43 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}
		msleep(10);

		
		x35_command[1] = 0x01;
		if (i2c_himax_write(ts->client, x35_command[0], &x35_command[1], 1, DEFAULT_RETRY_CNT) < 0) {
			E("%s i2c write 35 fail.\n", __func__);
			goto Flash_Dump_i2c_transfer_error;
		}

		msleep(10);

		
		himax_lock_flash(1);
		msleep(50);

		buffer_ptr = 128;
		I("Himax: Flash page write Complete~~~~~~~~~~~~~~~~~~~~~~~\n");
	}

	I("Complete~~~~~~~~~~~~~~~~~~~~~~~\n");
	if (local_flash_command == 0x01) {
		I(" buffer_ptr = %d \n", buffer_ptr);

		for (i = 0; i < buffer_ptr; i++) {
			I("%2.2X ", flash_buffer[i]);

			if ((i % 16) == 15) {
				I("\n");
			}
		}
		I("End~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	}

	i2c_himax_master_write(ts->client, x43_command, 1, 3);
	msleep(50);

	if (local_flash_command == 2) {
		struct file *fn;

		fn = filp_open(FLASH_DUMP_FILE, O_CREAT | O_WRONLY , 0);
		if (!IS_ERR(fn)) {
			fn->f_op->write(fn, flash_buffer, buffer_ptr * sizeof(uint8_t), &fn->f_pos);
			filp_close(fn, NULL);
		}
	}

#ifdef HX_RST_PIN_FUNC
	if (local_flash_command < 0x0F)
		himax_HW_reset(true, false);
#endif

	himax_int_enable(private_ts->client->irq, 1);
#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	queue_delayed_work(private_ts->himax_chip_monitor_wq, &private_ts->himax_chip_monitor, HX_POLLING_TIMES * HZ);
#endif
	setFlashDumpGoing(false);

	setFlashDumpComplete(1);
	setSysOperation(0);
	return;

Flash_Dump_i2c_transfer_error:

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, false);
#endif

	himax_int_enable(private_ts->client->irq, 1);
#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	queue_delayed_work(private_ts->himax_chip_monitor_wq, &private_ts->himax_chip_monitor, HX_POLLING_TIMES * HZ);
#endif
	setFlashDumpGoing(false);
	setFlashDumpComplete(0);
	setFlashDumpFail(1);
	setSysOperation(0);
	return;
}
#endif

#ifdef HX_TP_SYS_SELF_TEST
static ssize_t himax_chip_self_test_function(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val = 0x00;
	val = himax_chip_self_test();

	if (val == 0x00) {
		return scnprintf(buf, PAGE_SIZE, "Self_Test Pass\n");
	} else {
		return scnprintf(buf, PAGE_SIZE, "Self_Test Fail\n");
	}
}

static int himax_chip_self_test(void)
{
	uint8_t cmdbuf[11];
	uint8_t valuebuf[16];
	int i = 0, pf_value = 0x00;

	memset(cmdbuf, 0x00, sizeof(cmdbuf));
	memset(valuebuf, 0x00, sizeof(valuebuf));

	cmdbuf[0] = 0x06;
	i2c_himax_write(private_ts->client, 0xF1, &cmdbuf[0], 1, DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_write(private_ts->client, 0x83, &cmdbuf[0], 0, DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_write(private_ts->client, 0x81, &cmdbuf[0], 0, DEFAULT_RETRY_CNT);
	msleep(2000);

	i2c_himax_write(private_ts->client, 0x82, &cmdbuf[0], 0, DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_write(private_ts->client, 0x80, &cmdbuf[0], 0, DEFAULT_RETRY_CNT);
	msleep(120);

	cmdbuf[0] = 0x00;
	i2c_himax_write(private_ts->client, 0xF1, &cmdbuf[0], 1, DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_read(private_ts->client, 0xB1, valuebuf, 8, DEFAULT_RETRY_CNT);
	msleep(10);

	for (i = 0; i < 8; i++) {
		I("[Himax]: After slf test 0xB1 buff_back[%d] = 0x%x\n", i, valuebuf[i]);
	}

	msleep(30);

	if (valuebuf[0] == 0xAA) {
		I("[Himax]: self-test pass\n");
		pf_value = 0x0;
	} else {
		E("[Himax]: self-test fail\n");
		pf_value = 0x1;
	}

	return pf_value;
}

static DEVICE_ATTR(tp_self_test, (S_IWUSR | S_IRUGO), himax_chip_self_test_function, NULL);
#endif

#ifdef HX_TP_SYS_HITOUCH
static ssize_t himax_hitouch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (hitouch_command == 0) {
		ret += scnprintf(buf + ret, PAGE_SIZE, "Driver Version:2.0 \n");
	}

	return ret;
}

static ssize_t himax_hitouch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '0') {
		hitouch_command = 0;
	} else if (buf[0] == '1') {
		hitouch_is_connect = true;
		I("hitouch_is_connect = true\n");
	} else if (buf[0] == '2') {
		hitouch_is_connect = false;
		I("hitouch_is_connect = false\n");
	}
	return count;
}

static DEVICE_ATTR(hitouch, (S_IWUSR | S_IRUGO), himax_hitouch_show, himax_hitouch_store);
#endif

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
static ssize_t himax_cover_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	count = scnprintf(buf, PAGE_SIZE, "%d\n", ts->cover_enable);

	return count;
}

static ssize_t himax_cover_store(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t count)
{

	struct himax_ts_data *ts = private_ts;

	if (sysfs_streq(buf, "0"))
		ts->cover_enable = 0;
	else if (sysfs_streq(buf, "1"))
		ts->cover_enable = 1;
	else
		return -EINVAL;
	himax_set_cover_func(ts->cover_enable);

	I("%s: cover_enable = %d.\n", __func__, ts->cover_enable);

	return count;
}

static DEVICE_ATTR(cover, (S_IWUSR | S_IRUGO),
		   himax_cover_show, himax_cover_store);
#endif	
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_SMWP_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	count = scnprintf(buf, PAGE_SIZE, "%d\n", ts->SMWP_enable);

	return count;
}

static ssize_t himax_SMWP_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{

	struct himax_ts_data *ts = private_ts;

	if (sysfs_streq(buf, "0"))
		ts->SMWP_enable = 0;
	else if (sysfs_streq(buf, "1"))
		ts->SMWP_enable = 1;
	else
		return -EINVAL;

	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, ts->SMWP_enable);

	return count;
}

static DEVICE_ATTR(SMWP, (S_IWUSR | S_IRUGO),
		   himax_SMWP_show, himax_SMWP_store);

#endif

enum SR_REG_STATE {
	ALLOCATE_DEV_FAIL = -2,
	REGISTER_DEV_FAIL,
	SUCCESS,
};

static int register_sr_touch_device(void)
{
	struct himax_ts_data *ts = private_ts;
	ts->sr_input_dev = input_allocate_device();

	if (ts->sr_input_dev == NULL) {
		E("[SR]%s: Failed to allocate SR input device\n", __func__);
		return ALLOCATE_DEV_FAIL;
	}
	ts->sr_input_dev->name = "sr_touchscreen";
	set_bit(EV_SYN, ts->sr_input_dev->evbit);
	set_bit(EV_ABS, ts->sr_input_dev->evbit);
	set_bit(EV_KEY, ts->sr_input_dev->evbit);

	set_bit(KEY_BACK, ts->sr_input_dev->keybit);
	set_bit(KEY_HOME, ts->sr_input_dev->keybit);
	set_bit(KEY_MENU, ts->sr_input_dev->keybit);
	set_bit(KEY_SEARCH, ts->sr_input_dev->keybit);
	set_bit(BTN_TOUCH, ts->sr_input_dev->keybit);
	
	set_bit(KEY_APPSELECT, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->sr_input_dev->propbit);

	input_set_abs_params(ts->sr_input_dev, ABS_MT_TRACKING_ID,
			     0, 3, 0, 0);
	I("[SR]input_set_abs_params: mix_x %d, max_x %d,"
	  " min_y %d, max_y %d\n", ts->pdata->abs_x_min,
	  ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	input_set_abs_params(ts->sr_input_dev, ABS_MT_POSITION_X, ts->pdata->abs_x_min, ts->pdata->abs_x_max, 0, 0);
	input_set_abs_params(ts->sr_input_dev, ABS_MT_POSITION_Y, ts->pdata->abs_y_min, ts->pdata->abs_y_max, 0, 0);
	input_set_abs_params(ts->sr_input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, 0, 0);
	input_set_abs_params(ts->sr_input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, 0, 0);
	input_set_abs_params(ts->sr_input_dev, ABS_MT_WIDTH_MAJOR, ts->pdata->abs_width_min, ts->pdata->abs_width_max, 0, 0);

	if (input_register_device(ts->sr_input_dev)) {
		input_free_device(ts->sr_input_dev);
		E("[SR]%s: Unable to register %s input device\n",
		  __func__, ts->sr_input_dev->name);
		return REGISTER_DEV_FAIL;
	}
	return SUCCESS;
}

static ssize_t himax_get_en_sr(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;

	if (ts->sr_input_dev) {
		count += scnprintf(buf + count, PAGE_SIZE, "%s \n", ts->sr_input_dev->name);
	} else
		count += scnprintf(buf + count, PAGE_SIZE, "0\n");
	return count;
}
static ssize_t himax_set_en_sr(struct device *dev, struct device_attribute * attr,
			       const char *buf, size_t count)
{
	struct himax_ts_data *ts_data;
	ts_data = private_ts;
	if (buf[0]) {
		if (ts_data->sr_input_dev)
			I("[SR]%s: SR device already exist!\n", __func__);
		else
			I("[SR]%s: SR touch device enable result:%X\n", __func__, register_sr_touch_device());
	}
	return count;
}
static DEVICE_ATTR(sr_en, (S_IWUSR | S_IRUGO), himax_get_en_sr, himax_set_en_sr);
static int himax_touch_sysfs_init(void)
{
	int ret;
	android_touch_kobj = kobject_create_and_add("android_touch", NULL);
	if (android_touch_kobj == NULL) {
		E("%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug_level.attr);
	if (ret) {
		E("%s: create_file dev_attr_debug_level failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_vendor failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_reset.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_reset failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_attn.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_attn failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_enabled.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_enabled failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_layout.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_layout failed\n", __func__);
		return ret;
	}

#ifdef HX_TP_SYS_REGISTER
	register_command = 0;
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_register.attr);
	if (ret) {
		E("create_file dev_attr_register failed\n");
		return ret;
	}
#endif

#ifdef HX_TP_SYS_DIAG
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_diag.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_diag failed\n");
		return ret;
	}
#endif

#ifdef HX_TP_SYS_DEBUG
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug.attr);
	if (ret) {
		E("create_file dev_attr_debug failed\n");
		return ret;
	}
#endif

#ifdef HX_TP_SYS_FLASH_DUMP
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_flash_dump.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_flash_dump failed\n");
		return ret;
	}
#endif

#ifdef HX_TP_SYS_SELF_TEST
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_tp_self_test.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_tp_self_test failed\n");
		return ret;
	}
#endif

#ifdef HX_TP_SYS_HITOUCH
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_hitouch.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_hitouch failed\n");
		return ret;
	}
#endif

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (private_ts->cover_support) {
		ret = sysfs_create_file(android_touch_kobj, &dev_attr_cover.attr);
		if (ret) {
			E("sysfs_create_file dev_attr_cover failed\n");
			return ret;
		}
	}
#endif	
#endif

#ifdef HX_SMART_WAKEUP
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_SMWP.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_SMWP failed\n");
		return ret;
	}
#endif

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_sr_en.attr);
	if (ret) {
		E("[SR]%s: sysfs_create_file dev_attr_sr_en failed\n", __func__);
		return ret;
	}

#ifdef HX_MTK_DMA
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_loadconfig.attr);
	if (ret) {
		E("sysfs_create_file loadconfig, failed\n");
		return ret;
	}
#endif

	return 0 ;
}

static void himax_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_debug_level.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_reset.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_attn.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_enabled.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_layout.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_sr_en.attr);

#ifdef HX_TP_SYS_REGISTER
	sysfs_remove_file(android_touch_kobj, &dev_attr_register.attr);
#endif

#ifdef HX_TP_SYS_DIAG
	sysfs_remove_file(android_touch_kobj, &dev_attr_diag.attr);
#endif

#ifdef HX_TP_SYS_DEBUG
	sysfs_remove_file(android_touch_kobj, &dev_attr_debug.attr);
#endif

#ifdef HX_TP_SYS_FLASH_DUMP
	sysfs_remove_file(android_touch_kobj, &dev_attr_flash_dump.attr);
#endif

#ifdef HX_TP_SYS_SELF_TEST
	sysfs_remove_file(android_touch_kobj, &dev_attr_tp_self_test.attr);
#endif

#ifdef HX_TP_SYS_HITOUCH
	sysfs_remove_file(android_touch_kobj, &dev_attr_hitouch.attr);
#endif

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (private_ts->cover_support)
		sysfs_remove_file(android_touch_kobj, &dev_attr_cover.attr);
#endif	
#endif

#ifdef HX_SMART_WAKEUP
	sysfs_remove_file(android_touch_kobj, &dev_attr_SMWP.attr);
#endif

#ifdef HX_MTK_DMA
	sysfs_remove_file(android_touch_kobj, &dev_attr_loadconfig.attr);
#endif

	kobject_del(android_touch_kobj);
}
#endif

#ifdef CONFIG_OF
#if defined(HX_LOADIN_CONFIG)
static int himax_parse_config(struct himax_ts_data *ts, struct himax_config *pdata)
{
	struct himax_config *cfg_table;
	struct device_node *node = NULL, *pp = NULL;
	struct property *prop;
	uint8_t cnt = 0, i = 0;
	u32 data = 0;
	uint32_t coords[4] = {0};
	int len = 0;
	char str[6] = {0};

	node = ts->client->dev.of_node;
	if (node == NULL) {
		E(" %s, can't find device_node", __func__);
		return -ENODEV;
	}

	while ((pp = of_get_next_child(node, pp)))
		cnt++;

	if (!cnt)
		return -ENODEV;

	cfg_table = devm_kzalloc(&ts->client->dev,
				 cnt * (sizeof * cfg_table), GFP_KERNEL);

	if (!cfg_table)
		return -ENOMEM;

	pp = NULL;
	while ((pp = of_get_next_child(node, pp))) {
		if (of_property_read_u32(pp, "default_cfg", &data) == 0)
			cfg_table[i].default_cfg = data;

		if (of_property_read_u32(pp, "sensor_id", &data) == 0)
			cfg_table[i].sensor_id = (data);

		if (of_property_read_u32(pp, "fw_ver", &data) == 0)
			cfg_table[i].fw_ver = data;

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		cfg_table[i].cover_support = of_property_read_bool(pp, "cover_support");
#endif	
#endif

		if (of_property_read_u32_array(pp, "himax,tw-coords", coords, 4) == 0) {
			cfg_table[i].tw_x_min = coords[0], cfg_table[i].tw_x_max = coords[1];	
			cfg_table[i].tw_y_min = coords[2], cfg_table[i].tw_y_max = coords[3];	
		}

		if (of_property_read_u32_array(pp, "himax,pl-coords", coords, 4) == 0) {
			cfg_table[i].pl_x_min = coords[0], cfg_table[i].pl_x_max = coords[1];	
			cfg_table[i].pl_y_min = coords[2], cfg_table[i].pl_y_max = coords[3];	
		}

		prop = of_find_property(pp, "c1", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c1");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c1, prop->value, len);
		prop = of_find_property(pp, "c2", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c2");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c2, prop->value, len);
		prop = of_find_property(pp, "c3", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c3");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c3, prop->value, len);
		prop = of_find_property(pp, "c4", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c4");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c4, prop->value, len);
		prop = of_find_property(pp, "c5", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c5");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c5, prop->value, len);
		prop = of_find_property(pp, "c6", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c6");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c6, prop->value, len);
		prop = of_find_property(pp, "c7", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c7");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c7, prop->value, len);
		prop = of_find_property(pp, "c8", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c8");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c8, prop->value, len);
		prop = of_find_property(pp, "c9", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 3, "%s", "c9");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c9, prop->value, len);
		prop = of_find_property(pp, "c10", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c10");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c10, prop->value, len);
		prop = of_find_property(pp, "c11", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c11");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c11, prop->value, len);
		prop = of_find_property(pp, "c12", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c12");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c12, prop->value, len);
		prop = of_find_property(pp, "c13", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c13");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c13, prop->value, len);
		prop = of_find_property(pp, "c14", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c14");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c14, prop->value, len);
		prop = of_find_property(pp, "c15", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c15");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c15, prop->value, len);
		prop = of_find_property(pp, "c16", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c16");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c16, prop->value, len);
		prop = of_find_property(pp, "c17", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c17");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c17, prop->value, len);
		prop = of_find_property(pp, "c18", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c18");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c18, prop->value, len);
		prop = of_find_property(pp, "c19", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c19");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c19, prop->value, len);
		prop = of_find_property(pp, "c20", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c20");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c20, prop->value, len);
		prop = of_find_property(pp, "c21", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c21");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c21, prop->value, len);
		prop = of_find_property(pp, "c22", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c22");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c22, prop->value, len);
		prop = of_find_property(pp, "c23", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c23");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c23, prop->value, len);
		prop = of_find_property(pp, "c24", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c24");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c24, prop->value, len);
		prop = of_find_property(pp, "c25", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c25");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c25, prop->value, len);
		prop = of_find_property(pp, "c26", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c26");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c26, prop->value, len);
		prop = of_find_property(pp, "c27", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c27");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c27, prop->value, len);
		prop = of_find_property(pp, "c28", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c28");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c28, prop->value, len);
		prop = of_find_property(pp, "c29", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c29");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c29, prop->value, len);
		prop = of_find_property(pp, "c30", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c30");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c30, prop->value, len);
		prop = of_find_property(pp, "c31", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c31");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c31, prop->value, len);
		prop = of_find_property(pp, "c32", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c32");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c32, prop->value, len);
		prop = of_find_property(pp, "c33", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c33");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c33, prop->value, len);
		prop = of_find_property(pp, "c34", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c34");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c34, prop->value, len);
		prop = of_find_property(pp, "c35", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c35");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c35, prop->value, len);
		prop = of_find_property(pp, "c36", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c36");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c36, prop->value, len);
		prop = of_find_property(pp, "c37", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c37");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c37, prop->value, len);
		prop = of_find_property(pp, "c38", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c38");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c38, prop->value, len);
		prop = of_find_property(pp, "c39", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c39");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c39, prop->value, len);
		prop = of_find_property(pp, "c40", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c40");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c40, prop->value, len);
		prop = of_find_property(pp, "c41", &len);
		if ((!prop) || (!len)) {
			scnprintf(str, 4, "%s", "c41");
			goto of_find_property_error;
		}
		memcpy(cfg_table[i].c41, prop->value, len);

#if 1
		I(" DT#%d-def_cfg:%d,id:%05X, FW:0x%02X, len:%d, config version=[%02x]\n", i,
		  cfg_table[i].default_cfg, cfg_table[i].sensor_id,
		  cfg_table[i].fw_ver, cfg_table[i].length, cfg_table[i].c40[1]);
		
#endif
		i++;
of_find_property_error:
		if (!prop) {
			D(" %s:Looking up %s property in node %s failed",
			  __func__, str, pp->full_name);
			return -ENODEV;
		} else if (!len) {
			D(" %s:Invalid length of configuration data in %s\n",
			  __func__, str);
			return -EINVAL;
		}
	}

	i = 0;	
	while (cfg_table[i].fw_ver > ts->vendor_fw_ver_H) {
		i++;
	}
	if (cfg_table[i].default_cfg != 0)
		goto startloadconf;
	while (cfg_table[i].sensor_id > 0 && (cfg_table[i].sensor_id != ts->vendor_sensor_id)) {
		I(" id:%#x!=%#x, (i++)", cfg_table[i].sensor_id, ts->vendor_sensor_id);
		i++;
	}
startloadconf:
	if (i <= cnt) {
		I(" DT-%s cfg idx(%d) in cnt(%d)\n", __func__, i, cnt);
		pdata->fw_ver 		= cfg_table[i].fw_ver;
		pdata->sensor_id	= cfg_table[i].sensor_id;

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
		ts->cover_support = cfg_table[i].cover_support;
		if (ts->cover_support)
			I(" DT-%s: cover_support", __func__);
#endif	
#endif

		memcpy(pdata->c1, cfg_table[i].c1, sizeof(pdata->c1));
		memcpy(pdata->c2, cfg_table[i].c2, sizeof(pdata->c2));
		memcpy(pdata->c3, cfg_table[i].c3, sizeof(pdata->c3));
		memcpy(pdata->c4, cfg_table[i].c4, sizeof(pdata->c4));
		memcpy(pdata->c5, cfg_table[i].c5, sizeof(pdata->c5));
		memcpy(pdata->c6, cfg_table[i].c6, sizeof(pdata->c6));
		memcpy(pdata->c7, cfg_table[i].c7, sizeof(pdata->c7));
		memcpy(pdata->c8, cfg_table[i].c8, sizeof(pdata->c8));
		memcpy(pdata->c9, cfg_table[i].c9, sizeof(pdata->c9));
		memcpy(pdata->c10, cfg_table[i].c10, sizeof(pdata->c10));
		memcpy(pdata->c11, cfg_table[i].c11, sizeof(pdata->c11));
		memcpy(pdata->c12, cfg_table[i].c12, sizeof(pdata->c12));
		memcpy(pdata->c13, cfg_table[i].c13, sizeof(pdata->c13));
		memcpy(pdata->c14, cfg_table[i].c14, sizeof(pdata->c14));
		memcpy(pdata->c15, cfg_table[i].c15, sizeof(pdata->c15));
		memcpy(pdata->c16, cfg_table[i].c16, sizeof(pdata->c16));
		memcpy(pdata->c17, cfg_table[i].c17, sizeof(pdata->c17));
		memcpy(pdata->c18, cfg_table[i].c18, sizeof(pdata->c18));
		memcpy(pdata->c19, cfg_table[i].c19, sizeof(pdata->c19));
		memcpy(pdata->c20, cfg_table[i].c20, sizeof(pdata->c20));
		memcpy(pdata->c21, cfg_table[i].c21, sizeof(pdata->c21));
		memcpy(pdata->c22, cfg_table[i].c22, sizeof(pdata->c22));
		memcpy(pdata->c23, cfg_table[i].c23, sizeof(pdata->c23));
		memcpy(pdata->c24, cfg_table[i].c24, sizeof(pdata->c24));
		memcpy(pdata->c25, cfg_table[i].c25, sizeof(pdata->c25));
		memcpy(pdata->c26, cfg_table[i].c26, sizeof(pdata->c26));
		memcpy(pdata->c27, cfg_table[i].c27, sizeof(pdata->c27));
		memcpy(pdata->c28, cfg_table[i].c28, sizeof(pdata->c28));
		memcpy(pdata->c29, cfg_table[i].c29, sizeof(pdata->c29));
		memcpy(pdata->c30, cfg_table[i].c30, sizeof(pdata->c30));
		memcpy(pdata->c31, cfg_table[i].c31, sizeof(pdata->c31));
		memcpy(pdata->c32, cfg_table[i].c32, sizeof(pdata->c32));
		memcpy(pdata->c33, cfg_table[i].c33, sizeof(pdata->c33));
		memcpy(pdata->c34, cfg_table[i].c34, sizeof(pdata->c34));
		memcpy(pdata->c35, cfg_table[i].c35, sizeof(pdata->c35));
		memcpy(pdata->c36, cfg_table[i].c36, sizeof(pdata->c36));
		memcpy(pdata->c37, cfg_table[i].c37, sizeof(pdata->c37));
		memcpy(pdata->c38, cfg_table[i].c38, sizeof(pdata->c38));
		memcpy(pdata->c39, cfg_table[i].c39, sizeof(pdata->c39));
		memcpy(pdata->c40, cfg_table[i].c40, sizeof(pdata->c40));
		memcpy(pdata->c41, cfg_table[i].c41, sizeof(pdata->c41));

		ts->tw_x_min = cfg_table[i].tw_x_min, ts->tw_x_max = cfg_table[i].tw_x_max;	
		ts->tw_y_min = cfg_table[i].tw_y_min, ts->tw_y_max = cfg_table[i].tw_y_max;	

		ts->pl_x_min = cfg_table[i].pl_x_min, ts->pl_x_max = cfg_table[i].pl_x_max;	
		ts->pl_y_min = cfg_table[i].pl_y_min, ts->pl_y_max = cfg_table[i].pl_y_max;	

		I(" DT#%d-def_cfg:%d,id:%05X, FW:0x%02X, len:%d, config version=[%02x]\n", i,
		  cfg_table[i].default_cfg, cfg_table[i].sensor_id,
		  cfg_table[i].fw_ver, cfg_table[i].length, cfg_table[i].c40[1]);
		I(" DT-%s:tw-coords = %d, %d, %d, %d\n", __func__, ts->tw_x_min,
		  ts->tw_x_max, ts->tw_y_min, ts->tw_y_max);
		I(" DT-%s:pl-coords = %d, %d, %d, %d\n", __func__, ts->pl_x_min,
		  ts->pl_x_max, ts->pl_y_min, ts->pl_y_max);
	} else {
		E(" DT-%s cfg idx(%d) > cnt(%d)", __func__, i, cnt);
		return -EINVAL;
	}
	return 0;
}
#endif
static void himax_vk_parser(struct device_node *dt,
			    struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);
	if (node == NULL) {
		I(" DT-No vk info in DT\n");
		return;
	} else {
		while ((pp = of_get_next_child(node, pp)))
			cnt++;
		if (!cnt)
			return;

		vk = kzalloc(cnt * (sizeof * vk), GFP_KERNEL);
		pp = NULL;
		while ((pp = of_get_next_child(node, pp))) {
			if (of_property_read_u32(pp, "idx", &data) == 0)
				vk[i].index = data;
			if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
				vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
				vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
			} else
				I(" range faile\n");
			i++;
		}
		pdata->virtual_key = vk;
		for (i = 0; i < cnt; i++)
			I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index,
			  pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}

static int himax_parse_dt(struct himax_ts_data *ts,
			  struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->client->dev.of_node;
	u32 data = 0;
#ifdef MTK
	int gpio = 0;
#endif

	prop = of_find_property(dt, "himax,panel-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			D(" %s:Invalid panel coords size %d\n", __func__, coords_size);
	}

	if (of_property_read_u32_array(dt, "himax,panel-coords", coords, coords_size) == 0) {
		pdata->abs_x_min = coords[0], pdata->abs_x_max = coords[1];
		pdata->abs_y_min = coords[2], pdata->abs_y_max = coords[3];
		I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
		  pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			D(" %s:Invalid display coords size %d\n", __func__, coords_size);
	}
	rc = of_property_read_u32_array(dt, "himax,display-coords", coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		D(" %s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}
	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	I(" DT-%s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth,
	  pdata->screenHeight);

#ifdef QCT
	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);
#endif
#ifdef MTK
	if (of_property_read_u32(dt, "himax,irq-gpio", &gpio) == 0) {
		pdata->gpio_irq = gpio;
	}
#endif
	if (!gpio_is_valid(pdata->gpio_irq)) {
		I(" DT:gpio_irq value is not valid\n");
	}

#ifdef QCT
	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);
#endif
#ifdef MTK
	if (of_property_read_u32(dt, "himax,rst-gpio", &gpio) == 0) {
		pdata->gpio_reset = gpio;
	}
#endif
	if (!gpio_is_valid(pdata->gpio_reset)) {
		I(" DT:gpio_reset value is not valid\n");
	}

#ifdef QCT
	pdata->gpio_1v8_en = of_get_named_gpio(dt, "himax,1v8-gpio", 0);
#endif
#ifdef MTK
	if (of_property_read_u32(dt, "himax,1v8-gpio", &gpio) == 0) {
		pdata->gpio_1v8_en = gpio;
	}
#endif
	if (!gpio_is_valid(pdata->gpio_1v8_en)) {
		I(" DT:gpio_1v8_en value is not valid\n");
	}

#ifdef QCT
	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);
#endif
#ifdef MTK
	if (of_property_read_u32(dt, "himax,3v3-gpio", &gpio) == 0) {
		pdata->gpio_3v3_en = gpio;
	}
#endif
	if (!gpio_is_valid(pdata->gpio_3v3_en)) {
		I(" DT:gpio_3v3_en value is not valid\n");
	}

#ifdef QCT
	pdata->gpio_switch = of_get_named_gpio(dt, "himax,switch-gpio", 0);
#endif
#ifdef MTK
	if (of_property_read_u32(dt, "himax,switch-gpio", &gpio) == 0) {
		pdata->gpio_switch = gpio;
	}
#endif
	if (!gpio_is_valid(pdata->gpio_switch)) {
		I(" DT:gpio_switch is not valid\n");
	}

	I(" DT:gpio_irq=%d, gpio_reset=%d, gpio_1v8_en=%d, gpio_3v3_en=%d, gpio_switch=%d\n",
	  pdata->gpio_irq, pdata->gpio_reset,
	  pdata->gpio_1v8_en, pdata->gpio_3v3_en,
	  pdata->gpio_switch);

	I("3v3_en pin =%d, 1v8_en pin =%d, irq pin = %d, reset pin = %d\n",
	  gpio_get_value(pdata->gpio_3v3_en),
	  gpio_get_value(pdata->gpio_1v8_en),
	  himax_int_gpio_read(pdata->gpio_irq),
	  gpio_get_value(pdata->gpio_reset));
	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d", pdata->protocol_type);
	}

	if (of_property_read_u32(dt, "update_feature", &data) == 0) {
		pdata->update_feature = data;
		I(" DT:update_feature=%d", pdata->update_feature);
	}

#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	if (of_property_read_u32(dt, "proximity_bytp_enable", &data) == 0) {
		pdata->proximity_bytp_enable = data;
		I(" DT:proximity_bytp_enable=%d\n", pdata->proximity_bytp_enable);
	}
#endif
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (of_property_read_u32(dt, "hall_block_touch_time", &data) == 0) {
		pdata->hall_block_touch_time = data;
		I(" DT:hall_block_touch_time=%d\n", pdata->hall_block_touch_time);
	}
#endif	
#endif
	himax_vk_parser(dt, pdata);

	return 0;
}

#ifdef CONFIG_SYNC_TOUCH_STATUS
static void switch_sensor_hub(struct himax_ts_data *ts, int mode)
{
	I("%s: %d\n", __func__, mode);
	switch (mode) {
	case 0:
		touch_status(0);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		himax_switch_gpio_set(ts->pdata->gpio_switch, 0);
#else
		gpio_direction_output(ts->pdata->gpio_switch, 0);
#endif
		ts->i2c_to_mcu = 0;
		pr_info("[SensorHub] Switch touch i2c to CPU");
		break;
	case 1:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
		himax_switch_gpio_set(ts->pdata->gpio_switch, 1);
#else
		gpio_direction_output(ts->pdata->gpio_switch, 1);
#endif
		ts->i2c_to_mcu = 1;
		pr_info("[SensorHub] Switch touch i2c to MCU");
		touch_status(1);
		break;
	}
	mdelay(10);
}
#endif
#endif	

static int himax852xes_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0, err = -1;
	struct himax_ts_data *ts;
	struct himax_i2c_platform_data *pdata;
	uint8_t buf[1] = {0};
#if !defined(CONFIG_HTC_HELPER_FUNCTIONS) && !defined(MTK)
	char* mode[] = {
		"MFG_MODE_NORMAL",
		"MFG_MODE_FACTORY2",
		"MFG_MODE_RECOVERY",
		"MFG_MODE_CHARGE",
		"MFG_MODE_POWER_TEST",
		"MFG_MODE_OFFMODE_CHARGING",
		"MFG_MODE_MFGKERNEL_DIAG58",
		"MFG_MODE_GIFT_MODE",
		"MFG_MODE_MFGKERNEL",
		"MFG_MODE_MINI",
	};
#endif

	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}
#ifdef HX_MTK_DMA
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	gpDMABuf_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, (dma_addr_t *)&gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va) {
		E("Allocate DMA I2C Buffer failed\n");
		err = -ENOMEM;
		goto err_alloc_MTK_DMA_failed;
	}
	memset(gpDMABuf_va, 0, 4096);
#endif
	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	i2c_set_clientdata(client, ts);
	ts->client = client;
	ts->dev = &client->dev;

#if 0
	dev_set_name(ts->dev, HIMAX852xes_NAME);
#endif

	if (client->dev.of_node) { 
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			err = -ENOMEM;
			goto err_alloc_dt_pdata_failed;
		}
		ret = himax_parse_dt(ts, pdata);
		if (ret < 0) {
			I(" pdata is NULL for DT\n");
			err = -ENOMEM;
			goto err_dt_platform_data_fail;
		}
		ts->dev->platform_data = pdata;
		ret = himax_pinctrl_init(ts->dev);
		if (ret < 0) {
			err = -ENOMEM;
			goto err_dt_platform_data_fail;
		} else
			himax_pinctrl_select(ts->dev, 1);
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			I(" pdata is NULL(dev.platform_data)\n");
			err = -ENOMEM;
			goto err_get_platform_data_fail;
		}
	}

#ifdef HX_RST_PIN_FUNC
	ts->rst_gpio = pdata->gpio_reset;
#endif

#ifdef CONFIG_SYNC_TOUCH_STATUS
	ts->switch_gpio = pdata->gpio_switch;
#endif

	himax_gpio_power_config(ts->client, pdata);

#ifndef CONFIG_OF
	if (pdata->power) {
		ret = pdata->power(1);
		if (ret < 0) {
			E("%s: power on failed\n", __func__);
			err = -ENODEV;
			goto err_power_failed;
		}
	}
#endif
	mutex_init(&(ts->ts_report_mutex));
	private_ts = ts;

	I("%s, pdata, %p\n", __func__ , pdata);
	
	if (himax_ic_package_check(ts) == false) {
		err = -ENODEV;
		E("Himax chip doesn NOT EXIST\n");
		goto err_ic_package_failed;
	}
#if defined(CONFIG_HTC_HELPER_FUNCTIONS) || defined(MTK)
	mfg_mode = htc_get_bootmode();
#else
	mfg_mode = mode[board_mfg_mode()];
#endif
	I("%s\n", mfg_mode);
	if ((strcmp(mfg_mode, "offmode_charging") == 0) ||
	    (strcmp(mfg_mode, "charger") == 0) ||
	    (strcmp(mfg_mode, "recovery") == 0) ||
	    (strcmp(mfg_mode, "MFG_MODE_OFFMODE_CHARGING") == 0) ||
	    (strcmp(mfg_mode, "MFG_MODE_POWER_TEST") == 0) ||
	    (strcmp(mfg_mode, "MFG_MODE_RECOVERY") == 0)) {
		I(" %s: %s mode. Set touch chip to sleep mode and skip touch driver probe\n",
		  __func__, mfg_mode);

		buf[0] = HX_CMD_TSSOFF;
		ret = i2c_himax_master_write(client, buf, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: I2C access failed addr = 0x%x\n", __func__, client->addr);
		msleep(30);

		buf[0] = HX_CMD_TSSLPIN;
		ret = i2c_himax_master_write(client, buf, 1, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			E("%s: I2C access failed addr = 0x%x\n", __func__, client->addr);
		msleep(30);
		err = -ENODEV;
		goto err_off_mode;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;
#ifdef HX_TP_SYS_FLASH_DUMP
	ts->flash_wq = create_singlethread_workqueue("himax_flash_wq");
	if (!ts->flash_wq) {
		E("%s: create flash workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_flash_wq_failed;
	}

	INIT_WORK(&ts->flash_work, himax_ts_flash_work_func);

	setSysOperation(0);
	setFlashBuffer();
#endif

	himax_read_TP_info(client);
#ifdef HX_AUTO_UPDATE_FW
	if (i_update_FW() == false)
		I("NOT Have new FW=NOT UPDATE=\n");
	else
		I("Have new FW=UPDATE=\n");
#else
	if (himax_calculateChecksum(false) == 0) {
		E("%s: Checksum error, please reflash Touch firmware!\n", __func__);
		goto err_FW_CRC_failed;
	}
#endif

	ts->chosen_config_process = LOAD_ALL_CFG;

	
	if (himax_loadSensorConfig(client, pdata) < 0) {
		E("%s: Load Sesnsor configuration failed, unload driver.\n", __func__);
		err = -ENODEV;
		goto err_detect_failed;
	}

	calculate_point_number();
#ifdef HX_TP_SYS_DIAG
	setXChannel(HX_RX_NUM); 
	setYChannel(HX_TX_NUM); 

	setMutualBuffer();
	if (getMutualBuffer() == NULL) {
		E("%s: mutual buffer allocate fail failed\n", __func__);
		goto err_setchannel_failed;
	}
#ifdef HX_TP_SYS_2T2R
	if (Is_2T2R) {
		setXChannel_2(HX_RX_NUM_2); 
		setYChannel_2(HX_TX_NUM_2); 

		setMutualBuffer_2();

		if (getMutualBuffer_2() == NULL) {
			E("%s: mutual buffer 2 allocate fail failed\n", __func__);
			goto err_setchannel_failed;
		}
	}
#endif
#endif
#ifdef CONFIG_OF
	ts->power = pdata->power;
#endif
	ts->pdata = pdata;

	ts->x_channel = HX_RX_NUM;
	ts->y_channel = HX_TX_NUM;
	ts->nFinger_support = HX_MAX_PT;
	
	calcDataSize(ts->nFinger_support);
	I("%s: calcDataSize complete\n", __func__);

	ts->pdata->abs_pressure_min	= 0;
	ts->pdata->abs_pressure_max	= 200;
	ts->pdata->abs_width_min	= 0;
	ts->pdata->abs_width_max	= 200;
	pdata->cable_config[0]		= 0x90;
	pdata->cable_config[1]		= 0x00;

	ts->suspended			= false;
#if defined(HX_USB_DETECT)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	ts->hall_block_touch_event         = 0;
	ts->hall_block_touch_time          = pdata->hall_block_touch_time;
#endif	
#endif
	ts->protocol_type = pdata->protocol_type;
	I("%s: Use Protocol Type %c\n", __func__,
	  ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n",
		  __func__, ts->input_dev->name);
		err = -ENODEV;
		goto err_input_register_device_failed;
	}

#ifdef QCOM
	tamper_flag = get_tamper_sf();
#endif
	if (tamper_flag == 0) {
		ts->debug_log_level |= BIT(3);
		I("%s: Enable touch down/up debug log since not security-on device\n",
		  __func__);
		if (pdata->screenWidth > 0 && pdata->screenHeight > 0 &&
		    (pdata->abs_x_max - pdata->abs_x_min) > 0 &&
		    (pdata->abs_y_max - pdata->abs_y_min) > 0) {
			ts->widthFactor = (pdata->screenWidth << SHIFTBITS) / (pdata->abs_x_max - pdata->abs_x_min);
			ts->heightFactor = (pdata->screenHeight << SHIFTBITS) / (pdata->abs_y_max - pdata->abs_y_min);
			if (ts->widthFactor > 0 && ts->heightFactor > 0)
				ts->useScreenRes = 1;
			else {
				ts->heightFactor = 0;
				ts->widthFactor = 0;
				ts->useScreenRes = 0;
			}
		} else
			I("Enable finger debug with raw position mode!\n");
	}

#ifdef CONFIG_FB
	ts->himax_att_wq = create_singlethread_workqueue("HMX_ATT_reuqest");
	if (!ts->himax_att_wq) {
		E(" allocate HMX_att_wq failed\n");
		err = -ENOMEM;
		goto err_get_intr_bit_failed;
	}
	INIT_DELAYED_WORK(&ts->work_att, himax_fb_register);
	queue_delayed_work(ts->himax_att_wq, &ts->work_att, msecs_to_jiffies(15000));

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	ts->early_suspend.suspend = himax_ts_early_suspend;
	ts->early_suspend.resume = himax_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef HX_CHIP_STATUS_MONITOR
	ts->himax_chip_monitor_wq = create_singlethread_workqueue("himax_chip_monitor_wq");
	if (!ts->himax_chip_monitor_wq) {
		E(" %s: create workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_chip_monitor_wq_failed;
	}

	INIT_DELAYED_WORK(&ts->himax_chip_monitor, himax_chip_monitor_function);
	queue_delayed_work(ts->himax_chip_monitor_wq, &ts->himax_chip_monitor, HX_POLLING_TIMER * HZ);
#endif

#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	if (pdata->proximity_bytp_enable)
		wake_lock_init(&ts->ts_wake_lock, WAKE_LOCK_SUSPEND, HIMAX852xes_NAME);
#endif
#ifdef HX_SMART_WAKEUP
	ts->SMWP_enable = 0;
	wake_lock_init(&ts->ts_SMWP_wake_lock, WAKE_LOCK_SUSPEND, HIMAX852xes_NAME);
#endif

	wake_lock_init(&ts->ts_flash_wake_lock, WAKE_LOCK_SUSPEND, HIMAX852xes_NAME);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_sysfs_init();
#endif

#ifdef HX_ESD_WORKAROUND
	ESD_RESET_ACTIVATE = 0;
#endif
	HW_RESET_ACTIVATE = 0;

#if defined(HX_USB_DETECT)
	if (ts->cable_config)
		cable_detect_register_notifier(&himax_cable_status_handler);
#endif
#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (ts->cover_support)
		hallsensor_register_notifier(&hallsensor_status_handler);
#endif	
#endif

	err = himax_ts_register_interrupt(ts->client);
	if (err)
		goto err_register_interrupt_failed;

#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
	register_himax_touch_fw_update();
#endif

#ifdef CONFIG_SYNC_TOUCH_STATUS
	
	pr_info("Register to sensor_hub driver\n");
#endif
	if (mfg_flag)
		private_ts->update_feature = mfg_flag;

#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
	if (private_ts->update_feature) {
		if ((strcmp(mfg_mode, "normal")  == 0) ||
		    (strcmp(mfg_mode, "MFG_MODE_NORMAL") == 0) ||
		    (strcmp(mfg_mode, "factory2") == 0) ||
		    (strcmp(mfg_mode, "MFG_MODE_FACTORY2") == 0)) {
			I("[FW] %s mode, process firmware update check.\n", mfg_mode);
			kthread_run(updateFirmware, (void *)ts, "HMX_FW_UPDATE");
		} else
			I("[FW] skip firmware update since under %s mode.\n", mfg_mode);

	}
#endif
	mfg_flag = 0;

	return 0;

err_register_interrupt_failed:
	if (ts->irq_enabled) {
		himax_int_enable(ts->client->irq, 0);
		free_irq(ts->client->irq, ts);
	}
	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		destroy_workqueue(ts->himax_wq);
	}

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (ts->cover_support)
		hallsensor_unregister_notifier(&hallsensor_status_handler);
#endif	
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_sysfs_deinit();
#endif

	wake_lock_destroy(&ts->ts_flash_wake_lock);

#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->ts_SMWP_wake_lock);
#endif
#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	if (ts->pdata->proximity_bytp_enable)
		wake_lock_destroy(&ts->ts_wake_lock);
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
	destroy_workqueue(ts->himax_chip_monitor_wq);
err_create_chip_monitor_wq_failed:
#endif
#ifdef CONFIG_FB
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
	if (fb_unregister_client(&ts->fb_notif))
		I("Error occurred while unregistering fb_notifier.\n");
err_get_intr_bit_failed:
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	if (ts->protocol_type == PROTOCOL_TYPE_B)
		input_mt_destroy_slots(ts->input_dev);

err_input_register_device_failed:
#ifdef HX_TP_SYS_DIAG
err_setchannel_failed:
#endif
err_detect_failed:
err_FW_CRC_failed:
#ifdef HX_TP_SYS_FLASH_DUMP
	destroy_workqueue(ts->flash_wq);
err_create_flash_wq_failed:
#endif

err_off_mode:
err_ic_package_failed:

#ifndef CONFIG_OF
err_power_failed:
#endif

	himax_gpio_power_deconfig(ts->client, pdata);

#ifdef CONFIG_OF
	if (pdata->ts_pinctrl)
		himax_pinctrl_select(ts->dev, 0);
	himax_pinctrl_deinit(ts->dev);
#endif

err_dt_platform_data_fail:
	kfree(pdata);
err_alloc_dt_pdata_failed:
err_get_platform_data_fail:
	kfree(ts);

err_alloc_data_failed:
#ifdef HX_MTK_DMA
	if (gpDMABuf_va) {
		dma_free_coherent(&client->dev, 4096, gpDMABuf_va, (dma_addr_t)gpDMABuf_pa);
		gpDMABuf_va = NULL;
		gpDMABuf_pa = NULL;
	}
err_alloc_MTK_DMA_failed:
#endif
err_check_functionality_failed:
	return err;
}

static int himax852xes_remove(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);
	struct himax_i2c_platform_data *pdata = client->dev.platform_data;

	if (ts->irq_enabled) {
		himax_int_enable(ts->client->irq, 0);
		free_irq(ts->client->irq, ts);
	}
	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		destroy_workqueue(ts->himax_wq);
	}

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (ts->cover_support)
		hallsensor_unregister_notifier(&hallsensor_status_handler);
#endif	
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_sysfs_deinit();
#endif

	wake_lock_destroy(&ts->ts_flash_wake_lock);

#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->ts_SMWP_wake_lock);
#endif
#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	if (ts->pdata->proximity_bytp_enable)
		wake_lock_destroy(&ts->ts_wake_lock);
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
	destroy_workqueue(ts->himax_chip_monitor_wq);
#endif
#ifdef CONFIG_FB
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
	if (fb_unregister_client(&ts->fb_notif))
		I("Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	if (ts->protocol_type == PROTOCOL_TYPE_B)
		input_mt_destroy_slots(ts->input_dev);

#ifdef HX_TP_SYS_FLASH_DUMP
	destroy_workqueue(ts->flash_wq);
#endif

	himax_gpio_power_deconfig(ts->client, pdata);

	if (pdata->ts_pinctrl)
		himax_pinctrl_select(ts->dev, 0);
	himax_pinctrl_deinit(ts->dev);
	kfree(ts);

#ifdef HX_MTK_DMA
	if (gpDMABuf_va) {
		dma_free_coherent(&client->dev, 4096, gpDMABuf_va, (dma_addr_t)gpDMABuf_pa);
		gpDMABuf_va = NULL;
		gpDMABuf_pa = NULL;
	}
#endif

	return 0;

}

#ifdef CONFIG_SYNC_TOUCH_STATUS
static int himax852xes_setup_interrupt_mode(struct himax_ts_data *ts)
{
	uint8_t outData[5];
	uint8_t data[1] = {0x00};
	uint8_t register_command = 0x03;
	int ret = 0;
	outData[0] = 0x8C;
	outData[1] = 0x14;
	ret = i2c_himax_master_write(ts->client, &outData[0], 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);
	outData[0] = 0x8B;
	outData[1] = 0x00;
	outData[2] = register_command;
	ret = i2c_himax_master_write(ts->client, &outData[0], 3, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);
	ret = i2c_himax_read(ts->client, 0x5A, data, 1, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	outData[0] = 0x8C;
	outData[1] = 0x00;
	ret = i2c_himax_master_write(ts->client, &outData[0], 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);
	outData[0] = 0x8C;
	outData[1] = 0x14;
	ret = i2c_himax_master_write(ts->client, &outData[0], 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);
	outData[0] = 0x8B;
	outData[1] = 0x00;
	outData[2] = register_command;
	ret = i2c_himax_master_write(ts->client, &outData[0], 3, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);
	outData[0] = 0x40;
	outData[1] = data[0] & 0xFE;
	ret = i2c_himax_master_write(ts->client, &outData[0], 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);
	I("[himax]%s:CMD: FE(%x), %x\n", __func__, register_command, outData[1]);
	outData[0] = 0x8C;
	outData[1] = 0x00;
	ret = i2c_himax_master_write(ts->client, &outData[0], 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	udelay(100);

	return ret;
}
#endif

static int himax_ts_free_fingers(struct himax_ts_data *ts)
{
	unsigned char ii;

	mutex_lock(&(ts->ts_report_mutex));
	if (ts->protocol_type == PROTOCOL_TYPE_B) {
		for (ii = 0; ii < ts->nFinger_support; ii++) {
			input_mt_slot(ts->input_dev, ii);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		}
	} else
		input_mt_sync(ts->input_dev);

	input_sync(ts->input_dev);

	mutex_unlock(&(ts->ts_report_mutex));

	ts->pre_finger_mask = 0;

	return 0;
}

static void himax_post_resume_work_func(struct work_struct *dummy)
{
	struct himax_ts_data *ts = private_ts;

#ifdef CONFIG_SYNC_TOUCH_STATUS
	switch_sensor_hub(ts, 0);
#ifdef HX_RST_PIN_FUNC
	ts->chosen_config_process = LOAD_SHORT_CFG;
	himax_HW_reset(true, false);
	ts->chosen_config_process = LOAD_ALL_CFG;
#endif
#else
	
	i2c_himax_write_command(ts->client, 0x83, HIMAX_I2C_RETRY_TIMES);
	msleep(30);
	i2c_himax_write_command(ts->client, 0x81, HIMAX_I2C_RETRY_TIMES);
#endif

#ifdef HX_DOT_VIEW
#if defined(CONFIG_AK8789_HALLSENSOR) || defined(CONFIG_AK8789_HALLSENSOR_R2)
	if (ts->cover_support) {
		himax_set_cover_func(ts->cover_enable);
		I("[HL] cover_enable = %d.\n", ts->cover_enable);
	}
#endif
#endif

	atomic_set(&ts->suspend_mode, 0);
	himax_int_enable(ts->client->irq, 1);

#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	queue_delayed_work(ts->himax_chip_monitor_wq, &ts->himax_chip_monitor, HX_POLLING_TIMER * HZ); 
#endif

	ts->suspended = false;
	himax_ts_free_fingers(ts);
	I("%s: version: %02X,%02X-%02X-%02X\n", __func__,
	  ts->vendor_fw_ver_H, ts->vendor_fw_ver_L, ts->vendor_sensor_id, ts->vendor_config_ver);
}
static DECLARE_DELAYED_WORK(himax_post_resume_work, himax_post_resume_work_func);

static int himax852xes_suspend(struct device *dev)
{
	int ret;
	uint8_t buf[2] = {0};
#ifdef HX_CHIP_STATUS_MONITOR
	int t = 0;
#endif
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	if (ts->suspended) {
		I("%s: Already suspended. Skipped. \n", __func__);
		return 0;
	} else {
		ts->suspended = true;
		I("%s: enter \n", __func__);
	}

#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	if (ts->pdata->proximity_bytp_enable) {
		I("[Proximity],Proximity en=%d\r\n", g_proximity_en);
		if (g_proximity_en)
			I("[Proximity],Proximity %s\r\n", proximity_flag ? "NEAR" : "FAR");

		if ((g_proximity_en) && (proximity_flag)) {
			I("[Proximity],Proximity on,and Near won't enter deep sleep now.\n");
			atomic_set(&ts->suspend_mode, 1);
			ts->first_pressed = 0;
			ts->pre_finger_mask = 0;
			return 0;
		}
	}
#endif
#ifdef HX_TP_SYS_FLASH_DUMP
	if (getFlashDumpGoing()) {
		I("[himax] %s: Flash dump is going, reject suspend\n", __func__);
		return 0;
	}
#endif
#ifdef HX_TP_SYS_HITOUCH
	if (hitouch_is_connect) {
		I("[himax] %s: Hitouch connect, reject suspend\n", __func__);
		return 0;
	}
#endif

#ifdef HX_CHIP_STATUS_MONITOR
	if (HX_ON_HAND_SHAKING) { 
		for (t = 0; t < 100; t++) {
			if (HX_ON_HAND_SHAKING == 0) { 
				I("%s:HX_ON_HAND_SHAKING OK check %d times\n", __func__, t);
				break;
			} else
				msleep(1);
		}
		if (t == 100) {
			E("%s:HX_ON_HAND_SHAKING timeout reject suspend\n", __func__);
			return 0;
		}
	}
#endif

#ifdef HX_SMART_WAKEUP
	if (ts->SMWP_enable) {
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND = false;
		buf[0] = 0x8F;
		buf[1] = 0x20;
		ret = i2c_himax_master_write(ts->client, buf, 2, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
		}
		I("[himax] %s: SMART_WAKEUP enable, reject suspend\n", __func__);
		return 0;
	}
#endif

	himax_int_enable(ts->client->irq, 0);

#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
#endif

	
	buf[0] = HX_CMD_TSSOFF;
	ret = i2c_himax_master_write(ts->client, buf, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	msleep(40);

	buf[0] = HX_CMD_TSSLPIN;
	ret = i2c_himax_master_write(ts->client, buf, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0) {
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}

	if (!ts->use_irq) {
		ret = cancel_work_sync(&ts->work);
		if (ret)
			himax_int_enable(ts->client->irq, 1);
	}

#ifdef CONFIG_SYNC_TOUCH_STATUS
	
	ret = himax852xes_setup_interrupt_mode(ts);
	if (ret < 0) {
		E("[himax] %s: setup hub interrupt mod fail\n", __func__);
	}
	
	switch_sensor_hub(ts, 1);
#endif

	
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;
	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(0);

	return 0;
}

static int himax852xes_resume(struct device *dev)
{
#ifdef HX_SMART_WAKEUP
	int ret;
	uint8_t buf[2] = {0};
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	int t = 0;
#endif
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: version: %02X,%02X-%02X-%02X\n", __func__,
	  ts->vendor_fw_ver_H, ts->vendor_fw_ver_L, ts->vendor_sensor_id, ts->vendor_config_ver);
	if (atomic_read(&ts->in_flash))
		return 0;

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(1);
#ifdef HX_CHIP_STATUS_MONITOR
	if (HX_ON_HAND_SHAKING) { 
		for (t = 0; t < 100; t++) {
			if (HX_ON_HAND_SHAKING == 0) { 
				I("%s:HX_ON_HAND_SHAKING OK check %d times\n", __func__, t);
				break;
			} else
				msleep(1);
		}
		if (t == 100) {
			E("%s:HX_ON_HAND_SHAKING timeout reject resume\n", __func__);
			return 0;
		}
	}
#endif
#ifdef HX_SMART_WAKEUP
	if (ts->SMWP_enable) {
		
		i2c_himax_write_command(ts->client, 0x82, HIMAX_I2C_RETRY_TIMES);
		msleep(40);
		
		i2c_himax_write_command(ts->client, 0x80, HIMAX_I2C_RETRY_TIMES);
		buf[0] = 0x8F;
		buf[1] = 0x00;
		ret = i2c_himax_master_write(ts->client, buf, 2, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0) {
			E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
		}
		msleep(50);
	}
#endif
	schedule_delayed_work(&himax_post_resume_work, msecs_to_jiffies(0));

	return 0;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
		container_of(self, struct himax_ts_data, fb_notif);

	I(" %s\n", __func__);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	if (evdata && evdata->data && event == FB_EVENT_PREBLANK && ts && ts->client)
#else
	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK && ts && ts->client)
#endif
	{
		blank = evdata->data;
		switch (*blank) {
		case FB_BLANK_UNBLANK:
			himax852xes_resume(&ts->client->dev);
			break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax852xes_suspend(&ts->client->dev);
			break;
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h)
{
	struct himax_ts_data *ts;
	ts = container_of(h, struct himax_ts_data, early_suspend);
	himax852xes_suspend(ts->client, PMSG_SUSPEND);
}

static void himax_ts_late_resume(struct early_suspend *h)
{
	struct himax_ts_data *ts;
	ts = container_of(h, struct himax_ts_data, early_suspend);
	himax852xes_resume(ts->client);

}
#endif

static const struct dev_pm_ops himax852xes_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend	= himax852xes_suspend,
	.resume		= himax852xes_resume,
#else
	.suspend	= himax852xes_suspend,
#endif
};


static const struct i2c_device_id himax852xes_ts_id[] = {
	{HIMAX852xes_NAME, 0 },
	{}
};

#ifdef CONFIG_OF
static struct of_device_id himax_match_table[] = {
	{.compatible = "himax,852xes" },
	{},
};
#else
#define himax_match_table NULL
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	scnprintf(info->type, PAGE_SIZE, "%s", TPD_DEVICE);
	return 0;
}
#endif

static struct i2c_driver himax852xes_driver = {
	.id_table = himax852xes_ts_id,
	.probe = himax852xes_probe,
	.remove = himax852xes_remove,
#if defined(MTK) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
	.detect = tpd_detect,
#endif
	.driver = {
		.name = HIMAX852xes_NAME,
		.owner = THIS_MODULE,
		.of_match_table = himax_match_table,
#ifdef CONFIG_PM
		.pm = &himax852xes_pm_ops,
#endif
	},
};

static void __init himax852xes_init_async(void *unused, async_cookie_t cookie)
{
	I("%s: Enter\n", __func__);
	i2c_add_driver(&himax852xes_driver);
}

static int __init himax852xes_init(void)
{
	I("Himax 852xES touch panel driver init\n");
	async_schedule(himax852xes_init_async, NULL);
	return 0;
}

static void __exit himax852xes_exit(void)
{
	i2c_del_driver(&himax852xes_driver);
}
#ifdef QCT
module_init(himax852xes_init);
#endif
#ifdef MTK
device_initcall(himax852xes_init);
#endif

module_exit(himax852xes_exit);

#ifdef MTK
static int __init get_tamper_flag(char *str)
{
	int ret = kstrtouint(str, 0, &tamper_flag);
	pr_info(" %d: %d from %s",
		ret, tamper_flag, str);
	return ret;
}
early_param("td.sf", get_tamper_flag);
#endif

static int __init mfg_update_enable_flag(char *str)
{
	int ret = 0;
	mfg_flag = 1;
	pr_info("\n[TP] MFG_BUILD flag enable: %d from %s\n", mfg_flag, str);
	return ret;
}
early_param("androidboot.mfg.tpupdate", mfg_update_enable_flag);

MODULE_DESCRIPTION("Himax852xes driver");
MODULE_LICENSE("GPL");
