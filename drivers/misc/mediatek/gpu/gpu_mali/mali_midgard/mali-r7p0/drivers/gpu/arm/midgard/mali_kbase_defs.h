/*
 *
 * (C) COPYRIGHT 2011-2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */






#ifndef _KBASE_DEFS_H_
#define _KBASE_DEFS_H_

#include <mali_kbase_config.h>
#include <mali_base_hwconfig_features.h>
#include <mali_base_hwconfig_issues.h>
#include <mali_kbase_mem_lowlevel.h>
#include <mali_kbase_mmu_hw.h>
#include <mali_kbase_mmu_mode.h>
#include <mali_kbase_instr.h>

#include <linux/atomic.h>
#include <linux/mempool.h>
#include <linux/slab.h>

#ifdef CONFIG_MALI_FPGA_BUS_LOGGER
#include <linux/bus_logger.h>
#endif


#ifdef CONFIG_KDS
#include <linux/kds.h>
#endif				

#ifdef CONFIG_SYNC
#include "sync.h"
#endif				

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif				

#ifdef CONFIG_PM_DEVFREQ
#include <linux/devfreq.h>
#endif 

#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#ifdef HTC_CONFIG_PM_RUNTIME
#if defined(CONFIG_PM_RUNTIME) || \
	(defined(CONFIG_PM) && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
#define KBASE_PM_RUNTIME 1
#endif
#endif

#ifdef CONFIG_MALI_MIDGARD_ENABLE_TRACE
#define KBASE_TRACE_ENABLE 1
#endif

#ifndef KBASE_TRACE_ENABLE
#ifdef CONFIG_MALI_DEBUG
#define KBASE_TRACE_ENABLE 1
#else
#define KBASE_TRACE_ENABLE 0
#endif				
#endif				

#define KBASE_TRACE_DUMP_ON_JOB_SLOT_ERROR 1

#define ZAP_TIMEOUT             1000

#define RESET_TIMEOUT           500

#define KBASE_DISABLE_SCHEDULING_SOFT_STOPS 0

#define KBASE_DISABLE_SCHEDULING_HARD_STOPS 0

#define BASE_JM_MAX_NR_SLOTS        3

#define BASE_MAX_NR_AS              16

#define MIDGARD_MMU_PAGE_SIZE 12
#define MIDGARD_MMU_PAGE_MASK ((1ULL << MIDGARD_MMU_PAGE_SIZE) - 1)

#define MIDGARD_MMU_PA_BITS_NUM 48
#define MIDGARD_MMU_PA_MASK \
	(((1ULL << MIDGARD_MMU_PA_BITS_NUM) - 1) & ~MIDGARD_MMU_PAGE_MASK)

#define MIDGARD_MMU_VA_BITS_NUM 48
#define MIDGARD_MMU_VA_MASK \
	(((1ULL << MIDGARD_MMU_VA_BITS_NUM) - 1) & ~MIDGARD_MMU_PAGE_MASK)

#if MIDGARD_MMU_VA_BITS_NUM > 39
#define MIDGARD_MMU_TOPLEVEL    0
#else
#define MIDGARD_MMU_TOPLEVEL    1
#endif

#define GROWABLE_FLAGS_REQUIRED (KBASE_REG_PF_GROW | KBASE_REG_GPU_WR)

#define KBASEP_AS_NR_INVALID     (-1)

#define KBASE_LOCK_REGION_MAX_SIZE (63)
#define KBASE_LOCK_REGION_MIN_SIZE (11)

#define KBASE_TRACE_SIZE_LOG2 8	
#define KBASE_TRACE_SIZE (1 << KBASE_TRACE_SIZE_LOG2)
#define KBASE_TRACE_MASK ((1 << KBASE_TRACE_SIZE_LOG2)-1)

#include "mali_kbase_js_defs.h"
#include "mali_kbase_hwaccess_defs.h"

#define KBASEP_FORCE_REPLAY_DISABLED 0

#define KBASEP_FORCE_REPLAY_RANDOM_LIMIT 16

#define KBASE_KATOM_FLAG_BEEN_SOFT_STOPPPED (1<<1)
#define KBASE_KATOM_FLAGS_RERUN (1<<2)
#define KBASE_KATOM_FLAGS_JOBCHAIN (1<<3)
#define KBASE_KATOM_FLAG_BEEN_HARD_STOPPED (1<<4)
#define KBASE_KATOM_FLAG_IN_DISJOINT (1<<5)
#define KBASE_KATOM_FLAG_FAIL_PREV (1<<6)
#define KBASE_KATOM_FLAG_X_DEP_BLOCKED (1<<7)
#define KBASE_KATOM_FLAG_FAIL_BLOCKER (1<<8)
#define KBASE_KATOM_FLAG_JSCTX_RB_SUBMITTED (1<<9)
#define KBASE_KATOM_FLAG_HOLDING_CTX_REF (1<<10)
#define KBASE_KATOM_FLAG_SECURE (1<<11)


#define JS_COMMAND_SW_CAUSES_DISJOINT 0x100

#define JS_COMMAND_SW_BITS  (JS_COMMAND_SW_CAUSES_DISJOINT)

#if (JS_COMMAND_SW_BITS & JS_COMMAND_MASK)
#error JS_COMMAND_SW_BITS not masked off by JS_COMMAND_MASK. Must update JS_COMMAND_SW_<..> bitmasks
#endif

#define JS_COMMAND_SOFT_STOP_WITH_SW_DISJOINT \
		(JS_COMMAND_SW_CAUSES_DISJOINT | JS_COMMAND_SOFT_STOP)

#define KBASEP_ATOM_ID_INVALID BASE_JD_ATOM_COUNT

#ifdef CONFIG_DEBUG_FS
struct base_job_fault_event {

	u32 event_code;
	struct kbase_jd_atom *katom;
	struct work_struct job_fault_work;
	struct list_head head;
	int reg_offset;
};

#endif

struct kbase_jd_atom_dependency {
	struct kbase_jd_atom *atom;
	u8 dep_type;
};

static inline const struct kbase_jd_atom *const kbase_jd_katom_dep_atom(const struct kbase_jd_atom_dependency *dep)
{
	LOCAL_ASSERT(dep != NULL);

	return (const struct kbase_jd_atom * const)(dep->atom);
}

static inline const u8 kbase_jd_katom_dep_type(const struct kbase_jd_atom_dependency *dep)
{
	LOCAL_ASSERT(dep != NULL);

	return dep->dep_type;
}

static inline void kbase_jd_katom_dep_set(const struct kbase_jd_atom_dependency *const_dep,
		struct kbase_jd_atom *a, u8 type)
{
	struct kbase_jd_atom_dependency *dep;

	LOCAL_ASSERT(const_dep != NULL);

	dep = (struct kbase_jd_atom_dependency *)const_dep;

	dep->atom = a;
	dep->dep_type = type;
}

static inline void kbase_jd_katom_dep_clear(const struct kbase_jd_atom_dependency *const_dep)
{
	struct kbase_jd_atom_dependency *dep;

	LOCAL_ASSERT(const_dep != NULL);

	dep = (struct kbase_jd_atom_dependency *)const_dep;

	dep->atom = NULL;
	dep->dep_type = BASE_JD_DEP_TYPE_INVALID;
}

enum kbase_atom_gpu_rb_state {
	
	KBASE_ATOM_GPU_RB_NOT_IN_SLOT_RB,
	
	KBASE_ATOM_GPU_RB_WAITING_BLOCKED,
	KBASE_ATOM_GPU_RB_WAITING_FOR_CORE_AVAILABLE,
	
	KBASE_ATOM_GPU_RB_WAITING_AFFINITY,
	
	KBASE_ATOM_GPU_RB_WAITING_SECURE_MODE,
	
	KBASE_ATOM_GPU_RB_READY,
	
	KBASE_ATOM_GPU_RB_SUBMITTED,
	KBASE_ATOM_GPU_RB_RETURN_TO_JS
};

struct kbase_ext_res {
	u64 gpu_address;
	struct kbase_mem_phy_alloc *alloc;
};

struct kbase_jd_atom {
	struct work_struct work;
	ktime_t start_timestamp;
	u64 time_spent_us; 

	struct base_jd_udata udata;
	struct kbase_context *kctx;

	struct list_head dep_head[2];
	struct list_head dep_item[2];
	const struct kbase_jd_atom_dependency dep[2];

	u16 nr_extres;
	struct kbase_ext_res *extres;

	u32 device_nr;
	u64 affinity;
	u64 jc;
	enum kbase_atom_coreref_state coreref_state;
#ifdef CONFIG_KDS
	struct list_head node;
	struct kds_resource_set *kds_rset;
	bool kds_dep_satisfied;
#endif				
#ifdef CONFIG_SYNC
	struct sync_fence *fence;
	struct sync_fence_waiter sync_waiter;
#endif				

	
	enum base_jd_event_code event_code;
	base_jd_core_req core_req;	    
	int retry_submit_on_slot;

	union kbasep_js_policy_job_info sched_info;
	
	int sched_priority;

	int poking;		

	wait_queue_head_t completed;
	enum kbase_jd_atom_state status;
#ifdef CONFIG_GPU_TRACEPOINTS
	int work_id;
#endif
	
	int slot_nr;

	u32 atom_flags;

	int retry_count;

	enum kbase_atom_gpu_rb_state gpu_rb_state;

	u64 need_cache_flush_cores_retained;

	atomic_t blocked;

	
	struct kbase_jd_atom *x_pre_dep;
	
	struct kbase_jd_atom *x_post_dep;


	struct kbase_jd_atom_backend backend;
#ifdef CONFIG_DEBUG_FS
	struct base_job_fault_event fault_event;
#endif
};

static inline bool kbase_jd_katom_is_secure(const struct kbase_jd_atom *katom)
{
	return (bool)(katom->atom_flags & KBASE_KATOM_FLAG_SECURE);
}


#define KBASE_JD_DEP_QUEUE_SIZE 256

struct kbase_jd_context {
	struct mutex lock;
	struct kbasep_js_kctx_info sched_info;
	struct kbase_jd_atom atoms[BASE_JD_ATOM_COUNT];

	u32 job_nr;

	wait_queue_head_t zero_jobs_wait;

	
	struct workqueue_struct *job_done_wq;

	spinlock_t tb_lock;
	u32 *tb;
	size_t tb_wrap_offset;

#ifdef CONFIG_KDS
	struct kds_callback kds_cb;
#endif				
#ifdef CONFIG_GPU_TRACEPOINTS
	atomic_t work_id;
#endif
};

struct kbase_device_info {
	u32 features;
};

enum {
	KBASE_AS_POKE_STATE_IN_FLIGHT     = 1<<0,
	KBASE_AS_POKE_STATE_KILLING_POKE  = 1<<1
};

typedef u32 kbase_as_poke_state;

struct kbase_mmu_setup {
	u64	transtab;
	u64	memattr;
};

struct kbase_as {
	int number;

	struct workqueue_struct *pf_wq;
	struct work_struct work_pagefault;
	struct work_struct work_busfault;
	enum kbase_mmu_fault_type fault_type;
	u32 fault_status;
	u64 fault_addr;
	struct mutex transaction_mutex;

	struct kbase_mmu_setup current_setup;

	
	struct workqueue_struct *poke_wq;
	struct work_struct poke_work;
	
	int poke_refcount;
	
	kbase_as_poke_state poke_state;
	struct hrtimer poke_timer;
};

static inline int kbase_as_has_bus_fault(struct kbase_as *as)
{
	return as->fault_type == KBASE_MMU_FAULT_TYPE_BUS;
}

static inline int kbase_as_has_page_fault(struct kbase_as *as)
{
	return as->fault_type == KBASE_MMU_FAULT_TYPE_PAGE;
}

struct kbasep_mem_device {
	atomic_t used_pages;   

};

#define KBASE_TRACE_CODE(X) KBASE_TRACE_CODE_ ## X

enum kbase_trace_code {
#define KBASE_TRACE_CODE_MAKE_CODE(X) KBASE_TRACE_CODE(X)
#include "mali_kbase_trace_defs.h"
#undef  KBASE_TRACE_CODE_MAKE_CODE
	
	,
	
	KBASE_TRACE_CODE_COUNT
};

#define KBASE_TRACE_FLAG_REFCOUNT (((u8)1) << 0)
#define KBASE_TRACE_FLAG_JOBSLOT  (((u8)1) << 1)

struct kbase_trace {
	struct timespec timestamp;
	u32 thread_id;
	u32 cpu;
	void *ctx;
	bool katom;
	int atom_number;
	u64 atom_udata[2];
	u64 gpu_addr;
	unsigned long info_val;
	u8 code;
	u8 jobslot;
	u8 refcount;
	u8 flags;
};

enum kbase_timeline_pm_event {
	
	KBASEP_TIMELINE_PM_EVENT_FIRST,

	
	KBASE_TIMELINE_PM_EVENT_RESERVED_0 = KBASEP_TIMELINE_PM_EVENT_FIRST,

	KBASE_TIMELINE_PM_EVENT_GPU_STATE_CHANGED,

	KBASE_TIMELINE_PM_EVENT_GPU_ACTIVE,

	KBASE_TIMELINE_PM_EVENT_GPU_IDLE,

	KBASE_TIMELINE_PM_EVENT_RESERVED_4,

	KBASE_TIMELINE_PM_EVENT_RESERVED_5,

	KBASE_TIMELINE_PM_EVENT_RESERVED_6,

	KBASE_TIMELINE_PM_EVENT_CHANGE_GPU_STATE,

	KBASEP_TIMELINE_PM_EVENT_LAST = KBASE_TIMELINE_PM_EVENT_CHANGE_GPU_STATE,
};

#ifdef CONFIG_MALI_TRACE_TIMELINE
struct kbase_trace_kctx_timeline {
	atomic_t jd_atoms_in_flight;
	u32 owner_tgid;
};

struct kbase_trace_kbdev_timeline {
	u8 slot_atoms_submitted[BASE_JM_MAX_NR_SLOTS];

	
	atomic_t pm_event_uid[KBASEP_TIMELINE_PM_EVENT_LAST+1];
	
	atomic_t pm_event_uid_counter;
	bool l2_transitioning;
};
#endif 


struct kbasep_kctx_list_element {
	struct list_head link;
	struct kbase_context *kctx;
};

struct kbase_pm_device_data {
	struct mutex lock;

	
	int active_count;
	
	bool suspending;
	
	wait_queue_head_t zero_active_count_wait;

	u64 debug_core_mask;

	/**
	 * Lock protecting the power state of the device.
	 *
	 * This lock must be held when accessing the shader_available_bitmap,
	 * tiler_available_bitmap, l2_available_bitmap, shader_inuse_bitmap and
	 * tiler_inuse_bitmap fields of kbase_device, and the ca_in_transition
	 * and shader_poweroff_pending fields of kbase_pm_device_data. It is
	 * also held when the hardware power registers are being written to, to
	 * ensure that two threads do not conflict over the power transitions
	 * that the hardware should make.
	 */
	spinlock_t power_change_lock;

	 int (*callback_power_runtime_init)(struct kbase_device *kbdev);

	void (*callback_power_runtime_term)(struct kbase_device *kbdev);

	
	u32 dvfs_period;

	
	ktime_t gpu_poweroff_time;

	
	int poweroff_shader_ticks;

	
	int poweroff_gpu_ticks;

	struct kbase_pm_backend_data backend;
};

struct kbase_secure_ops {
	int (*secure_mode_enable)(struct kbase_device *kbdev);

	int (*secure_mode_disable)(struct kbase_device *kbdev);
};


struct kbase_mem_pool {
	struct kbase_device *kbdev;
	size_t              cur_size;
	size_t              max_size;
	spinlock_t          pool_lock;
	struct list_head    page_list;
	struct shrinker     reclaim;

	struct kbase_mem_pool *next_pool;
};


#define DEVNAME_SIZE	16

#define TRACE_MAP_COUNT	512
#define TRACE_MMU_REG_COUNT 256

struct gpu_fault_event {

	struct work_struct gpu_fault_work;
	u64 gpu_fault_address;
};


struct kbase_device {
	s8 slot_submit_count_irq[BASE_JM_MAX_NR_SLOTS];

	u32 hw_quirks_sc;
	u32 hw_quirks_tiler;
	u32 hw_quirks_mmu;
	u32 hw_quirks_jm;

	struct list_head entry;
	struct device *dev;
	struct miscdevice mdev;
	u64 reg_start;
	size_t reg_size;
	void __iomem *reg;
	struct {
		int irq;
		int flags;
	} irqs[3];
#ifdef CONFIG_HAVE_CLK
	struct clk *clock;
#endif
#ifdef CONFIG_REGULATOR
	struct regulator *regulator;
#endif
	char devname[DEVNAME_SIZE];

#ifdef CONFIG_MALI_NO_MALI
	void *model;
	struct kmem_cache *irq_slab;
	struct workqueue_struct *irq_workq;
	atomic_t serving_job_irq;
	atomic_t serving_gpu_irq;
	atomic_t serving_mmu_irq;
	spinlock_t reg_op_lock;
#endif				

	struct kbase_pm_device_data pm;
	struct kbasep_js_device_data js_data;
	struct kbase_mem_pool mem_pool;
	struct kbasep_mem_device memdev;
	struct kbase_mmu_mode const *mmu_mode;

	struct kbase_as as[BASE_MAX_NR_AS];

	spinlock_t mmu_mask_change;

	struct kbase_gpu_props gpu_props;

	
	unsigned long hw_issues_mask[(BASE_HW_ISSUE_END + BITS_PER_LONG - 1) / BITS_PER_LONG];
	
	unsigned long hw_features_mask[(BASE_HW_FEATURE_END + BITS_PER_LONG - 1) / BITS_PER_LONG];

	u64 shader_inuse_bitmap;

	
	u32 shader_inuse_cnt[64];

	
	u64 shader_needed_bitmap;

	
	u32 shader_needed_cnt[64];

	u32 tiler_inuse_cnt;

	u32 tiler_needed_cnt;

	struct {
		atomic_t count;
		atomic_t state;
	} disjoint_event;

	
	u32 l2_users_count;

	u64 shader_available_bitmap;
	u64 tiler_available_bitmap;
	u64 l2_available_bitmap;

	u64 shader_ready_bitmap;
	u64 shader_transitioning_bitmap;

	s8 nr_hw_address_spaces;			  
	s8 nr_user_address_spaces;			  

	
	struct {
		
		spinlock_t lock;

		struct kbase_context *kctx;
		u64 addr;

		struct kbase_context *suspended_kctx;
		struct kbase_uk_hwcnt_setup suspended_state;

		struct kbase_instr_backend backend;
	} hwcnt;

	struct kbase_vinstr_context *vinstr_ctx;

	/*value to be written to the irq_throttle register each time an irq is served */
	atomic_t irq_throttle_cycles;

#if KBASE_TRACE_ENABLE
	spinlock_t              trace_lock;
	u16                     trace_first_out;
	u16                     trace_next_in;
	struct kbase_trace            *trace_rbuf;
#endif

	u32 js_scheduling_period_ns;
	int js_soft_stop_ticks;
	int js_soft_stop_ticks_cl;
	int js_hard_stop_ticks_ss;
	int js_hard_stop_ticks_cl;
	int js_hard_stop_ticks_dumping;
	int js_reset_ticks_ss;
	int js_reset_ticks_cl;
	int js_reset_ticks_dumping;
	bool js_timeouts_updated;

	u32 reset_timeout_ms;

	struct mutex cacheclean_lock;

	
	void *platform_context;

	
	struct list_head        kctx_list;
	struct mutex            kctx_list_lock;

#ifdef CONFIG_MALI_MIDGARD_RT_PM
	struct delayed_work runtime_pm_workqueue;
#endif

#ifdef CONFIG_PM_DEVFREQ
	struct devfreq_dev_profile devfreq_profile;
	struct devfreq *devfreq;
	unsigned long current_freq;
	unsigned long current_voltage;
#ifdef CONFIG_DEVFREQ_THERMAL
	struct devfreq_cooling_device *devfreq_cooling;
#endif
#endif

	struct kbase_ipa_context *ipa_ctx;

#ifdef CONFIG_MALI_TRACE_TIMELINE
	struct kbase_trace_kbdev_timeline timeline;
#endif

#ifdef CONFIG_DEBUG_FS
	
	struct dentry *mali_debugfs_directory;
	
	struct dentry *debugfs_ctx_directory;

	
	bool job_fault_debug;
	wait_queue_head_t job_fault_wq;
	wait_queue_head_t job_fault_resume_wq;
	struct workqueue_struct *job_fault_resume_workq;
	struct list_head job_fault_event_list;
	struct kbase_context *kctx_fault;

#endif 

	
	u32 kbase_profiling_controls[FBDUMP_CONTROL_MAX];


#if MALI_CUSTOMER_RELEASE == 0
	int force_replay_limit;
	int force_replay_count;
	
	base_jd_core_req force_replay_core_req;
	bool force_replay_random;
#endif

	
	atomic_t ctx_num;

	struct kbase_hwaccess_data hwaccess;

	
	atomic_t faults_pending;

	
	bool poweroff_pending;


	
	u32 infinite_cache_active_default;
	size_t mem_pool_max_size_default;

	
	u32 system_coherency;

	
	struct kbase_secure_ops *secure_ops;

	bool secure_mode;

	bool secure_mode_support;


	
	void *mtk_config;
	
	unsigned int mtk_log;

#ifdef CONFIG_MALI_DEBUG
	wait_queue_head_t driver_inactive_wait;
	bool driver_inactive;
#endif 

#ifdef CONFIG_MALI_FPGA_BUS_LOGGER
	struct bus_logger_client *buslogger;
#endif
	u8 debug_gpu_page_tables;

	
	u32 mmu_reg_trace[2][TRACE_MMU_REG_COUNT];
	u32 mmu_reg_trace_index;
	
	struct workqueue_struct *gpu_fault_wq;
	struct gpu_fault_event gpu_fault_events;

};



#define JSCTX_RB_SIZE 256
#define JSCTX_RB_MASK (JSCTX_RB_SIZE-1)

struct jsctx_rb_entry {
	u16 atom_id;
};

struct jsctx_rb {
	struct jsctx_rb_entry entries[JSCTX_RB_SIZE];

	u16 read_idx; 
	u16 write_idx; 
	u16 running_idx; 
};

#define KBASE_API_VERSION(major, minor) ((((major) & 0xFFF) << 20)  | \
					 (((minor) & 0xFFF) << 8) | \
					 ((0 & 0xFF) << 0))

struct kbase_context {
	struct kbase_device *kbdev;
	int id; 
	unsigned long api_version;
	phys_addr_t pgd;
	struct list_head event_list;
	struct mutex event_mutex;
	bool event_closed;
	struct workqueue_struct *event_workq;

	bool is_compat;

	atomic_t                setup_complete;
	atomic_t                setup_in_progress;

	u64 *mmu_teardown_pages;

	struct page *aliasing_sink_page;

	struct mutex            reg_lock; 
	struct rb_root          reg_rbtree; 

	unsigned long    cookies;
	struct kbase_va_region *pending_regions[BITS_PER_LONG];

	wait_queue_head_t event_queue;
	pid_t tgid;
	pid_t pid;

	struct kbase_jd_context jctx;
	atomic_t used_pages;
	atomic_t         nonmapped_pages;

	struct kbase_mem_pool mem_pool;

	struct list_head waiting_soft_jobs;
#ifdef CONFIG_KDS
	struct list_head waiting_kds_resource;
#endif
	int as_nr;

	spinlock_t         mm_update_lock;
	struct mm_struct *process_mm;

#ifdef CONFIG_MALI_TRACE_TIMELINE
	struct kbase_trace_kctx_timeline timeline;
#endif
#ifdef CONFIG_DEBUG_FS
	
	char *mem_profile_data;
	
	size_t mem_profile_size;
	
	spinlock_t mem_profile_lock;
	struct dentry *kctx_dentry;

	
	unsigned int *reg_dump;
	atomic_t job_fault_count;
	struct list_head job_fault_resume_event_list;

#endif 

	struct jsctx_rb jsctx_rb
		[KBASE_JS_ATOM_SCHED_PRIO_COUNT][BASE_JM_MAX_NR_SLOTS];

	
	atomic_t atoms_pulled;
	
	atomic_t atoms_pulled_slot[BASE_JM_MAX_NR_SLOTS];
	
	bool pulled;
	u32 infinite_cache_active;
	
	u32 slots_pullable;

	
	bool as_pending;

	
	struct kbase_context_backend backend;

	
	struct work_struct work;

	
	struct kbase_vinstr_client *vinstr_cli;
	struct mutex vinstr_cli_lock;

	
	bool ctx_active;

	
	struct list_head completed_jobs;
	
	atomic_t work_count;

	u64 map_pa_trace[2][TRACE_MAP_COUNT];	
	u64 unmap_pa_trace[2][TRACE_MAP_COUNT];
	u32 map_pa_trace_index;
	u32 unmap_pa_trace_index;
	char process_name[256];
};

enum kbase_reg_access_type {
	REG_READ,
	REG_WRITE
};

enum kbase_share_attr_bits {
	
	SHARE_BOTH_BITS = (2ULL << 8),	
	SHARE_INNER_BITS = (3ULL << 8)	
};

#define HR_TIMER_DELAY_MSEC(x) (ns_to_ktime((x)*1000000U))
#define HR_TIMER_DELAY_NSEC(x) (ns_to_ktime(x))

#define KBASE_CLEAN_CACHE_MAX_LOOPS     100000
#define KBASE_AS_INACTIVE_MAX_LOOPS     100000

#define BASEP_JD_REPLAY_LIMIT 15

#endif				
