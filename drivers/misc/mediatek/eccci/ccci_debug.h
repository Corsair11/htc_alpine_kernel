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

#ifndef __CCCI_DEBUG_H__
#define __CCCI_DEBUG_H__

#define CORE "cor"
#define BM "bfm"
#define NET "net"
#define CHAR "chr"
#define SYSFS "sfs"
#define KERN "ken"
#define IPC "ipc"
#define RPC "rpc"

extern unsigned int ccci_debug_enable;
extern unsigned int ccci_traffic_debug_enable;
extern int ccci_log_write(const char *fmt, ...);
#define CCCI_LEGACY_DBG_LOG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)
#define CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 0 || ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)
#define CCCI_LEGACY_ERR_LOG(idx, tag, fmt, args...) pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args)

#define CCCI_INIT_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_INIT, CCCI_DUMP_TIME_FLAG, "[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_BOOTUP_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_BOOTUP, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_BOOTUP_DUMP_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_BOOTUP, 0, "[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_NORMAL_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_NORMAL, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_NOTICE_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_NORMAL, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[ccci%d]" fmt, (idx+1), ##args); \
	pr_notice("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_ERROR_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_NORMAL, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ERR_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_DEBUG_LOG(idx, tag, fmt, args...) CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args)

#define CCCI_REPEAT_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_REPEAT, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_REPEAT_ALWAYS_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_REPEAT, CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[ccci%d]" fmt, (idx+1), ##args); \
	if ( ccci_traffic_debug_enable == 1 ) \
		CCCI_LEGACY_ERR_LOG(idx, tag, fmt, ##args); \
	else \
		CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_MEM_LOG_TAG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_MEM_DUMP, CCCI_DUMP_TIME_FLAG|CCCI_DUMP_CURR_FLAG,\
			"[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_MEM_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_MEM_DUMP, 0, fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_HISTORY_LOG_1ST(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_HISTORY, CCCI_DUMP_TIME_FLAG|CCCI_DUMP_CURR_FLAG|CCCI_DUMP_CLR_BUF_FLAG,\
			"[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_HISTORY_LOG(idx, tag, fmt, args...) \
do { \
	ccci_dump_write(idx, CCCI_DUMP_HISTORY, 0, "[ccci%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)
#define CCCI_DBG_MSG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)
#define CCCI_INF_MSG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 0 || ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_REP_MSG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 0 || ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_RATE_LIMIT_MSG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_notice_ratelimited("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_NOTICE_MSG(idx, tag, fmt, args...) pr_notice("[ccci%d/ntc/" tag "]" fmt, (idx+1), ##args)
#define CCCI_ERR_MSG(idx, tag, fmt, args...) pr_err("[ccci%d/err/" tag "]" fmt, (idx+1), ##args)
#define CCCI_EXP_MSG(idx, tag, fmt, args...) pr_warn("[ccci%d/err/" tag "]" fmt, (idx+1), ##args)
#define CCCI_EXP_INF_MSG(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 0 || ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)
#define CCCI_DUMP_MSG1(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 0 || ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)
#define CCCI_DUMP_MSG2(idx, tag, fmt, args...) \
do { \
	if (ccci_debug_enable == 0 || ccci_debug_enable == 1) \
		pr_debug("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 2) \
		pr_warn("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if (ccci_debug_enable == 5 || ccci_debug_enable == 6) \
		pr_err("[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while (0)

#define CCCI_DBG_COM_MSG(fmt, args...)		 CCCI_DBG_MSG(0, "com", fmt, ##args)
#define CCCI_INF_COM_MSG(fmt, args...)		 CCCI_INF_MSG(0, "com", fmt, ##args)
#define CCCI_ERR_COM_MSG(fmt, args...)		 CCCI_ERR_MSG(0, "com", fmt, ##args)

#define CLDMA_TRACE
#define CCCI_SKB_TRACE

#endif				
