#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <mt-plat/htc_battery.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/htc_devices_dtb.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#if 0 
#include <linux/alarmtimer.h>
#endif 
#include <linux/delay.h>
#include <mt-plat/charging.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <cust_charging.h>
#include <mt-plat/mt_boot_common.h>

static struct htc_battery_info htc_batt_info;
static struct htc_battery_timer htc_batt_timer;

#define HTC_BATT_NAME "htc_battery"
#ifdef CONFIG_HTC_BATT_PCN0021
#define HTC_STATISTICS "htc_batt_stats_1.1"
#endif 

static int g_pwrsrc_dis_reason;

#define HTC_BATT_CHG_DIS_BIT_EOC	(1)
#define HTC_BATT_CHG_DIS_BIT_ID		(1<<1)
#define HTC_BATT_CHG_DIS_BIT_TMP	(1<<2)
#define HTC_BATT_CHG_DIS_BIT_OVP	(1<<3)
#define HTC_BATT_CHG_DIS_BIT_TMR	(1<<4)
#define HTC_BATT_CHG_DIS_BIT_MFG	(1<<5)
#define HTC_BATT_CHG_DIS_BIT_USR_TMR	(1<<6)
#define HTC_BATT_CHG_DIS_BIT_STOP_SWOLLEN	(1<<7)
#define HTC_BATT_CHG_DIS_BIT_USB_OVERHEAT	(1<<8)
#define HTC_BATT_CHG_DIS_BIT_FTM	(1<<9)
static int g_chg_dis_reason;
int g_chg_limit_reason;

#ifdef CONFIG_HTC_BATT_PCN0011
int suspend_highfreq_check_reason;
#endif 

#ifdef CONFIG_HTC_BATT_PCN0006
bool g_test_power_monitor;
bool g_flag_keep_charge_on;
bool g_flag_force_ac_chg;
bool g_flag_pa_fake_batt_temp;
bool g_flag_disable_safety_timer;
bool g_flag_disable_temp_protection;
bool g_flag_enable_batt_debug_log;
bool g_flag_ats_limit_chg;
#endif 

static int g_ftm_charger_control_flag = -1;

static int g_latest_chg_src = POWER_SUPPLY_TYPE_UNKNOWN;

#ifdef CONFIG_HTC_BATT_PCN0004
static unsigned int g_charger_ctrl_stat;
#endif 
#ifdef CONFIG_HTC_BATT_PCN0014
static bool g_htc_battery_probe_done = false;
#endif 
#ifdef CONFIG_HTC_BATT_PCN0017
#define CHK_UNKNOWN_CHG_REDETECT_PERIOD_MS	3000
#define MAX_REDETECT_TIME 3
static bool g_is_unknown_charger = false;
static int g_redetect_times = 0;
#endif 

static int g_batt_full_eoc_stop;

#ifdef CONFIG_HTC_BATT_PCN0001
static bool gs_update_PSY = false;
#endif 

#ifdef CONFIG_HTC_BATT_PCN0008
unsigned int g_total_level_raw;
unsigned int g_overheat_55_sec;
unsigned int g_batt_first_use_time;
unsigned int g_batt_cycle_checksum;
#endif 


#ifdef CONFIG_HTC_BATT_PCN0022
static bool g_usb_overheat = false;
static int g_usb_overheat_check_count = 0;
static int g_usb_temp = 0;
#endif 

static int gs_prev_charging_enabled = 0;

#ifdef CONFIG_HTC_BATT_PCN0002
static bool g_is_consistent_level_ready = false;
#endif 

#ifdef CONFIG_HTC_BATT_PCN0021
bool g_htc_stats_charging = false;
struct htc_statistics_category g_htc_stats_category_all;
struct htc_statistics_category g_htc_stats_category_full_low;
struct htc_charging_statistics g_htc_stats_data;
const int HTC_STATS_SAMPLE_NUMBER = 5;

enum {
    HTC_STATS_CATEGORY_ALL = 0,
    HTC_STATS_CATEGORY_FULL_LOW,
};
#endif 

static int g_thermal_batt_temp = 0;
extern CHR_CURRENT_ENUM g_temp_input_CC_value;

#define HTC_BATT_CHG_BI_BIT_CHGR                             (1)
#define HTC_BATT_CHG_BI_BIT_AGING                            (1<<1)
static int g_BI_data_ready = 0;
static struct timeval gs_batt_chgr_start_time = { 0, 0 };
static int g_batt_chgr_iusb = 0;
static int g_batt_chgr_ibat = 0;
static int g_batt_chgr_start_temp = 0;
static int g_batt_chgr_end_temp = 0;
static int g_batt_chgr_start_level = 0;
static int g_batt_chgr_end_level = 0;
static int g_batt_chgr_start_batvol = 0;
static int g_batt_chgr_end_batvol = 0;

static int g_batt_aging_bat_vol = 0;
static int g_batt_aging_level = 0;
static unsigned int g_pre_total_level_raw = 0;

#ifdef CONFIG_HTC_BATT_PCN0006
#define BATT_DEBUG(x...) do { \
	if (g_flag_enable_batt_debug_log) \
		printk(KERN_INFO"[BATT] " x); \
} while (0)
#else
#define BATT_DEBUG(x...) do { \
	printk(KERN_DEBUG"[BATT] " x); \
} while (0)
#endif 

#ifdef CONFIG_HTC_BATT_PCN0001
bool g_charging_full_to_close_charger = false;
#endif 
bool g_htc_battery_eoc_ever_reached = false;

struct dec_level_by_current_ma {
	int threshold_ma;
	int dec_level;
};

static struct dec_level_by_current_ma g_dec_level_curr_table[] = {
							{900, 2},
							{600, 4},
							{0, 6},
};

static char* htc_chr_type_data_str[] = {
        "NONE", 
        "UNKNOWN", 
        "UNKNOWN_TYPE", 
        "USB", 
        "USB_CDP", 
        "AC(USB_DCP)", 
        "USB_HVDCP", 
        "USB_HVDCP_3", 
        "PD_5V", 
        "PD_9V", 
        "PD_12V", 
        "USB_TYPE_C" 
};

static const int g_DEC_LEVEL_CURR_TABLE_SIZE = sizeof(g_dec_level_curr_table) / sizeof (g_dec_level_curr_table[0]);

#define BOUNDING_RECHARGE_NORMAL		5;
#define BOUNDING_RECHARGE_ATS		20;
static int is_bounding_fully_charged_level(void)
{
	static int s_pingpong = 1;
	int is_batt_chg_off_by_bounding = 0;
	int upperbd = htc_batt_info.rep.full_level;
	int current_level = htc_batt_info.rep.level;
	int lowerbd = upperbd;

	if (g_flag_ats_limit_chg) {
		lowerbd = upperbd - BOUNDING_RECHARGE_ATS;	
	} else {
		lowerbd = upperbd - BOUNDING_RECHARGE_NORMAL;	
	}

	if (0 < htc_batt_info.rep.full_level &&
			htc_batt_info.rep.full_level < 100) {

		if (lowerbd < 0)
			lowerbd = 0;

		if (s_pingpong == 1 && upperbd <= current_level) {
			BATT_LOG("%s: lowerbd=%d, upperbd=%d, current=%d,"
					" pingpong:1->0 turn off\n", __func__, lowerbd, upperbd, current_level);
			is_batt_chg_off_by_bounding = 1;
			s_pingpong = 0;
		} else if (s_pingpong == 0 && lowerbd < current_level) {
			BATT_LOG("%s: lowerbd=%d, upperbd=%d, current=%d,"
					" toward 0, turn off\n", __func__, lowerbd, upperbd, current_level);
			is_batt_chg_off_by_bounding = 1;
		} else if (s_pingpong == 0 && current_level <= lowerbd) {
			BATT_LOG("%s: lowerbd=%d, upperbd=%d, current=%d,"
					" pingpong:0->1 turn on\n", __func__, lowerbd, upperbd, current_level);
			s_pingpong = 1;
		} else {
			BATT_LOG("%s: lowerbd=%d, upperbd=%d, current=%d,"
					" toward %d, turn on\n", __func__, lowerbd, upperbd, current_level, s_pingpong);
		}
	}

	return is_batt_chg_off_by_bounding;
}

static void batt_set_check_timer(u32 seconds)
{
	BATT_DEBUG("%s(%u sec)\n", __func__, seconds);
	mod_timer(&htc_batt_timer.batt_timer,
			jiffies + msecs_to_jiffies(seconds * 1000));
}

#ifdef CONFIG_HTC_BATT_PCN0005
#define CHG_ONE_PERCENT_LIMIT_PERIOD_MS	(1000 * 60)
static void batt_check_overload(unsigned long time_since_last_update_ms)
{
	static unsigned int overload_count = 0;
	static unsigned long time_accumulation = 0;
	static unsigned int s_prev_level_raw = 0;

	BATT_LOG("Chk overload by CS=%d V=%d I=%d count=%d overload=%d is_full=%d,level=%d,level_raw=%d,deltaT=%lu,totalT=%lu\n",
			htc_batt_info.rep.charging_source, htc_batt_info.rep.batt_vol,
			BMT_status.ICharging, overload_count, htc_batt_info.rep.overload,
			htc_batt_info.rep.is_full,htc_batt_info.rep.level,htc_batt_info.rep.level_raw,
			time_since_last_update_ms,time_accumulation);
	if ((htc_batt_info.rep.charging_source > 0) &&
			(!htc_batt_info.rep.is_full) &&
			(BMT_status.ICharging > htc_batt_info.overload_curr_thr_ma)) {
		time_accumulation += time_since_last_update_ms;
		if (time_accumulation >= CHG_ONE_PERCENT_LIMIT_PERIOD_MS) {
			if (overload_count++ < 3) {
				htc_batt_info.rep.overload = 0;
			} else {
				htc_batt_info.rep.overload = 1;
			}
			time_accumulation = 0;
		}
	} else {
		if ((htc_batt_info.rep.charging_source > 0) && (htc_batt_info.rep.overload == 1)) {
			if (htc_batt_info.rep.level_raw > s_prev_level_raw) {
				overload_count = 0;
				time_accumulation = 0;
				htc_batt_info.rep.overload = 0;
			}
		} else { 
			overload_count = 0;
			time_accumulation = 0;
			htc_batt_info.rep.overload = 0;
		}
	}
	s_prev_level_raw = htc_batt_info.rep.level_raw;
}
#endif 

#if defined(CONFIG_HTC_BATT_PCN0002)||defined(CONFIG_HTC_BATT_PCN0008)
extern int get_partition_num_by_name(char *name);
int emmc_misc_write(int val, int offset)
{
	char filename[32] = "";
	int w_val = val;
	struct file *filp = NULL;
	ssize_t nread;
	int pnum = get_partition_num_by_name("para");

	if (pnum < 0) {
		pr_warn("unknown partition number for para partition: %d\n", pnum);
		return 0;
	}
	sprintf(filename, "/dev/block/mmcblk0p%d", pnum);

	filp = filp_open(filename, O_RDWR, 0);
	if (IS_ERR(filp)) {
		pr_info("unable to open file: %s\n", filename);
		return PTR_ERR(filp);
	}

	filp->f_pos = 147456 + offset; 
	nread = kernel_write(filp, (char *)&w_val, sizeof(int), filp->f_pos);
	pr_info("%X (%ld)\n", w_val, nread);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

	return 1;
}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0002
static void change_level_by_consistent_and_store_into_emmc(bool force)
{
	static unsigned int s_store_soc = 0;
	struct timespec xtime = CURRENT_TIME;
	unsigned long currtime_s = (xtime.tv_sec * MSEC_PER_SEC + xtime.tv_nsec / NSEC_PER_MSEC)/MSEC_PER_SEC;

	if(!g_is_consistent_level_ready){
		BATT_EMBEDDED("%s: Battery data not ready, don't save consistent data.\n", __func__);
		return;
	}

	if (force || (s_store_soc != htc_batt_info.rep.level)) {
		s_store_soc = htc_batt_info.rep.level;
		emmc_misc_write(STORE_MAGIC_NUM, STORE_MAGIC_OFFSET);
		emmc_misc_write(htc_batt_info.prev.level, STORE_SOC_OFFSET);
		emmc_misc_write(currtime_s, STORE_CURRTIME_OFFSET);
		emmc_misc_write(htc_batt_info.rep.batt_temp, STORE_TEMP_OFFSET);

		BATT_EMBEDDED("write battery data: level:%d, temp:%d, time:%lu",
			htc_batt_info.rep.level,htc_batt_info.rep.batt_temp,currtime_s);
	}
}

int batt_check_consistent(void)
{
	struct timespec xtime = CURRENT_TIME;
	unsigned long currtime_s = (xtime.tv_sec * MSEC_PER_SEC + xtime.tv_nsec / NSEC_PER_MSEC)/MSEC_PER_SEC;

	
	if (htc_batt_info.store.batt_stored_magic_num == STORE_MAGIC_NUM
		&& htc_batt_info.rep.batt_temp > 20
                && htc_batt_info.store.batt_stored_temperature > 20
		&& ((htc_batt_info.rep.level_raw > htc_batt_info.store.batt_stored_soc) ||
		(htc_batt_info.store.batt_stored_soc > (htc_batt_info.rep.level_raw + 1)))) {
        
		
		htc_batt_info.store.consistent_flag = true;
	}

	if (abs(htc_batt_info.store.batt_stored_soc - htc_batt_info.rep.level_raw) > 30) {
		BATT_EMBEDDED("%s: level diff over threashold(30), not consistent.\n", __func__);
		htc_batt_info.store.consistent_flag = false;
	}

	BATT_EMBEDDED("%s: magic_num=0x%X, level_raw=%d, store_soc=%d, current_time:%lu, store_time=%lu,"
				" batt_temp=%d, store_temp=%d, consistent_flag=%d", __func__,
				htc_batt_info.store.batt_stored_magic_num, htc_batt_info.rep.level_raw,
				htc_batt_info.store.batt_stored_soc,currtime_s,
				htc_batt_info.store.batt_stored_update_time,htc_batt_info.rep.batt_temp,
				htc_batt_info.store.batt_stored_temperature,
				htc_batt_info.store.consistent_flag);

	return htc_batt_info.store.consistent_flag;
}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0001
int htc_battery_level_adjust(void)
{
	return htc_batt_info.prev.level;
}

static void batt_check_critical_low_level(int *dec_level, int batt_current)
{
	int	i;

	for(i = 0; i < g_DEC_LEVEL_CURR_TABLE_SIZE; i++) {
		if (batt_current > g_dec_level_curr_table[i].threshold_ma) {
			*dec_level = g_dec_level_curr_table[i].dec_level;

			pr_debug("%s: i=%d, dec_level=%d, threshold_ma=%d\n",
				__func__, i, *dec_level, g_dec_level_curr_table[i].threshold_ma);
			break;
		}
	}
}

static void adjust_store_level(int *store_level, int drop_raw, int drop_ui, int prev) {
	int store = *store_level;
	
	store += drop_raw - drop_ui;
	if (store >= 0)
		htc_batt_info.rep.level = prev - drop_ui;
	else {
		htc_batt_info.rep.level = prev;
		store += drop_ui;
	}
	*store_level = store;
}

#define DISCHG_UPDATE_PERIOD_MS			(1000 * 60)
#define ONE_PERCENT_LIMIT_PERIOD_MS		(1000 * (60 + 10))
#define FIVE_PERCENT_LIMIT_PERIOD_MS	(1000 * (300 + 10))
#define ONE_MINUTES_MS					(1000 * (60 + 10))
#define FOURTY_MINUTES_MS				(1000 * (2400 + 10))
#define SIXTY_MINUTES_MS				(1000 * (3600 + 10))
#define CHG_ONE_PERCENT_LIMIT_PERIOD_MS	(1000 * 60)
#define QUICK_CHG_ONE_PERCENT_LIMIT_PERIOD_MS	(1000 * 30)
#define DEMO_GAP_WA						6
static void batt_level_adjust(unsigned long time_since_last_update_ms)
{
	static int s_first = 1;
	static int s_critical_low_enter = 0;
	static int s_store_level = 0;
	static int s_pre_five_digit = 0;
	static int s_five_digit = 0;
	static bool s_stored_level_flag = false;
	static bool s_allow_drop_one_percent_flag = false;
	int dec_level = 0;
	int dropping_level;
	int drop_raw_level;
	int allow_suspend_drop_level = 0;
	static unsigned long time_accumulated_level_change = 0;

	if (s_first) {
        gs_update_PSY = true;
		
#ifdef CONFIG_HTC_BATT_PCN0002
		if (batt_check_consistent())
			htc_batt_info.rep.level = htc_batt_info.store.batt_stored_soc;
		else
#endif 
			htc_batt_info.rep.level = htc_batt_info.rep.level_raw;

		htc_batt_info.prev.level = htc_batt_info.rep.level;
		htc_batt_info.prev.level_raw= htc_batt_info.rep.level_raw;
		s_pre_five_digit = htc_batt_info.rep.level / 10;
		htc_batt_info.prev.charging_source = htc_batt_info.rep.charging_source;
		BATT_LOG("pre_level=%d,pre_raw_level=%d,pre_five_digit=%d,pre_chg_src=%d\n",
			htc_batt_info.prev.level,htc_batt_info.prev.level_raw,s_pre_five_digit,
			htc_batt_info.prev.charging_source);
	}
	drop_raw_level = htc_batt_info.prev.level_raw - htc_batt_info.rep.level_raw;
	time_accumulated_level_change += time_since_last_update_ms;

	if ((htc_batt_info.prev.charging_source > 0) &&
		htc_batt_info.rep.charging_source == 0 && htc_batt_info.prev.level == 100) {
		BATT_LOG("%s: Cable plug out when level 100, reset timer.\n",__func__);
		time_accumulated_level_change = 0;
		htc_batt_info.rep.level = htc_batt_info.prev.level;

		return;
	}

	if (((htc_batt_info.rep.charging_source == 0)
			&& (s_stored_level_flag == false)) ||
			htc_batt_info.rep.overload ) {
		s_store_level = htc_batt_info.prev.level - htc_batt_info.rep.level_raw;
		BATT_LOG("%s: Cable plug out, to store difference between"
			" UI & SOC. store_level:%d, prev_level:%d, raw_level:%d\n"
			,__func__, s_store_level, htc_batt_info.prev.level, htc_batt_info.rep.level_raw);
		s_stored_level_flag = true;
	} else if (htc_batt_info.rep.charging_source > 0)
		s_stored_level_flag = false;

	if ((!gs_prev_charging_enabled &&
			!((htc_batt_info.prev.charging_source == 0) &&
				htc_batt_info.rep.charging_source > 0)) || htc_batt_info.rep.overload) {
		if (time_accumulated_level_change < DISCHG_UPDATE_PERIOD_MS
				&& !s_first && (htc_batt_info.rep.batt_vol > htc_batt_info.critical_low_voltage_mv)) {
			
			BATT_DEBUG("%s: total_time since last batt level update = %lu ms.",
			__func__, time_accumulated_level_change);
			htc_batt_info.rep.level = htc_batt_info.prev.level;
			s_store_level += drop_raw_level;

#ifdef CONFIG_HTC_BATT_PCN0002
			g_is_consistent_level_ready = true;
			change_level_by_consistent_and_store_into_emmc(0);
#endif 
			return;
		}

		if (htc_batt_info.rep.batt_vol < htc_batt_info.critical_low_voltage_mv) {
			s_critical_low_enter = 1;
			
			if (htc_batt_info.decreased_batt_level_check)
				batt_check_critical_low_level(&dec_level,
					htc_batt_info.rep.batt_current);
			else
				dec_level = 6;

			htc_batt_info.rep.level =
					(htc_batt_info.prev.level > dec_level) ? (htc_batt_info.prev.level - dec_level) : 0;

			BATT_LOG("%s: battery level force decreses %d%% from %d%%"
					" (soc=%d)on critical low (%d mV)(%d uA)\n", __func__, dec_level, htc_batt_info.prev.level,
						htc_batt_info.rep.level, htc_batt_info.critical_low_voltage_mv,
						htc_batt_info.rep.batt_current);
		} else {
			
			
			if ((htc_batt_info.rep.level_raw < 30) ||
					(htc_batt_info.prev.level > (htc_batt_info.prev.level_raw + 10)))
				s_allow_drop_one_percent_flag = true;

			
			htc_batt_info.rep.level = htc_batt_info.prev.level;

			if (time_since_last_update_ms <= ONE_PERCENT_LIMIT_PERIOD_MS) {
				if (1 <= drop_raw_level) {
					int drop_ui = 0;
					if (htc_batt_info.rep.batt_temp > 0)
						drop_ui = (2 <= drop_raw_level) ? 2 : 1;
					else
						drop_ui = (6 <= drop_raw_level) ? 6 : drop_raw_level;
					adjust_store_level(&s_store_level, drop_raw_level, drop_ui, htc_batt_info.prev.level);
					BATT_LOG("%s: remap: normal soc drop = %d%% in %lu ms."
							" UI only allow -%d%%, store_level:%d, ui:%d%%\n",
							__func__, drop_raw_level, time_since_last_update_ms,
							drop_ui, s_store_level, htc_batt_info.rep.level);
				}
#ifdef CONFIG_HTC_BATT_PCN0011
			} else if ((suspend_highfreq_check_reason & SUSPEND_HIGHFREQ_CHECK_BIT_TALK) &&
				(time_since_last_update_ms <= FIVE_PERCENT_LIMIT_PERIOD_MS)) {
				if (5 < drop_raw_level) {
					adjust_store_level(&s_store_level, drop_raw_level, 5, htc_batt_info.prev.level);
				} else if (1 <= drop_raw_level && drop_raw_level <= 5) {
					adjust_store_level(&s_store_level, drop_raw_level, 1, htc_batt_info.prev.level);
				}
				BATT_LOG("%s: remap: phone soc drop = %d%% in %lu ms.\n"
						" UI only allow -1%% or -5%%, store_level:%d, ui:%d%%\n",
						__func__, drop_raw_level, time_since_last_update_ms,
						s_store_level, htc_batt_info.rep.level);
#endif 
			} else {
				if (1 <= drop_raw_level) {
					if ((ONE_MINUTES_MS < time_since_last_update_ms) &&
							(time_since_last_update_ms <= FOURTY_MINUTES_MS)) {
						allow_suspend_drop_level = 4;
					} else if ((FOURTY_MINUTES_MS < time_since_last_update_ms) &&
							(time_since_last_update_ms <= SIXTY_MINUTES_MS)) {
						allow_suspend_drop_level = 6;
					} else if (SIXTY_MINUTES_MS < time_since_last_update_ms) {
						allow_suspend_drop_level = 8;
					}
					
					if (allow_suspend_drop_level != 0) {
						if (allow_suspend_drop_level <= drop_raw_level) {
							adjust_store_level(&s_store_level, drop_raw_level, allow_suspend_drop_level, htc_batt_info.prev.level);
						} else {
							adjust_store_level(&s_store_level, drop_raw_level, drop_raw_level, htc_batt_info.prev.level);
						}
					}
					BATT_LOG("%s: remap: suspend soc drop: %d%% in %lu ms."
							" UI only allow -1%% to -8%%, store_level:%d, ui:%d%%, suspend drop:%d%%\n",
							__func__, drop_raw_level, time_since_last_update_ms,
							s_store_level, htc_batt_info.rep.level, allow_suspend_drop_level);
				}
			}

			if ((s_allow_drop_one_percent_flag == false)
					&& (drop_raw_level == 0)) {
				htc_batt_info.rep.level = htc_batt_info.prev.level;
				BATT_DEBUG("%s: remap: no soc drop and no additional 1%%,"
						" ui:%d%%\n", __func__, htc_batt_info.rep.level);
			} else if ((s_allow_drop_one_percent_flag == true)
					&& (drop_raw_level == 0)
					&& (s_store_level > 0)) {
				s_store_level--;
				htc_batt_info.rep.level = htc_batt_info.prev.level - 1;
				s_allow_drop_one_percent_flag = false;
				BATT_LOG("%s: remap: drop additional 1%%. store_level:%d,"
						" ui:%d%%\n", __func__, s_store_level
						, htc_batt_info.rep.level);
			} else if (drop_raw_level < 0) {
				if (s_critical_low_enter) {
					BATT_LOG("%s: remap: level increase because of"
							" exit critical_low!\n", __func__);
				}
				s_store_level += drop_raw_level;
				htc_batt_info.rep.level = htc_batt_info.prev.level;
				BATT_LOG("%s: remap: soc increased. store_level:%d,"
						" ui:%d%%\n", __func__, s_store_level, htc_batt_info.rep.level);
			}

			
			s_five_digit = htc_batt_info.rep.level / 5;
			if (htc_batt_info.rep.level != 100) {
				
				if ((s_pre_five_digit <= 18) && (s_pre_five_digit > s_five_digit)) {
					s_allow_drop_one_percent_flag = true;
					BATT_LOG("%s: remap: allow to drop additional 1%% at next"
							" level:%d%%.\n", __func__, htc_batt_info.rep.level - 1);
				}
			}
			s_pre_five_digit = s_five_digit;

			if (s_critical_low_enter) {
				s_critical_low_enter = 0;
				pr_warn("[BATT] exit critical_low without charge!\n");
			}

			if (htc_batt_info.rep.batt_temp < 0 &&
				drop_raw_level == 0 &&
				s_store_level >= 2) {
				dropping_level = htc_batt_info.prev.level - htc_batt_info.rep.level;
				if((dropping_level == 1) || (dropping_level == 0)) {
					s_store_level = s_store_level - (2 - dropping_level);
					htc_batt_info.rep.level = htc_batt_info.rep.level -
						(2 - dropping_level);
				}
				BATT_LOG("%s: remap: enter low temperature section, "
						"store_level:%d%%, dropping_level:%d%%, "
						"prev_level:%d%%, level:%d%%.\n", __func__
						, s_store_level, htc_batt_info.prev.level, dropping_level
						, htc_batt_info.rep.level);
			}
#if 0
			if (s_store_level >= 2 && htc_batt_info.prev.level <= 10) {
				dropping_level = htc_batt_info.prev.level - htc_batt_info.rep.level;
				if((dropping_level == 1) || (dropping_level == 0)) {
					s_store_level = s_store_level - (2 - dropping_level);
					htc_batt_info.rep.level = htc_batt_info.rep.level -
						(2 - dropping_level);
				}
				BATT_LOG("%s: remap: UI level <= 10%% "
						"and allow drop 2%% maximum, "
						"store_level:%d%%, dropping_level:%d%%, "
						"prev_level:%d%%, level:%d%%.\n", __func__
						, s_store_level, dropping_level, htc_batt_info.prev.level
						, htc_batt_info.rep.level);
			}
#endif
		}

#if 0 
		if ((htc_batt_info.rep.level == 0) && (htc_batt_info.prev.level > 2)) {
			htc_batt_info.rep.level = 1;
			BATT_LOG("%s: battery level forcely report %d%%"
					" since prev_level=%d%%\n", __func__,
					htc_batt_info.rep.level, htc_batt_info.prev.level);
		}
#endif
	} else {

		if (htc_batt_info.rep.is_full) {
			if (htc_batt_info.smooth_chg_full_delay_min
					&& htc_batt_info.prev.level < 100) {
				
				if (time_accumulated_level_change <
						(htc_batt_info.smooth_chg_full_delay_min
						* CHG_ONE_PERCENT_LIMIT_PERIOD_MS)) {
					htc_batt_info.rep.level = htc_batt_info.prev.level;
				} else {
					htc_batt_info.rep.level = htc_batt_info.prev.level + 1;
				}
			} else {
				htc_batt_info.rep.level = 100; 
			}
		} else {
			if (htc_batt_info.prev.level > htc_batt_info.rep.level) {
				
				if (!htc_batt_info.rep.overload) {
					BATT_LOG("%s: pre_level=%d, new_level=%d, "
						"level drop but overloading doesn't happen!\n",
					__func__ , htc_batt_info.prev.level, htc_batt_info.rep.level);
					htc_batt_info.rep.level = htc_batt_info.prev.level;
				}
			}
			else if (99 < htc_batt_info.rep.level && htc_batt_info.prev.level == 99)
				htc_batt_info.rep.level = 99; 
			else if (htc_batt_info.prev.level < htc_batt_info.rep.level) {
				if(time_accumulated_level_change >
						QUICK_CHG_ONE_PERCENT_LIMIT_PERIOD_MS) {
					if ((htc_batt_info.rep.level > (htc_batt_info.prev.level + 1))
							&& htc_batt_info.prev.level < 98)
						htc_batt_info.rep.level = htc_batt_info.prev.level + 2;
					else
						htc_batt_info.rep.level = htc_batt_info.prev.level + 1;
				} else
					htc_batt_info.rep.level = htc_batt_info.prev.level;

				if (htc_batt_info.rep.level > 100)
					htc_batt_info.rep.level = 100;
			} else {
				BATT_DEBUG("%s: pre_level=%d, new_level=%d, "
					"level would use raw level!\n", __func__,
					htc_batt_info.prev.level, htc_batt_info.rep.level);
			}
			
			if (0 < htc_batt_info.rep.full_level &&
					htc_batt_info.rep.full_level < 100) {
				if((htc_batt_info.rep.level >
						htc_batt_info.rep.full_level) &&
						(htc_batt_info.rep.level <
						(htc_batt_info.rep.full_level + DEMO_GAP_WA))) {
					BATT_LOG("%s: block current_level=%d at "
							"full_level_dis_batt_chg=%d\n",
							__func__, htc_batt_info.rep.level,
							htc_batt_info.rep.full_level);
					htc_batt_info.rep.level =
								htc_batt_info.rep.full_level;
				}
			}
		}

		s_critical_low_enter = 0;
		s_allow_drop_one_percent_flag = false;
	}

	
	s_store_level = htc_batt_info.rep.level - htc_batt_info.rep.level_raw;

	
	if (htc_batt_info.rep.level == 0 && htc_batt_info.rep.batt_vol > 3250 &&
		htc_batt_info.rep.batt_temp > 0) {
		BATT_LOG("Not reach shutdown voltage, vol:%d\n", htc_batt_info.rep.batt_vol);
			htc_batt_info.rep.level = 1;
			s_store_level = 1;
	}

	
	if ((int)htc_batt_info.rep.level < 0 ) {
		BATT_LOG("Adjust error level, level:%d\n", htc_batt_info.rep.level);
		htc_batt_info.rep.level = 1;
		s_store_level = 0;
	}

	if ((htc_batt_info.rep.batt_temp < 0) &&
		(htc_batt_info.rep.level_raw == 0)){
		BATT_LOG("Force report 0 for temp < 0 and raw = 0\n");
		htc_batt_info.rep.level = 0;
	}

	if (htc_batt_info.rep.batt_vol < 3000) {
		BATT_LOG("Force report 0 for vol < 3V (%d)\n", htc_batt_info.rep.batt_vol);
		htc_batt_info.rep.level = 0;
	}
#ifdef CONFIG_HTC_BATT_PCN0002
	g_is_consistent_level_ready = true;
	change_level_by_consistent_and_store_into_emmc(0);
#endif 

	if (htc_batt_info.rep.level != htc_batt_info.prev.level){
		time_accumulated_level_change = 0;
		gs_update_PSY = true;
	}
	htc_batt_info.prev.level = htc_batt_info.rep.level;
	htc_batt_info.prev.level_raw = htc_batt_info.rep.level_raw;

	s_first = 0;
}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0018
void htc_update_bad_cable(int bad_cable)
{
	chg_error_force_update = 1;
	if (bad_cable)
		htc_batt_info.htc_extension |= HTC_EXT_BAD_CABLE_USED;
	else
		htc_batt_info.htc_extension &= ~HTC_EXT_BAD_CABLE_USED;
}
#endif 
static void batt_update_info_from_charger(void)
{
	htc_batt_info.prev.batt_temp = htc_batt_info.rep.batt_temp;

	htc_batt_info.rep.batt_current = ((battery_meter_get_battery_current_sign())?(-1):1)*(battery_meter_get_battery_current()/10);

	htc_batt_info.rep.batt_vol = battery_meter_get_battery_voltage(KAL_TRUE);

	htc_batt_info.rep.batt_temp = battery_meter_get_battery_temperature()*10;

	if (htc_batt_info.icharger->is_battery_full_eoc_stop)
		htc_batt_info.icharger->is_battery_full_eoc_stop(
				&g_batt_full_eoc_stop);

#ifdef CONFIG_HTC_BATT_PCN0016
	htc_batt_info.rep.is_htcchg_ext_mode =
		get_property(htc_batt_info.batt_psy, POWER_SUPPLY_PROP_HTCCHG_EXT);
#endif 

}

static void batt_update_info_from_gauge(void)
{
	htc_batt_info.rep.level = htc_batt_info.rep.level_raw;
}

#ifdef CONFIG_HTC_BATT_PCN0008
static void calculate_batt_cycle_info(unsigned long time_since_last_update_ms)
{
	time_t t = g_batt_first_use_time;
	struct tm timeinfo;
	const unsigned long timestamp_commit = 1439009536;	
	struct timeval rtc_now;

    do_gettimeofday(&rtc_now);
	
	if ( htc_batt_info.prev.charging_source && ( htc_batt_info.rep.level_raw > htc_batt_info.prev.level_raw ) )
		g_total_level_raw = g_total_level_raw + ( htc_batt_info.rep.level_raw - htc_batt_info.prev.level_raw );
	
	if ( htc_batt_info.prev.batt_temp >= 550 )
		g_overheat_55_sec += time_since_last_update_ms/1000;
	
	if (((g_batt_first_use_time < timestamp_commit) || (g_batt_first_use_time > rtc_now.tv_sec))
									&& ( timestamp_commit < rtc_now.tv_sec )) {
		g_batt_first_use_time = rtc_now.tv_sec;
		emmc_misc_write(g_batt_first_use_time, HTC_BATT_FIRST_USE_TIME);
		BATT_LOG("%s: g_batt_first_use_time modify!\n", __func__);
	}
	
	g_batt_cycle_checksum = g_batt_first_use_time + g_total_level_raw + g_overheat_55_sec;

	t = g_batt_first_use_time;
	time_to_tm(t, 0, &timeinfo);
	BATT_DEBUG("%s: g_batt_first_use_time = %04ld-%02d-%02d %02d:%02d:%02d, g_overheat_55_sec = %u, g_total_level_raw = %u\n",
		__func__, timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
		timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
		g_overheat_55_sec, g_total_level_raw);
	if (g_total_level_raw % 50 == 0) {
		BATT_LOG("%s: save batt cycle data every 50%%\n", __func__);
		
		emmc_misc_write(g_total_level_raw, HTC_BATT_TOTAL_LEVELRAW);
		emmc_misc_write(g_overheat_55_sec, HTC_BATT_OVERHEAT_MSEC);
		emmc_misc_write(g_batt_cycle_checksum, HTC_BATT_CYCLE_CHECKSUM);
	}
}

static void read_batt_cycle_info(void)
{
	struct timeval rtc_now;
	unsigned int checksum_now;

	do_gettimeofday(&rtc_now);

	BATT_LOG("%s: (read from devtree) g_batt_first_use_time = %u, g_overheat_55_sec = %u, g_total_level_raw = %u, g_batt_cycle_checksum = %u\n",
                        __func__, g_batt_first_use_time, g_overheat_55_sec, g_total_level_raw, g_batt_cycle_checksum);

	checksum_now = g_batt_first_use_time + g_total_level_raw + g_overheat_55_sec;

        if ((checksum_now != g_batt_cycle_checksum) || (g_batt_cycle_checksum == 0)) {
                BATT_LOG("%s: Checksum error: reset battery cycle data.\n", __func__);
                g_batt_first_use_time = rtc_now.tv_sec;
                g_overheat_55_sec = 0;
                g_total_level_raw = 0;
                emmc_misc_write(g_batt_first_use_time, HTC_BATT_FIRST_USE_TIME);
                BATT_LOG("%s: g_batt_first_use_time = %u, g_overheat_55_sec = %u, g_total_level_raw = %u\n",
                        __func__, g_batt_first_use_time, g_overheat_55_sec, g_total_level_raw);
        }
}
#endif 

void update_htc_extension_state(void)
{
	if (((g_batt_full_eoc_stop != 0) &&
		(htc_batt_info.rep.level == 100) &&
		(htc_batt_info.rep.level_raw == 100)))
		htc_batt_info.htc_extension |= HTC_EXT_CHG_FULL_EOC_STOP;
	else
		htc_batt_info.htc_extension &= ~HTC_EXT_CHG_FULL_EOC_STOP;

#ifdef CONFIG_HTC_BATT_PCN0017
	if (g_is_unknown_charger)
		htc_batt_info.htc_extension |= HTC_EXT_UNKNOWN_USB_CHARGER;
	else
		htc_batt_info.htc_extension &= ~HTC_EXT_UNKNOWN_USB_CHARGER;
#endif 

#ifdef CONFIG_HTC_BATT_PCN0019
	if ((htc_batt_info.rep.charging_source == POWER_SUPPLY_TYPE_USB_HVDCP) ||
		(htc_batt_info.rep.charging_source == POWER_SUPPLY_TYPE_USB_HVDCP_3))
		htc_batt_info.htc_extension |= HTC_EXT_QUICK_CHARGER_USED;
	else
		htc_batt_info.htc_extension &= ~HTC_EXT_QUICK_CHARGER_USED;
#endif 

}

void update_htc_chg_src(void)
{
	int chg_src = 0;
	switch (htc_batt_info.rep.charging_source) {
		case CHARGER_UNKNOWN:
			chg_src = 0;
			break;
		case STANDARD_HOST:
		case CHARGING_HOST:
#ifdef CONFIG_HTC_BATT_PCN0017
			if (g_is_unknown_charger)
				chg_src = 7; 
			else
#endif 
				chg_src = 1; 
			break;
		case NONSTANDARD_CHARGER:
		case STANDARD_CHARGER:
		case APPLE_2_1A_CHARGER:
		case APPLE_1_0A_CHARGER:
		case APPLE_0_5A_CHARGER:
			chg_src = 2; 
			break;
		case WIRELESS_CHARGER:
			chg_src = 4; 
			break;
		default:
			break;
        }
	htc_batt_info.rep.chg_src = chg_src;
}

#ifdef CONFIG_HTC_BATT_PCN0013
#define IBAT_MAX_UA			2900000
#define IBAT_MAX_UA_LOW		2800000
#define IBAT_HVDCP_LIMITED	2000000
#define IBAT_HVDCP_3_LIMITED	2100000
#define IBAT_LIMITED_UA		300000
#define IBAT_HEALTH_TEMP_WARM_HIGH	450
#define IBAT_HEALTH_TEMP_WARM_LOW	430
#define IBAT_HEALTH_TEMP_COOL		100
int update_ibat_setting (void)
{
	static bool s_prev_health_warm = false;
	static bool s_is_over_ibat = false;
	static bool s_prev_vol_limit = false;
	static int s_prev_ibat_health = POWER_SUPPLY_HEALTH_GOOD;
	static int ibat_new = IBAT_MAX_UA;
	int ibat_max = IBAT_MAX_UA;
	int ibat_hvdcp_limited = IBAT_HVDCP_3_LIMITED;

	if (htc_batt_info.rep.batt_current < -3010000)
		s_is_over_ibat = true;

	if (s_is_over_ibat)
		ibat_max = IBAT_MAX_UA_LOW;
	else
		ibat_max = IBAT_MAX_UA;

	
	if (htc_batt_info.rep.batt_temp >= IBAT_HEALTH_TEMP_WARM_HIGH)
		s_prev_ibat_health = POWER_SUPPLY_HEALTH_WARM;
	else if (htc_batt_info.rep.batt_temp <= IBAT_HEALTH_TEMP_COOL)
		s_prev_ibat_health = POWER_SUPPLY_HEALTH_COOL;
	else {
		if ((s_prev_ibat_health = POWER_SUPPLY_HEALTH_WARM) &&
			(htc_batt_info.rep.batt_temp < IBAT_HEALTH_TEMP_WARM_LOW))
			s_prev_ibat_health = POWER_SUPPLY_HEALTH_GOOD;
		else if ((s_prev_ibat_health = IBAT_HEALTH_TEMP_COOL) &&
			(htc_batt_info.rep.batt_temp > IBAT_HEALTH_TEMP_COOL))
			s_prev_ibat_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	if ((s_prev_ibat_health == POWER_SUPPLY_HEALTH_GOOD)
#ifdef CONFIG_HTC_BATT_PCN0006
		|| g_flag_keep_charge_on
#endif 
		) {
		if ((htc_batt_info.rep.batt_vol < 4100) || !s_prev_vol_limit) {
			s_prev_vol_limit = false;
			ibat_new =  ibat_max;
		} else if ((htc_batt_info.rep.batt_vol > 4250) || s_prev_vol_limit) {
			s_prev_vol_limit = true;
			ibat_new =  2100000;
		}
	} else if (s_prev_ibat_health == POWER_SUPPLY_HEALTH_COOL) {
		if ((htc_batt_info.rep.batt_vol > 4250) || s_prev_vol_limit) {
			s_prev_vol_limit = true;
			ibat_new =  1500000;
		} else if ((htc_batt_info.rep.batt_vol < 4100) || !s_prev_vol_limit) {
			s_prev_vol_limit = false;
			ibat_new =  2100000;
		}
	} else if (s_prev_ibat_health == POWER_SUPPLY_HEALTH_WARM) {
		if (htc_batt_info.rep.batt_vol < 4100) {
			ibat_new =   ibat_max;
		}
	}

	
	if (g_chg_limit_reason)
		ibat_new = IBAT_LIMITED_UA;

	return ibat_new;
}
#endif 

int htc_batt_schedule_batt_info_update(void)
{
#ifdef CONFIG_HTC_BATT_PCN0014
        if (!g_htc_battery_probe_done)
                return 1;
#endif 
        if (!work_pending(&htc_batt_timer.batt_work)) {
                wake_lock(&htc_batt_timer.battery_lock);
                queue_work(htc_batt_timer.batt_wq, &htc_batt_timer.batt_work);
        }
        return 0;
}


#ifdef CONFIG_HTC_BATT_PCN0022
#define USB_OVERHEAT_CHECK_PERIOD_MS 30000
static void is_usb_overheat_worker(struct work_struct *work)
{
        int usb_pwr_temp;
        static int last_usb_pwr_temp = 0;
        if (g_latest_chg_src > POWER_SUPPLY_TYPE_UNKNOWN) {
                if(!g_usb_overheat){
                        usb_pwr_temp = pm8996_get_usb_temp();
                        BATT_LOG("USB CONN Temp = %d\n",usb_pwr_temp);
			g_usb_temp = usb_pwr_temp;
                        if(usb_pwr_temp > 650)
				g_usb_overheat = true;
                        if(g_usb_overheat_check_count == 0)
                                last_usb_pwr_temp = usb_pwr_temp;
                        else{
				if(((usb_pwr_temp - last_usb_pwr_temp) > 300) &&( g_usb_overheat_check_count <= 6))
					g_usb_overheat = true;
                        }
                }
                g_usb_overheat_check_count++;
                if(!g_usb_overheat)
                        schedule_delayed_work(&htc_batt_info.is_usb_overheat_work, msecs_to_jiffies(USB_OVERHEAT_CHECK_PERIOD_MS));
                else{
                        BATT_LOG("%s: USB overheat! cable_in_usb_temp:%d, usb_temp:%d, count = %d\n",
                                __func__, last_usb_pwr_temp, usb_pwr_temp, g_usb_overheat_check_count);
                        htc_batt_schedule_batt_info_update();
                }
        }else{
                g_usb_overheat = false;
                g_usb_overheat_check_count = 0;
                last_usb_pwr_temp = 0;
		g_usb_temp = 0;
        }
}
#endif 

#define VFLOAT_COMP_WARM		0x10	
#define VFLOAT_COMP_COOL		0x0		
#define VFLOAT_COMP_NORMAL	0x0		
#define CHG_UNKNOWN_CHG_PERIOD_MS	9000
#define THERMAL_BATT_TEMP_UPDATE_TIME_THRES	1800	
#define THERMAL_BATT_TEMP_UPDATE_TIME_MAX	3610    
#define BI_BATT_CHGE_UPDATE_TIME_THRES		1800	
#define BI_BATT_CHGE_CHECK_TIME_THRES		36000	
static void batt_worker(struct work_struct *work)
{
	static int s_first = 1;
	static int s_prev_pwrsrc_enabled = 1;
	int pwrsrc_enabled = s_prev_pwrsrc_enabled;
	int charging_enabled = gs_prev_charging_enabled;
	int src = 0;
#ifdef CONFIG_HTC_BATT_PCN0013
	int ibat = 0;
	int ibat_new = 0;
#endif 
	unsigned long time_since_last_update_ms;
	unsigned long cur_jiffies;
	
	char *chr_src[] = {"NONE", "USB", "CHARGING_HOST",
		"NONSTANDARD_CHARGER", "AC(USB_DCP)", "APPLE_2_1A_CHARGER",
		"APPLE_1_0A_CHARGER", "APPLE_0_5A_CHARGER", "WIRELESS_CHARGER"};
	static struct timeval s_thermal_batt_update_time = { 0, 0 };
	struct timeval rtc_now;
	static bool batt_chgr_start_flag = false;

	
	cur_jiffies = jiffies;
	time_since_last_update_ms = htc_batt_timer.total_time_ms +
		((cur_jiffies - htc_batt_timer.batt_system_jiffies) * MSEC_PER_SEC / HZ);
	BATT_DEBUG("%s: total_time since last batt update = %lu ms.\n",
				__func__, time_since_last_update_ms);
	htc_batt_timer.total_time_ms = 0; 
	htc_batt_timer.batt_system_jiffies = cur_jiffies;

	
	if (htc_batt_info.rep.batt_vol < htc_batt_info.critical_low_voltage_mv)
		htc_batt_timer.time_out = BATT_LOW_TIMER_UPDATE_TIME;
	else
		htc_batt_timer.time_out = BATT_TIMER_UPDATE_TIME;

	del_timer_sync(&htc_batt_timer.batt_timer);
	batt_set_check_timer(htc_batt_timer.time_out);

	
	htc_batt_info.prev.charging_source = htc_batt_info.rep.charging_source;
	htc_batt_info.rep.charging_source = g_latest_chg_src;

	
	batt_update_info_from_gauge();
	batt_update_info_from_charger();
	update_htc_chg_src();

#ifdef CONFIG_HTC_BATT_PCN0007
	
	if (htc_batt_info.rep.charging_source != htc_batt_info.prev.charging_source) {
		if ((htc_batt_info.rep.charging_source != STANDARD_CHARGER)
#ifdef CONFIG_HTC_BATT_PCN0006
			|| g_flag_keep_charge_on || g_flag_disable_safety_timer
#endif 
			) {
			htc_safety_timer_en(0);
		} else {
			htc_safety_timer_en(1);
		}
	}
#endif 


#ifdef CONFIG_HTC_BATT_PCN0008
	
	if (s_first)
		read_batt_cycle_info();
	calculate_batt_cycle_info(time_since_last_update_ms);
#endif 

#ifdef CONFIG_HTC_BATT_PCN0001
	
	batt_level_adjust(time_since_last_update_ms);
#endif 



#ifdef CONFIG_HTC_BATT_PCN0005
	
	batt_check_overload(time_since_last_update_ms);
#endif 

	
	update_htc_extension_state();

	if ((int)htc_batt_info.rep.charging_source > CHARGER_UNKNOWN) {

		if(!batt_chgr_start_flag){
			do_gettimeofday(&rtc_now);
			gs_batt_chgr_start_time = rtc_now;
			batt_chgr_start_flag = true;
		}

		
#ifdef CONFIG_HTC_BATT_PCN0017
		if ((g_latest_chg_src == NONSTANDARD_CHARGER) && (g_redetect_times < MAX_REDETECT_TIME) &&
			(!delayed_work_pending(&htc_batt_info.chk_unknown_chg_work))) {
			schedule_delayed_work(&htc_batt_info.chk_unknown_chg_work,
						msecs_to_jiffies(CHK_UNKNOWN_CHG_REDETECT_PERIOD_MS));
		}
#endif 

		if (g_ftm_charger_control_flag == FTM_STOP_CHARGER)
			g_pwrsrc_dis_reason |= HTC_BATT_PWRSRC_DIS_BIT_FTM;
		else
			g_pwrsrc_dis_reason &= ~HTC_BATT_PWRSRC_DIS_BIT_FTM;

		if (is_bounding_fully_charged_level()) {
			if (g_flag_ats_limit_chg)
				g_chg_dis_reason |= HTC_BATT_CHG_DIS_BIT_STOP_SWOLLEN;
			else
				g_pwrsrc_dis_reason |= HTC_BATT_PWRSRC_DIS_BIT_MFG;
		} else {
			if (g_flag_ats_limit_chg)
				g_chg_dis_reason &= ~HTC_BATT_CHG_DIS_BIT_STOP_SWOLLEN;
			else
				g_pwrsrc_dis_reason &= ~HTC_BATT_PWRSRC_DIS_BIT_MFG;
		}
#ifdef CONFIG_HTC_BATT_PCN0022
		if (g_usb_overheat){
			g_chg_dis_reason |= HTC_BATT_CHG_DIS_BIT_USB_OVERHEAT;
			g_pwrsrc_dis_reason |= HTC_BATT_PWRSRC_DIS_BIT_USB_OVERHEAT;
		}else{
			g_pwrsrc_dis_reason &= ~HTC_BATT_PWRSRC_DIS_BIT_USB_OVERHEAT;
			g_chg_dis_reason &= ~HTC_BATT_CHG_DIS_BIT_USB_OVERHEAT;
		}
#endif 

		
		if (g_chg_dis_reason)
			charging_enabled = 0;
		else
			charging_enabled = 1;

		
		if (g_pwrsrc_dis_reason)
			pwrsrc_enabled = 0;
		else
			pwrsrc_enabled = 1;

#ifdef CONFIG_HTC_BATT_PCN0013
		
		ibat = get_property(htc_batt_info.batt_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX);
		ibat_new = update_ibat_setting();

#ifdef CONFIG_HTC_BATT_PCN0016
		if (ibat != ibat_new && htc_batt_info.rep.is_htcchg_ext_mode == false) {
#else
		if (ibat != ibat_new) {
#endif 
			BATT_EMBEDDED("set ibat(%d)", ibat_new);
			set_batt_psy_property(POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, ibat_new);
		}
#endif 
		if (htc_batt_info.rep.full_level != 100) {
			BATT_EMBEDDED("set full level charging_enable(%d)", charging_enabled);
			htc_set_chg_en(charging_enabled);
		} else if (charging_enabled != gs_prev_charging_enabled) {
			BATT_EMBEDDED("set charging_enable(%d)", charging_enabled);
			htc_set_chg_en(charging_enabled);
		}

		if (htc_batt_info.rep.full_level != 100) {
			if (!g_flag_keep_charge_on) {
				BATT_EMBEDDED("set full level pwrsrc_enable(%d)", pwrsrc_enabled);
				htc_set_pwr_en(pwrsrc_enabled);
			} else {
				if (pwrsrc_enabled)
					htc_set_pwr_en(pwrsrc_enabled);
			}
		} else if (pwrsrc_enabled != s_prev_pwrsrc_enabled) {
			BATT_EMBEDDED("set pwrsrc_enable(%d)", pwrsrc_enabled);
			htc_set_pwr_en(pwrsrc_enabled);
		}
	} else {
		
		if (htc_batt_info.prev.charging_source != htc_batt_info.rep.charging_source || s_first) {
			g_BI_data_ready &= ~HTC_BATT_CHG_BI_BIT_CHGR;
			batt_chgr_start_flag = false;
			g_batt_chgr_start_temp = 0;
			g_batt_chgr_start_level = 0;
			g_batt_chgr_start_batvol = 0;
			charging_enabled = 0;
			pwrsrc_enabled = 0;
#ifdef CONFIG_HTC_BATT_PCN0022
			g_usb_overheat = false;
			g_usb_overheat_check_count = 0;
			g_usb_temp = 0;
#endif 
		}
	}

	gs_prev_charging_enabled = charging_enabled;
	s_prev_pwrsrc_enabled = pwrsrc_enabled;

#if 0 
	if (s_first == 1 && !strcmp(htc_get_bootmode(),"ftm")
			&& (of_get_property(of_chosen, "is_mfg_build", NULL))) {
		pr_info("%s: Under FTM mode, disable charger first.", __func__);
		
		htc_set_pwr_en(0);
	}
#endif
	s_first = 0;


	if (htc_batt_info.icharger) {
		
		
		htc_batt_info.vbus = htc_batt_info.icharger->get_vbus();
	}

	if (htc_batt_info.rep.over_vchg || g_pwrsrc_dis_reason || g_chg_dis_reason) {
		htc_batt_info.rep.chg_en = 0;
	} else {
		src = htc_batt_info.rep.charging_source;
		if (src == CHARGER_UNKNOWN) {
			htc_batt_info.rep.chg_en = 0;
		} else if (src == STANDARD_HOST || src == POWER_SUPPLY_TYPE_USB_CDP ||
					(g_ftm_charger_control_flag == FTM_SLOW_CHARGE)) {
			htc_batt_info.rep.chg_en = 1;
		} else {
			htc_batt_info.rep.chg_en = 2;
		}
	}

	
	BATT_EMBEDDED("ID=%d,"
		"level=%d,"
		"level_raw=%d,"
		"vol=%dmV,"
		"temp=%d,"
		"current=%dmA,"
		"chg_name=%s,"
		"chg_src=%d,"
		"chg_en=%d,"
		"health=%d,"
		"overload=%d,"
		"duration=%dmin,"
		"vbus=%dmV,"
		
		"chg_limit_reason=%d,"
		"chg_stop_reason=%d,"
#ifdef CONFIG_HTC_BATT_PCN0002
		"consistent=%d,"
#endif 
		"flag=0x%08X,"
		
		"htc_ext=0x%02X,"
#ifdef CONFIG_HTC_BATT_PCN0008
		"level_accu=%d,"
#endif 
#ifdef CONFIG_HTC_BATT_PCN0016
		"htcchg=%d,"
#endif 
#ifdef CONFIG_HTC_BATT_PCN0022
		"usb_temp=%d,"
		"usb_overheat=%d,"
#endif 
		"cc=%d,"
		"pon_reason=%s,"
		"poff_reason=%s"
		,
		htc_batt_info.rep.batt_id,
		htc_batt_info.rep.level,
		htc_batt_info.rep.level_raw,
		htc_batt_info.rep.batt_vol,
		htc_batt_info.rep.batt_temp,
		htc_batt_info.rep.batt_current,
		chr_src[htc_batt_info.rep.charging_source],
		htc_batt_info.rep.chg_src,
		htc_batt_info.rep.chg_en,
		htc_batt_info.rep.health,
		htc_batt_info.rep.overload,
		0,
		htc_batt_info.vbus,
		
		htc_batt_info.current_limit_reason,
		g_pwrsrc_dis_reason,
#ifdef CONFIG_HTC_BATT_PCN0002
		(htc_batt_info.store.consistent_flag ? htc_batt_info.store.batt_stored_soc : (-1)),
#endif 
		htc_batt_info.k_debug_flag,
		
		htc_batt_info.htc_extension,
#ifdef CONFIG_HTC_BATT_PCN0008
		g_total_level_raw,
#endif 
#ifdef CONFIG_HTC_BATT_PCN0016
		htc_batt_info.rep.is_htcchg_ext_mode,
#endif 
#ifdef CONFIG_HTC_BATT_PCN0022
		g_usb_temp,
		g_usb_overheat? 1 : 0,
#endif 
		battery_meter_get_car(),
		htc_get_bootreason(),
		htc_get_poffreason()
		);

#ifdef CONFIG_HTC_BATT_PCN0001
	if(gs_update_PSY){
		gs_update_PSY = false;
		bat_update_thread_wakeup();
	}
#endif 

	
	do_gettimeofday(&rtc_now);
	if (((rtc_now.tv_sec - s_thermal_batt_update_time.tv_sec) > THERMAL_BATT_TEMP_UPDATE_TIME_THRES)
			&& ((rtc_now.tv_sec - s_thermal_batt_update_time.tv_sec) < THERMAL_BATT_TEMP_UPDATE_TIME_MAX)) {
		pr_info("[BATT][THERMAL] Update period: %ld, batt_temp = %d.\n",
			(long)(rtc_now.tv_sec - s_thermal_batt_update_time.tv_sec), htc_batt_info.rep.batt_temp);
		g_thermal_batt_temp = htc_batt_info.rep.batt_temp;
	}
	s_thermal_batt_update_time = rtc_now;

	if((batt_chgr_start_flag) && (g_batt_chgr_start_batvol ==0)){
		g_batt_chgr_start_temp = htc_batt_info.rep.batt_temp;
		g_batt_chgr_start_level = htc_batt_info.rep.level_raw;
		g_batt_chgr_start_batvol = htc_batt_info.rep.batt_vol;
	}

        if ((g_batt_aging_bat_vol == 0) || (htc_batt_info.rep.level_raw < g_batt_aging_level)) {
                g_batt_aging_bat_vol = htc_batt_info.rep.batt_vol;
                g_batt_aging_level = htc_batt_info.rep.level_raw;
        }

	if((g_BI_data_ready & HTC_BATT_CHG_BI_BIT_CHGR) == 0){
		if(batt_chgr_start_flag){
			if((rtc_now.tv_sec - gs_batt_chgr_start_time.tv_sec) > BI_BATT_CHGE_CHECK_TIME_THRES) {
				gs_batt_chgr_start_time = rtc_now;
			}else if((rtc_now.tv_sec - gs_batt_chgr_start_time.tv_sec) > BI_BATT_CHGE_UPDATE_TIME_THRES){
				g_batt_chgr_end_temp = htc_batt_info.rep.batt_temp;
				g_batt_chgr_end_level = htc_batt_info.rep.level_raw;
				g_batt_chgr_end_batvol = htc_batt_info.rep.batt_vol;
				g_batt_chgr_iusb = g_temp_input_CC_value;
				g_batt_chgr_ibat = htc_batt_info.rep.batt_current;
				g_BI_data_ready |= HTC_BATT_CHG_BI_BIT_CHGR;
				BATT_EMBEDDED("Trigger batt_chgr event.");
			}
		}
	}

	g_pre_total_level_raw = g_total_level_raw;

	wake_unlock(&htc_batt_timer.battery_lock);
}

#ifdef CONFIG_HTC_BATT_PCN0001
#define CHG_FULL_CHECK_PERIOD_MS	10000
#define CLEAR_FULL_STATE_BY_LEVEL_THR 97
#define CONSECUTIVE_COUNT             3
static void chg_full_check_worker(struct work_struct *work)
{
	u32 vol = 0;
	u32 max_vbat = 0;
	s32 curr = BMT_status.ICharging;
	static int chg_full_count = 0;
	static bool b_recharging_cycle = false;

	if (g_latest_chg_src > POWER_SUPPLY_TYPE_UNKNOWN) {
		if (htc_batt_info.rep.is_full) {
			if (curr < 0 && abs(curr) <= 50) {
				chg_full_count++;
				if (chg_full_count >= CONSECUTIVE_COUNT + 2) {
					g_charging_full_to_close_charger = true;
					g_htc_battery_eoc_ever_reached = true;
					chg_full_count = 0;
				}
			} else {
				chg_full_count = CONSECUTIVE_COUNT;
				g_charging_full_to_close_charger = false;
			}
			BATT_EMBEDDED("[stopcharger_checking]BattCurr=%d,Count=%d\n",curr,chg_full_count);
			if (htc_batt_info.rep.level_raw < CLEAR_FULL_STATE_BY_LEVEL_THR) {
				vol = battery_meter_get_battery_voltage(KAL_TRUE);
				max_vbat = 4400; 
				if (vol < (max_vbat - 100)) {
					chg_full_count = 0;
					htc_batt_info.rep.is_full = false;
				}
				BATT_EMBEDDED("%s: vol:%d, max_vbat=%d, is_full=%d", __func__,
					vol, max_vbat, htc_batt_info.rep.is_full);
			}
		} else {
			vol = battery_meter_get_battery_voltage(KAL_TRUE);
			if ((vol >= htc_batt_info.batt_full_voltage_mv)
				&& (curr < 0) && (abs(curr) <= htc_batt_info.batt_full_current_ma)) {
				chg_full_count++;
				if (chg_full_count >= CONSECUTIVE_COUNT)
					htc_batt_info.rep.is_full = true;
			} else {
				chg_full_count = 0;
				htc_batt_info.rep.is_full = false;
			}
			BATT_EMBEDDED("[show100_checking]BattVol=%d,BattCurr=%d,Count=%d\n",vol,curr,chg_full_count);
		}

		if ((htc_batt_info.htc_extension & 0x8)) {
			b_recharging_cycle = true;
			wake_unlock(&htc_batt_info.charger_exist_lock);
		} else {
			if((g_batt_full_eoc_stop != 0) && (b_recharging_cycle)) {
				wake_unlock(&htc_batt_info.charger_exist_lock);
			} else {
				b_recharging_cycle = false;
				wake_lock(&htc_batt_info.charger_exist_lock);
			}
		}
	} else {
		chg_full_count = 0;
		htc_batt_info.rep.is_full = false;
		wake_unlock(&htc_batt_info.charger_exist_lock);
		g_htc_battery_eoc_ever_reached = false;
		return;
	}

	schedule_delayed_work(&htc_batt_info.chg_full_check_work,
							msecs_to_jiffies(CHG_FULL_CHECK_PERIOD_MS));

}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0017
static void chk_unknown_chg_worker(struct work_struct *work)
{
	BATT_LOG("%s: re-detect charger. src=%d, times=%d\n",
		__func__, g_latest_chg_src, g_redetect_times);

	if (g_latest_chg_src != NONSTANDARD_CHARGER) {
		g_redetect_times = 0;
		return;
	}

	if (g_redetect_times >= MAX_REDETECT_TIME) {
		if (g_latest_chg_src == NONSTANDARD_CHARGER) {
			
			g_is_unknown_charger = true;
			gs_update_PSY = true;
			chg_error_force_update = 1;
			htc_batt_schedule_batt_info_update();
		}
	} else {
		htc_redetect_charger();
		g_redetect_times++;
		schedule_delayed_work(&htc_batt_info.chk_unknown_chg_work,
							msecs_to_jiffies(CHK_UNKNOWN_CHG_REDETECT_PERIOD_MS));
	}
}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0021
bool htc_stats_is_ac(int type)
{
    return type == POWER_SUPPLY_TYPE_USB_DCP ||
           type == POWER_SUPPLY_TYPE_USB_ACA ||
           type == POWER_SUPPLY_TYPE_USB_CDP;
}

const char* htc_stats_classify(unsigned long sum, int sample_count)
{
    char* ret;
    if (sum < sample_count * 5 * 60 * 60L) {
        ret = "A(0~5hr)";
    } else if (sum < sample_count * 8 * 60 * 60L ) {
        ret = "B(5~8hr)";
    } else if (sum < sample_count * 12 * 60 * 60L) {
        ret = "C(8~12hr)";
    } else if (sum < sample_count * 15 * 60 * 60L) {
        ret = "D(12~15hr)";
    } else if (sum < sample_count * 20 * 60 * 60L) {
        ret = "E(15~20hr)";
    } else {
        ret = "F(20hr~)";
    }
    return ret;
}

bool htc_stats_is_valid(int category, unsigned long chg_time, unsigned long dischg_time, int unplug_level, int plug_level)
{
    bool ret = false;
    switch (category)
    {
        case HTC_STATS_CATEGORY_ALL:
            ret = dischg_time >= 60 * 60L &&
                  chg_time >= 60 * 60L;
            break;
        case HTC_STATS_CATEGORY_FULL_LOW:
            ret = dischg_time >= 60 * 60L &&
                  chg_time >= 60 * 60L &&
                  unplug_level >= 80 &&
                  plug_level <= 30;
            break;
    }
    return ret;
}

void htc_stats_update(int category, unsigned long chg_time, unsigned long dischg_time)
{
    struct htc_statistics_category* category_ptr;
    switch (category)
    {
        case HTC_STATS_CATEGORY_ALL: category_ptr = &g_htc_stats_category_all; break;
        case HTC_STATS_CATEGORY_FULL_LOW: category_ptr = &g_htc_stats_category_full_low; break;
    }

    category_ptr->sample_count += 1;
    category_ptr->chg_time_sum += chg_time;
    category_ptr->dischg_time_sum += dischg_time;
}

const char* htc_stats_category2str(int category)
{
    const char* ret;
    switch (category)
    {
        case HTC_STATS_CATEGORY_ALL: ret = "all"; break;
        case HTC_STATS_CATEGORY_FULL_LOW: ret = "full_low"; break;
    }
    return ret;
}

void htc_stats_calculate_statistics_data(int category, unsigned long chg_time, unsigned long dischg_time)
{
    int unplug_level = g_htc_stats_data.end_chg_batt_level;
    int plug_level = g_htc_stats_data.begin_chg_batt_level;
    struct htc_statistics_category* category_ptr;

    switch (category) {
        case HTC_STATS_CATEGORY_ALL: category_ptr = &g_htc_stats_category_all; break;
        case HTC_STATS_CATEGORY_FULL_LOW: category_ptr = &g_htc_stats_category_full_low; break;
    }

    if (htc_stats_is_valid(category, chg_time, dischg_time, unplug_level, plug_level))
    {
        htc_stats_update(category, chg_time, dischg_time);
    }
    else
    {
        BATT_DEBUG("%s: statistics_%s: This sampling is invalid, chg_time=%ld, dischg_time=%ld\n",
            HTC_STATISTICS,
            htc_stats_category2str(category),
            chg_time,
            dischg_time);
    }

    if (category_ptr->sample_count >= HTC_STATS_SAMPLE_NUMBER)
    {
        BATT_DEBUG("%s: statistics_%s: group=%s, chg_time_sum=%ld, dischg_time_sum=%ld, sample_count=%d\n",
            HTC_STATISTICS,
            htc_stats_category2str(category),
            htc_stats_classify(category_ptr->dischg_time_sum, category_ptr->sample_count),
            category_ptr->chg_time_sum,
            category_ptr->dischg_time_sum,
            category_ptr->sample_count);

        
        category_ptr->sample_count = 0;
        category_ptr->chg_time_sum = 0L;
        category_ptr->dischg_time_sum = 0L;
     }
}

void htc_stats_update_charging_statistics(int latest, int prev)
{
    struct timeval rtc_now;
    struct tm time_info;
    char time_str[25];
    long dischg_time = 0L;
    long chg_time = 0L;
    char debug[10] = "[debug]";
    bool debug_flag = false;

    if (debug_flag) {
        BATT_DEBUG("%s: %s update_charging_statistics(): prev=%d, now=%d\n",
            HTC_STATISTICS, debug, prev, latest);
    }

    do_gettimeofday(&rtc_now);
    time_to_tm(rtc_now.tv_sec, 0, &time_info);
    snprintf(time_str, sizeof(time_str), "%04ld-%02d-%02d %02d:%02d:%02d UTC",
        time_info.tm_year+1900,
        time_info.tm_mon+1,
        time_info.tm_mday,
        time_info.tm_hour,
        time_info.tm_min,
        time_info.tm_sec);

    
    if (prev == POWER_SUPPLY_TYPE_UNKNOWN && htc_stats_is_ac(latest) && !g_htc_stats_charging)
    {
        g_htc_stats_charging = true;

        if (0 != g_htc_stats_data.end_chg_time)
        {
            
            dischg_time = rtc_now.tv_sec - g_htc_stats_data.end_chg_time;
            if (dischg_time < 0) dischg_time = 0L;
            BATT_DEBUG("%s: sampling: dischg_time=%ld(s), level=%d->%d, prev_chg_time=%ld(s)\n",
                HTC_STATISTICS,
                dischg_time,
                g_htc_stats_data.end_chg_batt_level,
                htc_batt_info.rep.level,
                g_htc_stats_data.end_chg_time - g_htc_stats_data.begin_chg_time);

            
            chg_time = g_htc_stats_data.end_chg_time - g_htc_stats_data.begin_chg_time;
            if (chg_time < 0) chg_time = 0L;

            htc_stats_calculate_statistics_data(HTC_STATS_CATEGORY_ALL, chg_time, dischg_time);
            htc_stats_calculate_statistics_data(HTC_STATS_CATEGORY_FULL_LOW, chg_time, dischg_time);
        }

        
        g_htc_stats_data.begin_chg_time = 0;
        g_htc_stats_data.end_chg_time = 0;
        g_htc_stats_data.begin_chg_batt_level = 0;
        g_htc_stats_data.end_chg_batt_level = 0;

        
        g_htc_stats_data.begin_chg_time = rtc_now.tv_sec;
        g_htc_stats_data.begin_chg_batt_level = htc_batt_info.rep.level;

        BATT_DEBUG("%s: begin charging: level=%d, at=%s\n",
            HTC_STATISTICS,
            g_htc_stats_data.begin_chg_batt_level,
            time_str);
    }

    
    if (htc_stats_is_ac(prev) && latest == POWER_SUPPLY_TYPE_UNKNOWN && g_htc_stats_charging)
    {
        g_htc_stats_charging = false;

        
        g_htc_stats_data.end_chg_time = rtc_now.tv_sec;
        g_htc_stats_data.end_chg_batt_level = htc_batt_info.rep.level;

        BATT_DEBUG("%s: end charging: level=%d, at=%s\n",
            HTC_STATISTICS,
            g_htc_stats_data.end_chg_batt_level,
            time_str);
    }
}
#endif 

void htc_battery_info_update(enum power_supply_property prop, int intval)
{
#ifdef CONFIG_HTC_BATT_PCN0014
	if (!g_htc_battery_probe_done)
		return;
#endif 
	switch (prop) {
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			
			g_latest_chg_src = intval;
			if (g_latest_chg_src != htc_batt_info.rep.charging_source) {
#ifdef CONFIG_HTC_BATT_PCN0021
				htc_stats_update_charging_statistics(g_latest_chg_src, htc_batt_info.rep.charging_source);
#endif 
				htc_batt_schedule_batt_info_update();
				if (!delayed_work_pending(&htc_batt_info.chg_full_check_work)
					&& (g_latest_chg_src > CHARGER_UNKNOWN)) {
						wake_lock(&htc_batt_info.charger_exist_lock);
					schedule_delayed_work(&htc_batt_info.chg_full_check_work,0);
				} else {
					if (delayed_work_pending(&htc_batt_info.chg_full_check_work)
						&& (g_latest_chg_src == POWER_SUPPLY_TYPE_UNKNOWN)) {
						BATT_LOG("cancel chg_full_check_work when plug out.\n");
						cancel_delayed_work_sync(&htc_batt_info.chg_full_check_work);
						schedule_delayed_work(&htc_batt_info.chg_full_check_work,0);
 					}
				}
#ifdef CONFIG_HTC_BATT_PCN0022
				if(!delayed_work_pending(&htc_batt_info.is_usb_overheat_work)) {
					schedule_delayed_work(&htc_batt_info.is_usb_overheat_work, 0);
				}
#endif 
#ifdef CONFIG_HTC_BATT_PCN0017
				if (g_latest_chg_src == 0){ 
					g_is_unknown_charger = false;
					g_redetect_times = 0;
					if (delayed_work_pending(&htc_batt_info.chk_unknown_chg_work))
						cancel_delayed_work(&htc_batt_info.chk_unknown_chg_work);
				}
#endif 
			}
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			if (htc_batt_info.rep.level_raw != intval) {
				htc_batt_info.rep.level_raw = intval;
				htc_batt_schedule_batt_info_update();
			}
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			if (htc_batt_info.rep.health != intval) {
				htc_batt_info.rep.health = intval;
				htc_batt_schedule_batt_info_update();
			}
			break;
		default:
			break;
	}
}

static int htc_batt_charger_control(enum charger_control_flag control)
{
	int ret = 0;

	BATT_EMBEDDED("%s: user switch charger to mode: %u", __func__, control);

	switch (control) {
		case STOP_CHARGER:
			g_chg_dis_reason |= HTC_BATT_CHG_DIS_BIT_USR_TMR;
			break;
		case ENABLE_CHARGER:
			g_chg_dis_reason &= ~HTC_BATT_CHG_DIS_BIT_USR_TMR;
			break;
		case DISABLE_PWRSRC:
			g_pwrsrc_dis_reason |= HTC_BATT_PWRSRC_DIS_BIT_API;
			break;
		case ENABLE_PWRSRC:
			g_pwrsrc_dis_reason &= ~HTC_BATT_PWRSRC_DIS_BIT_API;
			break;
#ifdef CONFIG_FPC_HTC_DISABLE_CHARGING
		case DISABLE_PWRSRC_FINGERPRINT:
			g_pwrsrc_dis_reason |= HTC_BATT_PWRSRC_DIS_BIT_FP;
			break;
		case ENABLE_PWRSRC_FINGERPRINT:
			g_pwrsrc_dis_reason &= ~HTC_BATT_PWRSRC_DIS_BIT_FP;
			break;
#endif 
		case ENABLE_LIMIT_CHARGER:
		case DISABLE_LIMIT_CHARGER:
			BATT_EMBEDDED("%s: skip charger_contorl(%d)", __func__, control);
			return ret;
			break;

		default:
			BATT_EMBEDDED("%s: unsupported charger_contorl(%d)", __func__, control);
			ret =  -1;
			break;

	}

	htc_batt_schedule_batt_info_update();

	return ret;
}

#ifdef CONFIG_FPC_HTC_DISABLE_CHARGING 
int htc_battery_charger_switch_internal(int enable)
{
	int rc = 0;

	switch (enable) {
	case ENABLE_PWRSRC_FINGERPRINT:
	case DISABLE_PWRSRC_FINGERPRINT:
		rc = htc_batt_charger_control(enable);
		break;
	default:
		pr_info("%s: invalid type, enable=%d\n", __func__, enable);
		return -EINVAL;
	}

	if (rc < 0)
		BATT_ERR("FP control charger failed!");

	return rc;
}
#endif 

int htc_battery_get_discharging_reason(void)
{
	return g_chg_dis_reason;
}

int htc_battery_get_pwrsrc_discharging_reason(void)
{
	return g_pwrsrc_dis_reason;
}

#ifdef CONFIG_HTC_BATT_PCN0004
static ssize_t htc_battery_set_full_level(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int rc = 0;
	int percent = 100;

	rc = kstrtoint(buf, 10, &percent);
	if (rc)
		return rc;

	htc_batt_info.rep.full_level = percent;

	BATT_LOG(" set full_level constraint as %d.\n", (int)percent);

    return count;
}

static ssize_t htc_battery_charger_stat(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_charger_ctrl_stat);
}

static ssize_t htc_battery_charger_switch(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int enable = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &enable);
	if (rc)
		return rc;

	BATT_EMBEDDED("Set charger_control:%d", enable);
	if (enable >= END_CHARGER)
		return -EINVAL;

	rc = htc_batt_charger_control(enable);
	if (rc < 0) {
		BATT_ERR("charger control failed!");
		return rc;
	}
	g_charger_ctrl_stat = enable;

    return count;
}

static ssize_t htc_battery_set_phone_call(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int phone_call = 0;
	int rc = 0;
	static bool slot_1 = false, slot_2 = false, slot_3 = false;
	static bool call_exist = false;
	int slotNo;

	rc = kstrtoint(buf, 10, &phone_call);
	if (rc)
		return rc;

	slotNo = phone_call>>1;
	if(phone_call & 0x1) {
		switch(slotNo) {
			case 0:
				slot_1 = true;
				break;
			case 1:
				slot_2 = true;
				break;
			case 5:
				slot_3 = true;
				break;
			default:
				break;
		}
	} else {
		switch(slotNo){
			case 0:
				slot_1 = false;
				break;
			case 1:
				slot_2 = false;
				break;
			case 5:
				slot_3 = false;
				break;
			default:
				break;
		}
	}
	if(slot_1 || slot_2 || slot_3)
		call_exist = true;
	else
		call_exist = false;
printk("[set_phone_call]slot_1=%d,slot_2=%d,slot_3=%d\n",slot_1,slot_2,slot_3);
	if (call_exist){
#ifdef CONFIG_HTC_BATT_PCN0011
		suspend_highfreq_check_reason |= SUSPEND_HIGHFREQ_CHECK_BIT_TALK;
#endif 
		g_chg_limit_reason |= HTC_BATT_CHG_LIMIT_BIT_TALK;
	} else {
#ifdef CONFIG_HTC_BATT_PCN0011
		suspend_highfreq_check_reason  &= ~SUSPEND_HIGHFREQ_CHECK_BIT_TALK;
#endif 
		g_chg_limit_reason  &= ~HTC_BATT_CHG_LIMIT_BIT_TALK;
	}
	return count;
}


static ssize_t htc_battery_set_net_call(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int net_call = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &net_call);
	if (rc)
		return rc;

	BATT_LOG("set context net_call=%d\n", net_call);

	if (net_call) {
#ifdef CONFIG_HTC_BATT_PCN0011
		suspend_highfreq_check_reason |= SUSPEND_HIGHFREQ_CHECK_BIT_SEARCH;
#endif 
#ifdef CONFIG_HTC_BATT_PCN0013
		g_chg_limit_reason |= HTC_BATT_CHG_LIMIT_BIT_NET_TALK;
#endif 
	} else {
#ifdef CONFIG_HTC_BATT_PCN0011
		suspend_highfreq_check_reason &= ~SUSPEND_HIGHFREQ_CHECK_BIT_SEARCH;
#endif 
#ifdef CONFIG_HTC_BATT_PCN0013
		g_chg_limit_reason &= ~HTC_BATT_CHG_LIMIT_BIT_NET_TALK;
#endif 

	}


	return count;
}

static ssize_t htc_battery_set_play_music(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int play_music = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &play_music);
	if (rc)
		return rc;

	BATT_LOG("set context play music=%d\n", play_music);

#ifdef CONFIG_HTC_BATT_PCN0011
	if (play_music)
		suspend_highfreq_check_reason |= SUSPEND_HIGHFREQ_CHECK_BIT_MUSIC;
	else
		suspend_highfreq_check_reason &= ~SUSPEND_HIGHFREQ_CHECK_BIT_MUSIC;
#endif 

    return count;
}

static ssize_t htc_battery_rt_vol(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", battery_meter_get_battery_voltage(KAL_TRUE));
}

static ssize_t htc_battery_rt_current(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", ((battery_meter_get_battery_current_sign())?(-1):1)*(battery_meter_get_battery_current()/10));
}

static ssize_t htc_battery_rt_temp(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", battery_meter_get_battery_temperature()*10);
}

static ssize_t htc_battery_rt_id(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
#ifdef CONFIG_HTC_BATT_PCN0006
	if (g_test_power_monitor)
		htc_batt_info.rep.batt_id = 77;
#endif 

    return snprintf(buf, PAGE_SIZE, "%d\n", htc_batt_info.rep.batt_id);
}

static ssize_t htc_battery_rt_id_mv(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", fgauge_get_battery_id_mv());
}

static ssize_t htc_battery_ftm_charger_stat(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", g_ftm_charger_control_flag);
}

static ssize_t htc_battery_ftm_charger_switch(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	int enable = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &enable);
	if (rc)
		return rc;

	BATT_EMBEDDED("Set ftm_charger_control:%d", enable);
	if (enable >= FTM_END_CHARGER)
		return -EINVAL;

	if (g_ftm_charger_control_flag == enable)
		return count;

	g_ftm_charger_control_flag = enable;

	if (g_ftm_charger_control_flag == FTM_STOP_CHARGER) {
		htc_batt_schedule_batt_info_update();
	} else if (g_ftm_charger_control_flag == FTM_SLOW_CHARGE) {
		g_ftm_battery_flag = KAL_TRUE;
		htc_batt_schedule_batt_info_update();
		charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		wake_up_bat();
	} else if (g_ftm_charger_control_flag == FTM_FAST_CHARGE) {
		g_ftm_battery_flag = KAL_TRUE;
		charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		htc_batt_schedule_batt_info_update();
		wake_up_bat();
	} else {
		g_ftm_battery_flag = KAL_FALSE;
	}

    return count;
}

static ssize_t htc_battery_over_vchg(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	htc_batt_info.rep.over_vchg = is_over_vchg();

	return snprintf(buf, PAGE_SIZE, "%d\n", htc_batt_info.rep.over_vchg);
}

static ssize_t htc_battery_overload(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%d\n", htc_batt_info.rep.overload);
}

static ssize_t htc_battery_show_htc_extension_attr(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", htc_batt_info.htc_extension);
}

static ssize_t htc_battery_show_eoc_ever_reached(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_htc_battery_eoc_ever_reached);
}

#ifdef CONFIG_HTC_BATT_PCN0022
static ssize_t htc_battery_show_usb_overheat(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_usb_overheat? 1:0);
}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0009
static ssize_t htc_battery_show_batt_attr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int len = 0;
	const char *chr_src[] = {"NONE", "USB", "CHARGING_HOST",
		"NONSTANDARD_CHARGER", "AC(USB_DCP)", "APPLE_2_1A_CHARGER",
		"APPLE_1_0A_CHARGER", "APPLE_0_5A_CHARGER", "WIRELESS_CHARGER"};
#ifdef CONFIG_HTC_BATT_PCN0008
	time_t t = g_batt_first_use_time;
	struct tm timeinfo;

	time_to_tm(t, 0, &timeinfo);
#endif 

	
	len += scnprintf(buf + len, PAGE_SIZE - len,
			"charging_source: %s;\n"
			"charging_enabled: %d;\n"
			"overload: %d;\n"
			"Percentage(%%): %d;\n"
			"Percentage_raw(%%): %d;\n"
#ifdef CONFIG_HTC_BATT_PCN0008
			"batt_cycle_first_use: %04ld/%02d/%02d/%02d:%02d:%02d\n"
			"batt_cycle_level_raw: %u;\n"
			"batt_cycle_overheat(s): %u;\n"
#endif 
			"htc_extension: 0x%x;\n",
			chr_src[htc_batt_info.rep.charging_source],
			htc_batt_info.rep.chg_en,
			htc_batt_info.rep.overload,
			htc_batt_info.rep.level,
			htc_batt_info.rep.level_raw,
#ifdef CONFIG_HTC_BATT_PCN0008
			timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
			g_total_level_raw,
			g_overheat_55_sec,
#endif 
			htc_batt_info.htc_extension
			);

	
	if (htc_batt_info.igauge) {
		if (htc_batt_info.igauge->get_attr_text)
			len += htc_batt_info.igauge->get_attr_text(buf + len,
						PAGE_SIZE - len);
	}

	
	if (htc_batt_info.icharger) {
		if (htc_batt_info.icharger->get_attr_text)
			len += htc_batt_info.icharger->get_attr_text(buf + len,
						PAGE_SIZE - len);
	}

	return len;
}
#endif 

static ssize_t htc_battery_show_cc_attr(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "cc:%d\n", battery_meter_get_car());
}

static ssize_t htc_battery_show_capacity_raw(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", htc_batt_info.rep.level_raw);
}


static ssize_t htc_battery_charging_source(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", htc_batt_info.rep.chg_src);
}


static ssize_t htc_charger_type(struct device *dev,
                 struct device_attribute *attr,
                char *buf)
{
	int chg_type, aicl_result;
	int standard_cable = 0; 

	aicl_result = g_temp_input_CC_value;

	switch(htc_batt_info.rep.charging_source){
		case CHARGER_UNKNOWN:
			chg_type = DATA_NO_CHARGING;
			break;
		case STANDARD_HOST:
			chg_type = DATA_USB;
			break;
		case CHARGING_HOST:
			chg_type = DATA_USB_CDP;
			break;
		case NONSTANDARD_CHARGER:
		case STANDARD_CHARGER:
			chg_type = DATA_AC;
			break;
		default:
			chg_type = DATA_UNKNOWN_TYPE;
			break;
	}

	return snprintf(buf, PAGE_SIZE, "charging_source=%s;iusb_current=%d;workable=%d\n", htc_chr_type_data_str[chg_type], aicl_result, standard_cable);
}

static ssize_t htc_thermal_batt_temp(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_thermal_batt_temp);
}

static ssize_t htc_batt_chgr_data(char *buf)
{
	int chg_type, len = 0;

	if(((g_BI_data_ready & HTC_BATT_CHG_BI_BIT_CHGR) == 0)||(g_batt_chgr_end_batvol == 0)){
		return len;
	}

	switch(htc_batt_info.rep.charging_source){
		case CHARGER_UNKNOWN:
			chg_type = DATA_NO_CHARGING;
			break;
		case STANDARD_HOST:
			chg_type = DATA_USB;
			break;
		case CHARGING_HOST:
			chg_type = DATA_USB_CDP;
			break;
		case NONSTANDARD_CHARGER:
		case STANDARD_CHARGER:
			chg_type = DATA_AC;
			break;
		default:
			chg_type = DATA_UNKNOWN_TYPE;
			break;
	}

        len = snprintf(buf, PAGE_SIZE, "appID: charging_log\n"
			"category: charging\n"
			"action: \n"
			"Attribute:\n"
			"{type, %s}\n"
			"{iusb, %d}\n"
			"{ibat, %d}\n"
			"{start_temp, %d}\n"
			"{end_temp, %d}\n"
			"{start_level, %d}\n"
			"{end_level, %d}\n"
			"{batt_vol_start, %d}\n"
			"{batt_vol_end, %d}\n"
                        "{chg_cycle, %d}\n"
                        "{overheat, %d}\n"
                        "{batt_level, %d}\n"
                        "{batt_vol, %d}\n"
			"{err_code, %d}\n",
			htc_chr_type_data_str[chg_type], g_batt_chgr_iusb, g_batt_chgr_ibat, g_batt_chgr_start_temp, g_batt_chgr_end_temp, g_batt_chgr_start_level, g_batt_chgr_end_level, g_batt_chgr_start_batvol, g_batt_chgr_end_batvol, g_total_level_raw, g_overheat_55_sec, g_batt_aging_level, g_batt_aging_bat_vol, 0);

	BATT_EMBEDDED("Charging_log:\n%s", buf);

	g_batt_chgr_end_batvol = 0;
	return len;
}

static ssize_t htc_batt_bidata(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	int len = 0;

	len = htc_batt_chgr_data(buf);

	return len;

}

#define BATT_CHK_MAX				10
static const unsigned int sc_cycle_num[BATT_CHK_MAX] = {10000, 20000, 30000, 35000, 40000, 45000, 50000, 55000, 60000, 65000};
#define BATT_CHK_OVERHEAT_TIME	7200 
static ssize_t htc_batt_check(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int idx = 0;
	int batt_status;

	while (idx < BATT_CHK_MAX) {
		if (g_total_level_raw < sc_cycle_num[idx])
			break;
		idx++;
	}

	if (g_overheat_55_sec > BATT_CHK_OVERHEAT_TIME)
		batt_status = BATT_CHK_MAX - idx -1;
	else
		batt_status = BATT_CHK_MAX - idx;

	if (batt_status < 0)
		batt_status = 0;

	return sprintf (buf, "%d\n", batt_status);
}

static ssize_t htc_clr_cycle(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count)
{
	g_total_level_raw = 0;
	g_overheat_55_sec = 0;
	g_batt_first_use_time = 0;
	g_batt_cycle_checksum= 0;

	BATT_LOG("CLEAR BATT CYCLE DATA\n");
	return count;
}

#ifdef CONFIG_HTC_BATT_PCN0014
static ssize_t htc_battery_state(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
        return snprintf(buf, PAGE_SIZE, "%d\n", g_htc_battery_probe_done);
}
#endif 

static struct device_attribute htc_battery_attrs[] = {
#ifdef CONFIG_HTC_BATT_PCN0009
	__ATTR(batt_attr_text, S_IRUGO, htc_battery_show_batt_attr, NULL),
#endif 
	__ATTR(full_level, S_IWUSR | S_IWGRP, NULL, htc_battery_set_full_level),
	__ATTR(full_level_dis_batt_chg, S_IWUSR | S_IWGRP, NULL, htc_battery_set_full_level),
	__ATTR(charger_control, S_IWUSR | S_IWGRP | S_IRUGO, htc_battery_charger_stat, htc_battery_charger_switch),
	__ATTR(phone_call, S_IWUSR | S_IWGRP, NULL, htc_battery_set_phone_call),
	__ATTR(net_call, S_IWUSR | S_IWGRP, NULL, htc_battery_set_net_call),
	__ATTR(play_music, S_IWUSR | S_IWGRP, NULL, htc_battery_set_play_music),
	__ATTR(batt_vol_now, S_IRUGO, htc_battery_rt_vol, NULL),
	__ATTR(batt_current_now, S_IRUGO, htc_battery_rt_current, NULL),
	__ATTR(batt_temp_now, S_IRUGO, htc_battery_rt_temp, NULL),
	__ATTR(batt_id, S_IRUGO, htc_battery_rt_id, NULL),
	__ATTR(batt_id_now, S_IRUGO, htc_battery_rt_id_mv, NULL),
	__ATTR(ftm_charger_control, S_IWUSR | S_IWGRP | S_IRUGO, htc_battery_ftm_charger_stat,
		htc_battery_ftm_charger_switch),
	__ATTR(over_vchg, S_IRUGO, htc_battery_over_vchg, NULL),
	__ATTR(overload, S_IRUGO, htc_battery_overload, NULL),
	__ATTR(htc_extension, S_IRUGO, htc_battery_show_htc_extension_attr, NULL),
	__ATTR(htc_eoc_ever_reached, S_IRUGO, htc_battery_show_eoc_ever_reached, NULL),
	__ATTR(batt_power_meter, S_IRUGO, htc_battery_show_cc_attr, NULL),
	__ATTR(capacity_raw, S_IRUGO, htc_battery_show_capacity_raw, NULL),
	
	__ATTR(charging_source, S_IRUGO, htc_battery_charging_source, NULL),
	
#ifdef CONFIG_HTC_BATT_PCN0014
	__ATTR(batt_state, S_IRUGO, htc_battery_state, NULL),
#endif 
#ifdef CONFIG_HTC_BATT_PCN0022
	__ATTR(usb_overheat, S_IRUGO, htc_battery_show_usb_overheat, NULL),
#endif 
	__ATTR(charger_type, S_IRUGO, htc_charger_type, NULL),
	__ATTR(thermal_batt_temp, S_IRUGO, htc_thermal_batt_temp, NULL),
	__ATTR(htc_batt_data, S_IRUGO, htc_batt_bidata, NULL),
	__ATTR(batt_chked, S_IRUGO, htc_batt_check, NULL),
	__ATTR(batt_asp_set, S_IWUSR | S_IWGRP, NULL, htc_clr_cycle),
};

int htc_battery_create_attrs(struct device *dev)
{
    int i = 0, rc = 0;

    for (i = 0; i < ARRAY_SIZE(htc_battery_attrs); i++) {
        rc = device_create_file(dev, &htc_battery_attrs[i]);
        if (rc)
            goto htc_attrs_failed;
    }

    goto succeed;

htc_attrs_failed:
    while (i--)
        device_remove_file(dev, &htc_battery_attrs[i]);
succeed:
    return rc;
}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0014
void htc_battery_probe_process(enum htc_batt_probe probe_type) {
	static int s_probe_finish_process = 0;

	s_probe_finish_process++;
	BATT_LOG("Probe process: (%d, %d)\n", probe_type, s_probe_finish_process);

	if (s_probe_finish_process == BATT_PROBE_MAX) {

		htc_batt_info.batt_psy = power_supply_get_by_name("battery");
		htc_batt_info.rep.level = BMT_status.UI_SOC2;
		htc_batt_info.rep.level_raw = BMT_status.UI_SOC2;

#ifdef CONFIG_HTC_BATT_PCN0006
		
		if (g_flag_keep_charge_on || g_flag_disable_safety_timer)
		    htc_safety_timer_en(0);

		if (g_test_power_monitor) {
			htc_batt_info.rep.level = POWER_MONITOR_BATT_CAPACITY;
			htc_batt_info.rep.level_raw = POWER_MONITOR_BATT_CAPACITY;
		}
#endif 

#ifdef CONFIG_HTC_BATT_PCN0003
		htc_batt_info.rep.batt_id = fgauge_get_batt_id();
		BATT_LOG("%s: batt id=%d\n", __func__, htc_batt_info.rep.batt_id);
		if (htc_batt_info.rep.batt_id == ERR_BATT_ID) {
			htc_set_chg_en(0);
			g_chg_dis_reason |= HTC_BATT_CHG_DIS_BIT_ID;
			g_pwrsrc_dis_reason |= HTC_BATT_PWRSRC_DIS_BIT_ID;
		}
#endif 
#ifdef ALPINE_BATT 
		if (htc_batt_info.rep.batt_id == 1) {
			htc_batt_info.batt_full_voltage_mv = 4300;
		}
#endif
		BATT_LOG("Probe process done.\n");
		g_htc_battery_probe_done = true;
		htc_battery_info_update(POWER_SUPPLY_PROP_CHARGE_TYPE, mt_get_charger_type());

#if defined(ALPINE_BATT)
		if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)) {
			g_charger_ctrl_stat = DISABLE_PWRSRC;
			htc_batt_charger_control(g_charger_ctrl_stat);
		}
#endif
	}
}
#endif 

static struct htc_battery_platform_data htc_battery_pdev_data = {
	
	
	.icharger.get_vbus = battery_meter_get_charger_voltage,
#ifdef CONFIG_HTC_BATT_PCN0009
	.icharger.get_attr_text = htc_charger_get_attr_text,
	.igauge.get_attr_text = htc_battery_meter_show_attr,
#endif 
	.icharger.is_battery_full_eoc_stop = htc_get_eoc_status,
};
static void batt_regular_timer_handler(unsigned long data) {
	htc_batt_schedule_batt_info_update();
}

#ifdef CONFIG_HTC_BATT_PCN0011
#if 0
static enum alarmtimer_restart
batt_check_alarm_handler(struct alarm *alarm, ktime_t time)
{
	
	return 0;
}
#endif
static int htc_battery_prepare(struct device *dev)
{
	struct timespec xtime;
	unsigned long cur_jiffies;
#if 0
	ktime_t interval;
	ktime_t next_alarm;
	s64 next_alarm_sec = 0;
	int check_time = 0;
#endif
	xtime = CURRENT_TIME;
	cur_jiffies = jiffies;
	htc_batt_timer.total_time_ms += (cur_jiffies -
			htc_batt_timer.batt_system_jiffies) * MSEC_PER_SEC / HZ;
	htc_batt_timer.batt_system_jiffies = cur_jiffies;
	htc_batt_timer.batt_suspend_ms = xtime.tv_sec * MSEC_PER_SEC +
					xtime.tv_nsec / NSEC_PER_MSEC;

	BATT_EMBEDDED("%s: passing time:%lu ms,", __func__, htc_batt_timer.total_time_ms);

#if 0
	if (suspend_highfreq_check_reason)
		check_time = BATT_SUSPEND_HIGHFREQ_CHECK_TIME;
	else
		check_time = BATT_SUSPEND_CHECK_TIME;

	interval = ktime_set(check_time - htc_batt_timer.total_time_ms / 1000, 0);
	next_alarm_sec = div_s64(interval.tv64, NSEC_PER_SEC);

	
	if (next_alarm_sec <= 1) {
		BATT_LOG("%s: passing time:%lu ms, trigger batt_work immediately."
			"(suspend_highfreq_check_reason=0x%x)\n", __func__,
			htc_batt_timer.total_time_ms,
			suspend_highfreq_check_reason);
		htc_batt_schedule_batt_info_update();
		return -EBUSY;
	}

	BATT_EMBEDDED("%s: passing time:%lu ms, alarm will be triggered after %lld sec."
		"(suspend_highfreq_check_reason=0x%x, htc_batt_info.state=0x%x)",
		__func__, htc_batt_timer.total_time_ms, next_alarm_sec,
		suspend_highfreq_check_reason, htc_batt_info.state);

	next_alarm = ktime_add(ktime_get_real(), interval);
	alarm_start(&htc_batt_timer.batt_check_wakeup_alarm, next_alarm);
#endif
	return 0;
}

static void htc_battery_complete(struct device *dev)
{
	struct timespec xtime;
	unsigned long resume_ms;
	unsigned long sr_time_period_ms;
	unsigned long check_time;
	int	batt_vol;

	xtime = CURRENT_TIME;
	htc_batt_timer.batt_system_jiffies = jiffies;
	resume_ms = xtime.tv_sec * MSEC_PER_SEC + xtime.tv_nsec / NSEC_PER_MSEC;
	sr_time_period_ms = resume_ms - htc_batt_timer.batt_suspend_ms;
	htc_batt_timer.total_time_ms += sr_time_period_ms;

	BATT_EMBEDDED("%s: sr_time_period=%lu ms; total passing time=%lu ms.",
			__func__, sr_time_period_ms, htc_batt_timer.total_time_ms);

	if (suspend_highfreq_check_reason)
		check_time = BATT_SUSPEND_HIGHFREQ_CHECK_TIME * MSEC_PER_SEC;
	else
		check_time = BATT_SUSPEND_CHECK_TIME * MSEC_PER_SEC;

	check_time -= CHECH_TIME_TOLERANCE_MS;

	batt_vol = battery_meter_get_battery_voltage(KAL_TRUE);
	if ((htc_batt_timer.total_time_ms >= check_time) || (batt_vol < 3250)) {
		BATT_LOG("trigger batt_work while resume."
				"(suspend_highfreq_check_reason=0x%x, batt_vol=%d\n",
				suspend_highfreq_check_reason, batt_vol);
		htc_batt_schedule_batt_info_update();
	}

	return;
}

static struct dev_pm_ops htc_battery_pm_ops = {
	.prepare = htc_battery_prepare,
	.complete = htc_battery_complete,
};
#endif 

static int reboot_consistent_command_call(struct notifier_block *nb,
	unsigned long event, void *data)
{
	if ((event != SYS_RESTART) && (event != SYS_POWER_OFF))
		goto end;

#ifdef CONFIG_HTC_BATT_PCN0008
	BATT_LOG("%s: save batt cycle data\n", __func__);
	
	emmc_misc_write(g_total_level_raw, HTC_BATT_TOTAL_LEVELRAW);
	emmc_misc_write(g_overheat_55_sec, HTC_BATT_OVERHEAT_MSEC);
	emmc_misc_write(g_batt_cycle_checksum, HTC_BATT_CYCLE_CHECKSUM);
#endif 
#ifdef CONFIG_HTC_BATT_PCN0002
	BATT_LOG("%s: save consistent data\n", __func__);
	change_level_by_consistent_and_store_into_emmc(1);
#endif 
end:
	return NOTIFY_DONE;
}

static struct notifier_block reboot_consistent_command = {
	.notifier_call = reboot_consistent_command_call,
};

static int htc_battery_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct htc_battery_platform_data *pdata = pdev->dev.platform_data;

	htc_batt_info.icharger = &pdata->icharger;
	htc_batt_info.igauge = &pdata->igauge;
	INIT_WORK(&htc_batt_timer.batt_work, batt_worker);
#ifdef CONFIG_HTC_BATT_PCN0001
	INIT_DELAYED_WORK(&htc_batt_info.chg_full_check_work, chg_full_check_worker);
#endif 
#ifdef CONFIG_HTC_BATT_PCN0022
	INIT_DELAYED_WORK(&htc_batt_info.is_usb_overheat_work, is_usb_overheat_worker);
#endif 
#ifdef CONFIG_HTC_BATT_PCN0017
	INIT_DELAYED_WORK(&htc_batt_info.chk_unknown_chg_work, chk_unknown_chg_worker);
#endif 
	init_timer(&htc_batt_timer.batt_timer);
	htc_batt_timer.batt_timer.function = batt_regular_timer_handler;
#if 0 
	alarm_init(&htc_batt_timer.batt_check_wakeup_alarm, ALARM_REALTIME,
			batt_check_alarm_handler);
#endif 
	htc_batt_timer.batt_wq = create_singlethread_workqueue("batt_timer");

	htc_batt_timer.time_out = BATT_TIMER_UPDATE_TIME;
	batt_set_check_timer(htc_batt_timer.time_out);

	
	ret = register_reboot_notifier(&reboot_consistent_command);
	if (ret)
		BATT_ERR("can't register reboot notifier, error = %d\n", ret);

#ifdef CONFIG_HTC_BATT_PCN0014
	htc_battery_probe_process(HTC_BATT_PROBE_DONE);
#endif 

	return 0;
}

static struct platform_device htc_battery_pdev = {
	.name = HTC_BATT_NAME,
	.id = -1,
	.dev    = {
		.platform_data = &htc_battery_pdev_data,
	},
};

static struct platform_driver htc_battery_driver = {
	.probe	= htc_battery_probe,
	.driver	= {
		.name	= HTC_BATT_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_HTC_BATT_PCN0011
		.pm = &htc_battery_pm_ops,
#endif 
	},
};

static int __init htc_battery_init(void)
{
#if defined(CONFIG_HTC_BATT_PCN0002)||defined(CONFIG_HTC_BATT_PCN0008)
	struct device_node *node;
#endif
	int ret = -EINVAL;
	u32 val;

	wake_lock_init(&htc_batt_timer.battery_lock, WAKE_LOCK_SUSPEND, "htc_battery");
	wake_lock_init(&htc_batt_info.charger_exist_lock, WAKE_LOCK_SUSPEND,"charger_exist_lock");

	htc_batt_info.k_debug_flag = get_kernel_flag();

#ifdef CONFIG_HTC_BATT_PCN0006
	g_test_power_monitor =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_TEST_PWR_SUPPLY) ? 1 : 0;
	g_flag_keep_charge_on =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_KEEP_CHARG_ON) ? 1 : 0;
	g_flag_force_ac_chg =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_ENABLE_FAST_CHARGE) ? 1 : 0;
	g_flag_pa_fake_batt_temp =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_FOR_PA_TEST) ? 1 : 0;
	g_flag_disable_safety_timer =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_DISABLE_SAFETY_TIMER) ? 1 : 0;
	g_flag_disable_temp_protection =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_DISABLE_TBATT_PROTECT) ? 1 : 0;
	g_flag_enable_batt_debug_log =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_ENABLE_BMS_CHARGER_LOG) ? 1 : 0;
	g_flag_ats_limit_chg =
		(htc_batt_info.k_debug_flag & KERNEL_FLAG_ATS_LIMIT_CHARGE) ? 1 : 0;
#endif 

	
	htc_batt_info.rep.batt_vol = 4000;
	htc_batt_info.rep.batt_id = 1;
	htc_batt_info.rep.batt_temp = 280;
	htc_batt_info.rep.batt_current = 0;
	htc_batt_info.rep.charging_source = CHARGER_UNKNOWN;
	htc_batt_info.rep.level = 33;
	htc_batt_info.rep.level_raw = 33;
	htc_batt_info.rep.full_level = 100;
	htc_batt_info.rep.status = POWER_SUPPLY_STATUS_UNKNOWN;
	htc_batt_info.rep.full_level_dis_batt_chg = 100;
	htc_batt_info.rep.overload = 0;
	htc_batt_info.rep.over_vchg = 0;
	htc_batt_info.rep.is_full = false;
	htc_batt_info.rep.health = POWER_SUPPLY_HEALTH_UNKNOWN;
	htc_batt_info.smooth_chg_full_delay_min = 3;
	htc_batt_info.decreased_batt_level_check = 1;
	htc_batt_info.critical_low_voltage_mv = 3200;
	htc_batt_info.batt_full_voltage_mv = 4350;
	htc_batt_info.batt_full_current_ma = LEVEL100_THRESHOLD;
	htc_batt_info.overload_curr_thr_ma = 0;
#ifdef CONFIG_HTC_BATT_PCN0002
	htc_batt_info.store.batt_stored_magic_num = 0;
	htc_batt_info.store.batt_stored_soc = 0;
	htc_batt_info.store.batt_stored_temperature = 0;
	htc_batt_info.store.batt_stored_update_time = 0;
	htc_batt_info.store.consistent_flag = false;
#endif 
	htc_batt_info.vbus = 0;
	htc_batt_info.current_limit_reason = 0;

#if defined(CONFIG_HTC_BATT_PCN0002)||defined(CONFIG_HTC_BATT_PCN0008)
	node = of_find_compatible_node(NULL, NULL, "htc,htc_battery_store");
	if (node) {
		if (!of_device_is_available(node)) {
			pr_err("%s: unavailable node.\n", __func__);
		} else {
#ifdef CONFIG_HTC_BATT_PCN0002
			
			ret = of_property_read_u32(node, "stored-batt-magic-num", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-magic-num.\n", __func__);
			else
				htc_batt_info.store.batt_stored_magic_num = val;
			
			ret = of_property_read_u32(node, "stored-batt-soc", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-soc.\n", __func__);
			else
				htc_batt_info.store.batt_stored_soc = val;
			
			ret = of_property_read_u32(node, "stored-batt-temperature", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-temperature.\n", __func__);
			else
				htc_batt_info.store.batt_stored_temperature = val;
			
			ret = of_property_read_u32(node, "stored-batt-update-time", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-update-time.\n", __func__);
			else
				htc_batt_info.store.batt_stored_update_time = val;
#endif 
#ifdef CONFIG_HTC_BATT_PCN0008
			
			ret = of_property_read_u32(node, "stored-batt-total-level", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-total-level.\n", __func__);
			else
				g_total_level_raw = val;
			
			ret = of_property_read_u32(node, "stored-batt-overheat-sec", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-overheat-sec.\n", __func__);
			else
				g_overheat_55_sec = val;
			
			ret = of_property_read_u32(node, "stored-batt-first-use", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-first-use.\n", __func__);
			else
				g_batt_first_use_time = val;
			
			ret = of_property_read_u32(node, "stored-batt-checksum", &val);
			if (ret)
				pr_err("%s: error reading stored-batt-checksum.\n", __func__);
			else
				g_batt_cycle_checksum = val;
#endif 
		}
	} else {
		pr_err("%s: can't find compatible 'htc,htc_battery_store'\n", __func__);
	}
#endif

	platform_device_register(&htc_battery_pdev);
	platform_driver_register(&htc_battery_driver);

	
	if (g_flag_ats_limit_chg)
		htc_batt_info.rep.full_level = 50;

	BATT_LOG("htc_battery_init done.\n");

	return 0;
}

module_init(htc_battery_init);
