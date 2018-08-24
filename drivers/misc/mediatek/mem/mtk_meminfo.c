/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/page.h>
#include <asm/setup.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <mt-plat/mtk_memcfg.h>
#include <mt-plat/mtk_meminfo.h>

#ifdef CONFIG_OF
static u64 kernel_mem_sz;
static u64 phone_dram_sz;	
static int dt_scan_memory(unsigned long node, const char *uname,
				int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	int i;
	const __be32 *reg, *endp;
	int l;
	struct dram_info *dram_info;

	
	if (type == NULL) {
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0) {
		return 0;
	}

	reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));
	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;

		kernel_mem_sz += size;
	}

	
	dram_info = (struct dram_info *)of_get_flat_dt_prop(node,
			"orig_dram_info", NULL);
	if (dram_info) {
		for (i = 0; i < dram_info->rank_num; i++)
			phone_dram_sz += dram_info->rank_info[i].size;
	}

	return node;
}

static int __init init_get_max_DRAM_size(void)
{
	if (!phone_dram_sz && !kernel_mem_sz) {
		if (of_scan_flat_dt(dt_scan_memory, NULL)) {
			pr_alert("init_get_max_DRAM_size done. phone_dram_sz: 0x%llx, kernel_mem_sz: 0x%llx\n",
				 (unsigned long long)phone_dram_sz,
				 (unsigned long long)kernel_mem_sz);
		} else {
			pr_err("init_get_max_DRAM_size fail\n");
			BUG();
		}
	}
	return 0;
}

phys_addr_t get_max_DRAM_size(void)
{
	if (!phone_dram_sz && !kernel_mem_sz)
		init_get_max_DRAM_size();
	return phone_dram_sz ?
		(phys_addr_t)phone_dram_sz : (phys_addr_t)kernel_mem_sz;
}
early_initcall(init_get_max_DRAM_size);
#else
phys_addr_t get_max_DRAM_size(void)
{
	return mtk_get_max_DRAM_size();
}
#endif 
EXPORT_SYMBOL(get_max_DRAM_size);

phys_addr_t get_memory_size(void)
{
	return get_max_DRAM_size();
}
EXPORT_SYMBOL(get_memory_size);

phys_addr_t get_phys_offset(void)
{
	return PHYS_OFFSET;
}
EXPORT_SYMBOL(get_phys_offset);

phys_addr_t get_zone_movable_cma_base(void)
{
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
	return memory_lowpower_cma_base();
#elif	defined(CONFIG_MTK_SVP)
	return memory_ssvp_cma_base();
#endif 
	return 0;
}

phys_addr_t get_zone_movable_cma_size(void)
{
#ifdef CONFIG_MTK_MEMORY_LOWPOWER
	return (phys_addr_t)memory_lowpower_cma_size();
#elif	defined(CONFIG_MTK_SVP)
	return (phys_addr_t)memory_ssvp_cma_size();
#endif 
	return 0;
}
