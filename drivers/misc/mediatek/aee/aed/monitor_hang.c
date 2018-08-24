/*
 * (C) Copyright 2010
 * MediaTek <www.MediaTek.com>
 *
 * Android Exception Device
 *
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <disp_assert_layer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <mt-plat/aee.h>
#include <linux/seq_file.h>
#include "aed.h"
#include <linux/pid.h>
#include <mt-plat/mt_boot_common.h>

static DEFINE_SPINLOCK(pwk_hang_lock);
static int wdt_kick_status;
static int hwt_kick_times;
static int pwk_start_monitor;

#define AEEIOCTL_RT_MON_Kick _IOR('p', 0x0A, int)

static long monitor_hang_ioctl(struct file *file, unsigned int cmd, unsigned long arg);



static int monitor_hang_open(struct inode *inode, struct file *filp)
{
	
	
	return 0;
}

static int monitor_hang_release(struct inode *inode, struct file *filp)
{
	
	return 0;
}

static unsigned int monitor_hang_poll(struct file *file, struct poll_table_struct *ptable)
{
	
	return 0;
}

static ssize_t monitor_hang_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	
	return 0;
}

static ssize_t monitor_hang_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{

	
	return 0;
}






static long monitor_hang_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	static long long monitor_status;

	if (cmd == AEEIOCTL_WDT_KICK_POWERKEY) {
		if ((int)arg == WDT_SETBY_WMS_DISABLE_PWK_MONITOR) {
			
			
			
		} else if ((int)arg == WDT_SETBY_WMS_ENABLE_PWK_MONITOR) {
			
			
			
		} else if ((int)arg < 0xf) {
			aee_kernel_wdt_kick_Powkey_api("Powerkey ioctl", (int)arg);
		}
		return ret;

	}
	
	if (cmd == AEEIOCTL_RT_MON_Kick) {
		pr_debug("AEEIOCTL_RT_MON_Kick ( %d)\n", (int)arg);
		aee_kernel_RT_Monitor_api((int)arg);
		return ret;
	}
	
	

	if (cmd == AEEIOCTL_SET_SF_STATE) {
		if (copy_from_user(&monitor_status, (void __user *)arg, sizeof(long long)))
			ret = -1;
		pr_debug("AEE_MONITOR_SET[status]: 0x%llx", monitor_status);
		return ret;
	} else if (cmd == AEEIOCTL_GET_SF_STATE) {
		if (copy_to_user((void __user *)arg, &monitor_status, sizeof(long long)))
			ret = -1;
		return ret;
	}

	return -1;
}




static const struct file_operations aed_wdt_RT_Monitor_fops = {
	.owner = THIS_MODULE,
	.open = monitor_hang_open,
	.release = monitor_hang_release,
	.poll = monitor_hang_poll,
	.read = monitor_hang_read,
	.write = monitor_hang_write,
	.unlocked_ioctl = monitor_hang_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = monitor_hang_ioctl,
#endif
};


static struct miscdevice aed_wdt_RT_Monitor_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "RT_Monitor",
	.fops = &aed_wdt_RT_Monitor_fops,
};



static int monitor_hang_init(void);

static int hang_detect_init(void);



static int __init monitor_hang_init(void)
{
	int err = 0;

	
	err = misc_register(&aed_wdt_RT_Monitor_dev);
	if (unlikely(err)) {
		LOGE("aee: failed to register aed_wdt_RT_Monitor_dev device!\n");
		return err;
	}
	hang_detect_init();
	
	

	return err;
}

static void __exit monitor_hang_exit(void)
{
	int err;

	err = misc_deregister(&aed_wdt_RT_Monitor_dev);
	if (unlikely(err))
		LOGE("failed to unregister RT_Monitor device!\n");
}





#define HD_PROC "hang_detect"

#define HD_INTER 30

static int hd_detect_enabled;
static int hd_timeout = 0x7fffffff;
static int hang_detect_counter = 0x7fffffff;
int InDumpAllStack = 0;
static int system_server_pid;
static int surfaceflinger_pid;
static int system_ui_pid;
static int init_pid;
static int mmcqd0;
static int mmcqd1;
static int debuggerd;
static int debuggerd64;

static int FindTaskByName(char *name)
{
	struct task_struct *task;
	int ret = -1;

	system_server_pid = 0;
	surfaceflinger_pid = 0;
	system_ui_pid = 0;
	init_pid = 0;
	mmcqd0 = 0;
	mmcqd1 = 0;
	debuggerd = 0;
	debuggerd64 = 0;

	read_lock(&tasklist_lock);
	for_each_process(task) {
		if (task && (strcmp(task->comm, "init") == 0)) {
			init_pid = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
		} else if (task && (strcmp(task->comm, "mmcqd/0") == 0)) {
			mmcqd0 = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
		} else if (task && (strcmp(task->comm, "mmcqd/1") == 0)) {
			mmcqd1 = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
		} else if (task && (strcmp(task->comm, "surfaceflinger") == 0)) {
			surfaceflinger_pid = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
		} else if (task && (strcmp(task->comm, "debuggerd") == 0)) {
			debuggerd = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
		} else if (task && (strcmp(task->comm, "debuggerd64") == 0)) {
			debuggerd64 = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
		} else if (task && (strcmp(task->comm, name) == 0)) {
			system_server_pid = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
			
		} else if (task && (strstr(task->comm, "systemui"))) {
			system_ui_pid = task->pid;
			pr_debug("[Hang_Detect] %s found pid:%d.\n", task->comm, task->pid);
			
		}
	 }
	read_unlock(&tasklist_lock);
	if (system_server_pid)
		ret = system_server_pid;
	else {
		LOGE("[Hang_Detect] system_server not found!\n");
		ret = -1;
	}
	return ret;
}

void sched_show_task_local(struct task_struct *p)
{
	unsigned long free = 0;
	int ppid;
	unsigned state;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	state = p->state ? __ffs(p->state) + 1 : 0;
	LOGE("%-15.15s %c", p->comm, state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
#if BITS_PER_LONG == 32
	if (state == TASK_RUNNING)
		LOGE(" running  ");
	else
		LOGE(" %08lx ", thread_saved_pc(p));
#else
	if (state == TASK_RUNNING)
		LOGE("  running task    ");
	else
		LOGE(" %016lx ", thread_saved_pc(p));
#endif
#ifdef CONFIG_DEBUG_STACK_USAGE
	free = stack_not_used(p);
#endif
	rcu_read_lock();
	ppid = task_pid_nr(rcu_dereference(p->real_parent));
	rcu_read_unlock();
	LOGE("%5lu %5d %6d 0x%08lx\n", free,
			task_pid_nr(p), ppid, (unsigned long)task_thread_info(p)->flags);

	print_worker_info("6", p);
	show_stack(p, NULL);
}

void show_state_filter_local(unsigned long state_filter)
{
	struct task_struct *g, *p;

#if BITS_PER_LONG == 32
	LOGE("  task                PC stack   pid father\n");
#else
	LOGE("  task                        PC stack   pid father\n");
#endif
	do_each_thread(g, p) {
		if ((!state_filter || (p->state & state_filter)) && !strstr(p->comm, "wdtk"))
			sched_show_task_local(p);
	} while_each_thread(g, p);
}

static void show_bt_by_pid(int task_pid)
{
	struct task_struct *t, *p;
	struct pid *pid;
	int count = 0;

	pid = find_get_pid(task_pid);
	t = p = get_pid_task(pid, PIDTYPE_PID);
	count = 0;
	if (NULL != p) {
		do {
			if (t)
				sched_show_task_local(t);
			if ((++count)%5 == 4)
				msleep(20);
		} while_each_thread(p, t);
		put_task_struct(t);
	}
	put_pid(pid);
}

static void ShowStatus(void)
{

	InDumpAllStack = 1;

	LOGE("[Hang_Detect] dump system_ui all thread bt\n");
	if (system_ui_pid)
		show_bt_by_pid(system_ui_pid);

	
	LOGE("[Hang_Detect] dump surfaceflinger all thread bt\n");
	if (surfaceflinger_pid)
		show_bt_by_pid(surfaceflinger_pid);

	
	LOGE("[Hang_Detect] dump system_server all thread bt\n");
	if (system_server_pid)
		show_bt_by_pid(system_server_pid);

	
	LOGE("[Hang_Detect] dump all D thread bt\n");
		show_state_filter_local(TASK_UNINTERRUPTIBLE);

	
	LOGE("[Hang_Detect] dump init all thread bt\n");
	if (init_pid)
		show_bt_by_pid(init_pid);

	
	LOGE("[Hang_Detect] dump mmcqd/0 all thread bt\n");
	if (mmcqd0)
		show_bt_by_pid(mmcqd0);
	
	LOGE("[Hang_Detect] dump mmcqd/1 all thread bt\n");
	if (mmcqd1)
		show_bt_by_pid(mmcqd1);

	LOGE("[Hang_Detect] dump debug_show_all_locks\n");
	debug_show_all_locks();
	LOGE("[Hang_Detect] show_free_areas\n");
	show_free_areas(0);
	system_server_pid = 0;
	surfaceflinger_pid = 0;
	system_ui_pid = 0;
	init_pid = 0;
	InDumpAllStack = 0;
	mmcqd0 = 0;
	mmcqd1 = 0;
	debuggerd = 0;
	debuggerd64 = 0;
}

static int hang_detect_thread(void *arg)
{

	
	struct sched_param param = {
		.sched_priority = 99 };

	LOGE("[Hang_Detect] hang_detect thread starts.\n");

	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		if ((1 == hd_detect_enabled) && (FindTaskByName("system_server") != -1)) {
			LOGE("[Hang_Detect] hang_detect thread counts down %d:%d.\n", hang_detect_counter, hd_timeout);

			if (hang_detect_counter <= 0)
				ShowStatus();

			if (hang_detect_counter == 0) {
				LOGE("[Hang_Detect] we should triger	HWT	...\n");
				if (aee_mode != AEE_MODE_CUSTOMER_USER) {
					aee_kernel_warning_api
						(__FILE__, __LINE__,
						 DB_OPT_NE_JBT_TRACES | DB_OPT_DISPLAY_HANG_DUMP,
						 "\nCRDISPATCH_KEY:SS Hang\n",
						 "we triger HWT ");
					msleep(30 * 1000);
				} else {	
					aee_kernel_exception_api(__FILE__, __LINE__,
						 DB_OPT_NE_JBT_TRACES | DB_OPT_DISPLAY_HANG_DUMP,
						 "\nCRDISPATCH_KEY:SS Hang\n",
						 "we triger HWT ");
					msleep(30 * 1000);
					local_irq_disable();
					while (1)
						;
					BUG();
				}
			}

			hang_detect_counter--;
		} else {
			
			if (1 == hd_detect_enabled) {
				hang_detect_counter = hd_timeout + 4;
				hd_detect_enabled = 0;
			}
			LOGE("[Hang_Detect] hang_detect disabled.\n");
		}

		msleep((HD_INTER) * 1000);
	}
	return 0;
}

void hd_test(void)
{
	hang_detect_counter = 0;
	hd_timeout = 0;
}

void aee_kernel_RT_Monitor_api(int lParam)
{
	if (0 == lParam) {
		hd_detect_enabled = 0;
		hang_detect_counter =
			hd_timeout;
		LOGE("[Hang_Detect] hang_detect disabled\n");
	} else if (lParam > 0) {
		if (lParam & 0x1000) {
			hang_detect_counter =
			hd_timeout = ((long)(lParam & 0x0fff) + HD_INTER - 1) / (HD_INTER);
		} else {
			hd_detect_enabled = 1;
			hang_detect_counter =
				hd_timeout = ((long)lParam + HD_INTER - 1) / (HD_INTER);
		}
		LOGE("[Hang_Detect] hang_detect enabled %d\n", hd_timeout);
	}
}

#if 0
static int hd_proc_cmd_read(char *buf,
		char **start,
		off_t offset,
		int count,
		int *eof,
		void *data) {

	
	int len = 0;

	LOGE("[Hang_Detect] read proc	%d\n", count);

	len =
		sprintf(buf, "%d:%d\n",
				hang_detect_counter,
				hd_timeout);

	hang_detect_counter = hd_timeout;

	return len;
}

static int hd_proc_cmd_write(struct file
		*file,
		const char
		*buf,
		unsigned long
		count,
		void *data) {

	
	
	

	int counter = 0;
	int retval = 0;

	retval = sscanf(buf, "%d ", &counter);
	if (retval == 0)
		LOGE("can not identify counter!\n");

	LOGE("[Hang_Detect] write	proc %d, original %d: %d\n", counter, hang_detect_counter, hd_timeout);

	if (counter > 0) {
		if (counter % HD_INTER != 0)
			hd_timeout = 1;
		else
			hd_timeout = 0;

		counter =
			counter / HD_INTER;
		hd_timeout += counter;
	} else if (counter == 0)
		hd_timeout = 0x7fffffff;
	else if (counter == -1)
		hd_test();
	else
		return count;

	hang_detect_counter = hd_timeout;

	return count;
}
#endif

int hang_detect_init(void)
{

	struct task_struct *hd_thread;
	

	unsigned char *name = "hang_detect";

	LOGD("[Hang_Detect] Initialize proc\n");

	
	

	LOGD("[Hang_Detect] create hang_detect thread\n");

	hd_thread =
		kthread_create
		(hang_detect_thread, NULL,
		 name);
	wake_up_process(hd_thread);

	return 0;
}



int aee_kernel_Powerkey_is_press(void)
{
	int ret = 0;

	ret = pwk_start_monitor;
	return ret;
}
EXPORT_SYMBOL(aee_kernel_Powerkey_is_press);

void aee_kernel_wdt_kick_Powkey_api(const
		char
		*module,
		int msg)
{
	spin_lock(&pwk_hang_lock);
	wdt_kick_status |= msg;
	spin_unlock(&pwk_hang_lock);

}
EXPORT_SYMBOL
(aee_kernel_wdt_kick_Powkey_api);


void aee_powerkey_notify_press(unsigned long
		pressed) {
	if (pressed) {	
		wdt_kick_status = 0;
		hwt_kick_times = 0;
		pwk_start_monitor = 1;
		LOGE("(%s) HW keycode powerkey\n", pressed ? "pressed" : "released");
	}
}
EXPORT_SYMBOL(aee_powerkey_notify_press);


int aee_kernel_wdt_kick_api(int kinterval)
{
	int ret = 0;

	if (pwk_start_monitor
			&& (get_boot_mode() ==
				NORMAL_BOOT)
			&&
			(FindTaskByName("system_server")
			 != -1)) {
		
		LOGE("Press powerkey!!	g_boot_mode=%d,wdt_kick_status=0x%x,tickTimes=0x%x,g_kinterval=%d,RT[%lld]\n",
				get_boot_mode(), wdt_kick_status, hwt_kick_times, kinterval, sched_clock());
		hwt_kick_times++;
		if ((kinterval * hwt_kick_times > 180)) {	
			pwk_start_monitor =
				0;
			
			if ((wdt_kick_status & (WDT_SETBY_Display | WDT_SETBY_SF))
					!= (WDT_SETBY_Display | WDT_SETBY_SF)) {
				if (aee_mode != AEE_MODE_CUSTOMER_USER)	{	
					
						
						
						
						
				} else {
					
						
						
						
						
						
				}
			}
		}
		if ((wdt_kick_status &
					(WDT_SETBY_Display |
					 WDT_SETBY_SF)) ==
				(WDT_SETBY_Display |
				 WDT_SETBY_SF)) {
			pwk_start_monitor =
				0;
			LOGE("[WDK] Powerkey Tick ok,kick_status 0x%08x,RT[%lld]\n ", wdt_kick_status, sched_clock());
		}
	}
	return ret;
}
EXPORT_SYMBOL(aee_kernel_wdt_kick_api);


module_init(monitor_hang_init);
module_exit(monitor_hang_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek AED Driver");
MODULE_AUTHOR("MediaTek Inc.");
