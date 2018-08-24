/*
 * SMP initialisation and IPI support
 * Based on arch/arm/kernel/smp.c
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/cache.h>
#include <linux/profile.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/seq_file.h>
#include <linux/irq.h>
#include <linux/percpu.h>
#include <linux/clockchips.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/irq_work.h>
#ifdef CONFIG_TRUSTY
#ifdef CONFIG_TRUSTY_INTERRUPT_MAP
#include <linux/trusty/trusty.h>
#else
#include <linux/irqdomain.h>
#endif
#endif

#include <asm/alternative.h>
#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/cpu_ops.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/processor.h>
#include <asm/smp_plat.h>
#include <asm/sections.h>
#include <asm/tlbflush.h>
#include <asm/ptrace.h>
#ifdef CONFIG_MTPROF
#include "mt_sched_mon.h"
#endif
#include <mt-plat/mtk_ram_console.h>

#define CREATE_TRACE_POINTS
#include <trace/events/ipi.h>

struct secondary_data secondary_data;

enum ipi_msg_type {
	IPI_RESCHEDULE,
	IPI_CALL_FUNC,
	IPI_CALL_FUNC_SINGLE,
	IPI_CPU_STOP,
	IPI_TIMER,
	IPI_IRQ_WORK,
#ifdef CONFIG_TRUSTY
	IPI_CUSTOM_FIRST,
	IPI_CUSTOM_LAST = 15,
#endif
};

#ifdef CONFIG_TRUSTY
#ifndef CONFIG_TRUSTY_INTERRUPT_MAP
struct irq_domain *ipi_custom_irq_domain;
#endif
#endif

static int boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	if (cpu_ops[cpu]->cpu_boot)
		return cpu_ops[cpu]->cpu_boot(cpu);

	return -EOPNOTSUPP;
}

static DECLARE_COMPLETION(cpu_running);

int __cpu_up(unsigned int cpu, struct task_struct *idle)
{
	int ret;

	secondary_data.stack = task_stack_page(idle) + THREAD_START_SP;
	__flush_dcache_area(&secondary_data, sizeof(secondary_data));

	ret = boot_secondary(cpu, idle);
	if (ret == 0) {
		wait_for_completion_timeout(&cpu_running,
					    msecs_to_jiffies(10000));

		if (!cpu_online(cpu)) {
			pr_crit("CPU%u: failed to come online\n", cpu);
			#if (defined CONFIG_ARCH_MT6797) || (defined CONFIG_ARCH_MT6755)
			BUG_ON(1);
			#endif
			ret = -EIO;
		}
	} else {
		pr_err("CPU%u: failed to boot: %d\n", cpu, ret);
	}

	secondary_data.stack = NULL;

	return ret;
}

static void smp_store_cpu_info(unsigned int cpuid)
{
	store_cpu_topology(cpuid);
}

asmlinkage void secondary_start_kernel(void)
{
	struct mm_struct *mm = &init_mm;
	unsigned int cpu = smp_processor_id();

	aee_rr_rec_hoplug(cpu, 1, 0);

	atomic_inc(&mm->mm_count);
	current->active_mm = mm;
	cpumask_set_cpu(cpu, mm_cpumask(mm));

	aee_rr_rec_hoplug(cpu, 2, 0);

	set_my_cpu_offset(per_cpu_offset(smp_processor_id()));
	pr_debug("CPU%u: Booted secondary processor\n", cpu);

	aee_rr_rec_hoplug(cpu, 3, 0);

	cpu_set_reserved_ttbr0();

	aee_rr_rec_hoplug(cpu, 4, 0);

	flush_tlb_all();

	aee_rr_rec_hoplug(cpu, 5, 0);

	preempt_disable();

	aee_rr_rec_hoplug(cpu, 6, 0);

	trace_hardirqs_off();

	aee_rr_rec_hoplug(cpu, 7, 0);

	if (cpu_ops[cpu]->cpu_postboot)
		cpu_ops[cpu]->cpu_postboot();

	aee_rr_rec_hoplug(cpu, 8, 0);

	cpuinfo_store_cpu();

	aee_rr_rec_hoplug(cpu, 9, 0);

	notify_cpu_starting(cpu);

	aee_rr_rec_hoplug(cpu, 10, 0);

	smp_store_cpu_info(cpu);

	aee_rr_rec_hoplug(cpu, 11, 0);

	set_cpu_online(cpu, true);

	aee_rr_rec_hoplug(cpu, 12, 0);

	complete(&cpu_running);

	aee_rr_rec_hoplug(cpu, 13, 0);

	local_dbg_enable();

	aee_rr_rec_hoplug(cpu, 14, 0);

	local_irq_enable();

	aee_rr_rec_hoplug(cpu, 15, 0);

	local_async_enable();

	aee_rr_rec_hoplug(cpu, 16, 0);

	cpu_startup_entry(CPUHP_ONLINE);

	aee_rr_rec_hoplug(cpu, 17, 0);
}

#ifdef CONFIG_HOTPLUG_CPU
static int op_cpu_disable(unsigned int cpu)
{
	if (!cpu_ops[cpu] || !cpu_ops[cpu]->cpu_die)
		return -EOPNOTSUPP;

	if (cpu_ops[cpu]->cpu_disable)
		return cpu_ops[cpu]->cpu_disable(cpu);

	return 0;
}

int __cpu_disable(void)
{
	unsigned int cpu = smp_processor_id();
	int ret;

	ret = op_cpu_disable(cpu);
	if (ret)
		return ret;

	aee_rr_rec_hoplug(cpu, 71, 0);

	set_cpu_online(cpu, false);

	aee_rr_rec_hoplug(cpu, 72, 0);

	migrate_irqs();

	aee_rr_rec_hoplug(cpu, 73, 0);

	clear_tasks_mm_cpumask(cpu);

	aee_rr_rec_hoplug(cpu, 74, 0);

	return 0;
}

static int op_cpu_kill(unsigned int cpu)
{
	if (!cpu_ops[cpu]->cpu_kill)
		return 1;

	return cpu_ops[cpu]->cpu_kill(cpu);
}

static DECLARE_COMPLETION(cpu_died);

void __cpu_die(unsigned int cpu)
{
	if (!wait_for_completion_timeout(&cpu_died, msecs_to_jiffies(5000))) {
		pr_crit("CPU%u: cpu didn't die\n", cpu);
		return;
	}
	pr_debug("CPU%u: shutdown\n", cpu);

	if (!op_cpu_kill(cpu))
		pr_warn("CPU%d may not have shut down cleanly\n", cpu);
}

void cpu_die(void)
{
	unsigned int cpu = smp_processor_id();

	aee_rr_rec_hoplug(cpu, 51, 0);

	idle_task_exit();

	aee_rr_rec_hoplug(cpu, 52, 0);

	local_irq_disable();

	aee_rr_rec_hoplug(cpu, 53, 0);

	
	complete(&cpu_died);

	aee_rr_rec_hoplug(cpu, 54, 0);

	cpu_ops[cpu]->cpu_die(cpu);

	aee_rr_rec_hoplug(cpu, 55, 0);

	BUG();
}
#endif

void __init smp_cpus_done(unsigned int max_cpus)
{
	unsigned long bogosum = loops_per_jiffy * num_online_cpus();

	pr_info("SMP: Total of %d processors activated (%lu.%02lu BogoMIPS).\n",
			num_online_cpus(), bogosum / (500000/HZ),
			(bogosum / (5000/HZ)) % 100);
	apply_alternatives_all();
}

void __init smp_prepare_boot_cpu(void)
{
	set_my_cpu_offset(per_cpu_offset(smp_processor_id()));
}

void __init smp_init_cpus(void)
{
	struct device_node *dn = NULL;
	unsigned int i, cpu = 1;
	bool bootcpu_valid = false;

	while ((dn = of_find_node_by_type(dn, "cpu"))) {
		const u32 *cell;
		u64 hwid;

		cell = of_get_property(dn, "reg", NULL);
		if (!cell) {
			pr_err("%s: missing reg property\n", dn->full_name);
			goto next;
		}
		hwid = of_read_number(cell, of_n_addr_cells(dn));

		if (hwid & ~MPIDR_HWID_BITMASK) {
			pr_err("%s: invalid reg property\n", dn->full_name);
			goto next;
		}

		for (i = 1; (i < cpu) && (i < NR_CPUS); i++) {
			if (cpu_logical_map(i) == hwid) {
				pr_err("%s: duplicate cpu reg properties in the DT\n",
					dn->full_name);
				goto next;
			}
		}

		if (hwid == cpu_logical_map(0)) {
			if (bootcpu_valid) {
				pr_err("%s: duplicate boot cpu reg property in DT\n",
					dn->full_name);
				goto next;
			}

			bootcpu_valid = true;

			continue;
		}

		if (cpu >= NR_CPUS)
			goto next;

		if (cpu_read_ops(dn, cpu) != 0)
			goto next;

		if (cpu_ops[cpu]->cpu_init(dn, cpu))
			goto next;

		pr_debug("cpu logical map 0x%llx\n", hwid);
		cpu_logical_map(cpu) = hwid;
next:
		cpu++;
	}

	
	if (cpu > NR_CPUS)
		pr_warning("no. of cores (%d) greater than configured maximum of %d - clipping\n",
			   cpu, NR_CPUS);

	if (!bootcpu_valid) {
		pr_err("DT missing boot CPU MPIDR, not enabling secondaries\n");
		return;
	}

	for (i = 0; i < NR_CPUS; i++)
		if (cpu_logical_map(i) != INVALID_HWID)
			set_cpu_possible(i, true);
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int err;
	unsigned int cpu, ncores = num_possible_cpus();

	init_cpu_topology();

	smp_store_cpu_info(smp_processor_id());

	if (max_cpus > ncores)
		max_cpus = ncores;

	
	if (max_cpus <= 1)
		return;

	max_cpus--;
	for_each_possible_cpu(cpu) {
		if (max_cpus == 0)
			break;

		if (cpu == smp_processor_id())
			continue;

		if (!cpu_ops[cpu])
			continue;

		err = cpu_ops[cpu]->cpu_prepare(cpu);
		if (err)
			continue;

		set_cpu_present(cpu, true);
		max_cpus--;
	}
}

void (*__smp_cross_call)(const struct cpumask *, unsigned int);

void __init set_smp_cross_call(void (*fn)(const struct cpumask *, unsigned int))
{
	__smp_cross_call = fn;
}

static const char *ipi_types[NR_IPI] __tracepoint_string = {
#define S(x,s)	[x] = s
	S(IPI_RESCHEDULE, "Rescheduling interrupts"),
	S(IPI_CALL_FUNC, "Function call interrupts"),
	S(IPI_CALL_FUNC_SINGLE, "Single function call interrupts"),
	S(IPI_CPU_STOP, "CPU stop interrupts"),
	S(IPI_TIMER, "Timer broadcast interrupts"),
	S(IPI_IRQ_WORK, "IRQ work interrupts"),
};

static void smp_cross_call(const struct cpumask *target, unsigned int ipinr)
{
	trace_ipi_raise(target, ipi_types[ipinr]);
	__smp_cross_call(target, ipinr);
}

void show_ipi_list(struct seq_file *p, int prec)
{
	unsigned int cpu, i;

	for (i = 0; i < NR_IPI; i++) {
		seq_printf(p, "%*s%u:%s", prec - 1, "IPI", i,
			   prec >= 4 ? " " : "");
		for_each_online_cpu(cpu)
			seq_printf(p, "%10u ",
				   __get_irq_stat(cpu, ipi_irqs[i]));
		seq_printf(p, "      %s\n", ipi_types[i]);
	}
}

u64 smp_irq_stat_cpu(unsigned int cpu)
{
	u64 sum = 0;
	int i;

	for (i = 0; i < NR_IPI; i++)
		sum += __get_irq_stat(cpu, ipi_irqs[i]);

	return sum;
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	smp_cross_call(mask, IPI_CALL_FUNC);
}

void arch_send_call_function_single_ipi(int cpu)
{
	smp_cross_call(cpumask_of(cpu), IPI_CALL_FUNC_SINGLE);
}

#ifdef CONFIG_IRQ_WORK
void arch_irq_work_raise(void)
{
	if (__smp_cross_call)
		smp_cross_call(cpumask_of(smp_processor_id()), IPI_IRQ_WORK);
}
#endif

static DEFINE_RAW_SPINLOCK(stop_lock);

static void ipi_cpu_stop(unsigned int cpu)
{
	if (system_state == SYSTEM_BOOTING ||
	    system_state == SYSTEM_RUNNING) {
		raw_spin_lock(&stop_lock);
		pr_crit("CPU%u: stopping\n", cpu);
		dump_stack();
		raw_spin_unlock(&stop_lock);
	}

	set_cpu_online(cpu, false);

	local_irq_disable();

	while (1)
		cpu_relax();
}

void handle_IPI(int ipinr, struct pt_regs *regs)
{
	unsigned int cpu = smp_processor_id();
	struct pt_regs *old_regs = set_irq_regs(regs);

	if ((unsigned)ipinr < NR_IPI) {
		trace_ipi_entry(ipi_types[ipinr]);
		__inc_irq_stat(cpu, ipi_irqs[ipinr]);
	}

	switch (ipinr) {
	case IPI_RESCHEDULE:
		scheduler_ipi();
		break;

	case IPI_CALL_FUNC:
		irq_enter();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_start(ipinr);
#endif
		generic_smp_call_function_interrupt();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_end(ipinr);
#endif
		irq_exit();
		break;

	case IPI_CALL_FUNC_SINGLE:
		irq_enter();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_start(ipinr);
#endif
		generic_smp_call_function_single_interrupt();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_end(ipinr);
#endif
		irq_exit();
		break;

	case IPI_CPU_STOP:
		irq_enter();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_start(ipinr);
#endif
		ipi_cpu_stop(cpu);
#ifdef CONFIG_MTPROF
		mt_trace_ISR_end(ipinr);
#endif
		irq_exit();
		break;

#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
	case IPI_TIMER:
		irq_enter();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_start(ipinr);
#endif
		tick_receive_broadcast();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_end(ipinr);
#endif
		irq_exit();
		break;
#endif

#ifdef CONFIG_IRQ_WORK
	case IPI_IRQ_WORK:
		irq_enter();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_start(ipinr);
#endif
		irq_work_run();
#ifdef CONFIG_MTPROF
		mt_trace_ISR_end(ipinr);
#endif
		irq_exit();
		break;
#endif

	default:
#ifdef CONFIG_TRUSTY
		if (ipinr >= IPI_CUSTOM_FIRST && ipinr <= IPI_CUSTOM_LAST)
#ifndef CONFIG_TRUSTY_INTERRUPT_MAP
			handle_domain_irq(ipi_custom_irq_domain, ipinr, regs);
#else
			handle_trusty_ipi(ipinr);
#endif
		else
#endif
		pr_crit("CPU%u: Unknown IPI message 0x%x\n", cpu, ipinr);
		break;
	}

	if ((unsigned)ipinr < NR_IPI)
		trace_ipi_exit(ipi_types[ipinr]);
	set_irq_regs(old_regs);
}

#ifdef CONFIG_TRUSTY
#ifndef CONFIG_TRUSTY_INTERRUPT_MAP
static void custom_ipi_enable(struct irq_data *data)
{
	smp_cross_call(cpumask_of(smp_processor_id()), data->irq);
}

static void custom_ipi_disable(struct irq_data *data)
{
}

static struct irq_chip custom_ipi_chip = {
	.name			= "CustomIPI",
	.irq_enable		= custom_ipi_enable,
	.irq_disable		= custom_ipi_disable,
};

static void handle_custom_ipi_irq(unsigned int irq, struct irq_desc *desc)
{
	if (!desc->action) {
		pr_crit("CPU%u: Unknown IPI message 0x%x, no custom handler\n",
			smp_processor_id(), irq);
		return;
	}

	if (!cpumask_test_cpu(smp_processor_id(), desc->percpu_enabled))
		return; 

	handle_percpu_devid_irq(irq, desc);
}

static int __init smp_custom_ipi_init(void)
{
	int ipinr;

	
	irq_alloc_descs(IPI_CUSTOM_FIRST, 0,
		IPI_CUSTOM_LAST - IPI_CUSTOM_FIRST + 1, 0);

	for (ipinr = IPI_CUSTOM_FIRST; ipinr <= IPI_CUSTOM_LAST; ipinr++) {
		irq_set_percpu_devid(ipinr);
		irq_set_chip_and_handler(ipinr, &custom_ipi_chip,
					 handle_custom_ipi_irq);
		set_irq_flags(ipinr, IRQF_VALID | IRQF_NOAUTOEN);
	}
	ipi_custom_irq_domain = irq_domain_add_legacy(NULL,
					IPI_CUSTOM_LAST - IPI_CUSTOM_FIRST + 1,
					IPI_CUSTOM_FIRST, IPI_CUSTOM_FIRST,
					&irq_domain_simple_ops,
					&custom_ipi_chip);

	return 0;
}
core_initcall(smp_custom_ipi_init);
#endif
#endif

void smp_send_reschedule(int cpu)
{
	smp_cross_call(cpumask_of(cpu), IPI_RESCHEDULE);
}

#ifdef CONFIG_GENERIC_CLOCKEVENTS_BROADCAST
void tick_broadcast(const struct cpumask *mask)
{
	smp_cross_call(mask, IPI_TIMER);
}
#endif

void smp_send_stop(void)
{
	unsigned long timeout;

	if (num_online_cpus() > 1) {
		cpumask_t mask;

		cpumask_copy(&mask, cpu_online_mask);
		cpu_clear(smp_processor_id(), mask);

		smp_cross_call(&mask, IPI_CPU_STOP);
	}

	
	timeout = USEC_PER_SEC;
	while (num_online_cpus() > 1 && timeout--)
		udelay(1);

	if (num_online_cpus() > 1)
		pr_warning("SMP: failed to stop secondary CPUs\n");
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}
