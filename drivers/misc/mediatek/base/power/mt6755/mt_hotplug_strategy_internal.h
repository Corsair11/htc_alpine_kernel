
#ifndef __MT_HOTPLUG_STRATEGY_INTERNAL_H__
#define __MT_HOTPLUG_STRATEGY_INTERNAL_H__


#include <linux/kernel.h>
#include <linux/io.h>		
#include <linux/platform_device.h>	
#include <linux/kthread.h>	
#include <linux/timer.h>	
#include <linux/sched/rt.h>	


#include <mach/mt_ppm_api.h>

#define HPS_TASK_PRIORITY                   (MAX_RT_PRIO - 3)
#define HPS_TIMER_INTERVAL_MS               (40)

#define HPS_PERIODICAL_BY_WAIT_QUEUE        (1)
#define HPS_PERIODICAL_BY_TIMER             (2)
#define HPS_PERIODICAL_BY_HR_TIMER          (3)

#define MAX_CPU_UP_TIMES                    (10)
#define MAX_CPU_DOWN_TIMES                  (100)
#define MAX_TLP_TIMES                       (10)
#define CPU_DMIPS_BIG_LITTLE_DIFF           (70)

#define DEF_CPU_UP_THRESHOLD                (95)
#define DEF_CPU_UP_TIMES                    (3)
#define DEF_CPU_DOWN_THRESHOLD              (85)
#define DEF_CPU_DOWN_TIMES                  (1)
#define DEF_TLP_TIMES                       (1)

#define EN_CPU_INPUT_BOOST                  (1)
#define DEF_CPU_INPUT_BOOST_CPU_NUM         (2)

#define EN_CPU_RUSH_BOOST                   (1)
#define DEF_CPU_RUSH_BOOST_THRESHOLD        (98)
#define DEF_CPU_RUSH_BOOST_TIMES            (1)

#define EN_HPS_LOG                          (1)
#define EN_ISR_LOG                          (0)

#define hps_emerg(fmt, args...)             pr_emerg("[HPS] " fmt, ##args)
#define hps_alert(fmt, args...)             pr_alert("[HPS] " fmt, ##args)
#define hps_crit(fmt, args...)              pr_crit("[HPS] " fmt, ##args)
#define hps_error(fmt, args...)             pr_err("[HPS] " fmt, ##args)
#define hps_warn(fmt, args...)              pr_debug("[HPS] " fmt, ##args)
#define hps_notice(fmt, args...)            pr_notice("[HPS] " fmt, ##args)
#define hps_info(fmt, args...)              pr_info("[HPS] " fmt, ##args)
#define hps_debug(fmt, args...)             pr_debug("[HPS] " fmt, ##args)

#if EN_ISR_LOG
#define hps_isr_info(fmt, args...)          hps_notice(fmt, ##args)
#else
#define hps_isr_info(fmt, args...)          hps_debug(fmt, ##args)
#endif

#define hps_read(addr)                      __raw_readl(IOMEM(addr))
#define hps_write(addr, val)                mt_reg_sync_writel(val, addr)



typedef enum {
	INIT_STATE_NOT_READY = 0,
	INIT_STATE_DONE
} hps_init_state_e;

typedef enum {
	STATE_LATE_RESUME = 0,
	STATE_EARLY_SUSPEND,
	STATE_SUSPEND,
	STATE_COUNT
} hps_ctxt_state_e;


typedef enum {
	ACTION_NONE = 0,
	ACTION_BASE_LITTLE,	
	ACTION_BASE_BIG,	
	ACTION_LIMIT_LITTLE,	
	ACTION_LIMIT_BIG,	
	ACTION_RUSH_BOOST_LITTLE,	
	ACTION_RUSH_BOOST_BIG,	
	ACTION_UP_LITTLE,	
	ACTION_UP_BIG,		
	ACTION_DOWN_LITTLE,	
	ACTION_DOWN_BIG,	
	ACTION_BIG_TO_LITTLE,	
	ACTION_INPUT,		
	ACTION_ROOT_2_LITTLE,	
	ACTION_ROOT_2_BIG,	
	ACTION_COUNT
} hps_ctxt_action_e;
typedef struct hps_func_data {
	int cores;
	unsigned int target_root_cpu;
	unsigned int action_LL;
	unsigned int action_L;
	unsigned int target_LL;
	unsigned int target_L;
	unsigned int limit_LL;
	unsigned int limit_L;
	unsigned int base_LL;
	unsigned int base_L;
} hps_func_data_t;
typedef struct hps_ctxt_struct {
	
	unsigned int init_state;
	unsigned int state;

	
	unsigned int enabled;
	unsigned int early_suspend_enabled;
	
	unsigned int suspend_enabled;	
	unsigned int cur_dump_enabled;
	unsigned int stats_dump_enabled;
	unsigned int power_mode;
	unsigned int ppm_power_mode;


	
	struct mutex lock;	
	struct task_struct *tsk_struct_ptr;
	wait_queue_head_t wait_queue;
	struct timer_list tmr_list;
	unsigned int periodical_by;
	struct hrtimer hr_timer;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es_handler;
#endif				
	struct platform_driver pdrv;

	
	unsigned int enabled_backup;
	unsigned int rush_boost_enabled_backup;

	
	unsigned int is_hmp;
	unsigned int is_amp;
	struct cpumask little_cpumask;
	struct cpumask big_cpumask;
	unsigned int little_cpu_id_min;
	unsigned int little_cpu_id_max;
	unsigned int big_cpu_id_min;
	unsigned int big_cpu_id_max;

	
	unsigned int up_threshold;
	unsigned int up_times;
	unsigned int down_threshold;
	unsigned int down_times;
	unsigned int input_boost_enabled;
	unsigned int input_boost_cpu_num;
	unsigned int rush_boost_enabled;
	unsigned int rush_boost_threshold;
	unsigned int rush_boost_times;
	unsigned int tlp_times;

	
	unsigned int little_num_base_perf_serv;
	unsigned int little_num_limit_thermal;
	unsigned int little_num_limit_low_battery;
	unsigned int little_num_limit_ultra_power_saving;
	unsigned int little_num_limit_power_serv;
	unsigned int big_num_base_perf_serv;
	unsigned int big_num_limit_thermal;
	unsigned int big_num_limit_low_battery;
	unsigned int big_num_limit_ultra_power_saving;
	unsigned int big_num_limit_power_serv;

#ifdef CONFIG_HTC_PNPMGR
	unsigned int little_num_base_pnpmgr;
	unsigned int little_num_limit_pnpmgr;
	unsigned int big_num_base_pnpmgr;
	unsigned int big_num_limit_pnpmgr;
#endif
	
	unsigned int cur_loads;
	unsigned int cur_tlp;
	unsigned int cur_iowait;
	unsigned int cur_nr_heavy_task;
	unsigned int up_loads_sum;
	unsigned int up_loads_count;
	unsigned int up_loads_history[MAX_CPU_UP_TIMES];
	unsigned int up_loads_history_index;
	unsigned int down_loads_sum;
	unsigned int down_loads_count;
	unsigned int down_loads_history[MAX_CPU_DOWN_TIMES];
	unsigned int down_loads_history_index;
	unsigned int rush_count;
	unsigned int tlp_sum;
	unsigned int tlp_count;
	unsigned int tlp_history[MAX_TLP_TIMES];
	unsigned int tlp_history_index;
	unsigned int tlp_avg;

	
	unsigned int wake_up_by_fasthotplug;
	unsigned int root_cpu;

	
	unsigned long action;
	atomic_t is_ondemand;

	
	unsigned int test0;
	unsigned int test1;
} hps_ctxt_t;

typedef struct hps_cpu_ctxt_struct {
	unsigned int load;
} hps_cpu_ctxt_t;

extern hps_ctxt_t hps_ctxt;
DECLARE_PER_CPU(hps_cpu_ctxt_t, hps_percpu_ctxt);
extern struct cpumask cpu_domain_big_mask;	
extern struct cpumask cpu_domain_little_mask;	

extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);
	

extern void hps_ctxt_reset_stas_nolock(void);
extern void hps_ctxt_reset_stas(void);
extern void hps_ctxt_print_basic(int toUart);
extern void hps_ctxt_print_algo_config(int toUart);
extern void hps_ctxt_print_algo_bound(int toUart);
extern void hps_ctxt_print_algo_stats_cur(int toUart);
extern void hps_ctxt_print_algo_stats_up(int toUart);
extern void hps_ctxt_print_algo_stats_down(int toUart);
extern void hps_ctxt_print_algo_stats_tlp(int toUart);
extern int hps_restart_timer(void);
extern int hps_del_timer(void);
extern int hps_core_deinit(void);
extern int hps_core_init(void);
extern int hps_core_deinit(void);
extern int hps_task_start(void);
extern void hps_task_stop(void);
extern void hps_task_wakeup_nolock(void);
extern void hps_task_wakeup(void);

extern void hps_algo_hmp(void);
extern void hps_algo_smp(void);
extern void hps_algo_amp(void);
extern int hps_procfs_init(void);

#define num_possible_little_cpus()          cpumask_weight(&hps_ctxt.little_cpumask)
#define num_possible_big_cpus()             cpumask_weight(&hps_ctxt.big_cpumask)

extern int hps_cpu_init(void);
extern int hps_cpu_deinit(void);
extern unsigned int num_online_little_cpus(void);
extern unsigned int num_online_big_cpus(void);
extern int hps_cpu_is_cpu_big(int cpu);
extern int hps_cpu_is_cpu_little(int cpu);
extern unsigned int hps_cpu_get_percpu_load(int cpu);
extern unsigned int hps_cpu_get_nr_heavy_task(void);
extern void hps_cpu_get_tlp(unsigned int *avg, unsigned int *iowait_avg);

extern struct cpumask cpu_domain_big_mask;
extern struct cpumask cpu_domain_little_mask;
extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);

extern unsigned int sched_get_percpu_load(int cpu, bool reset, bool use_maxfreq);
extern unsigned int sched_get_nr_heavy_task(void);

#endif				
