



#include "precomp.h"

#ifndef LINUX
#include <limits.h>
#else
#include <linux/limits.h>
#endif

#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>
#include "gl_cfg80211.h"
#include "gl_vendor.h"

#define RX_RESPONSE_TIMEOUT (1000)




#if CFG_MGMT_FRAME_HANDLING
static PROCESS_RX_MGT_FUNCTION apfnProcessRxMgtFrame[MAX_NUM_OF_FC_SUBTYPES] = {
#if CFG_SUPPORT_AAA
	aaaFsmRunEventRxAssoc,	
#else
	NULL,			
#endif 
	saaFsmRunEventRxAssoc,	
#if CFG_SUPPORT_AAA
	aaaFsmRunEventRxAssoc,	
#else
	NULL,			
#endif 
	saaFsmRunEventRxAssoc,	
#if (CFG_SUPPORT_ADHOC) || (CFG_SUPPORT_AAA)
	bssProcessProbeRequest,	
#else
	NULL,			
#endif 
	scanProcessBeaconAndProbeResp,	
	NULL,			
	NULL,			
	scanProcessBeaconAndProbeResp,	
	NULL,			
	saaFsmRunEventRxDisassoc,	
	authCheckRxAuthFrameTransSeq,	
	saaFsmRunEventRxDeauth,	
	nicRxProcessActionFrame,	
	NULL,			
	NULL			
};
#endif



VOID nicRxInitialize(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucMemHandle;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	UINT_32 i;

	DEBUGFUNC("nicRxInitialize");

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;

	
	kalMemZero((PVOID) prRxCtrl->pucRxCached, prRxCtrl->u4RxCachedSize);

	
	QUEUE_INITIALIZE(&prRxCtrl->rFreeSwRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rReceivedRfbList);
	QUEUE_INITIALIZE(&prRxCtrl->rIndicatedRfbList);

	pucMemHandle = prRxCtrl->pucRxCached;
	for (i = CFG_RX_MAX_PKT_NUM; i != 0; i--) {
		prSwRfb = (P_SW_RFB_T) pucMemHandle;

		nicRxSetupRFB(prAdapter, prSwRfb);
		nicRxReturnRFB(prAdapter, prSwRfb);

		pucMemHandle += ALIGN_4(sizeof(SW_RFB_T));
	}

	ASSERT(prRxCtrl->rFreeSwRfbList.u4NumElem == CFG_RX_MAX_PKT_NUM);
	
	ASSERT((ULONG) (pucMemHandle - prRxCtrl->pucRxCached) == prRxCtrl->u4RxCachedSize);

	
	RX_RESET_ALL_CNTS(prRxCtrl);

#if CFG_SDIO_RX_AGG
	prRxCtrl->pucRxCoalescingBufPtr = prAdapter->pucCoalescingBufCached;
	HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, CFG_SDIO_MAX_RX_AGG_NUM);
#else
	HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, 1);
#endif

#if CFG_HIF_STATISTICS
	prRxCtrl->u4TotalRxAccessNum = 0;
	prRxCtrl->u4TotalRxPacketNum = 0;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
	prRxCtrl->u4QueuedCnt = 0;
	prRxCtrl->u4DequeuedCnt = 0;
#endif

}				

VOID nicRxUninitialize(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	nicRxFlush(prAdapter);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rReceivedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
		} else {
			break;
		}
	} while (TRUE);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
			if (prSwRfb->pvPacket)
				kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
			prSwRfb->pvPacket = NULL;
		} else {
			break;
		}
	} while (TRUE);

}				

VOID nicRxFillRFB(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_HIF_RX_HEADER_T prHifRxHdr;

	UINT_32 u4PktLen = 0;
	UINT_32 u4MacHeaderLen;
	UINT_32 u4HeaderOffset;

	DEBUGFUNC("nicRxFillRFB");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(prHifRxHdr);

	u4PktLen = prHifRxHdr->u2PacketLen;

	u4HeaderOffset = (UINT_32) (prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_OFFSET_MASK);
	u4MacHeaderLen = (UINT_32) (prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_LEN)
	    >> HIF_RX_HDR_HEADER_LEN_OFFSET;

	
	

	prSwRfb->u2HeaderLen = (UINT_16) u4MacHeaderLen;
	prSwRfb->pvHeader = (PUINT_8) prHifRxHdr + HIF_RX_HDR_SIZE + u4HeaderOffset;
	prSwRfb->u2PacketLen = (UINT_16) (u4PktLen - (HIF_RX_HDR_SIZE + u4HeaderOffset));

	
	

#if 0
	if (prHifRxHdr->ucReorder & HIF_RX_HDR_80211_HEADER_FORMAT) {
		prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_802_11_FORMAT;
		DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_802_11_FORMAT\n");
	}

	if (prHifRxHdr->ucReorder & HIF_RX_HDR_DO_REORDER) {
		prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_DO_REORDERING;
		DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_DO_REORDERING\n");

		
		if (prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_BAR_FRAME) {
			prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_BAR_FRAME;
			DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_BAR_FRAME\n");
		}

		prSwRfb->u2SSN = prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_SEQ_NO_MASK;
		prSwRfb->ucTid = (UINT_8) ((prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_TID_MASK)
					   >> HIF_RX_HDR_TID_OFFSET);
		DBGLOG(RX, TRACE, "u2SSN = %d, ucTid = %d\n", prSwRfb->u2SSN, prSwRfb->ucTid);
	}

	if (prHifRxHdr->ucReorder & HIF_RX_HDR_WDS) {
		prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_AMP_WDS;
		DBGLOG(RX, TRACE, "HIF_RX_HDR_FLAG_AMP_WDS\n");
	}
#endif
}

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
VOID nicRxFillChksumStatus(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb, IN UINT_32 u4TcpUdpIpCksStatus)
{

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (prAdapter->u4CSUMFlags != CSUM_NOT_SUPPORTED) {
		if (u4TcpUdpIpCksStatus & RX_CS_TYPE_IPv4) {	
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
			if (u4TcpUdpIpCksStatus & RX_CS_STATUS_IP) {	
				prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_FAILED;
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_SUCCESS;
			}

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {	
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_TCP) {	
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_SUCCESS;
				}
			} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_UDP) {	
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_UDP) {	
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_SUCCESS;
				}
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
			}
		} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_IPv6) {	
			prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_SUCCESS;

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {	
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_TCP) {	
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_SUCCESS;
				}
			} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_UDP) {	
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_UDP) {	
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_FAILED;
				} else {
					prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_SUCCESS;
				}
			} else {
				prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
			}
		} else {
			prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
			prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
		}
	}

}
#endif 

VOID nicRxProcessPktWithoutReorder(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_TX_CTRL_T prTxCtrl;
	BOOLEAN fgIsRetained = FALSE;
	UINT_32 u4CurrentRxBufferCount;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	DEBUGFUNC("nicRxProcessPktWithoutReorder");
	

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prTxCtrl = &prAdapter->rTxCtrl;
	ASSERT(prTxCtrl);

	u4CurrentRxBufferCount = prRxCtrl->rFreeSwRfbList.u4NumElem;
	fgIsRetained = (((u4CurrentRxBufferCount +
			  qmGetRxReorderQueuedBufferCount(prAdapter) +
			  prTxCtrl->i4PendingFwdFrameCount) < CFG_RX_RETAINED_PKT_THRESHOLD) ? TRUE : FALSE);

	

	if (kalProcessRxPacket(prAdapter->prGlueInfo,
			       prSwRfb->pvPacket,
			       prSwRfb->pvHeader,
			       (UINT_32) prSwRfb->u2PacketLen, fgIsRetained, prSwRfb->aeCSUM) != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, ERROR, "kalProcessRxPacket return value != WLAN_STATUS_SUCCESS\n");
		ASSERT(0);

		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

		if (prStaRec) {
#if CFG_ENABLE_WIFI_DIRECT
			if (prStaRec->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX && prAdapter->fgIsP2PRegistered == TRUE)
				GLUE_SET_PKT_FLAG_P2P(prSwRfb->pvPacket);
#endif
#if CFG_ENABLE_BT_OVER_WIFI
			if (prStaRec->ucNetTypeIndex == NETWORK_TYPE_BOW_INDEX)
				GLUE_SET_PKT_FLAG_PAL(prSwRfb->pvPacket);
#endif

			
			STATS_RX_PASS2OS_INC(prStaRec, prSwRfb);
		}
		prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] = prSwRfb->pvPacket;
		prRxCtrl->ucNumIndPacket++;

	if (fgIsRetained) {
		prRxCtrl->apvRetainedPacket[prRxCtrl->ucNumRetainedPacket] = prSwRfb->pvPacket;
		prRxCtrl->ucNumRetainedPacket++;
		
		nicRxSetupRFB(prAdapter, prSwRfb);
		nicRxReturnRFB(prAdapter, prSwRfb);
	} else {
		prSwRfb->pvPacket = NULL;
		nicRxReturnRFB(prAdapter, prSwRfb);
	}
}

VOID nicRxProcessForwardPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_MSDU_INFO_T prMsduInfo, prRetMsduInfoList;
	P_TX_CTRL_T prTxCtrl;
	P_RX_CTRL_T prRxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessForwardPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	QUEUE_REMOVE_HEAD(&prTxCtrl->rFreeMsduInfoList, prMsduInfo, P_MSDU_INFO_T);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

	if (prMsduInfo && kalProcessRxPacket(prAdapter->prGlueInfo,
					     prSwRfb->pvPacket,
					     prSwRfb->pvHeader,
					     (UINT_32) prSwRfb->u2PacketLen,
					     prRxCtrl->rFreeSwRfbList.u4NumElem <
					     CFG_RX_RETAINED_PKT_THRESHOLD ? TRUE : FALSE,
					     prSwRfb->aeCSUM) == WLAN_STATUS_SUCCESS) {

		prMsduInfo->eSrc = TX_PACKET_FORWARDING;
		
		nicTxFillMsduInfo(prAdapter, prMsduInfo, (P_NATIVE_PACKET) (prSwRfb->pvPacket));
		
		prMsduInfo->ucNetworkType = HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr);

		
		prSwRfb->pvPacket = NULL;
		nicRxReturnRFB(prAdapter, prSwRfb);

		
		GLUE_INC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);

		
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
		prRetMsduInfoList = qmEnqueueTxPackets(prAdapter, prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

		if (prRetMsduInfoList != NULL) {	
			nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfoList);
			nicTxReturnMsduInfo(prAdapter, prRetMsduInfoList);
		}
		
		if (prTxCtrl->i4PendingFwdFrameCount > 0)
			kalSetEvent(prAdapter->prGlueInfo);
	} else		
		nicRxReturnRFB(prAdapter, prSwRfb);

}

VOID nicRxProcessGOBroadcastPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_SW_RFB_T prSwRfbDuplicated;
	P_TX_CTRL_T prTxCtrl;
	P_RX_CTRL_T prRxCtrl;
	P_HIF_RX_HEADER_T prHifRxHdr;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessGOBroadcastPkt");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(prHifRxHdr);

	ASSERT(CFG_NUM_OF_QM_RX_PKT_NUM >= 16);

	if (prRxCtrl->rFreeSwRfbList.u4NumElem
	    >= (CFG_RX_MAX_PKT_NUM - (CFG_NUM_OF_QM_RX_PKT_NUM - 16 ))) {

		
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfbDuplicated, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (prSwRfbDuplicated) {
			kalMemCopy(prSwRfbDuplicated->pucRecvBuff,
				   prSwRfb->pucRecvBuff, ALIGN_4(prHifRxHdr->u2PacketLen + HIF_RX_HW_APPENDED_LEN));

			prSwRfbDuplicated->ucPacketType = HIF_RX_PKT_TYPE_DATA;
			prSwRfbDuplicated->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);
			nicRxFillRFB(prAdapter, prSwRfbDuplicated);

			
			prSwRfbDuplicated->eDst = RX_PKT_DESTINATION_FORWARD;

			
			nicRxProcessForwardPkt(prAdapter, prSwRfbDuplicated);
		}
	} else {
		DBGLOG(RX, WARN, "Stop to forward BMC packet due to less free Sw Rfb %u\n",
				  prRxCtrl->rFreeSwRfbList.u4NumElem);
	}

	
	prSwRfb->eDst = RX_PKT_DESTINATION_HOST;
	nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);

}

VOID nicRxProcessDataPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prRetSwRfb, prNextSwRfb;
	P_HIF_RX_HEADER_T prHifRxHdr;
	P_STA_RECORD_T prStaRec;
	BOOLEAN fIsDummy = FALSE;

	DEBUGFUNC("nicRxProcessDataPacket");
	

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prHifRxHdr = prSwRfb->prHifRxHdr;
	prRxCtrl = &prAdapter->rRxCtrl;

	fIsDummy = (prHifRxHdr->u2PacketLen >= 12) ? FALSE : TRUE;

	nicRxFillRFB(prAdapter, prSwRfb);

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	{
		UINT_32 u4TcpUdpIpCksStatus;

		u4TcpUdpIpCksStatus = *((PUINT_32) ((ULONG) prHifRxHdr + (UINT_32) (ALIGN_4(prHifRxHdr->u2PacketLen))));
		nicRxFillChksumStatus(prAdapter, prSwRfb, u4TcpUdpIpCksStatus);

	}
#endif 

	prStaRec = cnmGetStaRecByIndex(prAdapter, prHifRxHdr->ucStaRecIdx);
	if (secCheckClassError(prAdapter, prSwRfb, prStaRec) == TRUE && prAdapter->fgTestMode == FALSE) {
#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4QueuedCnt++;
#endif
		prRetSwRfb = qmHandleRxPackets(prAdapter, prSwRfb);
		if (prRetSwRfb != NULL) {
			do {
				
				prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prRetSwRfb);
				if (fIsDummy == TRUE) {
					nicRxReturnRFB(prAdapter, prRetSwRfb);
					RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
					DBGLOG(RX, WARN, "Drop Dummy Packets");

				} else {
					switch (prRetSwRfb->eDst) {
					case RX_PKT_DESTINATION_HOST:
#if ARP_MONITER_ENABLE
					if (IS_STA_IN_AIS(prStaRec))
						qmHandleRxArpPackets(prAdapter, prRetSwRfb);
#endif
						nicRxProcessPktWithoutReorder(prAdapter, prRetSwRfb);
						break;

					case RX_PKT_DESTINATION_FORWARD:
						nicRxProcessForwardPkt(prAdapter, prRetSwRfb);
						break;

					case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
						nicRxProcessGOBroadcastPkt(prAdapter, prRetSwRfb);
						break;

					case RX_PKT_DESTINATION_NULL:
						nicRxReturnRFB(prAdapter, prRetSwRfb);
						RX_INC_CNT(prRxCtrl, RX_DST_NULL_DROP_COUNT);
						RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
						break;

					default:
						break;
					}
				}
#if CFG_HIF_RX_STARVATION_WARNING
				prRxCtrl->u4DequeuedCnt++;
#endif
				prRetSwRfb = prNextSwRfb;
			} while (prRetSwRfb);
		}
	} else {
		nicRxReturnRFB(prAdapter, prSwRfb);
		RX_INC_CNT(prRxCtrl, RX_CLASS_ERR_DROP_COUNT);
		RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
	}
}


UINT_8 nicRxProcessGSCNEvent(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_WIFI_EVENT_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	struct sk_buff *skb;
	struct wiphy *wiphy;

	UINT_32 real_num = 0;

	P_EVENT_GSCAN_SCAN_AVAILABLE_T prEventGscnAvailable;
	P_EVENT_GSCAN_RESULT_T prEventBuffer;
	P_WIFI_GSCAN_RESULT_T prEventGscnResult;
	INT_32 i4Status = -EINVAL;
	struct nlattr *attr;
	UINT_32 scan_id;
	UINT_8 scan_flag;
	P_EVENT_GSCAN_CAPABILITY_T prEventGscnCapbiblity;
	P_EVENT_GSCAN_SCAN_COMPLETE_T prEventGscnScnDone;
	P_WIFI_GSCAN_RESULT_T prEventGscnFullResult;
	P_PARAM_WIFI_GSCAN_RESULT prParamGscnFullResult;
	P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T prEventGscnSignificantChange;
	P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T prEventGscnGeofenceFound;

	P_PARAM_WIFI_GSCAN_RESULT prResults;

	DEBUGFUNC("nicRxProcessGSCNEvent");
	

	DBGLOG(SCN, INFO, "nicRxProcessGSCNEvent\n");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
	prGlueInfo = prAdapter->prGlueInfo;

	
	wiphy = priv_to_wiphy(prGlueInfo);

	

	
	switch (prEvent->ucEID) {
	case EVENT_ID_GSCAN_SCAN_AVAILABLE:
		{
			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_SCAN_AVAILABLE\n");

			prEventGscnAvailable = (P_EVENT_GSCAN_SCAN_AVAILABLE_T) (prEvent->aucBuffer);
			memcpy(prEventGscnAvailable, (P_EVENT_GSCAN_SCAN_AVAILABLE_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_SCAN_AVAILABLE_T));

			mtk_cfg80211_vendor_event_scan_results_available(wiphy, prGlueInfo->prDevHandler->ieee80211_ptr,
									 prEventGscnAvailable->u2Num);
		}
		break;

	case EVENT_ID_GSCAN_RESULT:
		{
			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_RESULT 2\n");

			prEventBuffer = (P_EVENT_GSCAN_RESULT_T) (prEvent->aucBuffer);
			prEventGscnResult = prEventBuffer->rResult;
			scan_id = prEventBuffer->u2ScanId;
			scan_flag = prEventBuffer->u2ScanFlags;
			real_num = prEventBuffer->u2NumOfResults;

			DBGLOG(SCN, INFO, "scan_id=%d, scan_flag =%d, real_num=%d\r\n", scan_id, scan_flag, real_num);

			skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(PARAM_WIFI_GSCAN_RESULT) * real_num);
			if (!skb) {
				DBGLOG(RX, TRACE, "%s allocate skb failed:%x\n", __func__, i4Status);
				return -ENOMEM;
			}

			attr = nla_nest_start(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS);
			
			{
				unsigned int __tmp = scan_id;

				if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_ID, sizeof(unsigned int), &__tmp) < 0))
					goto nla_put_failure;
			}
			
			{
				unsigned char __tmp = 1;

				if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_FLAGS, sizeof(u8), &__tmp) < 0))
					goto nla_put_failure;
			}
			
			{
				unsigned int __tmp = real_num;

				if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_NUM_OF_RESULTS,
					sizeof(unsigned int), &__tmp) < 0))
					goto nla_put_failure;
			}

			prResults = (P_PARAM_WIFI_GSCAN_RESULT) prEventGscnResult;
			if (prResults)
				DBGLOG(SCN, INFO, "ssid=%s, rssi=%d, channel=%d \r\n",
					prResults->ssid, prResults->rssi, prResults->channel);

			if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS,
				sizeof(PARAM_WIFI_GSCAN_RESULT)*real_num, prResults) < 0))
				goto nla_put_failure;

			DBGLOG(SCN, INFO, "NLA_PUT scan results over\t");

			if (attr)
				nla_nest_end(skb, attr);
			
			
			{
				unsigned char __tmp = 1;

				if (unlikely(nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE,
					sizeof(unsigned char), &__tmp) < 0))
					goto nla_put_failure;
			}


			i4Status = cfg80211_vendor_cmd_reply(skb);
			skb = NULL;
			DBGLOG(SCN, INFO, " i4Status %d\n", i4Status);
		}
		break;

	case EVENT_ID_GSCAN_CAPABILITY:
		{
			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_CAPABILITY\n");

			prEventGscnCapbiblity = (P_EVENT_GSCAN_CAPABILITY_T) (prEvent->aucBuffer);
			memcpy(prEventGscnCapbiblity, (P_EVENT_GSCAN_CAPABILITY_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_CAPABILITY_T));

			mtk_cfg80211_vendor_get_gscan_capabilities(wiphy, prGlueInfo->prDevHandler->ieee80211_ptr,
				prEventGscnCapbiblity, sizeof(EVENT_GSCAN_CAPABILITY_T));
		}
		break;

	case EVENT_ID_GSCAN_SCAN_COMPLETE:
		{
			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_SCAN_COMPLETE\n");
			prEventGscnScnDone = (P_EVENT_GSCAN_SCAN_COMPLETE_T) (prEvent->aucBuffer);
			memcpy(prEventGscnScnDone, (P_EVENT_GSCAN_SCAN_COMPLETE_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_SCAN_COMPLETE_T));

			mtk_cfg80211_vendor_event_complete_scan(wiphy, prGlueInfo->prDevHandler->ieee80211_ptr,
								prEventGscnScnDone->ucScanState);
		}
		break;

	case EVENT_ID_GSCAN_FULL_RESULT:
		{
			DBGLOG(SCN, INFO, "EVENT_ID_GSCAN_FULL_RESULT\n");

			prEventGscnFullResult = kalMemAlloc(sizeof(WIFI_GSCAN_RESULT_T), VIR_MEM_TYPE);
			if (prEventGscnFullResult)
				memcpy(prEventGscnFullResult, (P_WIFI_GSCAN_RESULT_T) (prEvent->aucBuffer),
					sizeof(WIFI_GSCAN_RESULT_T));

			prParamGscnFullResult = kalMemAlloc(sizeof(PARAM_WIFI_GSCAN_RESULT), VIR_MEM_TYPE);
			if (prEventGscnFullResult && prParamGscnFullResult) {
				kalMemZero(prParamGscnFullResult, sizeof(PARAM_WIFI_GSCAN_RESULT));
				memcpy(prParamGscnFullResult, prEventGscnFullResult, sizeof(WIFI_GSCAN_RESULT_T));

				mtk_cfg80211_vendor_event_full_scan_results(wiphy,
								    prGlueInfo->prDevHandler->ieee80211_ptr,
								    prParamGscnFullResult,
								    sizeof(PARAM_WIFI_GSCAN_RESULT));
			}
		}
		break;

	case EVENT_ID_GSCAN_SIGNIFICANT_CHANGE:
		{
			prEventGscnSignificantChange = (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer);
			memcpy(prEventGscnSignificantChange, (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_SIGNIFICANT_CHANGE_T));
		}
		break;

	case EVENT_ID_GSCAN_GEOFENCE_FOUND:
		{
			prEventGscnGeofenceFound = (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer);
			memcpy(prEventGscnGeofenceFound, (P_EVENT_GSCAN_SIGNIFICANT_CHANGE_T) (prEvent->aucBuffer),
			       sizeof(EVENT_GSCAN_SIGNIFICANT_CHANGE_T));
		}
		break;

	default:
		DBGLOG(SCN, INFO, "not GSCN event ????\n");
		break;
	}

	DBGLOG(SCN, INFO, "Done with GSCN event handling\n");
	return real_num;	

nla_put_failure:
	if (skb != NULL)
		kfree_skb(skb);
	DBGLOG(SCN, INFO, "nla_put_failure\n");
	return 0;		
}

VOID nicRxProcessEventPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_CMD_INFO_T prCmdInfo;
	P_MSDU_INFO_T prMsduInfo;
	P_WIFI_EVENT_T prEvent;
	P_GLUE_INFO_T prGlueInfo;

	DEBUGFUNC("nicRxProcessEventPacket");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
	prGlueInfo = prAdapter->prGlueInfo;

	DBGLOG(RX, EVENT, "prEvent->ucEID = 0x%02x\n", prEvent->ucEID);
	
	switch (prEvent->ucEID) {
	case EVENT_ID_CMD_RESULT:
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			P_EVENT_CMD_RESULT prCmdResult;

			prCmdResult = (P_EVENT_CMD_RESULT) ((PUINT_8) prEvent + EVENT_HDR_SIZE);

			
			ASSERT(prCmdInfo->fgSetQuery == FALSE || prCmdInfo->fgNeedResp == TRUE);

			if (prCmdResult->ucStatus == 0) {	
				if (prCmdInfo->pfCmdDoneHandler) {
					prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
				} else if (prCmdInfo->fgIsOid == TRUE) {
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
						       WLAN_STATUS_SUCCESS);
				}
			} else if (prCmdResult->ucStatus == 1) {	
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
						       WLAN_STATUS_FAILURE);
			} else if (prCmdResult->ucStatus == 2) {	
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
						       WLAN_STATUS_NOT_SUPPORTED);
			}
			
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

#if 0
	case EVENT_ID_CONNECTION_STATUS:
		
		{
			P_EVENT_CONNECTION_STATUS prConnectionStatus;

			prConnectionStatus = (P_EVENT_CONNECTION_STATUS) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_CONNECTION_STATUS = %d\n", prConnectionStatus->ucMediaStatus);
			if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_DISCONNECTED) {
				
				if (kalGetMediaStateIndicated(prGlueInfo) != PARAM_MEDIA_STATE_DISCONNECTED) {

					kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);

					prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();
				}
			} else if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_CONNECTED) {
				
				prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();

				
				prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen = prConnectionStatus->ucSsidLen;
				kalMemCopy(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
					   prConnectionStatus->aucSsid, prConnectionStatus->ucSsidLen);

				kalMemCopy(prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
					prConnectionStatus->aucBssid, MAC_ADDR_LEN);

				
				prAdapter->rWlanInfo.rCurrBssId.u4Privacy = prConnectionStatus->ucEncryptStatus;
				prAdapter->rWlanInfo.rCurrBssId.rRssi = 0;	
				
				prAdapter->rWlanInfo.rCurrBssId.eNetworkTypeInUse = PARAM_NETWORK_TYPE_AUTOMODE;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4BeaconPeriod
					= prConnectionStatus->u2BeaconPeriod;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4ATIMWindow
					= prConnectionStatus->u2ATIMWindow;
				prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4DSConfig
					= prConnectionStatus->u4FreqInKHz;
				prAdapter->rWlanInfo.ucNetworkType = prConnectionStatus->ucNetworkType;

				switch (prConnectionStatus->ucInfraMode) {
				case 0:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_IBSS;
					break;
				case 1:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_INFRA;
					break;
				case 2:
				default:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_AUTO_SWITCH;
					break;
				}
				
				kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_MEDIA_CONNECT, NULL, 0);
			}
		}
		break;

	case EVENT_ID_SCAN_RESULT:
		
		break;
#endif

	case EVENT_ID_RX_ADDBA:
		
		qmHandleEventRxAddBa(prAdapter, prEvent);
		break;

	case EVENT_ID_RX_DELBA:
		
		qmHandleEventRxDelBa(prAdapter, prEvent);
		break;

	case EVENT_ID_LINK_QUALITY:
#if CFG_ENABLE_WIFI_DIRECT && CFG_SUPPORT_P2P_RSSI_QUERY
		if (prEvent->u2PacketLen == EVENT_HDR_SIZE + sizeof(EVENT_LINK_QUALITY_EX)) {
			P_EVENT_LINK_QUALITY_EX prLqEx = (P_EVENT_LINK_QUALITY_EX) (prEvent->aucBuffer);

			if (prLqEx->ucIsLQ0Rdy)
				nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_AIS_INDEX, (P_EVENT_LINK_QUALITY) prLqEx);
			if (prLqEx->ucIsLQ1Rdy)
				nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_P2P_INDEX, (P_EVENT_LINK_QUALITY) prLqEx);
		} else {
			
			DBGLOG(P2P, WARN, "Old FW version, not support P2P RSSI query.\n");

			
			nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_AIS_INDEX,
					     (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
		}
#else
		nicUpdateLinkQuality(prAdapter, NETWORK_TYPE_AIS_INDEX, (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
#endif

		
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}
#ifndef LINUX
		if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_GREATER &&
		    prAdapter->rWlanInfo.rRssiTriggerValue >= (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
			prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

			kalIndicateStatusAndComplete(prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID)&(prAdapter->rWlanInfo.rRssiTriggerValue),
						     sizeof(PARAM_RSSI));
		} else if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_LESS
			   && prAdapter->rWlanInfo.rRssiTriggerValue <= (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
			prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

			kalIndicateStatusAndComplete(prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID)&(prAdapter->rWlanInfo.rRssiTriggerValue),
						     sizeof(PARAM_RSSI));
		}
#endif

		break;

	case EVENT_ID_MIC_ERR_INFO:
		{
			P_EVENT_MIC_ERR_INFO prMicError;
			
			P_STA_RECORD_T prStaRec;

			DBGLOG(RSN, EVENT, "EVENT_ID_MIC_ERR_INFO\n");

			prMicError = (P_EVENT_MIC_ERR_INFO) (prEvent->aucBuffer);
			prStaRec = cnmGetStaRecByAddress(prAdapter,
							 (UINT_8) NETWORK_TYPE_AIS_INDEX,
							 prAdapter->rWlanInfo.rCurrBssId.arMacAddress);
			ASSERT(prStaRec);

			if (prStaRec)
				rsnTkipHandleMICFailure(prAdapter, prStaRec, (BOOLEAN) prMicError->u4Flags);
			else
				DBGLOG(RSN, WARN, "No STA rec!!\n");
#if 0
			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

			
			prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

			
			prAuthEvent->arRequest[0].u4Length = sizeof(PARAM_AUTH_REQUEST_T);
			kalMemCopy((PVOID) prAuthEvent->arRequest[0].arBssid,
				(PVOID) prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
				PARAM_MAC_ADDR_LEN);

			if (prMicError->u4Flags != 0)
				prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
			else
				prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) prAuthEvent,
						     sizeof(PARAM_STATUS_INDICATION_T) + sizeof(PARAM_AUTH_REQUEST_T));
#endif
		}
		break;

	case EVENT_ID_ASSOC_INFO:
		{
			P_EVENT_ASSOC_INFO prAssocInfo;

			prAssocInfo = (P_EVENT_ASSOC_INFO) (prEvent->aucBuffer);

			kalHandleAssocInfo(prAdapter->prGlueInfo, prAssocInfo);
		}
		break;

	case EVENT_ID_802_11_PMKID:
		{
			P_PARAM_AUTH_EVENT_T prAuthEvent;
			PUINT_8 cp;
			UINT_32 u4LenOfUsedBuffer;

			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

			prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;

			u4LenOfUsedBuffer = (UINT_32) (prEvent->u2PacketLen - 8);

			prAuthEvent->arRequest[0].u4Length = u4LenOfUsedBuffer;

			cp = (PUINT_8) &prAuthEvent->arRequest[0];

			
			kalMemCopy(cp, (P_EVENT_PMKID_CANDIDATE_LIST_T) (prEvent->aucBuffer), prEvent->u2PacketLen - 8);

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) prAuthEvent,
						     sizeof(PARAM_STATUS_INDICATION_T) + u4LenOfUsedBuffer);
		}
		break;

#if 0
	case EVENT_ID_ACTIVATE_STA_REC_T:
		{
			P_EVENT_ACTIVATE_STA_REC_T prActivateStaRec;

			prActivateStaRec = (P_EVENT_ACTIVATE_STA_REC_T) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_ACTIVATE_STA_REC_T Index:%d, MAC:[%pM]\n",
				 prActivateStaRec->ucStaRecIdx, prActivateStaRec->aucMacAddr);

			qmActivateStaRec(prAdapter,
					 (UINT_32) prActivateStaRec->ucStaRecIdx,
					 ((prActivateStaRec->fgIsQoS) ? TRUE : FALSE),
					 prActivateStaRec->ucNetworkTypeIndex,
					 ((prActivateStaRec->fgIsAP) ? TRUE : FALSE), prActivateStaRec->aucMacAddr);

		}
		break;

	case EVENT_ID_DEACTIVATE_STA_REC_T:
		{
			P_EVENT_DEACTIVATE_STA_REC_T prDeactivateStaRec;

			prDeactivateStaRec = (P_EVENT_DEACTIVATE_STA_REC_T) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_DEACTIVATE_STA_REC_T Index:%d, MAC:[%pM]\n",
				 prDeactivateStaRec->ucStaRecIdx, prActivateStaRec->aucMacAddr);

			qmDeactivateStaRec(prAdapter, prDeactivateStaRec->ucStaRecIdx);
		}
		break;
#endif

	case EVENT_ID_SCAN_DONE:
		scnEventScanDone(prAdapter, (P_EVENT_SCAN_DONE) (prEvent->aucBuffer));
		break;

	case EVENT_ID_TX_DONE_STATUS:
		STATS_TX_PKT_DONE_INFO_DISPLAY(prAdapter, prEvent->aucBuffer);
		break;

	case EVENT_ID_TX_DONE:
	{
		P_EVENT_TX_DONE_T prTxDone;

		prTxDone = (P_EVENT_TX_DONE_T) (prEvent->aucBuffer);

		if (prTxDone->ucStatus) {
			DBGLOG(RX, INFO, "EVENT_ID_TX_DONE PacketSeq:%u ucStatus: %u SN: %u\n",
				prTxDone->ucPacketSeq, prTxDone->ucStatus, prTxDone->u2SequenceNumber);
			if (prTxDone->ucStatus == TX_RESULT_FW_FLUSH)
				prAdapter->ucFlushCount++;
		} else
			prAdapter->ucFlushCount = 0;

		
		if (prAdapter->ucFlushCount >= RX_FW_FLUSH_PKT_THRESHOLD) {
			DBGLOG(RX, ERROR, "FW flushed continusous packages :%d\n", prAdapter->ucFlushCount);
			prAdapter->ucFlushCount = 0;
			kalSendAeeWarning("[Fatal error! FW Flushed PKT too much!]", __func__);
			glDoChipReset();
		}

		
		prMsduInfo = nicGetPendingTxMsduInfo(prAdapter, prTxDone->ucPacketSeq);

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
		DBGLOG(RX, TRACE, "EVENT_ID_TX_DONE u4TimeStamp = %x u2AirDelay = %x\n",
					    prTxDone->au4Reserved1, prTxDone->au4Reserved2);

		wnmReportTimingMeas(prAdapter, prMsduInfo->ucStaRecIndex,
					   prTxDone->au4Reserved1, prTxDone->au4Reserved1 + prTxDone->au4Reserved2);
#endif

		if (prMsduInfo) {
			prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
							   (ENUM_TX_RESULT_CODE_T) (prTxDone->ucStatus));

			cnmMgtPktFree(prAdapter, prMsduInfo);
		}
	}
		break;
	case EVENT_ID_SLEEPY_NOTIFY:
		{
			P_EVENT_SLEEPY_NOTIFY prEventSleepyNotify;

			prEventSleepyNotify = (P_EVENT_SLEEPY_NOTIFY) (prEvent->aucBuffer);

			

			prAdapter->fgWiFiInSleepyState = (BOOLEAN) (prEventSleepyNotify->ucSleepyState);
		}
		break;
	case EVENT_ID_BT_OVER_WIFI:
#if CFG_ENABLE_BT_OVER_WIFI
		{
			UINT_8 aucTmp[sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)];
			P_EVENT_BT_OVER_WIFI prEventBtOverWifi;
			P_AMPC_EVENT prBowEvent;
			P_BOW_LINK_CONNECTED prBowLinkConnected;
			P_BOW_LINK_DISCONNECTED prBowLinkDisconnected;

			prEventBtOverWifi = (P_EVENT_BT_OVER_WIFI) (prEvent->aucBuffer);

			
			prBowEvent = (P_AMPC_EVENT) aucTmp;

			if (prEventBtOverWifi->ucLinkStatus == 0) {
				
				prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_CONNECTED;
				prBowEvent->rHeader.ucSeqNumber = 0;
				prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_CONNECTED);

				
				prBowLinkConnected = (P_BOW_LINK_CONNECTED) (prBowEvent->aucPayload);
				prBowLinkConnected->rChannel.ucChannelNum = prEventBtOverWifi->ucSelectedChannel;
				kalMemZero(prBowLinkConnected->aucPeerAddress, MAC_ADDR_LEN);	

				kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
			} else {
				
				prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_DISCONNECTED;
				prBowEvent->rHeader.ucSeqNumber = 0;
				prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_DISCONNECTED);

				
				prBowLinkDisconnected = (P_BOW_LINK_DISCONNECTED) (prBowEvent->aucPayload);
				prBowLinkDisconnected->ucReason = 0;	
				kalMemZero(prBowLinkDisconnected->aucPeerAddress, MAC_ADDR_LEN);	

				kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
			}
		}
		break;
#endif
	case EVENT_ID_STATISTICS:
		
		prAdapter->fgIsStatValid = TRUE;
		prAdapter->rStatUpdateTime = kalGetTimeTick();
		kalMemCopy(&prAdapter->rStatStruct, prEvent->aucBuffer, sizeof(EVENT_STATISTICS));

		
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

	case EVENT_ID_CH_PRIVILEGE:
		cnmChMngrHandleChEvent(prAdapter, prEvent);
		break;

	case EVENT_ID_BSS_ABSENCE_PRESENCE:
		qmHandleEventBssAbsencePresence(prAdapter, prEvent);
		break;

	case EVENT_ID_STA_CHANGE_PS_MODE:
		qmHandleEventStaChangePsMode(prAdapter, prEvent);
		break;
#if CFG_ENABLE_WIFI_DIRECT
	case EVENT_ID_STA_UPDATE_FREE_QUOTA:
		qmHandleEventStaUpdateFreeQuota(prAdapter, prEvent);
		break;
#endif
	case EVENT_ID_BSS_BEACON_TIMEOUT:
		if (prAdapter->fgDisBcnLostDetection == FALSE) {
			P_EVENT_BSS_BEACON_TIMEOUT_T prEventBssBeaconTimeout;

			prEventBssBeaconTimeout = (P_EVENT_BSS_BEACON_TIMEOUT_T) (prEvent->aucBuffer);

			DBGLOG(RX, INFO, "Beacon Timeout Reason = %u\n", prEventBssBeaconTimeout->ucReason);

			if (prEventBssBeaconTimeout->ucNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
				
				P_BSS_INFO_T prBssInfo;
				P_STA_RECORD_T prStaRec;

				prBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
				if (prBssInfo) {
					prStaRec = cnmGetStaRecByAddress(prAdapter,
									NETWORK_TYPE_AIS_INDEX,
									prBssInfo->aucBSSID);
					if (prStaRec)
						STATS_ENV_REPORT_DETECT(prAdapter, prStaRec->ucIndex);

					prBssInfo->u2DeauthReason = prEventBssBeaconTimeout->ucReason;
				}
				aisBssBeaconTimeout(prAdapter);
			}
#if CFG_ENABLE_WIFI_DIRECT
			else if ((prAdapter->fgIsP2PRegistered) &&
				 (prEventBssBeaconTimeout->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX))

				p2pFsmRunEventBeaconTimeout(prAdapter);
#endif
			else {
				DBGLOG(RX, ERROR, "EVENT_ID_BSS_BEACON_TIMEOUT: (ucNetTypeIdx = %d)\n",
						   prEventBssBeaconTimeout->ucNetTypeIndex);
			}
		}

		break;
	case EVENT_ID_UPDATE_NOA_PARAMS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam;

			prEventUpdateNoaParam = (P_EVENT_UPDATE_NOA_PARAMS_T) (prEvent->aucBuffer);

			if (prEventUpdateNoaParam->ucNetTypeIndex == NETWORK_TYPE_P2P_INDEX) {
				p2pProcessEvent_UpdateNOAParam(prAdapter,
							       prEventUpdateNoaParam->ucNetTypeIndex,
							       prEventUpdateNoaParam);
			} else {
				ASSERT(0);
			}
		}
#else
		ASSERT(0);
#endif
		break;

	case EVENT_ID_STA_AGING_TIMEOUT:
#if CFG_ENABLE_WIFI_DIRECT
		{
			if (prAdapter->fgDisStaAgingTimeoutDetection == FALSE) {
				P_EVENT_STA_AGING_TIMEOUT_T prEventStaAgingTimeout;
				P_STA_RECORD_T prStaRec;
				P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

				prEventStaAgingTimeout = (P_EVENT_STA_AGING_TIMEOUT_T) (prEvent->aucBuffer);
				prStaRec = cnmGetStaRecByIndex(prAdapter, prEventStaAgingTimeout->ucStaRecIdx);
				if (prStaRec == NULL)
					break;

				DBGLOG(RX, INFO, "EVENT_ID_STA_AGING_TIMEOUT %u %pM\n",
						    prEventStaAgingTimeout->ucStaRecIdx,
						    prStaRec->aucMacAddr);

				prBssInfo = &(prAdapter->rWifiVar.arBssInfo[prStaRec->ucNetTypeIndex]);

				bssRemoveStaRecFromClientList(prAdapter, prBssInfo, prStaRec);

				
				if (prAdapter->fgIsP2PRegistered)
					p2pFuncDisconnect(prAdapter, prStaRec, TRUE, REASON_CODE_DISASSOC_INACTIVITY);

			}
			
		}
#endif
		break;

	case EVENT_ID_AP_OBSS_STATUS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			rlmHandleObssStatusEventPkt(prAdapter, (P_EVENT_AP_OBSS_STATUS_T) prEvent->aucBuffer);
#endif
		break;

	case EVENT_ID_ROAMING_STATUS:
#if CFG_SUPPORT_ROAMING
		{
			P_ROAMING_PARAM_T prParam;

			prParam = (P_ROAMING_PARAM_T) (prEvent->aucBuffer);
			roamingFsmProcessEvent(prAdapter, prParam);
		}
#endif 
		break;
	case EVENT_ID_SEND_DEAUTH:
		{
			P_WLAN_MAC_HEADER_T prWlanMacHeader;
			P_STA_RECORD_T prStaRec;

			prWlanMacHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
			DBGLOG(RSN, INFO, "nicRx: aucAddr1: %pM, nicRx: aucAddr2: %pM\n",
					prWlanMacHeader->aucAddr1, prWlanMacHeader->aucAddr2);
			prStaRec = cnmGetStaRecByAddress(prAdapter, NETWORK_TYPE_AIS_INDEX, prWlanMacHeader->aucAddr2);
			if (prStaRec != NULL && prStaRec->ucStaState == STA_STATE_3) {
				DBGLOG(RSN, WARN, "Ignore Deauth for Rx Class 3 error!\n");
			} else {
				
				prSwRfb->pvHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
				if (WLAN_STATUS_SUCCESS == authSendDeauthFrame(prAdapter,
									       NULL,
									       prSwRfb,
									       REASON_CODE_CLASS_3_ERR,
									       (PFN_TX_DONE_HANDLER) NULL))
					DBGLOG(RSN, INFO, "Send Deauth for Rx Class3 Error\n");
				else
					DBGLOG(RSN, WARN, "failed to send deauth for Rx class3 error\n");
			}
		}
		break;

#if CFG_SUPPORT_RDD_TEST_MODE
	case EVENT_ID_UPDATE_RDD_STATUS:
		{
			P_EVENT_RDD_STATUS_T prEventRddStatus;

			prEventRddStatus = (P_EVENT_RDD_STATUS_T) (prEvent->aucBuffer);

			prAdapter->ucRddStatus = prEventRddStatus->ucRddStatus;
		}

		break;
#endif

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	case EVENT_ID_UPDATE_BWCS_STATUS:
		{
			P_PTA_IPC_T prEventBwcsStatus;

			prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			DBGLOG(RSN, INFO,
			"BCM BWCS Event: %02x%02x%02x%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);

			DBGLOG(RSN, INFO,
			"BCM BWCS Event: BTPParams[0]:%02x, BTPParams[1]:%02x, BTPParams[2]:%02x, BTPParams[3]:%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);
#endif

			kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_BWCS_UPDATE,
						     (PVOID) prEventBwcsStatus, sizeof(PTA_IPC_T));
		}

		break;

	case EVENT_ID_UPDATE_BCM_DEBUG:
		{
			P_PTA_IPC_T prEventBwcsStatus;

			prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			DBGLOG(RSN, INFO,
			"BCM FW status: %02x%02x%02x%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3]);

			DBGLOG(RSN, INFO,
			"BCM FW status: BTPParams[0]:%02x, BTPParams[1]:%02x, BTPParams[2]:%02x, BTPParams[3]:%02x\n",
			prEventBwcsStatus->u.aucBTPParams[0], prEventBwcsStatus->u.aucBTPParams[1],
			prEventBwcsStatus->u.aucBTPParams[2], prEventBwcsStatus->u.aucBTPParams[3];
#endif
		}

		break;
#endif

	case EVENT_ID_DEBUG_CODE:	
		{
			UINT_32 u4CodeId;

			DBGLOG(RSN, INFO, "[wlan-fw] function sequence: ");
			for (u4CodeId = 0; u4CodeId < 1000; u4CodeId++)
				DBGLOG(RSN, INFO, "%d ", prEvent->aucBuffer[u4CodeId]);
			DBGLOG(RSN, INFO, "\n\n");
		}
		break;

	case EVENT_ID_RFTEST_READY:

		
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;

	case EVENT_ID_GSCAN_SCAN_AVAILABLE:
	case EVENT_ID_GSCAN_CAPABILITY:
	case EVENT_ID_GSCAN_SCAN_COMPLETE:
	case EVENT_ID_GSCAN_FULL_RESULT:
	case EVENT_ID_GSCAN_SIGNIFICANT_CHANGE:
	case EVENT_ID_GSCAN_GEOFENCE_FOUND:
		nicRxProcessGSCNEvent(prAdapter, prSwRfb);
		break;

	case EVENT_ID_GSCAN_RESULT:
		{

			UINT_8 realnum = 0;

			DBGLOG(SCN, TRACE, "nicRxProcessGSCNEvent  ----->\n");
			realnum = nicRxProcessGSCNEvent(prAdapter, prSwRfb);
			DBGLOG(SCN, TRACE, "nicRxProcessGSCNEvent  <-----\n");

#if 0			
			if (g_GetResultsCmdCnt == 0) {
				DBGLOG(SCN, INFO,
					"FW report events more than the wifi_hal asked number, buffer the results\n");
				UINT_8 i = 0;

				for (i = 0; i < MAX_BUFFERED_GSCN_RESULTS; i++) {
#if 1
					if (!g_arGscanResultsIndicateNumber[i]) {
						DBGLOG(SCN, INFO,
						"found available index %d to insert results number %d into buffer\r\n",
						i, realnum);

						g_arGscnResultsTempBuffer[i] = prSwRfb;
						g_arGscanResultsIndicateNumber[i] = realnum;
						g_GetResultsBufferedCnt++;
						fgKeepprSwRfb = TRUE;
						DBGLOG(SCN, INFO, "results buffered in index[%d] \r\n", i);
						break;
					}
#endif
				}
				if (i == MAX_BUFFERED_GSCN_RESULTS)
					DBGLOG(SCN, INFO,
					"Gscn results buffer is full(all valid), no space to buffer result\r\n");
			} else if (g_GetResultsCmdCnt > 0) {
				DBGLOG(SCN, INFO, "FW report events match the wifi_hal asked number\n");
				g_GetResultsCmdCnt--;
			} else
				DBGLOG(SCN, INFO, "g_GetResultsCmdCnt < 0 ??? unexpected case\n");
#endif
			

		}
		break;

	case EVENT_ID_NLO_DONE:
		prAdapter->rWifiVar.rScanInfo.fgPscnOnnning = FALSE;

		DBGLOG(INIT, INFO, "EVENT_ID_NLO_DONE\n");
		scnEventNloDone(prAdapter, (P_EVENT_NLO_DONE_T) (prEvent->aucBuffer));

		break;

#if CFG_SUPPORT_BATCH_SCAN
	case EVENT_ID_BATCH_RESULT:
		DBGLOG(SCN, TRACE, "Got EVENT_ID_BATCH_RESULT");

		
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
#endif 

#if (CFG_SUPPORT_TDLS == 1)
	case EVENT_ID_TDLS:
		TdlsexEventHandle(prAdapter->prGlueInfo,
				  (UINT8 *) prEvent->aucBuffer, (UINT32) (prEvent->u2PacketLen - 8));
		break;
#endif 

#if (CFG_SUPPORT_STATISTICS == 1)
	case EVENT_ID_STATS_ENV:
		statsEventHandle(prAdapter->prGlueInfo,
				 (UINT8 *) prEvent->aucBuffer, (UINT32) (prEvent->u2PacketLen - 8));
		break;
#endif 
	case EVENT_ID_CHECK_REORDER_BUBBLE:
		qmHandleEventCheckReorderBubble(prAdapter, prEvent);
		break;
	case EVENT_ID_FW_LOG_ENV:
		{
			P_EVENT_FW_LOG_T prEventLog;

			prEventLog = (P_EVENT_FW_LOG_T) (prEvent->aucBuffer);
			DBGLOG(RX, INFO, "[F-L]%s\n", prEventLog->log);
		}
		break;

	default:
		prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			if (prCmdInfo->pfCmdDoneHandler)
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prEvent->aucBuffer);
			else if (prCmdInfo->fgIsOid)
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
			
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		}

		break;
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

VOID nicRxProcessMgmtPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	UINT_8 ucSubtype;
#if CFG_SUPPORT_802_11W
	BOOLEAN fgMfgDrop = FALSE;
#endif
	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	nicRxFillRFB(prAdapter, prSwRfb);

	ucSubtype = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FC_SUBTYPE) >> OFFSET_OF_FC_SUBTYPE;

#if 0				
	{
		P_HIF_RX_HEADER_T prHifRxHdr;
		UINT_16 u2TxFrameCtrl;

		prHifRxHdr = prSwRfb->prHifRxHdr;
		u2TxFrameCtrl = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FRAME_TYPE);
		
		
		

		DBGLOG(RX, INFO, "QM RX MGT: net %u sta idx %u wlan idx %u ssn %u ptype %u subtype %u 11 %u\n",
		(UINT_32) HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr), prHifRxHdr->ucStaRecIdx,
		prSwRfb->ucWlanIdx, (UINT_32) HIF_RX_HDR_GET_SN(prHifRxHdr),
		prSwRfb->ucPacketType, ucSubtype, HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr));

		
		
		
	}
#endif

	if ((prAdapter->fgTestMode == FALSE) && (prAdapter->prGlueInfo->fgIsRegistered == TRUE)) {
#if CFG_MGMT_FRAME_HANDLING
#if CFG_SUPPORT_802_11W
		fgMfgDrop = rsnCheckRxMgmt(prAdapter, prSwRfb, ucSubtype);
		if (fgMfgDrop) {
#if DBG
			LOG_FUNC("QM RX MGT: Drop Unprotected Mgmt frame!!!\n");
#endif
			nicRxReturnRFB(prAdapter, prSwRfb);
			RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
			return;
		}
#endif
		if (apfnProcessRxMgtFrame[ucSubtype]) {
			switch (apfnProcessRxMgtFrame[ucSubtype] (prAdapter, prSwRfb)) {
			case WLAN_STATUS_PENDING:
				return;
			case WLAN_STATUS_SUCCESS:
			case WLAN_STATUS_FAILURE:
				break;

			default:
				DBGLOG(RX, WARN,
				       "Unexpected MMPDU(0x%02X) returned with abnormal status\n", ucSubtype);
				break;
			}
		}
#endif
	}

	nicRxReturnRFB(prAdapter, prSwRfb);
}

#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
static VOID nicRxCheckWakeupReason(P_SW_RFB_T prSwRfb)
{
	PUINT_8 pvHeader = NULL;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_16 u2PktLen = 0;
	UINT_32 u4HeaderOffset;

	if (!prSwRfb)
		return;
	prHifRxHdr = prSwRfb->prHifRxHdr;
	if (!prHifRxHdr)
		return;

	switch (prSwRfb->ucPacketType) {
	case HIF_RX_PKT_TYPE_DATA:
	{
		UINT_16 u2Temp = 0;

		if (HIF_RX_HDR_GET_BAR_FLAG(prHifRxHdr)) {
			DBGLOG(RX, INFO, "BAR frame[SSN:%d, TID:%d] wakeup host\n",
				(UINT_16)HIF_RX_HDR_GET_SN(prHifRxHdr), (UINT_8)HIF_RX_HDR_GET_TID(prHifRxHdr));
			break;
		}
		u4HeaderOffset = (UINT_32)(prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_OFFSET_MASK);
		pvHeader = (PUINT_8)prHifRxHdr + HIF_RX_HDR_SIZE + u4HeaderOffset;
		u2PktLen = (UINT_16)(prHifRxHdr->u2PacketLen - (HIF_RX_HDR_SIZE + u4HeaderOffset));
		if (!pvHeader) {
			DBGLOG(RX, ERROR, "data packet but pvHeader is NULL!\n");
			break;
		}
		u2Temp = (pvHeader[ETH_TYPE_LEN_OFFSET] << 8) | (pvHeader[ETH_TYPE_LEN_OFFSET + 1]);

		switch (u2Temp) {
		case ETH_P_IPV4:
			u2Temp = *(UINT_16 *) &pvHeader[ETH_HLEN + 4];
			DBGLOG(RX, INFO, "IP Packet:%d.%d.%d.%d, to:%d.%d.%d.%d,ID 0x%04x wakeup host\n",
				pvHeader[ETH_HLEN + 12], pvHeader[ETH_HLEN + 13],
				pvHeader[ETH_HLEN + 14], pvHeader[ETH_HLEN + 15],
				pvHeader[ETH_HLEN + 16], pvHeader[ETH_HLEN + 17],
				pvHeader[ETH_HLEN + 18], pvHeader[ETH_HLEN + 19],
				u2Temp);
			break;
		case ETH_P_ARP:
		{
			PUINT_8 pucEthBody = &pvHeader[ETH_HLEN];
			UINT_16 u2OpCode = (pucEthBody[6] << 8) | pucEthBody[7];

			if (u2OpCode == ARP_PRO_REQ)
				DBGLOG(RX, INFO, "Arp Req From IP: %d.%d.%d.%d wakeup host\n",
					pucEthBody[14], pucEthBody[15], pucEthBody[16], pucEthBody[17]);
			else if (u2OpCode == ARP_PRO_RSP)
				DBGLOG(RX, INFO, "Arp Rsp from IP: %d.%d.%d.%d wakeup host\n",
					pucEthBody[14], pucEthBody[15], pucEthBody[16], pucEthBody[17]);
			break;
		}
		case ETH_P_1X:
		case ETH_P_PRE_1X:
#if CFG_SUPPORT_WAPI
		case ETH_WPI_1X:
#endif
		case ETH_P_AARP:
		case ETH_P_IPV6:
		case ETH_P_IPX:
		case 0x8100: 
		case 0x890d: 
			DBGLOG(RX, INFO, "Data Packet, EthType 0x%04x wakeup host\n", u2Temp);
			break;
		default:
			DBGLOG(RX, WARN, "maybe abnormal data packet, EthType 0x%04x wakeup host, dump it\n",
				u2Temp);
			DBGLOG_MEM8(RX, INFO, pvHeader, u2PktLen > 50 ? 50:u2PacketLen);
			break;
		}
		break;
	}
	case HIF_RX_PKT_TYPE_EVENT:
	{
		P_WIFI_EVENT_T prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;

		DBGLOG(RX, INFO, "Event 0x%02x wakeup host\n", prEvent->ucEID);
		break;
	}
	case HIF_RX_PKT_TYPE_MANAGEMENT:
	{
		UINT_8 ucSubtype;
		P_WLAN_MAC_MGMT_HEADER_T prWlanMgmtHeader;

		u4HeaderOffset = (UINT_32)(prHifRxHdr->ucHerderLenOffset & HIF_RX_HDR_HEADER_OFFSET_MASK);
		pvHeader = (PUINT_8)prHifRxHdr + HIF_RX_HDR_SIZE + u4HeaderOffset;
		if (!pvHeader) {
			DBGLOG(RX, ERROR, "Mgmt Frame but pvHeader is NULL!\n");
			break;
		}
		prWlanMgmtHeader = (P_WLAN_MAC_MGMT_HEADER_T)pvHeader;
		ucSubtype = (prWlanMgmtHeader->u2FrameCtrl & MASK_FC_SUBTYPE) >>
				OFFSET_OF_FC_SUBTYPE;
		DBGLOG(RX, INFO, "MGMT frame subtype: %d SeqCtrl %d wakeup host\n",
				ucSubtype, prWlanMgmtHeader->u2SeqCtrl);
		break;
	}
	default:
		DBGLOG(RX, WARN, "Unknown Packet %d wakeup host\n", prSwRfb->ucPacketType);
		break;
	}
}
#endif
VOID nicRxProcessRFBs(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxProcessRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	prRxCtrl->ucNumIndPacket = 0;
	prRxCtrl->ucNumRetainedPacket = 0;

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rReceivedRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (prSwRfb) {
#if CFG_SUPPORT_WAKEUP_REASON_DEBUG
			if (kalIsWakeupByWlan(prAdapter))
				nicRxCheckWakeupReason(prSwRfb);
#endif
			switch (prSwRfb->ucPacketType) {
			case HIF_RX_PKT_TYPE_DATA:
				nicRxProcessDataPacket(prAdapter, prSwRfb);
				break;

			case HIF_RX_PKT_TYPE_EVENT:
				nicRxProcessEventPacket(prAdapter, prSwRfb);
				break;

			case HIF_RX_PKT_TYPE_TX_LOOPBACK:
#if (CONF_HIF_LOOPBACK_AUTO == 1)
				{
					kalDevLoopbkRxHandle(prAdapter, prSwRfb);
					nicRxReturnRFB(prAdapter, prSwRfb);
				}
#else
				DBGLOG(RX, ERROR, "ucPacketType = %d\n", prSwRfb->ucPacketType);
#endif 
				break;

			case HIF_RX_PKT_TYPE_MANAGEMENT:
				nicRxProcessMgmtPacket(prAdapter, prSwRfb);
				break;

			default:
				RX_INC_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT);
				RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
				DBGLOG(RX, ERROR, "ucPacketType = %d\n", prSwRfb->ucPacketType);
				nicRxReturnRFB(prAdapter, prSwRfb);	
				break;
			}
		} else {
			break;
		}
	} while (TRUE);

	if (prRxCtrl->ucNumIndPacket > 0) {
		RX_ADD_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT, prRxCtrl->ucNumIndPacket);
		RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT, prRxCtrl->ucNumRetainedPacket);

		
		
#if CFG_NATIVE_802_11
		kalRxIndicatePkts(prAdapter->prGlueInfo, (UINT_32) prRxCtrl->ucNumIndPacket,
				  (UINT_32) prRxCtrl->ucNumRetainedPacket);
#else
		kalRxIndicatePkts(prAdapter->prGlueInfo, prRxCtrl->apvIndPacket, (UINT_32) prRxCtrl->ucNumIndPacket);
#endif
	}

}				

#if !CFG_SDIO_INTR_ENHANCE
WLAN_STATUS nicRxReadBuffer(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucBuf;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_32 u4PktLen = 0, u4ReadBytes;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	BOOLEAN fgResult = TRUE;
	UINT_32 u4RegValue;
	UINT_32 rxNum;

	DEBUGFUNC("nicRxReadBuffer");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	pucBuf = prSwRfb->pucRecvBuff;
	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(pucBuf);
	DBGLOG(RX, TRACE, "pucBuf= 0x%x, prHifRxHdr= 0x%x\n", pucBuf, prHifRxHdr);

	do {
		
		HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4RegValue);
		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read RX Packet Lentgh Error\n");
			return WLAN_STATUS_FAILURE;
		}
		
		if (u4RegValue == 0) {
			DBGLOG(RX, ERROR, "No RX packet\n");
			return WLAN_STATUS_FAILURE;
		}

		u4PktLen = u4RegValue & BITS(0, 15);
		if (u4PktLen != 0) {
			rxNum = 0;
		} else {
			rxNum = 1;
			u4PktLen = (u4RegValue & BITS(16, 31)) >> 16;
		}

		DBGLOG(RX, TRACE, "RX%d: u4PktLen = %d\n", rxNum, u4PktLen);

		
		u4ReadBytes = ALIGN_4(u4PktLen) + 4;
		HAL_READ_RX_PORT(prAdapter, rxNum, u4ReadBytes, pucBuf, CFG_RX_MAX_PKT_SIZE);

		
		
		if (u4PktLen != (UINT_32) prHifRxHdr->u2PacketLen) {
			DBGLOG(RX, ERROR, "Read u4PktLen = %d, prHifRxHdr->u2PacketLen: %d\n",
					   u4PktLen, prHifRxHdr->u2PacketLen);
#if DBG
			dumpMemory8((PUINT_8) prHifRxHdr,
				    (prHifRxHdr->u2PacketLen > 4096) ? 4096 : prHifRxHdr->u2PacketLen);
#endif
			ASSERT(0);
		}
		

		prSwRfb->ucPacketType = (UINT_8) (prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK);
		DBGLOG(RX, TRACE, "ucPacketType = %d\n", prSwRfb->ucPacketType);

		prSwRfb->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);

		
		if (!fgResult)
			return WLAN_STATUS_FAILURE;

		DBGLOG(RX, TRACE, "Dump RX buffer, length = 0x%x\n", u4ReadBytes);
		DBGLOG_MEM8(RX, TRACE, pucBuf, u4ReadBytes);
	} while (FALSE);

	return u4Status;
}

VOID nicRxReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	P_HIF_RX_HEADER_T prHifRxHdr;

	UINT_32 u4HwAppendDW;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxReceiveRFBs");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	do {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		if (!prSwRfb) {
			DBGLOG(RX, TRACE, "No More RFB\n");
			break;
		}
		
		if (nicRxReadBuffer(prAdapter, prSwRfb) == WLAN_STATUS_FAILURE) {
			DBGLOG(RX, TRACE, "halRxFillRFB failed\n");
			nicRxReturnRFB(prAdapter, prSwRfb);
			break;
		}

		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
		RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

		prHifRxHdr = prSwRfb->prHifRxHdr;
		u4HwAppendDW = *((PUINT_32) ((ULONG) prHifRxHdr + (UINT_32) (ALIGN_4(prHifRxHdr->u2PacketLen))));
		DBGLOG(RX, TRACE, "u4HwAppendDW = 0x%x\n", u4HwAppendDW);
		DBGLOG(RX, TRACE, "u2PacketLen = 0x%x\n", prHifRxHdr->u2PacketLen);
	} while (FALSE);	

	return;

}				

#else

WLAN_STATUS
nicRxEnhanceReadBuffer(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DataPort, IN UINT_16 u2RxLength, IN OUT P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucBuf;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_32 u4PktLen = 0;
	WLAN_STATUS u4Status = WLAN_STATUS_FAILURE;
	BOOLEAN fgResult = TRUE;

	DEBUGFUNC("nicRxEnhanceReadBuffer");

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	pucBuf = prSwRfb->pucRecvBuff;
	ASSERT(pucBuf);

	prHifRxHdr = prSwRfb->prHifRxHdr;
	ASSERT(prHifRxHdr);

	

	do {
		
		HAL_READ_RX_PORT(prAdapter,
				 u4DataPort, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN), pucBuf, CFG_RX_MAX_PKT_SIZE);

		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read RX Packet Lentgh Error\n");
			break;
		}

		u4PktLen = (UINT_32) (prHifRxHdr->u2PacketLen);
		

		prSwRfb->ucPacketType = (UINT_8) (prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK);
		

		prSwRfb->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);

		
		if (u4PktLen == 0) {
			DBGLOG(RX, ERROR, "Packet Length = %u\n", u4PktLen);
			ASSERT(0);
			break;
		}
		
		if (u4PktLen > CFG_RX_MAX_PKT_SIZE) {
			DBGLOG(RX, TRACE, "Read RX Packet Lentgh Error (%u)\n", u4PktLen);
			ASSERT(0);
			break;
		}

		u4Status = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	DBGLOG_MEM8(RX, TRACE, pucBuf, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN));
	return u4Status;
}

VOID nicRxSDIOReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
	P_SDIO_CTRL_T prSDIOCtrl;
	P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	UINT_32 i, rxNum;
	UINT_16 u2RxPktNum, u2RxLength = 0, u2Tmp = 0;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxSDIOReceiveRFBs");

	ASSERT(prAdapter);

	prSDIOCtrl = prAdapter->prSDIOCtrl;
	ASSERT(prSDIOCtrl);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	for (rxNum = 0; rxNum < 2; rxNum++) {
		u2RxPktNum =
		    (rxNum == 0 ? prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len : prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len);

		if (u2RxPktNum == 0)
			continue;

		for (i = 0; i < u2RxPktNum; i++) {
			if (rxNum == 0) {
				
				HAL_READ_RX_LENGTH(prAdapter, &u2RxLength, &u2Tmp);
			} else if (rxNum == 1) {
				
				HAL_READ_RX_LENGTH(prAdapter, &u2Tmp, &u2RxLength);
			}

			if (!u2RxLength)
				break;

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

			if (!prSwRfb) {
				DBGLOG(RX, TRACE, "No More RFB\n");
				break;
			}
			ASSERT(prSwRfb);

			if (nicRxEnhanceReadBuffer(prAdapter, rxNum, u2RxLength, prSwRfb) == WLAN_STATUS_FAILURE) {
				DBGLOG(RX, TRACE, "nicRxEnhanceRxReadBuffer failed\n");
				nicRxReturnRFB(prAdapter, prSwRfb);
				break;
			}
			

			KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
			QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
			RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
			KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		}
	}

	prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len = 0;
	prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len = 0;

}				

#endif 

#if CFG_SDIO_RX_AGG
VOID nicRxSDIOAggReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
	P_ENHANCE_MODE_DATA_STRUCT_T prEnhDataStr;
	P_RX_CTRL_T prRxCtrl;
	P_SDIO_CTRL_T prSDIOCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
	UINT_32 u4RxLength;
	UINT_32 i, rxNum;
	UINT_32 u4RxAggCount = 0, u4RxAggLength = 0;
	UINT_32 u4RxAvailAggLen, u4CurrAvailFreeRfbCnt;
	PUINT_8 pucSrcAddr;
	P_HIF_RX_HEADER_T prHifRxHdr;
	BOOLEAN fgResult = TRUE;
	BOOLEAN fgIsRxEnhanceMode;
	UINT_16 u2RxPktNum;
#if CFG_SDIO_RX_ENHANCE
	UINT_32 u4MaxLoopCount = CFG_MAX_RX_ENHANCE_LOOP_COUNT;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("nicRxSDIOAggReceiveRFBs");

	ASSERT(prAdapter);
	prEnhDataStr = prAdapter->prSDIOCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;
	prSDIOCtrl = prAdapter->prSDIOCtrl;

#if CFG_SDIO_RX_ENHANCE
	fgIsRxEnhanceMode = TRUE;
#else
	fgIsRxEnhanceMode = FALSE;
#endif


	do {
#if CFG_SDIO_RX_ENHANCE
		
		u4MaxLoopCount--;
		if (u4MaxLoopCount == 0)
			break;
#endif

		if (prEnhDataStr->rRxInfo.u.u2NumValidRx0Len == 0 && prEnhDataStr->rRxInfo.u.u2NumValidRx1Len == 0)
			break;

		for (rxNum = 0; rxNum < 2; rxNum++) {
			u2RxPktNum =
			    (rxNum ==
			     0 ? prEnhDataStr->rRxInfo.u.u2NumValidRx0Len : prEnhDataStr->rRxInfo.u.u2NumValidRx1Len);

			
			ASSERT(u2RxPktNum <= 16);

			if (u2RxPktNum > 16)
				continue;

			if (u2RxPktNum == 0)
				continue;

#if CFG_HIF_STATISTICS
			prRxCtrl->u4TotalRxAccessNum++;
			prRxCtrl->u4TotalRxPacketNum += u2RxPktNum;
#endif

			u4CurrAvailFreeRfbCnt = prRxCtrl->rFreeSwRfbList.u4NumElem;

			
			if (u4CurrAvailFreeRfbCnt < u2RxPktNum) {
#if CFG_HIF_RX_STARVATION_WARNING
				DbgPrint("FreeRfb is not enough: %d available, need %d\n", u4CurrAvailFreeRfbCnt,
					 u2RxPktNum);
				DbgPrint("Queued Count: %d / Dequeud Count: %d\n", prRxCtrl->u4QueuedCnt,
					 prRxCtrl->u4DequeuedCnt);
#endif
				continue;
			}
#if CFG_SDIO_RX_ENHANCE
			u4RxAvailAggLen =
			    CFG_RX_COALESCING_BUFFER_SIZE - (sizeof(ENHANCE_MODE_DATA_STRUCT_T) +
							     4 );
#else
			u4RxAvailAggLen = CFG_RX_COALESCING_BUFFER_SIZE;
#endif
			u4RxAggCount = 0;

			for (i = 0; i < u2RxPktNum; i++) {
				u4RxLength = (rxNum == 0 ?
					      (UINT_32) prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
					      (UINT_32) prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

				if (!u4RxLength) {
					ASSERT(0);
					break;
				}

				if (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN) < u4RxAvailAggLen) {
					if (u4RxAggCount < u4CurrAvailFreeRfbCnt) {
						u4RxAvailAggLen -= ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN);
						u4RxAggCount++;
					} else {
						
						ASSERT(0);
						break;
					}
				} else {
					
					ASSERT(0);
					break;
				}
			}

			u4RxAggLength = (CFG_RX_COALESCING_BUFFER_SIZE - u4RxAvailAggLen);
			
			

			HAL_READ_RX_PORT(prAdapter,
					 rxNum,
					 u4RxAggLength, prRxCtrl->pucRxCoalescingBufPtr, CFG_RX_COALESCING_BUFFER_SIZE);
			if (!fgResult) {
				DBGLOG(RX, ERROR, "Read RX Agg Packet Error\n");
				continue;
			}

			pucSrcAddr = prRxCtrl->pucRxCoalescingBufPtr;
			for (i = 0; i < u4RxAggCount; i++) {
				UINT_16 u2PktLength;

				u2PktLength = (rxNum == 0 ?
					       prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
					       prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
				QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

				ASSERT(prSwRfb);
				kalMemCopy(prSwRfb->pucRecvBuff, pucSrcAddr,
					   ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));

				
				STATS_RX_ARRIVE_TIME_RECORD(prSwRfb);	

				prHifRxHdr = prSwRfb->prHifRxHdr;
				ASSERT(prHifRxHdr);

				prSwRfb->ucPacketType =
				    (UINT_8) (prHifRxHdr->u2PacketType & HIF_RX_HDR_PACKET_TYPE_MASK);
				

				prSwRfb->ucStaRecIdx = (UINT_8) (prHifRxHdr->ucStaRecIdx);

				KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
				QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
				RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
				KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

				pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
				
			}

#if CFG_SDIO_RX_ENHANCE
			kalMemCopy(prAdapter->prSDIOCtrl, (pucSrcAddr + 4), sizeof(ENHANCE_MODE_DATA_STRUCT_T));

			
			if ((prSDIOCtrl->u4WHISR & WHISR_TX_DONE_INT) == 0 &&
			    (prSDIOCtrl->rTxInfo.au4WTSR[0] | prSDIOCtrl->rTxInfo.au4WTSR[1])) {
				prSDIOCtrl->u4WHISR |= WHISR_TX_DONE_INT;
			}

			if ((prSDIOCtrl->u4WHISR & BIT(31)) == 0 &&
			    HAL_GET_MAILBOX_READ_CLEAR(prAdapter) == TRUE &&
			    (prSDIOCtrl->u4RcvMailbox0 != 0 || prSDIOCtrl->u4RcvMailbox1 != 0)) {
				prSDIOCtrl->u4WHISR |= BIT(31);
			}

			
			nicProcessIST_impl(prAdapter,
					   prSDIOCtrl->u4WHISR & (~(WHISR_RX0_DONE_INT | WHISR_RX1_DONE_INT)));
#endif
		}

#if !CFG_SDIO_RX_ENHANCE
		prEnhDataStr->rRxInfo.u.u2NumValidRx0Len = 0;
		prEnhDataStr->rRxInfo.u.u2NumValidRx1Len = 0;
#endif
	} while ((prEnhDataStr->rRxInfo.u.u2NumValidRx0Len || prEnhDataStr->rRxInfo.u.u2NumValidRx1Len)
		&& fgIsRxEnhanceMode);

}
#endif 

WLAN_STATUS nicRxSetupRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	PVOID pvPacket;
	PUINT_8 pucRecvBuff;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (!prSwRfb->pvPacket) {
		kalMemZero(prSwRfb, sizeof(SW_RFB_T));
		pvPacket = kalPacketAlloc(prAdapter->prGlueInfo, CFG_RX_MAX_PKT_SIZE, &pucRecvBuff);
		if (pvPacket == NULL)
			return WLAN_STATUS_RESOURCES;

		prSwRfb->pvPacket = pvPacket;
		prSwRfb->pucRecvBuff = (PVOID) pucRecvBuff;
	} else {
		kalMemZero(((PUINT_8) prSwRfb + OFFSET_OF(SW_RFB_T, prHifRxHdr)),
			   (sizeof(SW_RFB_T) - OFFSET_OF(SW_RFB_T, prHifRxHdr)));
	}

	prSwRfb->prHifRxHdr = (P_HIF_RX_HEADER_T) (prSwRfb->pucRecvBuff);

	return WLAN_STATUS_SUCCESS;

}				

VOID nicRxReturnRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_RX_CTRL_T prRxCtrl;
	P_QUE_ENTRY_T prQueEntry;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prSwRfb);
	prRxCtrl = &prAdapter->rRxCtrl;
	prQueEntry = &prSwRfb->rQueEntry;

	ASSERT(prQueEntry);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

	if (prSwRfb->pvPacket) {
		
		QUEUE_INSERT_TAIL(&prRxCtrl->rFreeSwRfbList, prQueEntry);
	} else {
		
		QUEUE_INSERT_TAIL(&prRxCtrl->rIndicatedRfbList, prQueEntry);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
}				

VOID nicProcessRxInterrupt(IN P_ADAPTER_T prAdapter)
{
	P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;

	prGlueInfo->IsrRxCnt++;
#if CFG_SDIO_INTR_ENHANCE
#if CFG_SDIO_RX_AGG
	nicRxSDIOAggReceiveRFBs(prAdapter);
#else
	nicRxSDIOReceiveRFBs(prAdapter);
#endif
#else
	nicRxReceiveRFBs(prAdapter);
#endif 

	nicRxProcessRFBs(prAdapter);

	return;

}				

#if CFG_TCP_IP_CHKSUM_OFFLOAD
VOID nicRxUpdateCSUMStatistics(IN P_ADAPTER_T prAdapter, IN const ENUM_CSUM_RESULT_T aeCSUM[])
{
	P_RX_CTRL_T prRxCtrl;

	ASSERT(prAdapter);
	ASSERT(aeCSUM);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS) ||
		(aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_IP_SUCCESS_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_FAILED) || (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_FAILED)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_IP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_NONE) && (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_NONE)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	} else {
		ASSERT(0);
	}

	if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_SUCCESS) {
		
		RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_FAILED) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_FAILED_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_SUCCESS) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_FAILED) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_NONE) && (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_NONE)) {
		RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	} else {
		ASSERT(0);
	}

}				
#endif 

VOID nicRxQueryStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucCurrBuf = pucBuffer;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	 
	ASSERT(pu4Count);

	SPRINTF(pucCurrBuf, ("\n\nRX CTRL STATUS:"));
	SPRINTF(pucCurrBuf, ("\n==============="));
	SPRINTF(pucCurrBuf, ("\nFREE RFB w/i BUF LIST :%9u", prRxCtrl->rFreeSwRfbList.u4NumElem));
	SPRINTF(pucCurrBuf, ("\nFREE RFB w/o BUF LIST :%9u", prRxCtrl->rIndicatedRfbList.u4NumElem));
	SPRINTF(pucCurrBuf, ("\nRECEIVED RFB LIST     :%9u", prRxCtrl->rReceivedRfbList.u4NumElem));

	SPRINTF(pucCurrBuf, ("\n\n"));

	

}				

VOID nicRxClearStatistics(IN P_ADAPTER_T prAdapter)
{
	P_RX_CTRL_T prRxCtrl;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	RX_RESET_ALL_CNTS(prRxCtrl);
}

VOID nicRxQueryStatistics(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count)
{
	P_RX_CTRL_T prRxCtrl;
	PUINT_8 pucCurrBuf = pucBuffer;

	ASSERT(prAdapter);
	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	 
	ASSERT(pu4Count);

#define SPRINTF_RX_COUNTER(eCounter) \
	SPRINTF(pucCurrBuf, ("%-30s : %u\n", #eCounter, (UINT_32)prRxCtrl->au8Statistics[eCounter]))

	SPRINTF_RX_COUNTER(RX_MPDU_TOTAL_COUNT);
	SPRINTF_RX_COUNTER(RX_SIZE_ERR_DROP_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_INDICATION_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_RETURNED_COUNT);
	SPRINTF_RX_COUNTER(RX_DATA_RETAINED_COUNT);

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	SPRINTF_RX_COUNTER(RX_CSUM_TCP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UDP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_IP_FAILED_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_TCP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UDP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_IP_SUCCESS_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	SPRINTF_RX_COUNTER(RX_IP_V6_PKT_CCOUNT);
#endif

	

	nicRxClearStatistics(prAdapter);

}

WLAN_STATUS
nicRxWaitResponse(IN P_ADAPTER_T prAdapter,
		  IN UINT_8 ucPortIdx, OUT PUINT_8 pucRspBuffer, IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length)
{
	UINT_32 u4Value = 0, u4PktLen = 0;
	UINT_32 i = 0;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
	BOOLEAN fgResult = TRUE;
	UINT_32 u4Time, u4Current;

	DEBUGFUNC("nicRxWaitResponse");

	ASSERT(prAdapter);
	ASSERT(pucRspBuffer);
	ASSERT(ucPortIdx < 2);

	u4Time = kalGetTimeTick();

	do {
		
		HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4Value);

		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read Response Packet Error\n");
			return WLAN_STATUS_FAILURE;
		}

		if (ucPortIdx == 0)
			u4PktLen = u4Value & 0xFFFF;
		else
			u4PktLen = (u4Value >> 16) & 0xFFFF;


		if (u4PktLen == 0) {
			
			u4Current = kalGetTimeTick();

			if ((u4Current > u4Time) && ((u4Current - u4Time) > RX_RESPONSE_TIMEOUT)) {
				DBGLOG(RX, ERROR, "RX_RESPONSE_TIMEOUT1 %u %d %u\n", u4PktLen, i, u4Current);
				return WLAN_STATUS_FAILURE;
			} else if (u4Current < u4Time && ((u4Current + (0xFFFFFFFF - u4Time)) > RX_RESPONSE_TIMEOUT)) {
				DBGLOG(RX, ERROR, "RX_RESPONSE_TIMEOUT2 %u %d %u\n", u4PktLen, i, u4Current);
				return WLAN_STATUS_FAILURE;
			}

			
			kalUdelay(50);

			i++;
			continue;
		}
		if (u4PktLen > u4MaxRespBufferLen) {
			DBGLOG(RX, ERROR,
			       "Not enough Event Buffer: required length = 0x%x, available buffer length = %d\n",
				u4PktLen, u4MaxRespBufferLen);
			DBGLOG(RX, ERROR, "i = %d, u4PktLen = %u\n", i, u4PktLen);
			return WLAN_STATUS_FAILURE;
		}
		HAL_PORT_RD(prAdapter,
			    ucPortIdx == 0 ? MCR_WRDR0 : MCR_WRDR1, u4PktLen, pucRspBuffer, u4MaxRespBufferLen);

		
		if (!fgResult) {
			DBGLOG(RX, ERROR, "Read Response Packet Error\n");
			return WLAN_STATUS_FAILURE;
		}

		DBGLOG(RX, TRACE, "Dump Response buffer, length = 0x%x\n", u4PktLen);
		DBGLOG_MEM8(RX, TRACE, pucRspBuffer, u4PktLen);

		*pu4Length = u4PktLen;
		break;
	} while (TRUE);

	return u4Status;
}

VOID nicRxEnablePromiscuousMode(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

}				

VOID nicRxDisablePromiscuousMode(IN P_ADAPTER_T prAdapter)
{
	ASSERT(prAdapter);

}				

WLAN_STATUS nicRxFlush(IN P_ADAPTER_T prAdapter)
{
	P_SW_RFB_T prSwRfb;

	ASSERT(prAdapter);

	prSwRfb = qmFlushRxQueues(prAdapter);
	if (prSwRfb != NULL) {
		do {
			P_SW_RFB_T prNextSwRfb;

			
			prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prSwRfb);

			
			nicRxReturnRFB(prAdapter, prSwRfb);

			prSwRfb = prNextSwRfb;
		} while (prSwRfb);
	}

	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS nicRxProcessActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_WLAN_ACTION_FRAME prActFrame;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	if (prSwRfb->u2PacketLen < sizeof(WLAN_ACTION_FRAME) - 1)
		return WLAN_STATUS_INVALID_PACKET;
	prActFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;
	DBGLOG(RX, INFO, "Category %u\n", prActFrame->ucCategory);

	switch (prActFrame->ucCategory) {
	case CATEGORY_PUBLIC_ACTION:
		if (HIF_RX_HDR_GET_NETWORK_IDX(prSwRfb->prHifRxHdr) == NETWORK_TYPE_AIS_INDEX)
			aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
#if CFG_ENABLE_WIFI_DIRECT
		else if (prAdapter->fgIsP2PRegistered) {
			rlmProcessPublicAction(prAdapter, prSwRfb);

			p2pFuncValidateRxActionFrame(prAdapter, prSwRfb);

		}
#endif
		break;

	case CATEGORY_HT_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			rlmProcessHtAction(prAdapter, prSwRfb);
#endif
		break;
	case CATEGORY_VENDOR_SPECIFIC_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			p2pFuncValidateRxActionFrame(prAdapter, prSwRfb);
#endif
		break;
#if CFG_SUPPORT_802_11W
	case CATEGORY_SA_QUERT_ACTION:
		{
			P_HIF_RX_HEADER_T prHifRxHdr;

			prHifRxHdr = prSwRfb->prHifRxHdr;

			if ((HIF_RX_HDR_GET_NETWORK_IDX(prHifRxHdr) == NETWORK_TYPE_AIS_INDEX)
					&& prAdapter->rWifiVar.rAisSpecificBssInfo.fgMgmtProtection	) {
				if (!(prHifRxHdr->ucReserved & CONTROL_FLAG_UC_MGMT_NO_ENC)) {
					
					rsnSaQueryAction(prAdapter, prSwRfb);
				} else {
					DBGLOG(RSN, TRACE, "Un-Protected SA Query, do nothing\n");
				}
			}
		}
		break;
#endif
#if CFG_SUPPORT_802_11V
	case CATEGORY_WNM_ACTION:
		{
			wnmWNMAction(prAdapter, prSwRfb);
		}
		break;
#endif

#if CFG_SUPPORT_DFS		
	case CATEGORY_SPEC_MGT:
		{
			if (prAdapter->fgEnable5GBand == TRUE)
				rlmProcessSpecMgtAction(prAdapter, prSwRfb);
		}
		break;
#endif

#if (CFG_SUPPORT_TDLS == 1)
	case 12:		
		break;
#endif 

	default:
		break;
	}			

	return WLAN_STATUS_SUCCESS;
}