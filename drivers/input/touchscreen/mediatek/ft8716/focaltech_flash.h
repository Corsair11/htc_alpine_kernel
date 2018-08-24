#ifndef __LINUX_FT_FLASH_H__
#define __LINUX_FT_FLASH_H__

#include "focaltech_core.h"

#ifdef CONFIG_TOUCHSCREEN_TOUCH_FW_UPDATE
#include <linux/firmware.h>
#include <linux/input/touch_fw_update.h>
#include <linux/async.h>
#include <linux/wakelock.h>
#endif


int fts_ctpm_fw_upgrade_with_sys_fs(struct i2c_client *client, unsigned char *pbt_buf, int fwsize);
int fts_update_fw_chip_id(struct fts_ts_data *data);

int fts_update_fw_vendor_id(struct fts_ts_data *data);

int fts_update_fw_ver(struct fts_ts_data *data);



#endif

