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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "cmdq_record_private.h"
#include "cmdq_reg.h"
#include "cmdq_virtual.h"
#include "cmdq_mdp_common.h"
#include "cmdq_device.h"

#ifndef CMDQ_USE_CCF
#include <mach/mt_clkmgr.h>
#endif				

#define CMDQ_TEST

#ifdef CMDQ_TEST

#define CMDQ_TESTCASE_PARAMETER_MAX		4
#define CMDQ_MONITOR_EVENT_MAX			10

static DEFINE_MUTEX(gCmdqTestProcLock);

typedef enum CMDQ_TEST_TYPE_ENUM {
	CMDQ_TEST_TYPE_NORMAL = 0,
	CMDQ_TEST_TYPE_SECURE = 1,
	CMDQ_TEST_TYPE_MONITOR_EVENT = 2,
	CMDQ_TEST_TYPE_MONITOR_POLL = 3,
	CMDQ_TEST_TYPE_OPEN_COMMAND_DUMP = 4,
	CMDQ_TEST_TYPE_DUMP_DTS = 5,
	CMDQ_TEST_TYPE_FEATURE_CONFIG = 6,

	CMDQ_TEST_TYPE_MAX	
} CMDQ_TEST_TYPE_ENUM;

typedef enum CMDQ_MOITOR_TYPE_ENUM {
	CMDQ_MOITOR_TYPE_FLUSH = 0,
	CMDQ_MOITOR_TYPE_WFE = 1,	
	CMDQ_MOITOR_TYPE_WAIT_NO_CLEAR = 2,
	CMDQ_MOITOR_TYPE_QUERYREGISTER = 3,

	CMDQ_MOITOR_TYPE_MAX	
} CMDQ_MOITOR_TYPE_ENUM;

typedef struct cmdqMonitorEventStruct {
	bool status;

	cmdqRecHandle cmdqHandle;
	cmdqBackupSlotHandle slotHandle;
	uint32_t monitorNUM;
	uint32_t waitType[CMDQ_MONITOR_EVENT_MAX];
	uint64_t monitorEvent[CMDQ_MONITOR_EVENT_MAX];
	uint32_t previousValue[CMDQ_MONITOR_EVENT_MAX];
} cmdqMonitorEventStruct;

typedef struct cmdqMonitorPollStruct {
	bool status;

	cmdqRecHandle cmdqHandle;
	cmdqBackupSlotHandle slotHandle;
	uint64_t pollReg;
	uint64_t pollValue;
	uint64_t pollMask;
	uint32_t delayTime;
	struct delayed_work delayContinueWork;
} cmdqMonitorPollStruct;

static int64_t gCmdqTestConfig[CMDQ_MONITOR_EVENT_MAX];
static bool gCmdqTestSecure;
static cmdqMonitorEventStruct gEventMonitor;
static cmdqMonitorPollStruct gPollMonitor;

static struct proc_dir_entry *gCmdqTestProcEntry;

static int32_t _test_submit_async(cmdqRecHandle handle, TaskStruct **ppTask)
{
	cmdqCommandStruct desc = {
		.scenario = handle->scenario,
		.priority = handle->priority,
		.engineFlag = handle->engineFlag,
		.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer,
		.blockSize = handle->blockSize,
	};

	
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTaskAsync(&desc, NULL, 0, ppTask);
}

static void testcase_scenario(void)
{
	cmdqRecHandle hRec;
	int32_t ret;
	int i = 0;

	CMDQ_MSG("%s\n", __func__);

	
	for (i = 0; i < CMDQ_MAX_SCENARIO_COUNT; ++i) {
		if (cmdq_core_is_request_from_user_space(i))
			continue;

		CMDQ_MSG("testcase_scenario id:%d\n", i);
		cmdqRecCreate((CMDQ_SCENARIO_ENUM) i, &hRec);
		cmdqRecReset(hRec);
		cmdqRecSetSecure(hRec, false);
		ret = cmdqRecFlush(hRec);
	}

	cmdqRecDestroy(hRec);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer;

static void _testcase_sync_token_timer_func(unsigned long data)
{
	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
}

static void _testcase_sync_token_timer_loop_func(unsigned long data)
{
	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);

	
	mod_timer(&timer, jiffies + msecs_to_jiffies(10));
}

static void testcase_sync_token(void)
{
	cmdqRecHandle hRec;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hRec);

	do {
		cmdqRecReset(hRec);
		cmdqRecSetSecure(hRec, gCmdqTestSecure);

		
		setup_timer(&timer, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
		mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

		
		cmdqRecWait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdqRecFlush(hRec);
		CMDQ_MSG("waiting done\n");

		
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
		del_timer(&timer);
	} while (0);

	CMDQ_MSG("%s, timeout case\n", __func__);
	
	
	
	do {
		cmdqRecReset(hRec);
		cmdqRecSetSecure(hRec, gCmdqTestSecure);

		
		cmdqRecWait(hRec, CMDQ_SYNC_TOKEN_USER_0);

		CMDQ_MSG("start waiting\n");
		ret = cmdqRecFlush(hRec);
		CMDQ_MSG("waiting done\n");

		
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

		BUG_ON(ret >= 0);
	} while (0);

	cmdqRecDestroy(hRec);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_suspend_resume(void)
{
	cmdqRecHandle hReqA;
	TaskStruct *pTaskA;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	
	
	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	do {
		
		cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_ALL, &hReqA);
		cmdqRecReset(hReqA);
		cmdqRecSetSecure(hReqA, gCmdqTestSecure);
		cmdqRecWait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8);

		ret = _test_submit_async(hReqA, &pTaskA);

		CMDQ_MSG("%s pTask %p, engine:0x%llx, scenario:%d\n",
			 __func__, pTaskA, pTaskA->engineFlag, pTaskA->scenario);
		CMDQ_MSG("%s start suspend+resume thread 0========\n", __func__);
		cmdq_core_suspend_HW_thread(0, __LINE__);
		CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(0), 0x00);	
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

		msleep_interruptible(500);
		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);
	} while (0);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecDestroy(hReqA);
	

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_errors(void)
{
	cmdqRecHandle hReq;
	cmdqRecHandle hLoop;
	TaskStruct *pTask;
	int32_t ret;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	const uint32_t UNKNOWN_OP = 0x50;
	uint32_t *pCommand;

	ret = 0;
	do {
		
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);

		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoop);
		cmdqRecReset(hLoop);
		cmdqRecSetSecure(hLoop, false);
		cmdqRecPoll(hLoop, CMDQ_TEST_MMSYS_DUMMY_PA, 1, 0xFFFFFFFF);
		cmdqRecStartLoop(hLoop);

		CMDQ_MSG("=============== INIFINITE Wait ===================\n");

		cmdqCoreClearEvent(CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &hReq);

		
		for (ret = 0; ret < CMDQ_MAX_ENGINE_COUNT; ++ret)
			hReq->engineFlag |= 1LL << ret;

		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdqRecWait(hReq, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdqRecFlush(hReq);

		CMDQ_MSG("=============== INIFINITE JUMP ===================\n");

		
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);
		cmdqCoreClearEvent(CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdqRecWait(hReq, CMDQ_EVENT_MDP_RSZ0_EOF);
		cmdq_append_command(hReq, CMDQ_CODE_JUMP, 0, 8);	
		ret = _test_submit_async(hReq, &pTask);
		msleep_interruptible(500);
		ret = cmdqCoreWaitAndReleaseTask(pTask, 8000);

		CMDQ_MSG("================ POLL INIFINITE ====================\n");

		CMDQ_MSG("testReg: %lx\n", MMSYS_DUMMY_REG);

		CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0x0);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdqRecPoll(hReq, CMDQ_TEST_MMSYS_DUMMY_PA, 1, 0xFFFFFFFF);
		cmdqRecFlush(hReq);

		CMDQ_MSG("================= INVALID INSTR =================\n");

		
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		cmdq_append_command(hReq, CMDQ_CODE_JUMP, -1, 0);
		cmdqRecFlush(hReq);

		CMDQ_MSG("================= INVALID INSTR: UNKNOWN OP(0x%x) =================\n",
			 UNKNOWN_OP);
		CMDQ_MSG("%s line:%d\n", __func__, __LINE__);

		
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, gCmdqTestSecure);
		{
			pCommand = (uint32_t *) ((uint8_t *) hReq->pBuffer + hReq->blockSize);
			*pCommand++ = 0x0;
			*pCommand++ = (UNKNOWN_OP << 24);
			hReq->blockSize += 8;
		}
		cmdqRecFlush(hReq);

	} while (0);

	cmdqRecDestroy(hReq);
	cmdqRecDestroy(hLoop);

	CMDQ_MSG("%s END\n", __func__);
}

static int32_t finishCallback(unsigned long data)
{
	CMDQ_LOG("callback() with data=0x%08lx\n", data);
	return 0;
}

static void testcase_fire_and_forget(void)
{
	cmdqRecHandle hReqA, hReqB;

	CMDQ_MSG("%s\n", __func__);
	do {
		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqA);
		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqB);
		cmdqRecReset(hReqA);
		cmdqRecReset(hReqB);
		cmdqRecSetSecure(hReqA, gCmdqTestSecure);
		cmdqRecSetSecure(hReqB, gCmdqTestSecure);

		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdqRecFlushAsync(hReqA);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
		cmdqRecFlushAsyncCallback(hReqB, finishCallback, 443);
		CMDQ_MSG("%s %d\n", __func__, __LINE__);
	} while (0);

	cmdqRecDestroy(hReqA);
	cmdqRecDestroy(hReqB);

	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;
static void testcase_async_request(void)
{
	cmdqRecHandle hReqA, hReqB;
	TaskStruct *pTaskA, *pTaskB;
	int32_t ret = 0;

	CMDQ_MSG("%s\n", __func__);

	
	setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(1000));

	setup_timer(&timer_reqB, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_1);
	

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	do {
		cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hReqA);
		cmdqRecReset(hReqA);
		cmdqRecSetSecure(hReqA, gCmdqTestSecure);
		cmdqRecWait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
		cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1);
		cmdq_append_command(hReqA, CMDQ_CODE_JUMP, 0, 8);

		cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hReqB);
		cmdqRecReset(hReqB);
		cmdqRecSetSecure(hReqB, gCmdqTestSecure);
		cmdqRecWait(hReqB, CMDQ_SYNC_TOKEN_USER_1);
		cmdq_append_command(hReqB, CMDQ_CODE_EOC, 0, 1);
		cmdq_append_command(hReqB, CMDQ_CODE_JUMP, 0, 8);

		ret = _test_submit_async(hReqA, &pTaskA);
		ret = _test_submit_async(hReqB, &pTaskB);

		CMDQ_MSG("%s start wait sleep========\n", __func__);
		msleep_interruptible(500);

		CMDQ_MSG("%s start wait A========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);

		CMDQ_MSG("%s start wait B, this should timeout========\n", __func__);
		ret = cmdqCoreWaitAndReleaseTask(pTaskB, 600);
		CMDQ_MSG("%s wait B get %d ========\n", __func__, ret);

	} while (0);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

	cmdqRecDestroy(hReqA);
	cmdqRecDestroy(hReqB);

	del_timer(&timer_reqA);
	del_timer(&timer_reqB);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_multiple_async_request(void)
{
#define TEST_REQ_COUNT 24
	cmdqRecHandle hReq[TEST_REQ_COUNT] = { 0 };
	TaskStruct *pTask[TEST_REQ_COUNT] = { 0 };
	int32_t ret = 0;
	int i;

	CMDQ_MSG("%s\n", __func__);

	setup_timer(&timer, &_testcase_sync_token_timer_loop_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(10));

	
	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < TEST_REQ_COUNT; ++i) {
		ret = cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReq[i]);
		if (0 > ret) {
			CMDQ_ERR("%s cmdqRecCreate failed:%d, i:%d\n ", __func__, ret, i);
			continue;
		}

		cmdqRecReset(hReq[i]);

		
		hReq[i]->engineFlag = (1LL << CMDQ_ENG_MDP_CAMIN);

		cmdqRecSetSecure(hReq[i], gCmdqTestSecure);
		cmdqRecWait(hReq[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_rec_finalize_command(hReq[i], false);

		
		hReq[i]->priority = i;

		_test_submit_async(hReq[i], &pTask[i]);

		CMDQ_MSG("======== create task[%2d]=0x%p done ========\n", i, pTask[i]);
	}

	
	for (i = 0; i < TEST_REQ_COUNT; ++i) {

		if (NULL == pTask[i]) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		msleep_interruptible(100);

		CMDQ_LOG("======== wait task[%2d]=0x%p ========\n", i, pTask[i]);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 1000);
		cmdqRecDestroy(hReq[i]);
	}

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	del_timer(&timer);

	CMDQ_MSG("%s END\n", __func__);
}


static void testcase_async_request_partial_engine(void)
{
	int32_t ret = 0;
	int i;
	CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,
		CMDQ_SCENARIO_JPEG_DEC,
		CMDQ_SCENARIO_PRIMARY_MEMOUT,
		CMDQ_SCENARIO_SUB_DISP,
		CMDQ_SCENARIO_DEBUG,
	};

	cmdqRecHandle hReq;
	TaskStruct *pTasks[(sizeof(scn) / sizeof(scn[0]))] = { 0 };
	struct timer_list *timers;

	timers = kmalloc(sizeof(scn) / sizeof(scn[0]) * sizeof(struct timer_list), GFP_ATOMIC);
	if (NULL == timers)
		return;

	CMDQ_MSG("%s\n", __func__);

	
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		setup_timer(&timers[i], &_testcase_sync_token_timer_func,
			    CMDQ_SYNC_TOKEN_USER_0 + i);
		mod_timer(&timers[i], jiffies + msecs_to_jiffies(400 * (1 + i)));
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);

		cmdqRecCreate(scn[i], &hReq);
		cmdqRecReset(hReq);
		cmdqRecSetSecure(hReq, false);
		cmdqRecWait(hReq, CMDQ_SYNC_TOKEN_USER_0 + i);
		cmdq_rec_finalize_command(hReq, false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_submit_async(hReq, &pTasks[i]);
	}

	cmdqRecDestroy(hReq);

	
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
		ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(3000));

	
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);
		del_timer(&timers[i]);
	}

	if (timers != NULL) {
		kfree(timers);
		timers = NULL;
	}

	CMDQ_MSG("%s END\n", __func__);

}

static void _testcase_unlock_all_event_timer_func(unsigned long data)
{
	uint32_t token = 0;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_MSG("trigger events\n");
	for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token) {
		
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
	}
}

static void testcase_sync_token_threaded(void)
{
	CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,	
		CMDQ_SCENARIO_JPEG_DEC,	
		CMDQ_SCENARIO_TRIGGER_LOOP	
	};
	int32_t ret = 0;
	int i = 0;
	uint32_t token = 0;
	struct timer_list eventTimer;
	cmdqRecHandle hReq[(sizeof(scn) / sizeof(scn[0]))] = { 0 };
	TaskStruct *pTasks[(sizeof(scn) / sizeof(scn[0]))] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i) {
		setup_timer(&eventTimer, &_testcase_unlock_all_event_timer_func, 0);
		mod_timer(&eventTimer, jiffies + msecs_to_jiffies(500));

		
		
		
		cmdqRecCreate(scn[i], &hReq[i]);
		cmdqRecReset(hReq[i]);
		cmdqRecSetSecure(hReq[i], false);
		for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token)
			cmdqRecWait(hReq[i], (CMDQ_EVENT_ENUM) token);

		cmdq_rec_finalize_command(hReq[i], false);

		CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
		ret = _test_submit_async(hReq[i], &pTasks[i]);
	}


	
	msleep_interruptible(1000);
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
		ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(5000));

	
	for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
		cmdqRecDestroy(hReq[i]);

	del_timer(&eventTimer);
	CMDQ_MSG("%s END\n", __func__);
}

static struct timer_list g_loopTimer;
static int g_loopIter;
static cmdqRecHandle hLoopReq;

static void _testcase_loop_timer_func(unsigned long data)
{
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	g_loopIter++;
}

static void testcase_loop(void)
{
	int status = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoopReq);
	cmdqRecReset(hLoopReq);
	cmdqRecSetSecure(hLoopReq, false);
	cmdqRecWait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);

	setup_timer(&g_loopTimer, &_testcase_loop_timer_func, CMDQ_SYNC_TOKEN_USER_0);
	mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	g_loopIter = 0;

	
	status = cmdqRecStartLoop(hLoopReq);
	BUG_ON(status != 0);

	
	CMDQ_MSG("============testcase_loop start loop\n");
	status = cmdqRecStartLoop(hLoopReq);
	BUG_ON(status >= 0);

	cmdqRecDumpCommand(hLoopReq);

	
	while (g_loopIter < 20)
		msleep_interruptible(2000);

	msleep_interruptible(2000);

	CMDQ_MSG("============testcase_loop stop timer\n");
	cmdqRecDestroy(hLoopReq);
	del_timer(&g_loopTimer);

	CMDQ_MSG("%s\n", __func__);
}

static unsigned long gLoopCount = 0L;
static void _testcase_trigger_func(unsigned long data)
{
	
	CMDQ_MSG("_testcase_trigger_func");
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);

	
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));
	gLoopCount++;
}


static void testcase_trigger_thread(void)
{
	cmdqRecHandle hTrigger, hConfig;
	int32_t ret = 0;
	int index = 0;

	CMDQ_MSG("%s\n", __func__);

	
	setup_timer(&timer, &_testcase_trigger_func, 0);
	mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

	do {
		
		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
		cmdqRecReset(hTrigger);
		
		

		
		

		
		

		
		

		cmdqRecWait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);

		
		ret = cmdqRecStartLoop(hTrigger);

		
		cmdqRecCreate(CMDQ_SCENARIO_JPEG_DEC, &hConfig);


		hConfig->priority = CMDQ_THR_PRIO_NORMAL;
		cmdqRecReset(hConfig);
		
		for (index = 0; index < 10; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);

		ret = cmdqRecFlush(hConfig);
		CMDQ_MSG("flush 0\n");

		hConfig->priority = CMDQ_THR_PRIO_DISPLAY_CONFIG;
		cmdqRecReset(hConfig);
		
		for (index = 0; index < 10; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);

		ret = cmdqRecFlush(hConfig);
		CMDQ_MSG("flush 1\n");

		cmdqRecReset(hConfig);
		
		for (index = 0; index < 500; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);

		ret = cmdqRecFlush(hConfig);
		CMDQ_MSG("flush 2\n");

		
		while (gLoopCount < 20)
			msleep_interruptible(2000);
	} while (0);

	del_timer(&timer);
	cmdqRecDestroy(hTrigger);
	cmdqRecDestroy(hConfig);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_prefetch_scenarios(void)
{
	
	
	cmdqRecHandle hConfig;
	int32_t ret = 0;
	int index = 0, scn = 0;
	const int INSTRUCTION_COUNT = 500;

	CMDQ_MSG("%s\n", __func__);

	
	for (scn = 0; scn < CMDQ_MAX_SCENARIO_COUNT; ++scn) {
		if (cmdq_core_is_request_from_user_space(scn))
			continue;

		CMDQ_MSG("testcase_prefetch_scenarios scenario:%d\n", scn);
		cmdqRecCreate((CMDQ_SCENARIO_ENUM) scn, &hConfig);
		cmdqRecReset(hConfig);
		
		for (index = 0; index < INSTRUCTION_COUNT; ++index)
			cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);

		ret = cmdqRecFlush(hConfig);
		BUG_ON(ret < 0);
	}

	cmdqRecDestroy(hConfig);
	CMDQ_MSG("%s END\n", __func__);
}

#ifndef CMDQ_USE_CCF
void testcase_clkmgr_impl(cgCLKID gateId,
			  char *name,
			  const unsigned long testWriteReg,
			  const uint32_t testWriteValue,
			  const unsigned long testReadReg, const bool verifyWriteResult)
{
#ifndef CONFIG_MTK_FPGA
	uint32_t value = 0;

	CMDQ_MSG("====== %s:%s ======\n", __func__, name);
	CMDQ_VERBOSE("clk:%d, name:%s\n", gateId, name);
	CMDQ_VERBOSE("write reg(0x%lx) to 0x%08x, read reg(0x%lx), verify write result:%d\n",
		     testWriteReg, testWriteValue, testReadReg, verifyWriteResult);

	
	CMDQ_MSG("enable_clock\n");
	enable_clock(gateId, name);

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if ((true == verifyWriteResult) && (testWriteValue != value)) {
		CMDQ_ERR("when enable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		
	}

	
	CMDQ_MSG("disable_clock\n");
	disable_clock(gateId, name);

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if (0 != value) {
		CMDQ_ERR("when disable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		
	}
#endif
}
#else
void testcase_clkmgr_impl(CMDQ_ENG_ENUM engine,
			  char *name,
			  const unsigned long testWriteReg,
			  const uint32_t testWriteValue,
			  const unsigned long testReadReg, const bool verifyWriteResult)
{
#ifndef CONFIG_MTK_FPGA
	uint32_t value = 0;

	CMDQ_MSG("====== %s:%s ======\n", __func__, name);
	CMDQ_VERBOSE("clk engine:%d, name:%s\n", engine, name);
	CMDQ_VERBOSE("write reg(0x%lx) to 0x%08x, read reg(0x%lx), verify write result:%d\n",
		     testWriteReg, testWriteValue, testReadReg, verifyWriteResult);

	
	CMDQ_MSG("enable_clock\n");
	if (engine == CMDQ_ENG_CMDQ) {
		
		cmdq_dev_enable_gce_clock(true);
	} else {
		
		cmdq_mdp_get_func()->enableMdpClock(true, engine);
	}

	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if ((true == verifyWriteResult) && (testWriteValue != value)) {
		CMDQ_ERR("when enable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		
	}

	
	CMDQ_MSG("disable_clock\n");
	if (engine == CMDQ_ENG_CMDQ) {
		
		cmdq_dev_enable_gce_clock(false);
	} else {
		
		cmdq_mdp_get_func()->enableMdpClock(false, engine);
	}


	CMDQ_REG_SET32(testWriteReg, testWriteValue);
	value = CMDQ_REG_GET32(testReadReg);
	if (0 != value) {
		CMDQ_ERR("when disable clock reg(0x%lx) = 0x%08x\n", testReadReg, value);
		
	}
#endif
}
#endif				

static void testcase_clkmgr(void)
{
	CMDQ_MSG("%s\n", __func__);
#ifdef CMDQ_PWR_AWARE
#ifndef CMDQ_USE_CCF
	testcase_clkmgr_impl(MT_CG_INFRA_GCE,
			     "CMDQ_TEST",
			     CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG),
			     0xFFFFDEAD, CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG), true);
#else
	testcase_clkmgr_impl(CMDQ_ENG_CMDQ,
				 "CMDQ_TEST",
				 CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG),
				 0xFFFFDEAD, CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG), true);
#endif				
	cmdq_mdp_get_func()->testcaseClkmgrMdp();
#endif				

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_dram_access(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	uint32_t *regResults;
	dma_addr_t regResultsMVA;
	dma_addr_t dstMVA;
	uint32_t argA;
	uint32_t subsysCode;
	uint32_t *pCmdEnd = NULL;
	unsigned long long data64;

	CMDQ_MSG("%s\n", __func__);

	regResults = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
					       sizeof(uint32_t) * 2, &regResultsMVA, GFP_KERNEL);

	if (regResults) {
		
		regResults[0] = 0xdeaddead;	
		regResults[1] = 0xffffffff;	
	}

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	
	
	
	
	
	argA = CMDQ_TEST_MMSYS_DUMMY_PA;
	subsysCode = cmdq_core_subsys_from_phys_addr(argA);

	pCmdEnd = (uint32_t *) (((char *)handle->pBuffer) + handle->blockSize);

	CMDQ_MSG("pCmdEnd initial=0x%p, reg MVA=%pa, size=%d\n",
		 pCmdEnd, &regResultsMVA, handle->blockSize);

	
	*pCmdEnd = (uint32_t) CMDQ_PHYS_TO_AREG(regResultsMVA);
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	    ((regResultsMVA >> 32) & 0xffff) |
#endif
	    ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	pCmdEnd += 1;

	
	
	
	

	
	*pCmdEnd = CMDQ_DATA_REG_DEBUG;
	pCmdEnd += 1;
	*pCmdEnd =
	    (CMDQ_CODE_READ << 24) | (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (6 <<
												21);
	pCmdEnd += 1;

	
	dstMVA = regResultsMVA + 4;	
	*pCmdEnd = ((uint32_t) dstMVA);
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	    ((dstMVA >> 32) & 0xffff) |
#endif
	    ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (4 << 21);
	pCmdEnd += 1;

	
	*pCmdEnd = CMDQ_DATA_REG_DEBUG;
	pCmdEnd += 1;
	*pCmdEnd = (CMDQ_CODE_WRITE << 24) |
	    (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) | (6 << 21);

	pCmdEnd += 1;

	handle->blockSize += 4 * 8;	

	cmdqRecDumpCommand(handle);

	cmdqRecFlush(handle);

	cmdqRecDumpCommand(handle);

	cmdqRecDestroy(handle);

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_DEBUG_DST);

	CMDQ_MSG("regResults=[0x%08x, 0x%08x]\n", regResults[0], regResults[1]);
	CMDQ_MSG("CMDQ_DATA_REG_DEBUG=0x%08x, CMDQ_DATA_REG_DEBUG_DST=0x%llx\n",
		 CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG)), data64);

	if (regResults[1] != regResults[0]) {
		
		CMDQ_ERR("ERROR!!!!!!\n");
	} else {
		
		CMDQ_MSG("OK!!!!!!\n");
	}

	cmdq_core_free_hw_buffer(cmdq_dev_get(), 2 * sizeof(uint32_t), regResults,
				 regResultsMVA);

	CMDQ_MSG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_long_command(void)
{
	int i;
	cmdqRecHandle handle;
	uint32_t data;
	uint32_t pattern = 0x0;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	
	for (i = 0; i < 64 * 1024 / 8; ++i) {
		pattern = i;
		cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pattern, ~0);
	}
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	
	do {
		if (true == gCmdqTestSecure) {
			CMDQ_LOG("%s, timeout case in secure path\n", __func__);
			break;
		}

		data = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
		if (pattern != data) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x, not pattern 0x%08x\n", data,
				 pattern);
		}
	} while (0);
	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_perisys_apb(void)
{
#ifdef CMDQ_GPR_SUPPORT
	
	
	

	const uint32_t MSDC_SW_DBG_SEL_PA = 0x11230000 + 0xA0;
	const uint32_t MSDC_SW_DBG_OUT_PA = 0x11230000 + 0xA4;
	const uint32_t AUDIO_TOP_CONF0_PA = 0x11220000;

#ifdef CMDQ_OF_SUPPORT
	const unsigned long MSDC_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,MSDC0");
	const unsigned long AUDIO_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,AUDIO");
	const unsigned long MSDC_SW_DBG_OUT = MSDC_VA_BASE + 0xA4;
	const unsigned long AUDIO_TOP_CONF0 = AUDIO_VA_BASE;

	
	
#else
	const uint32_t MSDC_SW_DBG_OUT = 0xF1230000 + 0xA4;
	const uint32_t AUDIO_TOP_CONF0 = 0xF1220000;
#endif

	const uint32_t AUDIO_TOP_MASK = ~0 & ~(1 << 28 |
					       1 << 21 |
					       1 << 17 |
					       1 << 16 |
					       1 << 15 |
					       1 << 11 |
					       1 << 10 |
					       1 << 7 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 1 | 1 << 0);
	cmdqRecHandle handle = NULL;
	uint32_t data = 0;
	uint32_t dataRead = 0;

	CMDQ_MSG("%s\n", __func__);
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWrite(handle, MSDC_SW_DBG_SEL_PA, 1, ~0);
	cmdqRecFlush(handle);
	
	data = CMDQ_REG_GET32(MSDC_SW_DBG_OUT);
	CMDQ_MSG("MSDC_SW_DBG_OUT = 0x%08x=====\n", data);

	
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecReadToDataRegister(handle, MSDC_SW_DBG_OUT_PA, CMDQ_DATA_REG_PQ_COLOR);
	cmdqRecFlush(handle);

	
	dataRead = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != dataRead) {
		
		CMDQ_ERR("TEST FAIL: CMDQ_DATA_REG_PQ_COLOR is 0x%08x, different=====\n", dataRead);
	}

	CMDQ_REG_SET32(AUDIO_TOP_CONF0, ~0);
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("write 0xFFFFFFFF to AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	CMDQ_REG_SET32(AUDIO_TOP_CONF0, 0);
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("Before AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	cmdqRecReset(handle);
	cmdqRecWrite(handle, AUDIO_TOP_CONF0_PA, ~0, AUDIO_TOP_MASK);
	cmdqRecFlush(handle);
	
	data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
	CMDQ_MSG("after AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
	if (data != AUDIO_TOP_MASK) {
		
		CMDQ_ERR("TEST FAIL: AUDIO_TOP_CONF0 is 0x%08x=====\n", data);
	}

	cmdqRecDestroy(handle);

#ifdef CMDQ_OF_SUPPORT
	
	cmdq_dev_free_module_base_VA(MSDC_VA_BASE);
	cmdq_dev_free_module_base_VA(AUDIO_VA_BASE);
#endif

	CMDQ_MSG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif				
}

static void testcase_write_address(void)
{
	dma_addr_t pa = 0;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqCoreAllocWriteAddress(3, &pa);
	CMDQ_LOG("ALLOC: 0x%pa\n", &pa);
	value = cmdqCoreReadWriteAddress(pa);
	CMDQ_LOG("value 0: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 1);
	CMDQ_LOG("value 1: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 2);
	CMDQ_LOG("value 2: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 3);
	CMDQ_LOG("value 3: 0x%08x\n", value);
	value = cmdqCoreReadWriteAddress(pa + 4);
	CMDQ_LOG("value 4: 0x%08x\n", value);

	value = cmdqCoreReadWriteAddress(pa + (4 * 20));
	CMDQ_LOG("value 80: 0x%08x\n", value);

	
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:0, it's a error case\n");
	cmdqCoreFreeWriteAddress(0);

	
	CMDQ_LOG("cmdqCoreFreeWriteAddress, pa:%pa, it's a ok case\n", &pa);
	cmdqCoreFreeWriteAddress(pa);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write_from_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	uint32_t value;
	const uint32_t PATTERN = 0xFFFFDEAD;
	const uint32_t srcGprId = CMDQ_DATA_REG_DEBUG;
	const uint32_t dstRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	const unsigned long dstRegVA = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(dstRegVA, 0x0);

	
	CMDQ_REG_SET32(CMDQ_GPR_R32(srcGprId), PATTERN);
	value = CMDQ_REG_GET32(CMDQ_GPR_R32(srcGprId));
	if (PATTERN != value) {
		CMDQ_ERR("init CMDQ_DATA_REG_DEBUG to 0x%08x failed, value: 0x%08x\n", PATTERN,
			 value);
	}

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWriteFromDataRegister(handle, srcGprId, dstRegPA);
	cmdqRecFlush(handle);

	cmdqRecDumpCommand(handle);

	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(dstRegVA);
	if (PATTERN != value) {
		CMDQ_ERR("%s failed, dstReg value is not 0x%08x, value: 0x%08x\n", __func__,
			 PATTERN, value);
	}

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_read_to_data_reg(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	uint32_t data;
	unsigned long long data64;
	unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST, 0x1234567890ABCDEFULL);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xbeefbeef);	
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0x0);	

	cmdq_get_func()->dumpGPR();

	
#if 1
	cmdqRecReadToDataRegister(handle, CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
				  CMDQ_DATA_REG_PQ_COLOR_DST);
#else
	
	
	
	
	

	
	const uint32_t srcDataReg = CMDQ_DATA_REG_PQ_COLOR;
	const uint32_t dstDataReg = CMDQ_DATA_REG_PQ_COLOR_DST;
	
	
	cmdq_append_command(handle,
			    CMDQ_CODE_RAW,
			    (CMDQ_CODE_MOVE << 24) | (dstDataReg << 16) | (4 << 21) | (2 << 21),
			    srcDataReg);
#endif

	
	cmdqRecReadToDataRegister(handle, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);

	cmdqRecFlush(handle);
	cmdqRecDumpCommand(handle);
	cmdqRecDestroy(handle);

	cmdq_get_func()->dumpGPR();

	
	data = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
	if (data != 0xdeaddead) {
		
		CMDQ_ERR("[Read 32 bit from GPR_Rx]TEST FAIL: PQ reg value is 0x%08x\n", data);
	}

	data64 = 0LL;
	data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST);
	if (0xbeefbeef != data64) {
		CMDQ_ERR("[Read 64 bit from GPR_Px]TEST FAIL: PQ_DST reg value is 0x%llx\n",
			 data64);
	}

	CMDQ_MSG("%s END\n", __func__);
	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_write_reg_from_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const uint32_t PATTEN = 0xBCBCBCBC;
	cmdqRecHandle handle;
	cmdqBackupSlotHandle hSlot = 0;
	uint32_t value = 0;
	long long value64 = 0LL;

	const CMDQ_DATA_REGISTER_ENUM dstRegId = CMDQ_DATA_REG_DEBUG;
	const CMDQ_DATA_REGISTER_ENUM srcRegId = CMDQ_DATA_REG_DEBUG_DST;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, 0xdeaddead);
	CMDQ_REG_SET32(CMDQ_GPR_R32(dstRegId), 0xdeaddead);
	CMDQ_REG_SET64_GPR_PX(srcRegId, 0xdeaddeaddeaddead);

	cmdqBackupAllocateSlot(&hSlot, 1);
	cmdqBackupWriteSlot(hSlot, 0, PATTEN);
	cmdqBackupReadSlot(hSlot, 0, &value);
	if (PATTEN != value) {
		
		CMDQ_ERR("%s, slot init failed\n", __func__);
	}

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	
	cmdqRecReset(handle);

	cmdqRecSetSecure(handle, gCmdqTestSecure);

	
	cmdqRecBackupWriteRegisterFromSlot(handle, hSlot, 0, CMDQ_TEST_MMSYS_DUMMY_PA);

	
	cmdqRecFlush(handle);

	
	cmdqRecDumpCommand(handle);

	
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (PATTEN != value) {
		
		CMDQ_ERR("%s failed, value:0x%x\n", __func__, value);
	}

	value = CMDQ_REG_GET32(CMDQ_GPR_R32(dstRegId));
	value64 = CMDQ_REG_GET64_GPR_PX(srcRegId);
	CMDQ_LOG("srcGPR(%x):0x%llx\n", srcRegId, value64);
	CMDQ_LOG("dstGPR(%x):0x%08x\n", dstRegId, value);

	
	cmdqBackupFreeSlot(hSlot);

	CMDQ_MSG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_backup_reg_to_slot(void)
{
#ifdef CMDQ_GPR_SUPPORT
	cmdqRecHandle handle;
	unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	cmdqBackupSlotHandle hSlot = 0;
	int i;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	
	cmdqBackupAllocateSlot(&hSlot, 5);

	for (i = 0; i < 5; ++i)
		cmdqBackupWriteSlot(hSlot, i, i);

	for (i = 0; i < 5; ++i) {
		cmdqBackupReadSlot(hSlot, i, &value);
		if (value != i) {
			
			CMDQ_ERR("testcase_cmdqBackupWriteSlot FAILED!!!!!\n");
		}
		CMDQ_LOG("testcase_cmdqBackupWriteSlot OK!!!!!\n");
	}

	
	cmdqRecReset(handle);

	cmdqRecSetSecure(handle, gCmdqTestSecure);

	
	for (i = 0; i < 5; ++i)
		cmdqRecBackupRegisterToSlot(handle, hSlot, i, CMDQ_TEST_MMSYS_DUMMY_PA);

	
	cmdqRecFlush(handle);

	
	cmdqRecDumpCommand(handle);

	
	cmdqRecDestroy(handle);

	
	for (i = 0; i < 5; ++i) {
		cmdqBackupReadSlot(hSlot, i, &value);
		CMDQ_LOG("backup slot %d = 0x%08x\n", i, value);

		if (value != 0xdeaddead) {
			
			CMDQ_ERR("content error!!!!!!!!!!!!!!!!!!!!\n");
		}
	}

	
	cmdqBackupFreeSlot(hSlot);

	CMDQ_MSG("%s END\n", __func__);

	return;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return;
#endif
}

static void testcase_update_value_to_slot(void)
{
	int32_t i;
	uint32_t value;
	cmdqRecHandle handle;
	cmdqBackupSlotHandle hSlot = 0;
	const uint32_t PATTERNS[] = {
		0xDEAD0000, 0xDEAD0001, 0xDEAD0002, 0xDEAD0003, 0xDEAD0004
	};

	CMDQ_MSG("%s\n", __func__);

	
	cmdqBackupAllocateSlot(&hSlot, 5);

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	for (i = 0; i < 5; ++i)
		cmdqRecBackupUpdateSlot(handle, hSlot, i, PATTERNS[i]);

	cmdqRecFlush(handle);
	cmdqRecDumpCommand(handle);
	cmdqRecDestroy(handle);

	
	for (i = 0; i < 5; ++i) {
		cmdqBackupReadSlot(hSlot, i, &value);

		if (PATTERNS[i] != value) {
			CMDQ_ERR("slot[%d] = 0x%08x...content error! It should be 0x%08x\n",
				 i, value, PATTERNS[i]);
		} else {
			CMDQ_LOG("slot[%d] = 0x%08x\n", i, value);
		}
	}

	
	cmdqBackupFreeSlot(hSlot);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_poll(void)
{
	cmdqRecHandle handle;

	uint32_t value = 0;
	uint32_t pollingVal = 0x00003001;

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	
	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, pollingVal);
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	CMDQ_MSG("target value is 0x%08x\n", value);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pollingVal, ~0);

	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (pollingVal != value) {
		
		CMDQ_ERR("polling target value is 0x%08x\n", value);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write_with_mask(void)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	const uint32_t MASK = (1 << 16);
	const uint32_t EXPECT_RESULT = PATTERN & MASK;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, 0x0);

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, MASK);
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (EXPECT_RESULT != value) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, EXPECT_RESULT);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_write(void)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_prefetch(void)
{
	cmdqRecHandle handle;
	int i;
	uint32_t value = 0;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);	
	const uint32_t testRegPA = CMDQ_TEST_MMSYS_DUMMY_PA;
	const unsigned long testRegVA = CMDQ_TEST_MMSYS_DUMMY_VA;
	const uint32_t REP_COUNT = 500;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(testRegVA, ~0);

	
	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	for (i = 0; i < REP_COUNT; ++i)
		cmdqRecWrite(handle, testRegPA, PATTERN, ~0);

	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	msleep_interruptible(1000);

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	for (i = 0; i < REP_COUNT; ++i)
		cmdqRecWrite(handle, testRegPA, PATTERN, ~0);

	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	cmdqRecFlushAsync(handle);
	msleep_interruptible(1000);

	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(testRegVA);
	if (value != PATTERN) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_backup_register(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	cmdqRecHandle handle;
	int ret = 0;
	uint32_t regAddr[3] = { CMDQ_TEST_MMSYS_DUMMY_PA,
		CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
		CMDQ_GPR_R32_PA(CMDQ_DATA_REG_2D_SHARPNESS_0)
	};
	uint32_t regValue[3] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xAAAAAAAA);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xBBBBBBBB);
	CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0xCCCCCCCC);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	ret = cmdqRecFlushAndReadRegister(handle, 3, regAddr, regValue);
	cmdqRecDestroy(handle);

	if (regValue[0] != 0xAAAAAAAA) {
		
		CMDQ_ERR("regValue[0] is 0x%08x, wrong!\n", regValue[0]);
	}
	if (regValue[1] != 0xBBBBBBBB) {
		
		CMDQ_ERR("regValue[1] is 0x%08x, wrong!\n", regValue[1]);
	}
	if (regValue[2] != 0xCCCCCCCC) {
		
		CMDQ_ERR("regValue[2] is 0x%08x, wrong!\n", regValue[2]);
	}

	CMDQ_MSG("%s END\n", __func__);

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_get_result(void)
{
#ifdef CMDQ_GPR_SUPPORT
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	int i;
	cmdqRecHandle handle;
	int ret = 0;
	cmdqCommandStruct desc = { 0 };

	int registers[1] = { CMDQ_TEST_MMSYS_DUMMY_PA };
	int result[1] = { 0 };

	CMDQ_MSG("%s\n", __func__);

	
	
	
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_ALL, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	
	cmdq_rec_finalize_command(handle, false);

	
	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;

	desc.regRequest.count = 1;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)registers;
	desc.regValue.count = 1;
	desc.regValue.regValues = (cmdqU32Ptr_t) (unsigned long)result;

	desc.secData.isSecure = handle->secData.isSecure;
	desc.secData.addrMetadataCount = 0;
	desc.secData.addrMetadataMaxCount = 0;
	desc.secData.waitCookie = 0;
	desc.secData.resetExecCnt = false;

	CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

	
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX1_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX2_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX3_STREAM_EOF);

	for (i = 0; i < 1; ++i) {
		ret = cmdqCoreSubmitTask(&desc);
		if (CMDQ_U32_PTR(desc.regValue.regValues)[0] != 0xdeaddead) {
			CMDQ_ERR("TEST FAIL: reg value is 0x%08x\n",
				 CMDQ_U32_PTR(desc.regValue.regValues)[0]);
		}
	}

	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
	return;
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

static void testcase_emergency_buffer(void)
{
	

	const uint32_t longCommandSize = 160 * 1024;
	const uint32_t submitTaskCount = 4;
	cmdqRecHandle handle;
	int32_t i;

	CMDQ_MSG("%s\n", __func__);

	
	if (0 > cmdq_core_enable_emergency_buffer_test(true))
		return;

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	for (i = 0; i < (longCommandSize / CMDQ_INST_SIZE); i++)
		cmdqRecReadToDataRegister(handle, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);

	
	for (i = 0; i < submitTaskCount; i++) {
		CMDQ_LOG("async submit large command(size: %d), count:%d\n", longCommandSize, i);
		cmdqRecFlushAsync(handle);
	}

	msleep_interruptible(1000);

	
	cmdq_core_enable_emergency_buffer_test(false);
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static int _testcase_simplest_command_loop_submit(const uint32_t loop, CMDQ_SCENARIO_ENUM scenario,
						  const long long engineFlag,
						  const bool isSecureTask)
{
	cmdqRecHandle handle;
	int32_t i;

	CMDQ_MSG("%s\n", __func__);

	cmdqRecCreate(scenario, &handle);
	for (i = 0; i < loop; i++) {
		CMDQ_MSG("pid: %d, flush:%4d, engineFlag:0x%llx, isSecureTask:%d\n",
			 current->pid, i, engineFlag, isSecureTask);
		cmdqRecReset(handle);
		cmdqRecSetSecure(handle, isSecureTask);
		handle->engineFlag = engineFlag;
		cmdqRecFlush(handle);
	}
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);

	return 0;
}

static int _testcase_thread_dispatch(void *data)
{
	long long engineFlag;

	engineFlag = *((long long *)data);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG, engineFlag, false);

	return 0;
}

static void testcase_thread_dispatch(void)
{
	char threadName[20];
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const long long engineFlag1 = (0x1 << CMDQ_ENG_ISP_IMGI) | (0x1 << CMDQ_ENG_ISP_IMGO);
	const long long engineFlag2 = (0x1 << CMDQ_ENG_MDP_RDMA0) | (0x1 << CMDQ_ENG_MDP_WDMA);

	CMDQ_MSG("%s\n", __func__);
	CMDQ_MSG("=============== 2 THREAD with different engines ===============\n");

	sprintf(threadName, "cmdqKTHR_%llx", engineFlag1);
	pKThread1 = kthread_run(_testcase_thread_dispatch, (void *)(&engineFlag1), threadName);
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	sprintf(threadName, "cmdqKTHR_%llx", engineFlag2);
	pKThread2 = kthread_run(_testcase_thread_dispatch, (void *)(&engineFlag2), threadName);
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
		return;
	}

	msleep_interruptible(5 * 1000);

	
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, engineFlag1, false);
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, engineFlag2, false);

	CMDQ_MSG("%s END\n", __func__);
}

static int _testcase_full_thread_array(void *data)
{
	
	

	cmdqRecHandle handle;
	int32_t i;

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);

	for (i = 0; i < 50; i++) {
		CMDQ_LOG("pid: %d, flush:%6d\n", current->pid, i);

		if (40 == i) {
			CMDQ_LOG("set token: %d to 1\n", CMDQ_SYNC_TOKEN_USER_0);
			cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		}

		cmdqRecFlushAsync(handle);
	}
	cmdqRecDestroy(handle);

	return 0;
}

static void testcase_full_thread_array(void)
{
	char threadName[20];
	struct task_struct *pKThread;

	CMDQ_MSG("%s\n", __func__);

	sprintf(threadName, "cmdqKTHR");
	pKThread = kthread_run(_testcase_full_thread_array, NULL, threadName);
	if (IS_ERR(pKThread)) {
		
		CMDQ_ERR("create thread failed, thread:%s\n", threadName);
	}

	msleep_interruptible(5 * 1000);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_module_full_dump(void)
{
	cmdqRecHandle handle;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();

	CMDQ_MSG("%s\n", __func__);

	
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(1);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	
	handle->engineFlag = ~(CMDQ_ENG_DISP_GROUP_BITS);

	CMDQ_LOG("%s, engine: 0x%llx, it's a timeout case\n", __func__, handle->engineFlag);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecFlush(handle);

	
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(0);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_profile_marker(void)
{
	cmdqRecHandle handle;
	
	

	CMDQ_MSG("%s\n", __func__);

	CMDQ_MSG("%s: write op without profile marker\n", __func__);
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBCBCBCBC, ~0);
	cmdqRecFlush(handle);

	CMDQ_MSG("%s: write op with profile marker\n", __func__);
	cmdqRecReset(handle);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0x11111111, ~0);
	cmdqRecProfileMarker(handle, "WRI_BEGIN");
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0x22222222, ~0);
	cmdqRecProfileMarker(handle, "WRI_END");

	cmdqRecDumpCommand(handle);
	cmdqRecFlush(handle);

	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_estimate_command_exec_time(void)
{
	cmdqRecHandle handle;
	cmdqBackupSlotHandle hSlot = 0;

	cmdqBackupAllocateSlot(&hSlot, 1);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);

	CMDQ_MSG("%s\n", __func__);

	CMDQ_LOG("=====write(1), write_w_mask(2), poll(2), wait(2), sync(1), eof(1), jump(1)\n");
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBBBBBBBA, ~0);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBBBBBBBB, 0x1);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xBBBBBBBC, 0x3);

	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xCCCCCCCA, ~0);
	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, 0xCCCCCCCB, 0x1);

	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecClearEventToken(handle, CMDQ_SYNC_TOKEN_USER_1);

	cmdqRecDumpCommand(handle);
	cmdqRecEstimateCommandExecTime(handle);

	CMDQ_LOG("=====slots...\n");

	cmdqRecReset(handle);
	cmdqRecBackupRegisterToSlot(handle, hSlot, 0, CMDQ_TEST_MMSYS_DUMMY_PA);
	cmdqRecBackupWriteRegisterFromSlot(handle, hSlot, 0, CMDQ_TEST_MMSYS_DUMMY_PA);
	cmdqRecBackupUpdateSlot(handle, hSlot, 0, 0xDEADDEAD);

	cmdqRecDumpCommand(handle);
	cmdqRecEstimateCommandExecTime(handle);

	CMDQ_MSG("%s END\n", __func__);

	cmdqBackupFreeSlot(hSlot);
	cmdqRecDestroy(handle);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec.h"
#include "cmdq_sec_iwc_common.h"
#include "cmdqSecTl_Api.h"
int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwcCommand,
						       TaskStruct *pTask, int32_t thread,
						       CmdqSecFillIwcCB iwcFillCB, void *data);
#endif

void testcase_secure_basic(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s\n", __func__);

	do {

		CMDQ_MSG("=========== Hello cmdqSecTl ===========\n ");
		status =
		    cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_TEST_HELLO_TL, NULL,
								   CMDQ_INVALID_THREAD, NULL, NULL);
		if (0 > status) {
			
			CMDQ_ERR("entry cmdqSecTL failed, status:%d\n", status);
		}

		CMDQ_MSG("=========== Hello cmdqSecDr ===========\n ");
		status =
		    cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_TEST_DUMMY, NULL,
								   CMDQ_INVALID_THREAD, NULL, NULL);
		if (0 > status) {
			
			CMDQ_ERR("entry cmdqSecDr failed, status:%d\n", status);
		}
	} while (0);

	CMDQ_MSG("%s END\n", __func__);
#endif
}

void testcase_secure_disp_scenario(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	
	
	
	cmdqRecHandle hDISP;
	cmdqRecHandle hDisableDISP;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);

	CMDQ_MSG("%s\n", __func__);
	CMDQ_LOG("=========== secure primary path ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &hDISP);
	cmdqRecReset(hDISP);
	cmdqRecSetSecure(hDISP, true);

	cmdqRecWrite(hDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);

	cmdqRecFlush(hDISP);
	cmdqRecDestroy(hDISP);
	CMDQ_LOG("=========== disp secure primary path ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH, &hDisableDISP);
	cmdqRecReset(hDisableDISP);
	cmdqRecSetSecure(hDisableDISP, true);
	cmdqRecWrite(hDisableDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlush(hDisableDISP);
	cmdqRecDestroy(hDisableDISP);

	CMDQ_MSG("%s END\n", __func__);
#endif
}

void testcase_secure_meta_data(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdqRecHandle hReqMDP;
	cmdqRecHandle hReqDISP;
	const uint32_t PATTERN_MDP = (1 << 0) | (1 << 2) | (1 << 16);
	const uint32_t PATTERN_DISP = 0xBCBCBCBC;
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	CMDQ_MSG("=========== MDP case ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqMDP);
	cmdqRecReset(hReqMDP);
	cmdqRecSetSecure(hReqMDP, true);

	
	hReqMDP->engineFlag =
	    (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WDMA) | (1LL << CMDQ_ENG_MDP_WROT0);

	
	cmdqRecSecureEnableDAPC(hReqMDP,
				(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WDMA) |
				(1LL << CMDQ_ENG_MDP_WROT0));
	cmdqRecSecureEnablePortSecurity(hReqMDP,
					(1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_WDMA) |
					(1LL << CMDQ_ENG_MDP_WROT0));

	
	cmdqRecWrite(hReqMDP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_MDP, ~0);

	cmdqRecFlush(hReqMDP);
	cmdqRecDestroy(hReqMDP);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN_MDP) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN_MDP);
	}

	CMDQ_MSG("=========== DISP case ===========\n");
	cmdqRecCreate(CMDQ_SCENARIO_SUB_DISP, &hReqDISP);
	cmdqRecReset(hReqDISP);
	cmdqRecSetSecure(hReqDISP, true);

	
	cmdqRecSecureEnableDAPC(hReqDISP, (1LL << CMDQ_ENG_DISP_WDMA1));
	cmdqRecSecureEnablePortSecurity(hReqDISP, (1LL << CMDQ_ENG_DISP_WDMA1));

	
	cmdqRecWrite(hReqDISP, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN_DISP, ~0);

	cmdqRecFlush(hReqDISP);
	cmdqRecDestroy(hReqDISP);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN_DISP) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN_DISP);
	}

	CMDQ_MSG("%s END\n", __func__);
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
#endif

}

void testcase_submit_after_error_happened(void)
{
	cmdqRecHandle handle;
	const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
	const uint32_t pollingVal = 0x00003001;

	CMDQ_MSG("%s\n", __func__);
	CMDQ_MSG("=========== timeout case ===========\n");

	
	
	CMDQ_REG_SET32(MMSYS_DUMMY_REG, ~0);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);

	cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pollingVal, ~0);
	cmdqRecFlush(handle);

	CMDQ_MSG("=========== okay case ===========\n");
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_write_stress_test(void)
{
	int32_t loop;

	CMDQ_MSG("%s\n", __func__);

	loop = 1;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	loop = 100;
	CMDQ_MSG("=============== loop x %d ===============\n", loop);
	_testcase_simplest_command_loop_submit(loop, CMDQ_SCENARIO_DEBUG, 0, gCmdqTestSecure);

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_multiple_command(void)
{
#define TEST_PREFETCH_MARKER_LOOP 2

	int32_t i;
	int32_t ret;
	cmdqRecHandle handle[TEST_PREFETCH_MARKER_LOOP] = { 0 };
	TaskStruct *pTask[TEST_PREFETCH_MARKER_LOOP] = { 0 };

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s\n", __func__);
	for (i = 0; i < TEST_PREFETCH_MARKER_LOOP; i++) {
		CMDQ_MSG("=============== flush:%d/%d ===============\n",
			i, TEST_PREFETCH_MARKER_LOOP);

		cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &(handle[i]));
		cmdqRecReset(handle[i]);
		cmdqRecSetSecure(handle[i], false);

		
		cmdqRecEnablePrefetch(handle[i]);
		cmdqRecWait(handle[i], CMDQ_SYNC_TOKEN_USER_0);
		cmdqRecDisablePrefetch(handle[i]);

		
		cmdqRecWrite(handle[i], CMDQ_TEST_MMSYS_DUMMY_PA, 0x3000, ~0);

		cmdq_rec_finalize_command(handle[i], false);
		cmdqRecDumpCommand(handle[i]);

		ret = _test_submit_async(handle[i], &pTask[i]);
	}

	for (i = 0; i < TEST_PREFETCH_MARKER_LOOP; ++i) {
		if (NULL == pTask[i]) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		msleep_interruptible(100);

		CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
		cmdqRecDestroy(handle[i]);
	}

	CMDQ_MSG("%s END\n", __func__);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static int _testcase_concurrency(void *data)
{
	uint32_t securePath;

	securePath = *((uint32_t *) data);

	CMDQ_MSG("start secure(%d) path\n", securePath);
	_testcase_simplest_command_loop_submit(1000, CMDQ_SCENARIO_DEBUG,
					       (0x1 << CMDQ_ENG_MDP_RSZ0), securePath);

	return 0;
}
#endif

static void testcase_concurrency_for_normal_path_and_secure_path(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct task_struct *pKThread1;
	struct task_struct *pKThread2;
	const uint32_t securePath[2] = { 0, 1 };

	CMDQ_MSG("%s\n", __func__);

	pKThread1 = kthread_run(_testcase_concurrency, (void *)(&securePath[0]), "cmdqNormal");
	if (IS_ERR(pKThread1)) {
		CMDQ_ERR("create cmdqNormal failed\n");
		return;
	}

	pKThread2 = kthread_run(_testcase_concurrency, (void *)(&securePath[1]), "cmdqSecure");
	if (IS_ERR(pKThread2)) {
		CMDQ_ERR("create cmdqSecure failed\n");
		return;
	}

	msleep_interruptible(5 * 1000);

	
	_testcase_simplest_command_loop_submit(1, CMDQ_SCENARIO_DEBUG, 0x0, false);

	CMDQ_MSG("%s END\n", __func__);

	return;
#endif
}

void testcase_async_write_stress_test(void)
{
#if 0
#define LOOP 100

	int32_t i;
	int32_t ret;
	cmdqRecHandle handle[LOOP] = { 0 };
	TaskStruct *pTask[LOOP] = { 0 };

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s\n", __func__);
	for (i = 0; i < LOOP; i++) {
		CMDQ_MSG("=============== flush:%d/%d ===============\n", i, LOOP);

		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &(handle[i]));
		cmdqRecReset(handle[i]);
		cmdqRecSetSecure(handle[i], gCmdqTestSecure);

		cmdqRecWait(handle[i], CMDQ_SYNC_TOKEN_USER_0);

		cmdq_rec_finalize_command(handle[i], false);

		ret = _test_submit_async(handle[i], &pTask[i]);
	}

	
	for (i = 0; i < LOOP; ++i) {

		if (NULL == pTask[i]) {
			CMDQ_ERR("%s pTask[%d] is NULL\n ", __func__, i);
			continue;
		}

		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		msleep_interruptible(100);

		CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
		ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
		cmdqRecDestroy(handle[i]);
	}

	CMDQ_MSG("%s END\n", __func__);
#endif
}

static void testcase_nonsuspend_irq(void)
{
	cmdqRecHandle handle, handle2;
	TaskStruct *pTask, *pTask2;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);
	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle, false);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdqRecReset(handle2);
	cmdqRecSetSecure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	
	cmdqRecWait(handle2, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle2, false);

	_test_submit_async(handle, &pTask);
	_test_submit_async(handle2, &pTask2);

	msleep_interruptible(500);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);

	
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	CMDQ_MSG("IRQ: After set user sw token\n");

	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdqCoreWaitAndReleaseTask(pTask2, 500);
	cmdqRecDestroy(handle);
	cmdqRecDestroy(handle2);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_module_full_mdp_engine(void)
{
	cmdqRecHandle handle;
	const bool alreadyEnableLog = cmdq_core_should_print_msg();

	CMDQ_MSG("%s\n", __func__);

	
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(1);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	
	handle->engineFlag = ~(CMDQ_ENG_DISP_GROUP_BITS);

	CMDQ_LOG("%s, engine: 0x%llx, it's a engine clock test case\n",
		 __func__, handle->engineFlag);

	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecFlush(handle);

	
	if (false == alreadyEnableLog)
		cmdq_core_set_log_level(0);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_trigger_engine_dispatch_check(void)
{
	cmdqRecHandle handle, handle2, hTrigger;
	TaskStruct *pTask;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	uint32_t loopIndex = 0;

	CMDQ_MSG("%s\n", __func__);

	
	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);
	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_rec_finalize_command(handle, false);
	_test_submit_async(handle, &pTask);

	
	cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
	cmdqRecReset(hTrigger);
	cmdqRecWait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecStartLoop(hTrigger);
	
	CMDQ_MSG("%s before start sleep and trigger token\n", __func__);
	for (loopIndex = 0; loopIndex < 10; loopIndex++) {
		msleep_interruptible(500);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		CMDQ_MSG("%s after sleep 5000 and send (%d)\n", __func__, loopIndex);
	}

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle2);
	cmdqRecReset(handle2);
	cmdqRecSetSecure(handle2, gCmdqTestSecure);
	handle2->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdqRecWrite(handle2, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlush(handle2);
	cmdqRecDestroy(handle2);

	
	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdqRecDestroy(handle);
	cmdqRecDestroy(hTrigger);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_complicated_engine_thread(void)
{
#define TASK_COUNT 6
	cmdqRecHandle handle[TASK_COUNT] = { 0 };
	TaskStruct *pTask[TASK_COUNT] = { 0 };
	uint64_t engineFlag[TASK_COUNT] = { 0 };
	uint32_t taskIndex = 0;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	
	engineFlag[0] = (1LL << CMDQ_ENG_MDP_RDMA0);
	engineFlag[1] = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_RSZ0);
	engineFlag[2] = (1LL << CMDQ_ENG_MDP_RSZ0);
	engineFlag[3] = (1LL << CMDQ_ENG_MDP_TDSHP0);
	engineFlag[4] = (1LL << CMDQ_ENG_MDP_RDMA0) | (1LL << CMDQ_ENG_MDP_TDSHP0);
	engineFlag[5] = (1LL << CMDQ_ENG_MDP_TDSHP0) | (1LL << CMDQ_ENG_MDP_RSZ0);

	for (taskIndex = 0; taskIndex < TASK_COUNT; taskIndex++) {
		
		cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle[taskIndex]);
		cmdqRecReset(handle[taskIndex]);
		cmdqRecSetSecure(handle[taskIndex], gCmdqTestSecure);
		handle[taskIndex]->engineFlag = engineFlag[taskIndex];
		cmdqRecWait(handle[taskIndex], CMDQ_SYNC_TOKEN_USER_0);
		cmdq_rec_finalize_command(handle[taskIndex], false);
		_test_submit_async(handle[taskIndex], &pTask[taskIndex]);
	}

	for (taskIndex = 0; taskIndex < TASK_COUNT; taskIndex++) {
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		
		cmdqCoreWaitAndReleaseTask(pTask[taskIndex], 500);
		cmdqRecDestroy(handle[taskIndex]);
		msleep_interruptible(1000);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_append_task_verify(void)
{
	cmdqRecHandle handle, handle2;
	TaskStruct *pTask, *pTask2;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	uint32_t loopIndex = 0;

	CMDQ_MSG("%s\n", __func__);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle2);
	for (loopIndex = 0; loopIndex < 2; loopIndex++) {
		
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
		
		CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

		
		
		cmdqRecReset(handle);
		cmdqRecSetSecure(handle, gCmdqTestSecure);
		if (loopIndex == 1)
			cmdqRecEnablePrefetch(handle);
		cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
		if (loopIndex == 1)
			cmdqRecDisablePrefetch(handle);
		cmdq_rec_finalize_command(handle, false);

		
		cmdqRecReset(handle2);
		cmdqRecSetSecure(handle2, gCmdqTestSecure);
		if (loopIndex == 1)
			cmdqRecEnablePrefetch(handle2);
		cmdqRecWrite(handle2, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
		if (loopIndex == 1)
			cmdqRecDisablePrefetch(handle2);
		cmdq_rec_finalize_command(handle2, false);

		_test_submit_async(handle, &pTask);
		_test_submit_async(handle2, &pTask2);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
		
		cmdqCoreWaitAndReleaseTask(pTask, 500);
		cmdqCoreWaitAndReleaseTask(pTask2, 500);

		
		value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
		if (value != PATTERN) {
			
			CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
		}
	}

	cmdqRecDestroy(handle);
	cmdqRecDestroy(handle2);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_manual_suspend_resume_test(void)
{
	cmdqRecHandle handle;
	TaskStruct *pTask, *pTask2;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle, false);

	_test_submit_async(handle, &pTask);

	
	cmdqCoreSuspend();
	cmdqCoreResumedNotifier();

	_test_submit_async(handle, &pTask2);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	
	cmdqCoreWaitAndReleaseTask(pTask2, 500);
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_timeout_wait_early_test(void)
{
	cmdqRecHandle handle;
	TaskStruct *pTask;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle, false);

	_test_submit_async(handle, &pTask);

	cmdqRecFlush(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	
	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_timeout_reorder_test(void)
{
	cmdqRecHandle handle;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, false);
	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdq_rec_finalize_command(handle, false);
	handle->priority = 0;
	cmdqRecFlushAsync(handle);
	handle->priority = 2;
	cmdqRecFlushAsync(handle);
	handle->priority = 4;
	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_error_irq(void)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	TaskStruct *pTask;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);
	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdqRecWait(handle, CMDQ_SYNC_TOKEN_USER_0);
	cmdqRecFlushAsync(handle);

	
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdq_append_command(handle, CMDQ_CODE_JUMP, -1, 0);
	cmdqRecDumpCommand(handle);
	cmdqRecFlushAsync(handle);

	
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlushAsync(handle);

	
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	{
		const uint32_t UNKNOWN_OP = 0x50;
		uint32_t *pCommand;

		pCommand = (uint32_t *) ((uint8_t *) handle->pBuffer + handle->blockSize);
		*pCommand++ = 0x0;
		*pCommand++ = (UNKNOWN_OP << 24);
		handle->blockSize += 8;
	}
	cmdqRecFlushAsync(handle);

	
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = (1LL << CMDQ_ENG_MDP_RDMA0);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdq_rec_finalize_command(handle, false);
	_test_submit_async(handle, &pTask);

	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	cmdqCoreWaitAndReleaseTask(pTask, 500);
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_open_buffer_dump(int32_t scenario, int32_t bufferSize)
{
	CMDQ_MSG("%s\n", __func__);

	CMDQ_LOG("[TESTCASE]CONFIG: bufferSize: %d, scenario: %d\n", bufferSize, scenario);
	cmdq_core_set_command_buffer_dump(scenario, bufferSize);

	CMDQ_MSG("%s END\n", __func__);
}

static void testcase_check_dts_correctness(void)
{
	CMDQ_MSG("%s\n", __func__);

	cmdq_dev_test_dts_correctness();

	CMDQ_MSG("%s END\n", __func__);
}

static int32_t testcase_monitor_callback(unsigned long data)
{
	uint32_t i;
	uint32_t monitorValue[CMDQ_MONITOR_EVENT_MAX];
	uint32_t durationTime[CMDQ_MONITOR_EVENT_MAX];

	if (false == gEventMonitor.status)
		return 0;

	for (i = 0; i < gEventMonitor.monitorNUM; i++) {
		
		cmdqBackupReadSlot(gEventMonitor.slotHandle, i, &monitorValue[i]);

		switch (gEventMonitor.waitType[i]) {
		case CMDQ_MOITOR_TYPE_WFE:
			durationTime[i] = (monitorValue[i] - gEventMonitor.previousValue[i]) * 76;
			CMDQ_LOG("[MONITOR][WFE] event: %s, duration: (%u ns)\n",
				cmdq_core_get_event_name_ENUM(gEventMonitor.monitorEvent[i]), durationTime[i]);
			CMDQ_MSG("[MONITOR][WFE] time:(%u ns)\n", monitorValue[i]);
			break;
		case CMDQ_MOITOR_TYPE_WAIT_NO_CLEAR:
			durationTime[i] = (monitorValue[i] - gEventMonitor.previousValue[i]) * 76;
			CMDQ_LOG("[MONITOR][Wait] event: %s, duration: (%u ns)\n",
				cmdq_core_get_event_name_ENUM(gEventMonitor.monitorEvent[i]), durationTime[i]);
			CMDQ_MSG("[MONITOR] time:(%u ns)\n", monitorValue[i]);
			break;
		case CMDQ_MOITOR_TYPE_QUERYREGISTER:
			CMDQ_LOG("[MONITOR] Register:0x08%llx, value:(0x04%x)\n", gEventMonitor.monitorEvent[i],
				monitorValue[i]);
			break;
		}
		
		gEventMonitor.previousValue[i] = monitorValue[i];
	}

	return 0;
}

static void testcase_monitor_trigger_initialization(void)
{
	
	cmdqBackupAllocateSlot(&gEventMonitor.slotHandle, CMDQ_MONITOR_EVENT_MAX);
	
	cmdqRecCreate(CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP, &gEventMonitor.cmdqHandle);
	cmdqRecReset(gEventMonitor.cmdqHandle);
	
	cmdqRecEnablePrefetch(gEventMonitor.cmdqHandle);
}

static void testcase_monitor_trigger(uint32_t waitType, uint64_t monitorEvent)
{
	int32_t eventID;
	bool successAddInstruction = false;

	CMDQ_MSG("%s\n", __func__);

	if (true == gEventMonitor.status) {
		
		gEventMonitor.status = false;

		CMDQ_LOG("stop monitor thread\n");

		
		cmdqRecStopLoop(gEventMonitor.cmdqHandle);
		
		cmdqBackupFreeSlot(gEventMonitor.slotHandle);
		
		cmdqRecDestroy(gEventMonitor.cmdqHandle);
		
		memset(&(gEventMonitor), 0x0, sizeof(gEventMonitor));
	}

	if (0 == gEventMonitor.monitorNUM) {
		
		testcase_monitor_trigger_initialization();
	} else if (gEventMonitor.monitorNUM >= CMDQ_MONITOR_EVENT_MAX) {
		waitType = CMDQ_MOITOR_TYPE_FLUSH;
		CMDQ_LOG("[MONITOR] reach MAX monitor number: %d, force flush\n", gEventMonitor.monitorNUM);
	}

	switch (waitType) {
	case CMDQ_MOITOR_TYPE_FLUSH:
		if (gEventMonitor.monitorNUM > 0) {
			CMDQ_LOG("start monitor thread\n");

			
			cmdqRecDisablePrefetch(gEventMonitor.cmdqHandle);
			
			gEventMonitor.status = true;
			
			cmdqRecStartLoopWithCallback(gEventMonitor.cmdqHandle, &testcase_monitor_callback, 0);
			cmdqRecDumpCommand(gEventMonitor.cmdqHandle);
		}
		break;
	case CMDQ_MOITOR_TYPE_WFE:
		eventID = (int32_t)monitorEvent;
		if (eventID >= 0 && eventID < CMDQ_SYNC_TOKEN_MAX) {
			cmdqRecWait(gEventMonitor.cmdqHandle, eventID);
			cmdqRecBackupRegisterToSlot(gEventMonitor.cmdqHandle, gEventMonitor.slotHandle,
				gEventMonitor.monitorNUM, CMDQ_APXGPT2_COUNT);
			successAddInstruction = true;
		}
		break;
	case CMDQ_MOITOR_TYPE_WAIT_NO_CLEAR:
		eventID = (int32_t)monitorEvent;
		if (eventID >= 0 && eventID < CMDQ_SYNC_TOKEN_MAX) {
			cmdqRecWaitNoClear(gEventMonitor.cmdqHandle, eventID);
			cmdqRecBackupRegisterToSlot(gEventMonitor.cmdqHandle, gEventMonitor.slotHandle,
				gEventMonitor.monitorNUM, CMDQ_APXGPT2_COUNT);
			successAddInstruction = true;
		}
		break;
	case CMDQ_MOITOR_TYPE_QUERYREGISTER:
		cmdqRecBackupRegisterToSlot(gEventMonitor.cmdqHandle, gEventMonitor.slotHandle,
				gEventMonitor.monitorNUM, monitorEvent);
		successAddInstruction = true;
		break;
	}

	if (true == successAddInstruction) {
		gEventMonitor.waitType[gEventMonitor.monitorNUM] = waitType;
		gEventMonitor.monitorEvent[gEventMonitor.monitorNUM] = monitorEvent;
		gEventMonitor.monitorNUM++;
	}

	CMDQ_MSG("%s\n", __func__);
}

static void testcase_poll_monitor_delay_continue(struct work_struct *workItem)
{
	
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_POLL_MONITOR);
	CMDQ_LOG("monitor after delay: (%d)ms, start polling again\n", gPollMonitor.delayTime);
}

static int32_t testcase_poll_monitor_callback(unsigned long data)
{
	uint32_t pollTime;

	if (false == gPollMonitor.status)
		return 0;

	cmdqBackupReadSlot(gPollMonitor.slotHandle, 0, &pollTime);
	CMDQ_LOG("monitor, time: (%u ns), regAddr: 0x%08llx, regValue: 0x%08llx, regMask=0x%08llx\n",
		pollTime, gPollMonitor.pollReg, gPollMonitor.pollValue, gPollMonitor.pollMask);
	schedule_delayed_work(&gPollMonitor.delayContinueWork, gPollMonitor.delayTime);

	return 0;
}

static void testcase_poll_monitor_trigger(uint64_t pollReg, uint64_t pollValue, uint64_t pollMask)
{
	CMDQ_MSG("%s\n", __func__);

	if (true == gPollMonitor.status) {
		
		gPollMonitor.status = false;

		CMDQ_LOG("stop polling monitor thread: regAddr: 0x%08llx\n", gPollMonitor.pollReg);

		
		cmdqRecStopLoop(gPollMonitor.cmdqHandle);
		
		cmdqBackupFreeSlot(gPollMonitor.slotHandle);
		cmdqRecDestroy(gPollMonitor.cmdqHandle);
		
		memset(&(gPollMonitor), 0x0, sizeof(gPollMonitor));
	}

	if (-1 == pollReg)
		return;

	CMDQ_LOG("start polling monitor thread, regAddr=0x%08llx, regValue=0x%08llx, regMask=0x%08llx\n",
			pollReg, pollValue, pollMask);

	
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_POLL_MONITOR);
	
	cmdqBackupAllocateSlot(&gPollMonitor.slotHandle, 1);
	
	cmdqRecCreate(CMDQ_SCENARIO_LOWP_TRIGGER_LOOP, &gPollMonitor.cmdqHandle);
	cmdqRecReset(gPollMonitor.cmdqHandle);
	
	cmdqRecWait(gPollMonitor.cmdqHandle, CMDQ_SYNC_TOKEN_POLL_MONITOR);
	if (0 == cmdqRecPoll(gPollMonitor.cmdqHandle, pollReg, pollValue, pollMask)) {
		cmdqRecBackupRegisterToSlot(gPollMonitor.cmdqHandle, gPollMonitor.slotHandle, 0, CMDQ_APXGPT2_COUNT);
		
		gPollMonitor.pollReg = pollReg;
		gPollMonitor.pollValue = pollValue;
		gPollMonitor.pollMask = pollMask;
		gPollMonitor.delayTime = 1;
		gPollMonitor.status = true;
		INIT_DELAYED_WORK(&gPollMonitor.delayContinueWork, testcase_poll_monitor_delay_continue);
		
		cmdqRecStartLoopWithCallback(gPollMonitor.cmdqHandle, &testcase_poll_monitor_callback, 0);
		
		cmdqRecDumpCommand(gPollMonitor.cmdqHandle);
	} else {
		
		cmdqBackupFreeSlot(gPollMonitor.slotHandle);
		cmdqRecDestroy(gPollMonitor.cmdqHandle);
	}

	CMDQ_MSG("%s\n", __func__);
}

static void testcase_acquire_resource(CMDQ_EVENT_ENUM resourceEvent, bool acquireExpected)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	int32_t acquireResult;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	acquireResult = cmdqRecWriteForResource(handle, resourceEvent,
		CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	if (acquireResult < 0) {
		
		if (acquireExpected) {
			
			CMDQ_ERR("Acquire resource fail: it's not expected!\n");
		} else {
			
			CMDQ_LOG("Acquire resource fail: it's expected!\n");
		}
	} else {
		if (!acquireExpected) {
			
			CMDQ_ERR("Acquire resource success: it's not expected!\n");
		} else {
			
			CMDQ_LOG("Acquire resource success: it's expected!\n");
		}
	}
	cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN && acquireExpected) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

static int32_t testcase_res_release_cb(CMDQ_EVENT_ENUM resourceEvent)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);

	CMDQ_MSG("%s\n", __func__);
	

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	
	cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_USER_0);
	
	cmdqRecWriteAndReleaseResource(handle, resourceEvent,
		CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);

	CMDQ_MSG("%s END\n", __func__);
	return 0;
}

static int32_t testcase_res_available_cb(CMDQ_EVENT_ENUM resourceEvent)
{
	CMDQ_MSG("%s\n", __func__);
	testcase_acquire_resource(resourceEvent, true);
	CMDQ_MSG("%s END\n", __func__);
	return 0;
}

static void testcase_notify_and_delay_submit(uint32_t delayTimeMS)
{
	cmdqRecHandle handle;
	const uint32_t PATTERN = (1 << 0) | (1 << 2) | (1 << 16);
	uint32_t value = 0;
	const uint64_t engineFlag = (1LL << CMDQ_ENG_MDP_WROT0);
	const CMDQ_EVENT_ENUM resourceEvent = CMDQ_SYNC_RESOURCE_WROT0;
	uint32_t contDelay;

	CMDQ_MSG("%s\n", __func__);

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	cmdqCoreSetResourceCallback(resourceEvent,
		testcase_res_available_cb, testcase_res_release_cb);

	testcase_acquire_resource(resourceEvent, true);

	
	if (delayTimeMS > 0) {
		CMDQ_MSG("Before delay for acquire\n");
		msleep_interruptible(delayTimeMS);
		CMDQ_MSG("Before lock and delay\n");
		cmdqCoreLockResource(engineFlag, true);
		msleep_interruptible(delayTimeMS);
		CMDQ_MSG("After lock and delay\n");
	}

	
	CMDQ_REG_SET32(CMDQ_TEST_MMSYS_DUMMY_VA, ~0);

	
	cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
	cmdqRecSetSecure(handle, gCmdqTestSecure);
	handle->engineFlag = engineFlag;
	cmdqRecWaitNoClear(handle, resourceEvent);
	cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
	cmdqRecFlushAsync(handle);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
	msleep_interruptible(2000);

	
	for (contDelay = 300; contDelay < CMDQ_DELAY_RELEASE_RESOURCE_MS*1.2; contDelay += 300) {
		CMDQ_MSG("Before delay and flush\n");
		msleep_interruptible(contDelay);
		CMDQ_MSG("After delay\n");
		cmdqRecFlush(handle);
		CMDQ_MSG("After flush\n");
	}
	
	cmdqRecFlushAsync(handle);
	testcase_acquire_resource(resourceEvent, false);

	cmdqRecFlushAsync(handle);
	cmdqRecDestroy(handle);

	
	value = CMDQ_REG_GET32(CMDQ_TEST_MMSYS_DUMMY_VA);
	if (value != PATTERN) {
		
		CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
	}

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_round(uint32_t loopCount, uint32_t cmdCount, bool withMask, bool withWait)
{
#define TEST_PREFETCH_LOOP 3

	int32_t i, j, k;
	int32_t ret;
	cmdqRecHandle handle[TEST_PREFETCH_LOOP] = {0};
	TaskStruct *pTask[TEST_PREFETCH_LOOP] = { 0 };

	
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

	CMDQ_MSG("%s: count:%d, withMask:%d, withWait:%d\n", __func__, cmdCount, withMask, withWait);
	for (i = 0; i < TEST_PREFETCH_LOOP; i++) {
		CMDQ_MSG("=============== flush:%d/%d ===============\n", i, TEST_PREFETCH_LOOP);

		for (k = 0; k < loopCount; k++) {
			CMDQ_MSG("=============== loop:%d/%d ===============\n", k, loopCount);
			cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &(handle[i]));
			cmdqRecReset(handle[i]);
			cmdqRecSetSecure(handle[i], false);

			
			if (i == 1)
				cmdqRecEnablePrefetch(handle[i]); 

			if (withWait)
				cmdqRecWait(handle[i], CMDQ_SYNC_TOKEN_USER_0);

			cmdqRecProfileMarker(handle[i], "ANA_BEGIN");
			for (j = 0; j < cmdCount; j++) {
				
				if (withMask)
					cmdqRecWrite(handle[i], CMDQ_TEST_MMSYS_DUMMY_PA, 0x3210, ~0xfff0);
				else
					cmdqRecWrite(handle[i], CMDQ_TEST_MMSYS_DUMMY_PA, 0x3210, ~0);
			}

			if (i == 1)
				cmdqRecDisablePrefetch(handle[i]); 

			cmdqRecProfileMarker(handle[i], "ANA_END");
			cmdq_rec_finalize_command(handle[i], false);

			ret = _test_submit_async(handle[i], &pTask[i]);

			if (withWait) {
				msleep_interruptible(500);
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_USER_0);
			}

			CMDQ_MSG("wait 0x%p, i:%2d========\n", pTask[i], i);
			ret = cmdqCoreWaitAndReleaseTask(pTask[i], 500);
			cmdqRecDestroy(handle[i]);
		}
	}

	CMDQ_MSG("%s END\n", __func__);
}

void testcase_prefetch_from_DTS(void)
{
	int32_t i, j;
	uint32_t thread_prefetch_size;

	CMDQ_MSG("%s\n", __func__);

	for (i = 0; i < CMDQ_MAX_THREAD_COUNT; i++) {
		thread_prefetch_size = cmdq_core_get_thread_prefetch_size(i);

		for (j = 100; j <= (thread_prefetch_size + 60); j += 40) {
			testcase_prefetch_round(1, j, false, true);
			testcase_prefetch_round(1, j, false, false);
		}
	}

	CMDQ_MSG("%s END\n", __func__);
}

typedef enum CMDQ_TESTCASE_ENUM {
	CMDQ_TESTCASE_ALL = 0,
	CMDQ_TESTCASE_BASIC = 1,
	CMDQ_TESTCASE_ERROR = 2,
	CMDQ_TESTCASE_FPGA = 3,
	CMDQ_TESTCASE_READ_REG_REQUEST,	
	CMDQ_TESTCASE_GPR,
	CMDQ_TESTCASE_SW_TIMEOUT_HANDLE,

	CMDQ_TESTCASE_END,	
} CMDQ_TESTCASE_ENUM;

static void testcase_general_handling(int32_t testID)
{
	switch (testID) {
#ifdef CMDQ_DVFS_SPECIAL
	case 999:
		cmdq_dump_special_thread("");
		break;
#endif
	case 121:
		testcase_prefetch_from_DTS();
		break;
	case 120:
		testcase_notify_and_delay_submit(16);
		break;
	case 119:
		testcase_check_dts_correctness();
		break;
	case 118:
		testcase_error_irq();
		break;
	case 117:
		testcase_timeout_reorder_test();
		break;
	case 116:
		testcase_timeout_wait_early_test();
		break;
	case 115:
		testcase_manual_suspend_resume_test();
		break;
	case 114:
		testcase_append_task_verify();
		break;
	case 113:
		testcase_trigger_engine_dispatch_check();
		break;
	case 112:
		testcase_complicated_engine_thread();
		break;
	case 111:
		testcase_module_full_mdp_engine();
		break;
	case 110:
		testcase_nonsuspend_irq();
		break;
	case 109:
		testcase_estimate_command_exec_time();
		break;
	case 108:
		testcase_profile_marker();
		break;
	case 107:
		testcase_prefetch_multiple_command();
		break;
	case 106:
		testcase_concurrency_for_normal_path_and_secure_path();
		break;
	case 105:
		testcase_async_write_stress_test();
		break;
	case 104:
		testcase_submit_after_error_happened();
		break;
	case 103:
		testcase_secure_meta_data();
		break;
	case 102:
		testcase_secure_disp_scenario();
		break;
	case 101:
		testcase_write_stress_test();
		break;
	case 100:
		testcase_secure_basic();
		break;
	case 99:
		testcase_write();
		testcase_write_with_mask();
		break;
	case 98:
		testcase_errors();
		break;
	case 97:
		testcase_scenario();
		break;
	case 96:
		testcase_sync_token();
		break;
	case 95:
		testcase_write_address();
		break;
	case 94:
		testcase_async_request();
		break;
	case 93:
		testcase_async_suspend_resume();
		break;
	case 92:
		testcase_async_request_partial_engine();
		break;
	case 91:
		testcase_prefetch_scenarios();
		break;
	case 90:
		testcase_loop();
		break;
	case 89:
		testcase_trigger_thread();
		break;
	case 88:
		testcase_multiple_async_request();
		break;
	case 87:
		testcase_get_result();
		break;
	case 86:
		testcase_read_to_data_reg();
		break;
	case 85:
		testcase_dram_access();
		break;
	case 84:
		testcase_backup_register();
		break;
	case 83:
		testcase_fire_and_forget();
		break;
	case 82:
		testcase_sync_token_threaded();
		break;
	case 81:
		testcase_long_command();
		break;
	case 80:
		testcase_clkmgr();
		break;
	case 79:
		testcase_perisys_apb();
		break;
	case 78:
		testcase_backup_reg_to_slot();
		break;
	case 77:
		testcase_thread_dispatch();
		break;
	case 76:
		testcase_emergency_buffer();
		break;
	case 75:
		testcase_full_thread_array();
		break;
	case 74:
		testcase_module_full_dump();
		break;
	case 73:
		testcase_write_from_data_reg();
		break;
	case 72:
		testcase_update_value_to_slot();
		break;
	case 71:
		testcase_poll();
		break;
	case 70:
		testcase_write_reg_from_slot();
		break;
	case CMDQ_TESTCASE_FPGA:
		testcase_write();
		testcase_write_with_mask();
		testcase_poll();
		testcase_scenario();
		testcase_estimate_command_exec_time();
		testcase_prefetch_multiple_command();
		testcase_write_stress_test();
		testcase_async_suspend_resume();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_trigger_thread();
		testcase_multiple_async_request();
		testcase_get_result();
		testcase_dram_access();
		testcase_backup_register();
		testcase_fire_and_forget();
		testcase_sync_token_threaded();
		testcase_long_command();
		testcase_backup_reg_to_slot();
		testcase_write_from_data_reg();
		testcase_update_value_to_slot();
		break;
	case CMDQ_TESTCASE_ERROR:
		testcase_errors();
		break;
	case CMDQ_TESTCASE_BASIC:
		testcase_write();
		testcase_poll();
		testcase_scenario();
		break;
	case CMDQ_TESTCASE_READ_REG_REQUEST:
		testcase_get_result();
		break;
	case CMDQ_TESTCASE_GPR:
		testcase_read_to_data_reg();	
		testcase_dram_access();
		break;
	case CMDQ_TESTCASE_ALL:
		testcase_multiple_async_request();
		testcase_read_to_data_reg();
		testcase_get_result();
		testcase_errors();

		testcase_scenario();
		testcase_sync_token();

		testcase_write();
		testcase_poll();
		testcase_write_address();
		testcase_async_request();
		testcase_async_suspend_resume();
		testcase_async_request_partial_engine();
		testcase_prefetch_scenarios();
		testcase_loop();
		testcase_trigger_thread();
		testcase_prefetch();

		

		testcase_long_command();

		
		testcase_dram_access();
		testcase_perisys_apb();
		testcase_backup_register();
		testcase_fire_and_forget();

		testcase_backup_reg_to_slot();

		testcase_emergency_buffer();

		testcase_thread_dispatch();
		testcase_full_thread_array();
		testcase_module_full_dump();
		break;
	default:
		CMDQ_LOG("[TESTCASE]CONFIG Not Found: gCmdqTestSecure: %d, testType: %lld\n",
			 gCmdqTestSecure, gCmdqTestConfig[0]);
		break;
	}
}

ssize_t cmdq_test_proc(struct file *fp, char __user *u, size_t s, loff_t *l)
{
	int64_t testParameter[CMDQ_TESTCASE_PARAMETER_MAX];

	mutex_lock(&gCmdqTestProcLock);
	
	smp_mb();

	CMDQ_LOG("[TESTCASE]CONFIG: gCmdqTestSecure: %d, testType: %lld\n",
		 gCmdqTestSecure, gCmdqTestConfig[0]);
	CMDQ_LOG("[TESTCASE]CONFIG PARAMETER: [1]: %lld, [2]: %lld, [3]: %lld\n",
		 gCmdqTestConfig[1], gCmdqTestConfig[2], gCmdqTestConfig[3]);
	memcpy(testParameter, gCmdqTestConfig, sizeof(testParameter));
	mutex_unlock(&gCmdqTestProcLock);

	
	CMDQ_MSG("//\n//\n//\ncmdq_test_proc\n");

	cmdq_get_func()->testSetup();

	switch (testParameter[0]) {
	case CMDQ_TEST_TYPE_NORMAL:
	case CMDQ_TEST_TYPE_SECURE:
		testcase_general_handling((int32_t)testParameter[1]);
		break;
	case CMDQ_TEST_TYPE_MONITOR_EVENT:
		
		testcase_monitor_trigger((uint32_t)testParameter[1], (uint64_t)testParameter[2]);
		break;
	case CMDQ_TEST_TYPE_MONITOR_POLL:
		
		testcase_poll_monitor_trigger((uint64_t)testParameter[1], (uint64_t)testParameter[2],
			(uint64_t)testParameter[3]);
		break;
	case CMDQ_TEST_TYPE_OPEN_COMMAND_DUMP:
		
		testcase_open_buffer_dump((int32_t)testParameter[1], (int32_t)testParameter[2]);
		break;
	case CMDQ_TEST_TYPE_DUMP_DTS:
		cmdq_core_dump_dts_setting();
		break;
	case CMDQ_TEST_TYPE_FEATURE_CONFIG:
		if (0 > (int32_t)testParameter[1])
			cmdq_core_dump_feature();
		else
			cmdq_core_set_feature((int32_t)testParameter[1], (uint32_t)testParameter[2]);

	default:
		break;
	}

	cmdq_get_func()->testCleanup();

	CMDQ_MSG("cmdq_test_proc ended\n");
	return 0;
}

static ssize_t cmdq_write_test_proc_config(struct file *file,
					   const char __user *userBuf, size_t count, loff_t *data)
{
	char desc[50];
	long long int testConfig[CMDQ_TESTCASE_PARAMETER_MAX];
	int32_t len = 0;

	do {
		
		len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
		if (copy_from_user(desc, userBuf, len)) {
			CMDQ_ERR("TEST_CONFIG: data fail, length:%d\n", len);
			break;
		}
		desc[len] = '\0';

		
		memset(testConfig, -1, sizeof(testConfig));

		
		if (sscanf(desc, "%lld %lld %lld %lld", &testConfig[0], &testConfig[1],
			&testConfig[2], &testConfig[3]) <= 0) {
			
			CMDQ_ERR("TEST_CONFIG: sscanf failed, len:%d\n", len);
			break;
		}

		if ((testConfig[0] < 0) || (testConfig[0] >= CMDQ_TEST_TYPE_MAX)) {
			CMDQ_ERR("TEST_CONFIG: testType:%lld, newTestSuit:%lld\n", testConfig[0], testConfig[1]);
			break;
		}

		mutex_lock(&gCmdqTestProcLock);
		
		smp_mb();

		memcpy(&gCmdqTestConfig, &testConfig, sizeof(testConfig));
		if (testConfig[0] == CMDQ_TEST_TYPE_NORMAL)
			gCmdqTestSecure = false;
		else
			gCmdqTestSecure = true;

		mutex_unlock(&gCmdqTestProcLock);
	} while (0);

	return count;
}

void cmdq_test_init_setting(void)
{
	memset(&(gEventMonitor), 0x0, sizeof(gEventMonitor));
	memset(&(gPollMonitor), 0x0, sizeof(gPollMonitor));
}

static int cmdq_test_open(struct inode *pInode, struct file *pFile)
{
	return 0;
}

static const struct file_operations cmdq_fops = {
	.owner = THIS_MODULE,
	.open = cmdq_test_open,
	.read = cmdq_test_proc,
	.write = cmdq_write_test_proc_config,
};

static int __init cmdq_test_init(void)
{
	CMDQ_MSG("cmdq_test_init\n");

	
	gCmdqTestProcEntry = proc_mkdir("cmdq_test", NULL);
	if (NULL != gCmdqTestProcEntry) {
		if (NULL == proc_create("test", 0660, gCmdqTestProcEntry, &cmdq_fops)) {
			
			CMDQ_MSG("cmdq_test_init failed\n");
		}
	}

	return 0;
}

static void __exit cmdq_test_exit(void)
{
	CMDQ_MSG("cmdq_test_exit\n");
	if (NULL != gCmdqTestProcEntry) {
		proc_remove(gCmdqTestProcEntry);
		gCmdqTestProcEntry = NULL;
	}
}
module_init(cmdq_test_init);
module_exit(cmdq_test_exit);

MODULE_LICENSE("GPL");
#endif				
