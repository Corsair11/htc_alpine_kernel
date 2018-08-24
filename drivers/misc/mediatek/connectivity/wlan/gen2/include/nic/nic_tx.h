


#ifndef _NIC_TX_H
#define _NIC_TX_H



#define NIC_TX_RESOURCE_POLLING_TIMEOUT     256
#define NIC_TX_RESOURCE_POLLING_DELAY_MSEC  50


#if defined(MT6620)
#if CFG_SLT_SUPPORT
    
#define NIC_TX_BUFF_COUNT_TC0       0	
#define NIC_TX_BUFF_COUNT_TC1       16	
#define NIC_TX_BUFF_COUNT_TC2       0	
#define NIC_TX_BUFF_COUNT_TC3       0	
#define NIC_TX_BUFF_COUNT_TC4       4	
#define NIC_TX_BUFF_COUNT_TC5       0	
#else
    
#define NIC_TX_BUFF_COUNT_TC0       6	
#define NIC_TX_BUFF_COUNT_TC1       8	
#define NIC_TX_BUFF_COUNT_TC2       8	
#define NIC_TX_BUFF_COUNT_TC3       8	
#define NIC_TX_BUFF_COUNT_TC4       4	
#define NIC_TX_BUFF_COUNT_TC5       2	
#endif
#elif defined(MT6628)
#if (CFG_SRAM_SIZE_OPTION == 0)
#define NIC_TX_BUFF_COUNT_TC0       1	
#define NIC_TX_BUFF_COUNT_TC1       20	
#define NIC_TX_BUFF_COUNT_TC2       1	
#define NIC_TX_BUFF_COUNT_TC3       1	
#define NIC_TX_BUFF_COUNT_TC4       4	
#define NIC_TX_BUFF_COUNT_TC5       1	
#elif (CFG_SRAM_SIZE_OPTION == 1)
#define NIC_TX_BUFF_COUNT_TC0       1	
#define NIC_TX_BUFF_COUNT_TC1       36	
#define NIC_TX_BUFF_COUNT_TC2       1	
#define NIC_TX_BUFF_COUNT_TC3       1	
#define NIC_TX_BUFF_COUNT_TC4       4	
#define NIC_TX_BUFF_COUNT_TC5       1	
#elif (CFG_SRAM_SIZE_OPTION == 2)
#define NIC_TX_BUFF_COUNT_TC0       1	
#define NIC_TX_BUFF_COUNT_TC1       48	
#define NIC_TX_BUFF_COUNT_TC2       1	
#define NIC_TX_BUFF_COUNT_TC3       1	
#define NIC_TX_BUFF_COUNT_TC4       4	
#define NIC_TX_BUFF_COUNT_TC5       1	
#else
#error "> Set TX_BUFF_COUNT_TC error!"
#endif
#endif

#define NIC_TX_BUFF_SUM                     (NIC_TX_BUFF_COUNT_TC0 + \
					    NIC_TX_BUFF_COUNT_TC1 + \
					    NIC_TX_BUFF_COUNT_TC2 + \
					    NIC_TX_BUFF_COUNT_TC3 + \
					    NIC_TX_BUFF_COUNT_TC4 + \
					    NIC_TX_BUFF_COUNT_TC5)
#if CFG_ENABLE_FW_DOWNLOAD

#define NIC_TX_INIT_BUFF_COUNT_TC0               8
#define NIC_TX_INIT_BUFF_COUNT_TC1               0
#define NIC_TX_INIT_BUFF_COUNT_TC2               0
#define NIC_TX_INIT_BUFF_COUNT_TC3               0
#define NIC_TX_INIT_BUFF_COUNT_TC4               0
#define NIC_TX_INIT_BUFF_COUNT_TC5               0

#define NIC_TX_INIT_BUFF_SUM                    (NIC_TX_INIT_BUFF_COUNT_TC0 + \
						    NIC_TX_INIT_BUFF_COUNT_TC1 + \
						    NIC_TX_INIT_BUFF_COUNT_TC2 + \
						    NIC_TX_INIT_BUFF_COUNT_TC3 + \
						    NIC_TX_INIT_BUFF_COUNT_TC4 + \
						    NIC_TX_INIT_BUFF_COUNT_TC5)

#endif

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
#define NIC_TX_TIME_THRESHOLD                       100	
#endif

typedef enum _ENUM_TRAFFIC_CLASS_INDEX_T {
	TC0_INDEX = 0,		
	TC1_INDEX,		
	TC2_INDEX,		
	TC3_INDEX,		
	TC4_INDEX,		
	TC5_INDEX,		
	TC_NUM			
} ENUM_TRAFFIC_CLASS_INDEX_T;

typedef enum _ENUM_TX_STATISTIC_COUNTER_T {
	TX_MPDU_TOTAL_COUNT = 0,
	TX_INACTIVE_BSS_DROP,
	TX_INACTIVE_STA_DROP,
	TX_FORWARD_OVERFLOW_DROP,
	TX_AP_BORADCAST_DROP,
	TX_STATISTIC_COUNTER_NUM
} ENUM_TX_STATISTIC_COUNTER_T;

typedef struct _TX_TCQ_STATUS_T {
	UINT_8 aucFreeBufferCount[TC_NUM];
	UINT_8 aucMaxNumOfBuffer[TC_NUM];
} TX_TCQ_STATUS_T, *P_TX_TCQ_STATUS_T;

typedef struct _TX_TCQ_ADJUST_T {
	INT_8 acVariation[TC_NUM];
} TX_TCQ_ADJUST_T, *P_TX_TCQ_ADJUST_T;

typedef struct _TX_CTRL_T {
	UINT_32 u4TxCachedSize;
	PUINT_8 pucTxCached;


	TX_TCQ_STATUS_T rTc;

	PUINT_8 pucTxCoalescingBufPtr;

	QUE_T rFreeMsduInfoList;

	
	
	INT_32 i4TxMgmtPendingNum;

	
	QUE_T rTxMgmtTxingQueue;

#if CFG_HIF_STATISTICS
	UINT_32 u4TotalTxAccessNum;
	UINT_32 u4TotalTxPacketNum;
#endif
	UINT_32 au4Statistics[TX_STATISTIC_COUNTER_NUM];

	
	INT_32 i4PendingFwdFrameCount;

} TX_CTRL_T, *P_TX_CTRL_T;

typedef enum _ENUM_TX_PACKET_SRC_T {
	TX_PACKET_OS,
	TX_PACKET_OS_OID,
	TX_PACKET_FORWARDING,
	TX_PACKET_MGMT,
	TX_PACKET_NUM
} ENUM_TX_PACKET_SRC_T;

typedef enum _ENUM_HIF_TX_PACKET_TYPE_T {
	HIF_TX_PACKET_TYPE_DATA = 0,
	HIF_TX_PACKET_TYPE_COMMAND,
	HIF_TX_PACKET_TYPE_HIF_LB,
	HIF_TX_PACKET_TYPE_MGMT
} ENUM_HIF_TX_PACKET_TYPE_T, *P_ENUM_HIF_TX_PACKET_TYPE_T;

typedef enum _ENUM_TX_RESULT_CODE_T {
	TX_RESULT_SUCCESS = 0,
	TX_RESULT_LIFE_TIMEOUT,
	TX_RESULT_RTS_ERROR,
	TX_RESULT_MPDU_ERROR,
	TX_RESULT_AGING_TIMEOUT,
	TX_RESULT_FLUSHED,
	TX_RESULT_DROPPED_IN_DRIVER = 32,
	TX_RESULT_FW_FLUSH = 78,
	TX_RESULT_NUM
} ENUM_TX_RESULT_CODE_T, *P_ENUM_TX_RESULT_CODE_T;

struct _WLAN_CFG_ENTRY_T {
	UINT_8 aucKey[WLAN_CFG_KEY_LEN_MAX];
	UINT_8 aucValue[WLAN_CFG_VALUE_LEN_MAX];
	WLAN_CFG_SET_CB pfSetCb;
	PVOID pPrivate;
	UINT_32 u4Flags;
};

struct _WLAN_CFG_T {
	UINT_32 u4WlanCfgEntryNumMax;
	UINT_32 u4WlanCfgKeyLenMax;
	UINT_32 u4WlanCfgValueLenMax;
	WLAN_CFG_ENTRY_T arWlanCfgBuf[WLAN_CFG_ENTRY_NUM_MAX];
};

typedef WLAN_STATUS(*PFN_TX_DONE_HANDLER) (IN P_ADAPTER_T prAdapter,
					   IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
typedef struct _PKT_PROFILE_T {
	BOOLEAN fgIsValid;
#if CFG_PRINT_RTP_PROFILE
	BOOLEAN fgIsPrinted;
	UINT_16 u2IpSn;
	UINT_16 u2RtpSn;
	UINT_8 ucTcxFreeCount;
#endif
	OS_SYSTIME rHardXmitArrivalTimestamp;
	OS_SYSTIME rEnqueueTimestamp;
	OS_SYSTIME rDequeueTimestamp;
	OS_SYSTIME rHifTxDoneTimestamp;
} PKT_PROFILE_T, *P_PKT_PROFILE_T;
#endif



struct _MSDU_INFO_T {
	QUE_ENTRY_T rQueEntry;
	P_NATIVE_PACKET prPacket;

	ENUM_TX_PACKET_SRC_T eSrc;	
	UINT_8 ucUserPriority;

	
	UINT_8 ucTC;		
	UINT_8 ucPacketType;	
	UINT_8 ucStaRecIndex;
	UINT_8 ucNetworkType;	
	UINT_8 ucFormatID;	
	BOOLEAN fgIs802_1x;	
	BOOLEAN fgIs802_11;	
	UINT_16 u2PalLLH;	
	UINT_16 u2AclSN;	
	UINT_8 ucPsForwardingType;	
	UINT_8 ucPsSessionID;	
	BOOLEAN fgIsBurstEnd;	
	BOOLEAN fgIsBIP;	
	BOOLEAN fgIsBasicRate;	

	
	UINT_8 ucMacHeaderLength;
	UINT_8 ucLlcLength;	
	UINT_16 u2FrameLength;
	UINT_8 aucEthDestAddr[MAC_ADDR_LEN];	

	
	UINT_8 ucTxSeqNum;
	PFN_TX_DONE_HANDLER pfTxDoneHandler;
	BOOLEAN fgNeedTxDoneStatus;

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
	PKT_PROFILE_T rPktProfile;
#endif
	COMMAND_TYPE eCmdType;
	UINT_8 ucCID;
	UINT_32 u4InqueTime;
};




#define TX_INC_CNT(prTxCtrl, eCounter)              \
	{((P_TX_CTRL_T)prTxCtrl)->au4Statistics[eCounter]++; }

#define TX_ADD_CNT(prTxCtrl, eCounter, u8Amount)    \
	{((P_TX_CTRL_T)prTxCtrl)->au4Statistics[eCounter] += (UINT_32)u8Amount; }

#define TX_GET_CNT(prTxCtrl, eCounter)              \
	(((P_TX_CTRL_T)prTxCtrl)->au4Statistics[eCounter])

#define TX_RESET_ALL_CNTS(prTxCtrl)                 \
	{kalMemZero(&prTxCtrl->au4Statistics[0], sizeof(prTxCtrl->au4Statistics)); }

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
#define PRINT_PKT_PROFILE(_pkt_profile, _note) \
{ \
	if (!(_pkt_profile)->fgIsPrinted) { \
		DBGLOG(TX, TRACE, "X[%u] E[%u] D[%u] HD[%u] B[%d] RTP[%d] %s\n", \
		(UINT_32)((_pkt_profile)->rHardXmitArrivalTimestamp), \
		(UINT_32)((_pkt_profile)->rEnqueueTimestamp), \
		(UINT_32)((_pkt_profile)->rDequeueTimestamp), \
		(UINT_32)((_pkt_profile)->rHifTxDoneTimestamp), \
		(UINT_8)((_pkt_profile)->ucTcxFreeCount), \
		(UINT_16)((_pkt_profile)->u2RtpSn), \
		(_note)); \
		(_pkt_profile)->fgIsPrinted = TRUE; \
	} \
}

#define CHK_PROFILES_DELTA(_pkt1, _pkt2, _delta) \
	   (CHECK_FOR_TIMEOUT((_pkt1)->rHardXmitArrivalTimestamp, (_pkt2)->rHardXmitArrivalTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt1)->rEnqueueTimestamp, (_pkt2)->rEnqueueTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt1)->rDequeueTimestamp, (_pkt2)->rDequeueTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt1)->rHifTxDoneTimestamp, (_pkt2)->rHifTxDoneTimestamp, (_delta)))

#define CHK_PROFILE_DELTA(_pkt, _delta) \
	   (CHECK_FOR_TIMEOUT((_pkt)->rEnqueueTimestamp, (_pkt)->rHardXmitArrivalTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt)->rDequeueTimestamp, (_pkt)->rEnqueueTimestamp, (_delta)) || \
	    CHECK_FOR_TIMEOUT((_pkt)->rHifTxDoneTimestamp, (_pkt)->rDequeueTimestamp, (_delta)))
#endif

VOID nicTxInitialize(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicTxAcquireResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC);

WLAN_STATUS nicTxPollingResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC);

BOOLEAN nicTxReleaseResource(IN P_ADAPTER_T prAdapter, IN UINT_8 *aucTxRlsCnt);

WLAN_STATUS nicTxResetResource(IN P_ADAPTER_T prAdapter);

UINT_8 nicTxGetResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC);

WLAN_STATUS nicTxMsduInfoList(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

WLAN_STATUS nicTxMsduQueue(IN P_ADAPTER_T prAdapter, UINT_8 ucPortIdx, P_QUE_T prQue);

WLAN_STATUS nicTxCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC);

VOID nicTxRelease(IN P_ADAPTER_T prAdapter);

VOID nicProcessTxInterrupt(IN P_ADAPTER_T prAdapter);

VOID nicTxFreeMsduInfoPacket(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

VOID nicTxReturnMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

BOOLEAN nicTxFillMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prNdisPacket);

WLAN_STATUS nicTxAdjustTcq(IN P_ADAPTER_T prAdapter);

WLAN_STATUS nicTxFlush(IN P_ADAPTER_T prAdapter);

#if CFG_ENABLE_FW_DOWNLOAD
WLAN_STATUS nicTxInitCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC);

WLAN_STATUS nicTxInitResetResource(IN P_ADAPTER_T prAdapter);
#endif

WLAN_STATUS nicTxEnqueueMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

UINT_32 nicTxGetFreeCmdCount(IN P_ADAPTER_T prAdapter);


#endif 
