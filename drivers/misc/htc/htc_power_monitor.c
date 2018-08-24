#include <linux/rtc.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kernel_stat.h>
#include <linux/workqueue.h>
#include "../mediatek/include/mt-plat/mtk_thermal_monitor.h"
#include <linux/stacktrace.h>
#include <linux/slab.h>
#include <linux/pm_wakeup.h>

#define MAX_PID				32768
#define POWER_PROFILE_POLLING_TIME	60000 
#define THERMAL_LOG_SHOWN_INTERVAL	60000 
#define NUM_BUSY_PROCESS_CHECK		5

#ifdef CONFIG_VM_EVENT_COUNTERS
#include <linux/mm.h>
#include <linux/vmstat.h>
unsigned long prev_vm_event[NR_VM_EVENT_ITEMS];
#ifdef CONFIG_ZONE_DMA
#define TEXT_FOR_DMA(xx) xx "_dma",
#else
#define TEXT_FOR_DMA(xx)
#endif

#ifdef CONFIG_ZONE_DMA32
#define TEXT_FOR_DMA32(xx) xx "_dma32",
#else
#define TEXT_FOR_DMA32(xx)
#endif

#ifdef CONFIG_HIGHMEM
#define TEXT_FOR_HIGHMEM(xx) xx "_high",
#else
#define TEXT_FOR_HIGHMEM(xx)
#endif
#define TEXTS_FOR_ZONES(xx) TEXT_FOR_DMA(xx) TEXT_FOR_DMA32(xx) xx "_normal", \
					TEXT_FOR_HIGHMEM(xx) xx "_movable",
const char * const vm_event_text[] = {
	"pgpgin",
	"pgpgout",
	"pswpin",
	"pswpout",

	TEXTS_FOR_ZONES("pgalloc")

	"pgfree",
	"pgactivate",
	"pgdeactivate",

	"pgfault",
	"pgmajfault",
	"pgfmfault",

	TEXTS_FOR_ZONES("pgrefill")
	TEXTS_FOR_ZONES("pgsteal_kswapd")
	TEXTS_FOR_ZONES("pgsteal_direct")
	TEXTS_FOR_ZONES("pgscan_kswapd")
	TEXTS_FOR_ZONES("pgscan_direct")
	"pgscan_direct_throttle",

#ifdef CONFIG_NUMA
	"zone_reclaim_failed",
#endif
	"pginodesteal",
	"slabs_scanned",
	"kswapd_inodesteal",
	"kswapd_low_wmark_hit_quickly",
	"kswapd_high_wmark_hit_quickly",
	"pageoutrun",
	"allocstall",

	"pgrotated",
	"drop_pagecache",
	"drop_slab",

#ifdef CONFIG_NUMA_BALANCING
	"numa_pte_updates",
	"numa_huge_pte_updates",
	"numa_hint_faults",
	"numa_hint_faults_local",
	"numa_pages_migrated",
#endif
#ifdef CONFIG_MIGRATION
	"pgmigrate_success",
	"pgmigrate_fail",
#endif
#ifdef CONFIG_COMPACTION
	"compact_migrate_scanned",
	"compact_free_scanned",
	"compact_isolated",
	"compact_stall",
	"compact_fail",
	"compact_success",
#endif

#ifdef CONFIG_HUGETLB_PAGE
	"htlb_buddy_alloc_success",
	"htlb_buddy_alloc_fail",
#endif
	"unevictable_pgs_culled",
	"unevictable_pgs_scanned",
	"unevictable_pgs_rescued",
	"unevictable_pgs_mlocked",
	"unevictable_pgs_munlocked",
	"unevictable_pgs_cleared",
	"unevictable_pgs_stranded",

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	"thp_fault_alloc",
	"thp_fault_fallback",
	"thp_collapse_alloc",
	"thp_collapse_alloc_failed",
	"thp_split",
	"thp_zero_page_alloc",
	"thp_zero_page_alloc_failed",
#endif

#ifdef CONFIG_MEMORY_BALLOON
	"balloon_inflate",
	"balloon_deflate",
#ifdef CONFIG_BALLOON_COMPACTION
	"balloon_migrate",
#endif
#endif
#ifdef CONFIG_DEBUG_TLBFLUSH
#ifdef CONFIG_SMP
	"nr_tlb_remote_flush",    
	"nr_tlb_remote_flush_received",
#endif 
	"nr_tlb_local_flash_all",
	"nr_tlb_local_flush_one",
#endif 
#ifdef CONFIG_DEBUG_VM_VMACACHE
	"vmcache_find_calls",
	"vmcache_find_hits",
#endif
};
#endif


struct _htc_kernel_top {
	struct delayed_work dwork;
	int *curr_proc_delta;
        int *curr_proc_delta_kernel;
	int *curr_proc_pid;
	unsigned int *prev_proc_stat;
        unsigned int *prev_proc_stat_kernel;
	struct task_struct **proc_ptr_array;
	struct kernel_cpustat curr_cpustat;
	struct kernel_cpustat prev_cpustat;
	unsigned long cpustat_time;
	int top_loading_pid[NUM_BUSY_PROCESS_CHECK];
	unsigned long curr_cpu_usage[8][NR_STATS];
	unsigned long prev_cpu_usage[8][NR_STATS];
	spinlock_t lock;
};
struct _htc_kernel_top *htc_kernel_top;

static struct workqueue_struct *htc_pm_monitor_wq = NULL;

static const char * const thermal_dev_name[MTK_THERMAL_SENSOR_COUNT] = {
	[MTK_THERMAL_SENSOR_CPU]	= "cpu",
	[MTK_THERMAL_SENSOR_ABB]	= "abb",
	[MTK_THERMAL_SENSOR_PMIC]	= "pmic",
	[MTK_THERMAL_SENSOR_BATTERY]	= "batt",
	[MTK_THERMAL_SENSOR_MD1]	= "md1",
	[MTK_THERMAL_SENSOR_MD2]	= "md2",
	[MTK_THERMAL_SENSOR_WIFI]	= "wifi",
	[MTK_THERMAL_SENSOR_BATTERY2]	= "batt2",
	[MTK_THERMAL_SENSOR_BUCK]	= "buck",
	[MTK_THERMAL_SENSOR_AP]		= "ap",
	[MTK_THERMAL_SENSOR_PCB1]	= "pcb1",
	[MTK_THERMAL_SENSOR_PCB2]	= "pcb2",
	[MTK_THERMAL_SENSOR_SKIN]	= "skin",
	[MTK_THERMAL_SENSOR_XTAL]	= "xtal",
	[MTK_THERMAL_SENSOR_MD_PA]	= "md_pa"
};

static int son = 0;

extern void htc_show_wakeup_sources(void);
extern int aee_rr_last_fiq_step(void);
extern void htc_show_interrupts(void);
static void htc_show_thermal_temp(void)
{
	int i = 0, temp = 0;
	static int count = 0;

	if (++count >= (THERMAL_LOG_SHOWN_INTERVAL / POWER_PROFILE_POLLING_TIME)) {
		for (i = 0; i < MTK_THERMAL_SENSOR_COUNT; i++) {
			temp = mtk_thermal_get_temp(i);
			if (temp == -127000)
				continue;
			printk("[K][THERMAL] Sensor %2d (%5s) = %d\n", i, thermal_dev_name[i], temp);
		}
		count = 0;
	}

	return;
}

static void sort_cputime_by_pid(int *src, int *pid_pos, int pid_cnt, int *result)
{
    int i = 0, j = 0, k = 0, l = 0;
    int pid_found = 0;

    for (i = 0; i < NUM_BUSY_PROCESS_CHECK; i++) {
        result[i] = 0;
        if (i == 0) {
            for (j = 0; j < pid_cnt; j++) {
                k = pid_pos[j];
                
                if(src[result[i]] < src[k]) {
                    result[i] = k;
                }
            }
        } else {
            for (j = 0; j < pid_cnt; j++) {
                k = pid_pos[j];
                
                for (l = 0; l < i; l++) {
                    
                    if (result[l] == k) {
                        pid_found = 1;
                        break;
                    }
                }
                
                if (pid_found) {
                    pid_found = 0;
                    continue;
                }

                
                if(src[result[i]] < src[k]) {
                    result[i] = k;
                }
            }
        }
    }
	return;
}

#ifdef arch_idle_time
static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);

	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);

	return iowait;
}
#else
static u64 get_idle_time(int cpu)
{
	u64 idle = 0;
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait;
	u64 iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}
#endif

static void get_all_cpustat(struct _htc_kernel_top *ktop, struct kernel_cpustat *cpu_stat)
{
	int cpu = 0;

	if (!cpu_stat)
		return;

	memset(cpu_stat, 0, sizeof(struct kernel_cpustat));

	for_each_possible_cpu(cpu) {
		cpu_stat->cpustat[CPUTIME_USER] += kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
		cpu_stat->cpustat[CPUTIME_NICE] += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];
		cpu_stat->cpustat[CPUTIME_SYSTEM] += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
		cpu_stat->cpustat[CPUTIME_IDLE] += get_idle_time(cpu);
		cpu_stat->cpustat[CPUTIME_IOWAIT] += get_iowait_time(cpu);
		cpu_stat->cpustat[CPUTIME_IRQ] += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
		cpu_stat->cpustat[CPUTIME_SOFTIRQ] += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
		cpu_stat->cpustat[CPUTIME_STEAL] += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
		cpu_stat->cpustat[CPUTIME_GUEST] += kcpustat_cpu(cpu).cpustat[CPUTIME_GUEST];
		cpu_stat->cpustat[CPUTIME_GUEST_NICE] += kcpustat_cpu(cpu).cpustat[CPUTIME_GUEST_NICE];
		ktop->curr_cpu_usage[cpu][CPUTIME_USER] = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
		ktop->curr_cpu_usage[cpu][CPUTIME_NICE] = kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];
		ktop->curr_cpu_usage[cpu][CPUTIME_SYSTEM] = kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
		ktop->curr_cpu_usage[cpu][CPUTIME_IDLE] = get_idle_time(cpu);
	}

	return;
}

static unsigned long htc_calculate_cpustat_time(struct kernel_cpustat curr_cpustat,
						struct kernel_cpustat prev_cpustat)
{
	unsigned long user_time = 0, system_time = 0, io_time = 0;
	unsigned long irq_time = 0, idle_time = 0;

	user_time = (unsigned long) ((curr_cpustat.cpustat[CPUTIME_USER] +
					curr_cpustat.cpustat[CPUTIME_NICE]) -
					(prev_cpustat.cpustat[CPUTIME_USER] +
					prev_cpustat.cpustat[CPUTIME_NICE]));
	system_time = (unsigned long) (curr_cpustat.cpustat[CPUTIME_SYSTEM] -
					prev_cpustat.cpustat[CPUTIME_SYSTEM]);
	io_time = (unsigned long) (curr_cpustat.cpustat[CPUTIME_IOWAIT] -
					prev_cpustat.cpustat[CPUTIME_IOWAIT]);
	irq_time = (unsigned long) ((curr_cpustat.cpustat[CPUTIME_IRQ] +
					curr_cpustat.cpustat[CPUTIME_SOFTIRQ]) -
					(prev_cpustat.cpustat[CPUTIME_IRQ] +
					prev_cpustat.cpustat[CPUTIME_SOFTIRQ]));
	idle_time = (unsigned long) ((curr_cpustat.cpustat[CPUTIME_IDLE] > prev_cpustat.cpustat[CPUTIME_IDLE]) ?
					curr_cpustat.cpustat[CPUTIME_IDLE] - prev_cpustat.cpustat[CPUTIME_IDLE] : 0);

	idle_time += (unsigned long) ((curr_cpustat.cpustat[CPUTIME_STEAL] +
					curr_cpustat.cpustat[CPUTIME_GUEST]) -
					(prev_cpustat.cpustat[CPUTIME_STEAL] +
					prev_cpustat.cpustat[CPUTIME_GUEST]));

	return (user_time + system_time + io_time + irq_time + idle_time);
}

static void htc_calc_kernel_top(struct _htc_kernel_top *ktop)
{
	int pid_cnt = 0;
	ulong flags;
	struct task_struct *proc;
	struct task_cputime cputime;

	if(ktop->proc_ptr_array == NULL ||
	   ktop->curr_proc_delta == NULL ||
           ktop->curr_proc_delta_kernel == NULL ||
	   ktop->curr_proc_pid == NULL ||
	   ktop->prev_proc_stat == NULL ||
	   ktop->prev_proc_stat_kernel == NULL)
		return;

	spin_lock_irqsave(&ktop->lock, flags);
	
	for_each_process(proc) {
		if (proc->pid < MAX_PID) {
			thread_group_cputime(proc, &cputime);
			ktop->curr_proc_delta[proc->pid] =
				(cputime.utime + cputime.stime) -
				ktop->prev_proc_stat[proc->pid];
                        ktop->curr_proc_delta_kernel[proc->pid] =
                                cputime.stime -
                                ktop->prev_proc_stat_kernel[proc->pid];
			ktop->proc_ptr_array[proc->pid] = proc;

			if (ktop->curr_proc_delta[proc->pid] > 0) {
				ktop->curr_proc_pid[pid_cnt] = proc->pid;
				pid_cnt++;
			}
		}
	}
	sort_cputime_by_pid(ktop->curr_proc_delta, ktop->curr_proc_pid, pid_cnt, ktop->top_loading_pid);

	
	get_all_cpustat(ktop, &ktop->curr_cpustat);
	ktop->cpustat_time = htc_calculate_cpustat_time(ktop->curr_cpustat, ktop->prev_cpustat);

	
	for_each_process(proc) {
		if (proc->pid < MAX_PID) {
			thread_group_cputime(proc, &cputime);
			ktop->prev_proc_stat[proc->pid] = cputime.stime + cputime.utime;
			ktop->prev_proc_stat_kernel[proc->pid] = cputime.stime;
		}
	}
	memcpy(&ktop->prev_cpustat, &ktop->curr_cpustat, sizeof(struct kernel_cpustat));
	spin_unlock_irqrestore(&ktop->lock, flags);

	return;
}

#define MAX_STACK_DEPTH   64
static void htc_show_kernel_top(struct _htc_kernel_top *ktop)
{
	int top_n_pid = 0, i;
	int cpu = 0;
	unsigned long usage = 0, total = 0, idle_time = 0, proc_usage = 0;
	unsigned long usage_tmp[8];

	
	printk("[K]\tCPU Usage\t\tPID\t\tName\t\tCPU Time\n");
	for (i = 0; i < NUM_BUSY_PROCESS_CHECK; i++) {
		if (ktop->cpustat_time > 0) {
			top_n_pid = ktop->top_loading_pid[i];
			proc_usage = ktop->curr_proc_delta[top_n_pid] * 100 / ktop->cpustat_time;
			printk("[K]\t%8lu%%\t\t%d\t\t%s\t\t%d\n",
				proc_usage,
				top_n_pid,
				ktop->proc_ptr_array[top_n_pid]->comm,
				ktop->curr_proc_delta[top_n_pid]);
		}
	}
	
	for_each_possible_cpu(cpu) {
		usage = (ktop->curr_cpu_usage[cpu][CPUTIME_USER] -
			ktop->prev_cpu_usage[cpu][CPUTIME_USER]) +
			(ktop->curr_cpu_usage[cpu][CPUTIME_NICE] -
			ktop->prev_cpu_usage[cpu][CPUTIME_NICE]) +
			(ktop->curr_cpu_usage[cpu][CPUTIME_SYSTEM] -
			ktop->prev_cpu_usage[cpu][CPUTIME_SYSTEM]);
		idle_time = (ktop->curr_cpu_usage[cpu][CPUTIME_IDLE] > ktop->prev_cpu_usage[cpu][CPUTIME_IDLE]) ?
			     ktop->curr_cpu_usage[cpu][CPUTIME_IDLE] - ktop->prev_cpu_usage[cpu][CPUTIME_IDLE] : 0;
		total = usage + idle_time;
		usage_tmp[cpu] =  usage *100 / total;
		ktop->prev_cpu_usage[cpu][CPUTIME_USER] = ktop->curr_cpu_usage[cpu][CPUTIME_USER];
		ktop->prev_cpu_usage[cpu][CPUTIME_NICE] = ktop->curr_cpu_usage[cpu][CPUTIME_NICE];
		ktop->prev_cpu_usage[cpu][CPUTIME_SYSTEM] = ktop->curr_cpu_usage[cpu][CPUTIME_SYSTEM];
		ktop->prev_cpu_usage[cpu][CPUTIME_IDLE] = ktop->curr_cpu_usage[cpu][CPUTIME_IDLE];
	}
	printk("[K] CPU usage per core: [%lu] [%lu] [%lu] [%lu] [%lu] [%lu] [%lu] [%lu]\n",\
		usage_tmp[0], usage_tmp[1], usage_tmp[2], usage_tmp[3],\
		usage_tmp[4], usage_tmp[5], usage_tmp[6], usage_tmp[7]);

	memset(ktop->curr_proc_delta, 0, sizeof(int) * MAX_PID);
	memset(ktop->proc_ptr_array, 0, sizeof(struct task_struct *) * MAX_PID);
	memset(ktop->curr_proc_pid, 0, sizeof(int) * MAX_PID);

	return;
}

extern char* saved_command_line;
static int is_son(void)
{
	char *p;
	int td_sf = 0;
	size_t sf_len = strlen("td.sf=");
	size_t cmdline_len = strlen(saved_command_line);
	p = saved_command_line;
	for (p = saved_command_line; p < saved_command_line + cmdline_len - sf_len; p++) {
		if (!strncmp(p, "td.sf=", sf_len)) {
			p += sf_len;
			if (*p != '0')
				td_sf = 1;
			break;
		}
	}
	return td_sf;
}

static unsigned int pm_monitor_en = POWER_PROFILE_POLLING_TIME;
static void htc_pm_monitor_work_func(struct work_struct *work)
{
	struct _htc_kernel_top *ktop = container_of(work, struct _htc_kernel_top,
					dwork.work);
	struct timespec ts;
	struct rtc_time tm;

	unsigned long vm_event[NR_VM_EVENT_ITEMS];
	int i, size;

	if(pm_monitor_en == 0)
		return;
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec - (sys_tz.tz_minuteswest * 60), &tm);
	printk("[K][PM] hTC PM Statistic start (%02d-%02d %02d:%02d:%02d)\n",
		tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	
	clk_is_on();
	subsystem_is_on();

	
	htc_show_interrupts();
	
	htc_show_wakeup_sources();
	htc_print_active_wakeup_sources();
	
	htc_show_thermal_temp();
	
	htc_calc_kernel_top(ktop);
	htc_show_kernel_top(ktop);
	

	if(!son){
		all_vm_events(vm_event);
		vm_event[PGPGIN] /= 2;
		
		vm_event[PGPGOUT] /= 2;
		size = sizeof(vm_event_text)/sizeof(vm_event_text[0]);
		if(size != NR_VM_EVENT_ITEMS){
			printk("[K][PM] hTC please help to check if vm_event_text size %d can match with NR_VM_EVENT_ITEMS %d\n", size, NR_VM_EVENT_ITEMS);
		}else{
			for(i = 0; i < NR_VM_EVENT_ITEMS; i++) {
			if (vm_event[i] - prev_vm_event[i] > 0)
				pr_info("[K] %s = %lu\n", vm_event_text[i], vm_event[i] - prev_vm_event[i]);
			}
		}
		memcpy(prev_vm_event, vm_event, sizeof(unsigned long) * NR_VM_EVENT_ITEMS);
	}

	printk("[K][PM] hTC PM Statistic done\n");
	queue_delayed_work(htc_pm_monitor_wq, &ktop->dwork, msecs_to_jiffies(pm_monitor_en));
}


static int monitor_thread_get(void *data, u64 *val)
{
	*val = *(u64 *)(&pm_monitor_en);
	return 0;
}

static int monitor_thread_set(void *data, u64 val)
{
	if(val != 0){
		pm_monitor_en = (unsigned int)(val*1000);
		queue_delayed_work(htc_pm_monitor_wq, &htc_kernel_top->dwork,\
			msecs_to_jiffies(pm_monitor_en));
	}
	else
		pm_monitor_en = 0;

	printk("%s\n", (val == 0)? "disable pm_monitor thread" : " enable pm_monitor thread");
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(monitor_thread_fops, monitor_thread_get, monitor_thread_set, "%lld\n");
extern void htc_get_wakeup_source_cnt(u32 index, char *buf);
static int wakeup_source_show(struct seq_file *m, void *unused)
{
	unsigned int i;
	unsigned char buf[256] = {0};

	seq_printf(m, "index\t   sleep\tdeepidle\t soidle3\t  soidle\t    name\n");
	for(i=0; i<33; i++) {
		htc_get_wakeup_source_cnt(i, buf);
		seq_printf(m, "%s\n", buf);
	}
	return 0;
}

static int wakeup_source_cnt_open(struct inode *inode, struct file *file)
{
        return single_open(file, wakeup_source_show, inode->i_private);
}

static const struct file_operations wakeup_source_cnt_fops = {
        .open           = wakeup_source_cnt_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static int __init init_pm_debugfs(void)
{
	static struct dentry *pm_dbgfs_base;
	pm_dbgfs_base = debugfs_create_dir("htc_pm", NULL);
	if (!pm_dbgfs_base)
		return -ENOMEM;

	if (!debugfs_create_file("power_profile_time", S_IRUGO, pm_dbgfs_base, NULL, &monitor_thread_fops))
		return -ENOMEM;

	if (!debugfs_create_file("wakeup_info", S_IRUGO, pm_dbgfs_base, NULL, &wakeup_source_cnt_fops))
		return -ENOMEM;

	return 0;
}

static int __init pm_monitor_thread(void)
{
	int cpu;
	if (htc_pm_monitor_wq == NULL)
		
		htc_pm_monitor_wq = create_workqueue("htc_pm_monitor_wq");

	if (!htc_pm_monitor_wq) {
		pr_err("[K] Fail to create htc_pm_monitor_wq\n");
		return -1;
	}

	printk("[K] Success to create htc_pm_monitor_wq.\n");
	htc_kernel_top = vmalloc(sizeof(*htc_kernel_top));
	spin_lock_init(&htc_kernel_top->lock);

	htc_kernel_top->prev_proc_stat = vmalloc(sizeof(unsigned int) * MAX_PID);
        htc_kernel_top->prev_proc_stat_kernel = vmalloc(sizeof(unsigned int) * MAX_PID);
	htc_kernel_top->curr_proc_delta = vmalloc(sizeof(int) * MAX_PID);
        htc_kernel_top->curr_proc_delta_kernel = vmalloc(sizeof(int) * MAX_PID);
	htc_kernel_top->proc_ptr_array = vmalloc(sizeof(struct task_struct *) * MAX_PID);
	htc_kernel_top->curr_proc_pid = vmalloc(sizeof(int) * MAX_PID);

	memset(htc_kernel_top->prev_proc_stat, 0, sizeof(unsigned int) * MAX_PID);
        memset(htc_kernel_top->prev_proc_stat_kernel, 0, sizeof(unsigned int) * MAX_PID);
	memset(htc_kernel_top->curr_proc_delta, 0, sizeof(int) * MAX_PID);
        memset(htc_kernel_top->curr_proc_delta_kernel, 0, sizeof(int) * MAX_PID);
	memset(htc_kernel_top->proc_ptr_array, 0, sizeof(struct task_struct *) * MAX_PID);
	memset(htc_kernel_top->curr_proc_pid, 0, sizeof(int) * MAX_PID);

	for_each_possible_cpu(cpu) {
		memset(htc_kernel_top->curr_cpu_usage[cpu], 0, NR_STATS);
		memset(htc_kernel_top->prev_cpu_usage[cpu], 0, NR_STATS);
	}

	INIT_DELAYED_WORK(&htc_kernel_top->dwork, htc_pm_monitor_work_func);
	queue_delayed_work(htc_pm_monitor_wq, &htc_kernel_top->dwork, msecs_to_jiffies(POWER_PROFILE_POLLING_TIME));
	return 0;
}

static int __init htc_monitor_init(void)
{
	son = is_son();

	pm_monitor_thread();

	init_pm_debugfs();

	return 0;
}

static void __exit htc_monitor_exit(void)
{
	return;
}

module_init(htc_monitor_init);
module_exit(htc_monitor_exit);

MODULE_DESCRIPTION("HTC Power Utility Profile driver");
MODULE_AUTHOR("Kenny Liu <kenny_liu@htc.com>");
MODULE_LICENSE("GPL");
