/*
** Id: @(#) gl_rst.c@@
*/

/*! \file   gl_rst.c
    \brief  Main routines for supporintg MT6620 whole-chip reset mechanism

    This file contains the support routines of Linux driver for MediaTek Inc. 802.11
    Wireless LAN Adapters.
*/

/*
** Log: gl_rst.c
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 11 10 2011 cp.wu
 * [WCXRP00001098] [MT6620 Wi-Fi][Driver] Replace printk by DBG LOG macros in linux porting layer
 * 1. eliminaite direct calls to printk in porting layer.
 * 2. replaced by DBGLOG, which would be XLOG on ALPS platforms.
 *
 * 04 22 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for
 * RESET_START and RESET_END events
 * skip power-off handshaking when RESET indication is received.
 *
 * 04 14 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for
 * RESET_START and RESET_END events
 * 1. add code to put whole-chip resetting trigger when abnormal firmware assertion is detected
 * 2. add dummy function for both Win32 and Linux part.
 *
 * 03 30 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for
 * RESET_START and RESET_END events
 * use netlink unicast instead of broadcast
 *
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include "precomp.h"
#include "gl_rst.h"

#if CFG_CHIP_RESET_SUPPORT

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
static BOOLEAN fgResetTriggered = FALSE;
BOOLEAN fgIsResetting = FALSE;
UINT32 g_IsNeedDoChipReset;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static RESET_STRUCT_T wifi_rst;

static void mtk_wifi_reset(struct work_struct *work);
static void mtk_wifi_trigger_reset(struct work_struct *work);

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static void *glResetCallback(ENUM_WMTDRV_TYPE_T eSrcType,
			     ENUM_WMTDRV_TYPE_T eDstType,
			     ENUM_WMTMSG_TYPE_T eMsgType, void *prMsgBody, unsigned int u4MsgLength);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for
 *        1. register wifi reset callback
 *        2. initialize wifi reset work
 *
 * @param none
 *
 * @retval none
 */
/*----------------------------------------------------------------------------*/
VOID glResetInit(VOID)
{
#if (defined(MT6797) && (MTK_WCN_SINGLE_MODULE == 0)) || defined(MT6630)
	/* 1. Register reset callback */
	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_WIFI, (PF_WMT_CB) glResetCallback);
#endif
	/* 2. Initialize reset work */
	INIT_WORK(&(wifi_rst.rst_work), mtk_wifi_reset);
	INIT_WORK(&(wifi_rst.rst_trigger_work), mtk_wifi_trigger_reset);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for
 *        1. deregister wifi reset callback
 *
 * @param none
 *
 * @retval none
 */
/*----------------------------------------------------------------------------*/
VOID glResetUninit(VOID)
{
#if (defined(MT6797) && (MTK_WCN_SINGLE_MODULE == 0)) || defined(MT6630)
	/* 1. Deregister reset callback */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_WIFI);
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is invoked when there is reset messages indicated
 *
 * @param   eSrcType
 *          eDstType
 *          eMsgType
 *          prMsgBody
 *          u4MsgLength
 *
 * @retval
 */
/*----------------------------------------------------------------------------*/
static void *glResetCallback(ENUM_WMTDRV_TYPE_T eSrcType,
			     ENUM_WMTDRV_TYPE_T eDstType,
			     ENUM_WMTMSG_TYPE_T eMsgType, void *prMsgBody, unsigned int u4MsgLength)
{
	switch (eMsgType) {
	case WMTMSG_TYPE_RESET:
		if (u4MsgLength == sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
			P_ENUM_WMTRSTMSG_TYPE_T prRstMsg = (P_ENUM_WMTRSTMSG_TYPE_T) prMsgBody;

			switch (*prRstMsg) {
			case WMTRSTMSG_RESET_START:
				DBGLOG(INIT, WARN, "Whole chip reset start!\n");
				fgIsResetting = TRUE;
				fgResetTriggered = FALSE;
				wifi_reset_start();
				break;

			case WMTRSTMSG_RESET_END:
				DBGLOG(INIT, WARN, "Whole chip reset end!\n");
				fgIsResetting = FALSE;
				wifi_rst.rst_data = RESET_SUCCESS;
				schedule_work(&(wifi_rst.rst_work));
				break;

			case WMTRSTMSG_RESET_END_FAIL:
				DBGLOG(INIT, WARN, "Whole chip reset fail!\n");
				fgIsResetting = FALSE;
				wifi_rst.rst_data = RESET_FAIL;
				schedule_work(&(wifi_rst.rst_work));
				break;

			default:
				break;
			}
		}
		break;

	default:
		break;
	}

	return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for wifi reset
 *
 * @param   skb
 *          info
 *
 * @retval  0
 *          nonzero
 */
/*----------------------------------------------------------------------------*/
static void mtk_wifi_reset(struct work_struct *work)
{
	RESET_STRUCT_T *rst = container_of(work, RESET_STRUCT_T, rst_work);

	wifi_reset_end(rst->rst_data);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for generating reset request to WMT
 *
 * @param   None
 *
 * @retval  None
 */
/*----------------------------------------------------------------------------*/
VOID glSendResetRequest(VOID)
{
	/* WMT thread would trigger whole chip reset itself */
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for checking if connectivity chip is resetting
 *
 * @param   None
 *
 * @retval  TRUE
 *          FALSE
 */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsResetting(VOID)
{
	return fgIsResetting;
}

static void mtk_wifi_trigger_reset(struct work_struct *work)
{
	BOOLEAN fgResult = FALSE;

	fgResetTriggered = TRUE;
	fgResult = mtk_wcn_wmt_assert(WMTDRV_TYPE_WIFI, 0x40);
	DBGLOG(INIT, INFO, "reset result %d\n", fgResult);
}

BOOLEAN kalIsResetTriggeringOrResetting(VOID)
{
	BOOLEAN fgReseting = FALSE;

#if CFG_WMT_RESET_API_SUPPORT
	/* Protect more scope than using only one kalIsResetting() */
	fgReseting = (kalIsResetting() || fgResetTriggered);
#endif
	return fgReseting;
}

BOOLEAN glResetTrigger(P_ADAPTER_T prAdapter)
{
	BOOLEAN fgResult = TRUE;

#if CFG_WMT_RESET_API_SUPPORT
	if (kalIsResetting() || fgResetTriggered) {
		DBGLOG(INIT, ERROR,
			"Skip triggering whole-chip reset during resetting! Chip[%04X E%u]\n",
			MTK_CHIP_REV,
			wlanGetEcoVersion(prAdapter));
		DBGLOG(INIT, ERROR,
			"FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
			(prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			(prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			(prAdapter->rVerInfo.u2FwPeerVersion >> 8),
			(prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7)));

		fgResult = TRUE;
	} else {
		DBGLOG(INIT, ERROR,
		"Trigger whole-chip reset! Chip[%04X E%u] FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
			     MTK_CHIP_REV,
			     wlanGetEcoVersion(prAdapter),
			     (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			     (prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			     (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			     (prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			     (prAdapter->rVerInfo.u2FwPeerVersion >> 8),
			     (prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7)));

		schedule_work(&(wifi_rst.rst_trigger_work));
	}
#endif

	return fgResult;
}
UINT32 wlan_polling_cpupcr(UINT32 times, UINT32 sleep)
{
#if defined(MT6797)
	UINT32 j;

	for (j = 0; j < times; j++) {
		DBGLOG(INIT, ERROR, "i:%d,cpupcr:%08x\n", j, wmt_plat_read_cpupcr());
		kalMsleep(sleep);
	}
#else
	DBGLOG(INIT, ERROR, "This chip don't support polling cpupcr\n");
#endif
	return 0;
}

#endif /* CFG_CHIP_RESET_SUPPORT */
