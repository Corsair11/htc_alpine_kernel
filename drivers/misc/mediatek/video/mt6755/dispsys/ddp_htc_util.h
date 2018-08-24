#ifndef DDP_HTC_UTIL_H
#define DDP_HTC_UTIL_H

#include <ddp_gamma.h>
#include <disp_lcm.h>
#include "lm36274_i2c.h"

#define DCS_MAX_CNT 64
#define DSI_DEBUG_BUF 1024
#define DSI_MIN_COUNT 9

#define RGB_MIN_COUNT 9
#define RGB_CALI_BASE 0xFF
#define RGB_CHECK(x) ((x > 0) && (x <= 0xFF))
#define RGB_CALI(gamma, gain) ((unsigned int)(gamma * gain / RGB_CALI_BASE))
#define BKLT_CALI_BASE 10000
#define BKLT_CHECK(x) ((x > 0) && (x <= 20000))
#define BKLT_CALI(brightness, gain) ((unsigned int)(brightness * gain / BKLT_CALI_BASE))
#define DISP_CALI_NVRAM_FILE_NAME "/data/nvram/APCFG/APRDCL/HTC_DISP_CALI"

enum {
	RGB_CALI_ENABLE_INDEX = 0,
	RGB_CALI_READ_INDEX,
	BKLT_CALI_ENABLE_INDEX,
	BKLT_CALI_READ_INDEX,
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned int count;
	unsigned int para_list[DCS_MAX_CNT];
};

struct attr_status {
       char *title;
       u32 req_value;
       u32 cur_value;
       u32 def_value;
};

struct disp_cali_gain {
	unsigned char r;
	unsigned char g;
	unsigned char b;
	int bklt;
};

struct disp_cali_gamma {
	unsigned int lut_r[DISP_GAMMA_LUT_SIZE];
	unsigned int lut_g[DISP_GAMMA_LUT_SIZE];
	unsigned int lut_b[DISP_GAMMA_LUT_SIZE];
};

void htc_debugfs_init(void);
void htc_display_sysfs_init(void);
void htc_update_rgb_cali_data(DISP_GAMMA_LUT_T *gamma_lut);
unsigned int htc_update_bklt_cali_data(unsigned int last_brightness);

#endif
