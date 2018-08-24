


#ifndef _SCAN_H
#define _SCAN_H



#include "gl_vendor.h"

extern BOOLEAN flgTdlsTestExtCapElm;
extern UINT8 aucTdlsTestExtCapElm[];

/*! Maximum buffer size of SCAN list */
#define SCN_MAX_BUFFER_SIZE                 (CFG_MAX_NUM_BSS_LIST * ALIGN_4(sizeof(BSS_DESC_T)))

#define SCN_RM_POLICY_EXCLUDE_CONNECTED     BIT(0)	
#define SCN_RM_POLICY_TIMEOUT               BIT(1)	
#define SCN_RM_POLICY_OLDEST_HIDDEN         BIT(2)	
#define SCN_RM_POLICY_SMART_WEAKEST         BIT(3)	
#define SCN_RM_POLICY_ENTIRE                BIT(4)	

#define SCN_BSS_DESC_SAME_SSID_THRESHOLD    20	

#define SCN_BSS_DESC_REMOVE_TIMEOUT_SEC     15	

#define SCN_PROBE_DELAY_MSEC                0

#define SCN_ADHOC_BSS_DESC_TIMEOUT_SEC      5	

#define SCN_NLO_NETWORK_CHANNEL_NUM         (4)

#define SCAN_REQ_SSID_WILDCARD              BIT(0)
#define SCAN_REQ_SSID_P2P_WILDCARD          BIT(1)
#define SCAN_REQ_SSID_SPECIFIED             BIT(2)

#define SCN_SSID_MAX_NUM                    CFG_SCAN_SSID_MAX_NUM
#define SCN_SSID_MATCH_MAX_NUM              CFG_SCAN_SSID_MATCH_MAX_NUM

#define SWC_NUM_BSSID_THRESHOLD_DEFAULT 8
#define SWC_RSSI_WINDSIZE_DEFAULT 8
#define LOST_AP_WINDOW 16
#define MAX_CHANNEL_NUM_PER_BUCKETS 8

#define SCN_BSS_JOIN_FAIL_THRESOLD				4
#define SCN_BSS_JOIN_FAIL_CNT_RESET_SEC				15
#define SCN_BSS_JOIN_FAIL_RESET_STEP				2

#if CFG_SUPPORT_BATCH_SCAN
#define SCAN_BATCH_REQ_START                BIT(0)
#define SCAN_BATCH_REQ_STOP                 BIT(1)
#define SCAN_BATCH_REQ_RESULT               BIT(2)
#endif

typedef enum _ENUM_SCAN_TYPE_T {
	SCAN_TYPE_PASSIVE_SCAN = 0,
	SCAN_TYPE_ACTIVE_SCAN,
	SCAN_TYPE_NUM
} ENUM_SCAN_TYPE_T, *P_ENUM_SCAN_TYPE_T;

typedef enum _ENUM_SCAN_STATE_T {
	SCAN_STATE_IDLE = 0,
	SCAN_STATE_SCANNING,
	SCAN_STATE_NUM
} ENUM_SCAN_STATE_T;

typedef enum _ENUM_SCAN_CHANNEL_T {
	SCAN_CHANNEL_FULL = 0,
	SCAN_CHANNEL_2G4,
	SCAN_CHANNEL_5G,
	SCAN_CHANNEL_P2P_SOCIAL,
	SCAN_CHANNEL_SPECIFIED,
	SCAN_CHANNEL_NUM
} ENUM_SCAN_CHANNEL, *P_ENUM_SCAN_CHANNEL;

typedef struct _MSG_SCN_FSM_T {
	MSG_HDR_T rMsgHdr;	
	UINT_32 u4Dummy;
} MSG_SCN_FSM_T, *P_MSG_SCN_FSM_T;

typedef enum _ENUM_POSTPONE_SCHED_SCAN_REQUEST_T {
	SCHED_SCAN_POSTPONE_START = 0,
	SCHED_SCAN_POSTPONE_STOP,
	SCHED_SCAN_POSTPONE_NUM
} ENUM_POSTPONE_SCHED_SCAN_REQUEST_T;


typedef enum _ENUM_PSCAN_STATE_T {
	PSCN_IDLE = 1,
	PSCN_SCANNING,
	PSCN_RESET,
	PSCAN_STATE_T_NUM
} ENUM_PSCAN_STATE_T;

struct _BSS_DESC_T {
	LINK_ENTRY_T rLinkEntry;

	UINT_8 aucBSSID[MAC_ADDR_LEN];
	UINT_8 aucSrcAddr[MAC_ADDR_LEN];	

	BOOLEAN fgIsConnecting;	
	BOOLEAN fgIsConnected;	

	BOOLEAN fgIsHiddenSSID;	
	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];

	OS_SYSTIME rUpdateTime;

	ENUM_BSS_TYPE_T eBSSType;

	UINT_16 u2CapInfo;

	UINT_16 u2BeaconInterval;
	UINT_16 u2ATIMWindow;

	UINT_16 u2OperationalRateSet;
	UINT_16 u2BSSBasicRateSet;
	BOOLEAN fgIsUnknownBssBasicRate;

	BOOLEAN fgIsERPPresent;
	BOOLEAN fgIsHTPresent;

	UINT_8 ucPhyTypeSet;	

	UINT_8 ucChannelNum;

	ENUM_CHNL_EXT_T eSco;	
	ENUM_BAND_T eBand;

	UINT_8 ucDTIMPeriod;

	BOOLEAN fgIsLargerTSF;	

	UINT_8 ucRCPI;

	UINT_8 ucWmmFlag;	

	UINT_32 u4RsnSelectedGroupCipher;
	UINT_32 u4RsnSelectedPairwiseCipher;
	UINT_32 u4RsnSelectedAKMSuite;

	UINT_16 u2RsnCap;

	RSN_INFO_T rRSNInfo;
	RSN_INFO_T rWPAInfo;
#if 1				
	WAPI_INFO_T rIEWAPI;
	BOOLEAN fgIEWAPI;
#endif
	BOOLEAN fgIERSN;
	BOOLEAN fgIEWPA;

	
	UINT_8 ucEncLevel;

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2PPresent;
	BOOLEAN fgIsP2PReport;	
	P_P2P_DEVICE_DESC_T prP2pDesc;

	UINT_8 aucIntendIfAddr[MAC_ADDR_LEN];	
 
 

	LINK_T rP2pDeviceList;


#endif

	BOOLEAN fgIsIEOverflow;	
	UINT_16 u2RawLength;	
	UINT_16 u2IELength;	

	ULARGE_INTEGER u8TimeStamp;	
	UINT_8 aucRawBuf[CFG_RAW_BUFFER_SIZE];
	UINT_8 aucIEBuf[CFG_IE_BUFFER_SIZE];
	UINT_8 ucJoinFailureCount;
	OS_SYSTIME rJoinFailTime;
};

typedef struct _SCAN_PARAM_T {	
	
	ENUM_SCAN_TYPE_T eScanType;

	
	ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex;

	
	UINT_8 ucSSIDType;
	UINT_8 ucSSIDNum;

	
	UINT_8 ucSpecifiedSSIDLen[SCN_SSID_MAX_NUM];

	
	UINT_8 aucSpecifiedSSID[SCN_SSID_MAX_NUM][ELEM_MAX_LEN_SSID];

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgFindSpecificDev;	
	UINT_8 aucDiscoverDevAddr[MAC_ADDR_LEN];
	BOOLEAN fgIsDevType;
	P2P_DEVICE_TYPE_T rDiscoverDevType;

	UINT_16 u2PassiveListenInterval;
	
#endif				

	BOOLEAN fgIsObssScan;
	BOOLEAN fgIsScanV2;

	
	UINT_16 u2ProbeDelayTime;

	
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];

	
	UINT_8 ucSeqNum;

	
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];

} SCAN_PARAM_T, *P_SCAN_PARAM_T;

typedef struct _NLO_PARAM_T {	
	SCAN_PARAM_T rScanParam;

	
	BOOLEAN fgStopAfterIndication;
	UINT_8 ucFastScanIteration;
	UINT_16 u2FastScanPeriod;
	UINT_16 u2SlowScanPeriod;

	
	UINT_8 ucMatchSSIDNum;
	UINT_8 ucMatchSSIDLen[SCN_SSID_MATCH_MAX_NUM];
	UINT_8 aucMatchSSID[SCN_SSID_MATCH_MAX_NUM][ELEM_MAX_LEN_SSID];

	UINT_8 aucCipherAlgo[SCN_SSID_MATCH_MAX_NUM];
	UINT_16 au2AuthAlgo[SCN_SSID_MATCH_MAX_NUM];
	UINT_8 aucChannelHint[SCN_SSID_MATCH_MAX_NUM][SCN_NLO_NETWORK_CHANNEL_NUM];
	P_BSS_DESC_T aprPendingBssDescToInd[SCN_SSID_MATCH_MAX_NUM];
} NLO_PARAM_T, *P_NLO_PARAM_T;

#if 1

typedef struct _GSCN_CHANNEL_INFO_T {
	UINT_8 ucBand;
	UINT_8 ucChannel;	
	UINT_8 ucPassive;	
	UINT_8 aucReserved[1];

	UINT_32 u4DwellTimeMs;	
	
} GSCN_CHANNEL_INFO_T, *P_GSCN_CHANNEL_INFO_T;

typedef struct _GSCAN_CHANNEL_BUCKET_T {

	UINT_16 u2BucketIndex;	
	UINT_8 ucBucketFreqMultiple;	
	UINT_8 ucReportFlag;
	UINT_8 ucNumChannels;
	UINT_8 aucReserved[3];
	WIFI_BAND eBand;	
	GSCN_CHANNEL_INFO_T arChannelList[GSCAN_MAX_CHANNELS];	
} GSCAN_CHANNEL_BUCKET_T, *P_GSCAN_CHANNEL_BUCKET_T;

typedef struct _CMD_GSCN_REQ_T {
	UINT_8 ucFlags;
	UINT_8 ucNumScnToCache;
	UINT_8 aucReserved[2];
	UINT_32 u4BufferThreshold;
	UINT_32 u4BasePeriod;	
	UINT_32 u4NumBuckets;
	UINT_32 u4MaxApPerScan;	
	

	GSCAN_CHANNEL_BUCKET_T arChannelBucket[GSCAN_MAX_BUCKETS];
} CMD_GSCN_REQ_T, *P_CMD_GSCN_REQ_T;

#endif

typedef struct _CMD_GSCN_SCN_COFIG_T {
	UINT_8 ucNumApPerScn;		
	UINT_32 u4NumScnToCache;	
	UINT_32 u4BufferThreshold;	
} CMD_GSCN_SCN_COFIG_T, *P_CMD_GSCN_SCN_COFIG_T;

typedef struct _CMD_GET_GSCAN_RESULT {
	UINT_8 ucVersion;
	UINT_8 aucReserved[2];
	UINT_8 ucFlush;
	UINT_32 u4Num;
} CMD_GET_GSCAN_RESULT_T, *P_CMD_GET_GSCAN_RESULT_T;

typedef struct _CMD_BATCH_REQ_T {
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	UINT_8 ucCmd;		
	UINT_8 ucMScan;		
	UINT_8 ucBestn;		
	UINT_8 ucRtt;		
	UINT_8 ucChannel;	
	UINT_8 ucChannelType;
	UINT_8 ucChannelListNum;
	UINT_8 aucReserved[3];
	UINT_32 u4Scanfreq;	
	CHANNEL_INFO_T arChannelList[32];	
} CMD_BATCH_REQ_T, *P_CMD_BATCH_REQ_T;

typedef struct _PSCN_PARAM_T {
	UINT_8 ucVersion;
	CMD_NLO_REQ rCurrentCmdNloReq;
	CMD_BATCH_REQ_T rCurrentCmdBatchReq;
	CMD_GSCN_REQ_T rCurrentCmdGscnReq;
	BOOLEAN fgNLOScnEnable;
	BOOLEAN fgBatchScnEnable;
	BOOLEAN fgGScnEnable;
	UINT_32 u4BasePeriod;	
} PSCN_PARAM_T, *P_PSCN_PARAM_T;

typedef struct _SCAN_INFO_T {
	ENUM_SCAN_STATE_T eCurrentState;	

	OS_SYSTIME rLastScanCompletedTime;

	SCAN_PARAM_T rScanParam;
	NLO_PARAM_T rNloParam;

	UINT_32 u4NumOfBssDesc;

	UINT_8 aucScanBuffer[SCN_MAX_BUFFER_SIZE];

	LINK_T rBSSDescList;

	LINK_T rFreeBSSDescList;

	LINK_T rPendingMsgList;

	
	BOOLEAN fgIsSparseChannelValid;
	RF_CHANNEL_INFO_T rSparseChannel;

	
	BOOLEAN fgNloScanning;
	BOOLEAN fgPscnOnnning;
	BOOLEAN fgGScnConfigSet;
	BOOLEAN fgGScnParamSet;
	BOOLEAN fgIsPostponeSchedScan;
	ENUM_POSTPONE_SCHED_SCAN_REQUEST_T eCurrendSchedScanReq;


	P_PSCN_PARAM_T prPscnParam;
	PARAM_SCHED_SCAN_REQUEST rSchedScanRequest;
	ENUM_PSCAN_STATE_T eCurrentPSCNState;
	BOOLEAN fgRptDisconnectPostponedToScanDone;
} SCAN_INFO_T, *P_SCAN_INFO_T;

typedef struct _PARTIAL_SCAN_INFO_T {
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
} PARTIAL_SCAN_INFO, *P_PARTIAL_SCAN_INFO;

typedef struct _MSG_SCN_SCAN_REQ_T {
	MSG_HDR_T rMsgHdr;	
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	ENUM_SCAN_TYPE_T eScanType;
	UINT_8 ucSSIDType;	
	UINT_8 ucSSIDLength;
	UINT_8 aucSSID[PARAM_MAX_LEN_SSID];
#if CFG_ENABLE_WIFI_DIRECT
	UINT_16 u2ChannelDwellTime;	
#endif
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} MSG_SCN_SCAN_REQ, *P_MSG_SCN_SCAN_REQ;

typedef struct _MSG_SCN_SCAN_REQ_V2_T {
	MSG_HDR_T rMsgHdr;	
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	ENUM_SCAN_TYPE_T eScanType;
	UINT_8 ucSSIDType;	
	UINT_8 ucSSIDNum;
	P_PARAM_SSID_T prSsid;
	UINT_16 u2ProbeDelay;
	UINT_16 u2ChannelDwellTime;	
	ENUM_SCAN_CHANNEL eScanChannel;
	UINT_8 ucChannelListNum;
	RF_CHANNEL_INFO_T arChnlInfoList[MAXIMUM_OPERATION_CHANNEL_LIST];
	UINT_16 u2IELen;
	UINT_8 aucIE[MAX_IE_LENGTH];
} MSG_SCN_SCAN_REQ_V2, *P_MSG_SCN_SCAN_REQ_V2;

typedef struct _MSG_SCN_SCAN_CANCEL_T {
	MSG_HDR_T rMsgHdr;	
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsChannelExt;
#endif
} MSG_SCN_SCAN_CANCEL, *P_MSG_SCN_SCAN_CANCEL;

typedef enum _ENUM_SCAN_STATUS_T {
	SCAN_STATUS_DONE = 0,
	SCAN_STATUS_CANCELLED,
	SCAN_STATUS_FAIL,
	SCAN_STATUS_BUSY,
	SCAN_STATUS_NUM
} ENUM_SCAN_STATUS, *P_ENUM_SCAN_STATUS;

typedef struct _MSG_SCN_SCAN_DONE_T {
	MSG_HDR_T rMsgHdr;	
	UINT_8 ucSeqNum;
	UINT_8 ucNetTypeIndex;
	ENUM_SCAN_STATUS eScanStatus;
} MSG_SCN_SCAN_DONE, *P_MSG_SCN_SCAN_DONE;

#if CFG_SUPPORT_AGPS_ASSIST
typedef enum {
	AGPS_PHY_A,
	AGPS_PHY_B,
	AGPS_PHY_G,
} AP_PHY_TYPE;

typedef struct _AGPS_AP_INFO_T {
	UINT_8 aucBSSID[6];
	INT_16 i2ApRssi;	
	UINT_16 u2Channel;	
	AP_PHY_TYPE ePhyType;
} AGPS_AP_INFO_T, *P_AGPS_AP_INFO_T;

typedef struct _AGPS_AP_LIST_T {
	UINT_8 ucNum;
	AGPS_AP_INFO_T arApInfo[32];
} AGPS_AP_LIST_T, *P_AGPS_AP_LIST_T;
#endif

typedef struct _CMD_SET_PSCAN_PARAM {
	UINT_8 ucVersion;
	CMD_NLO_REQ rCmdNloReq;
	CMD_BATCH_REQ_T rCmdBatchReq;
	CMD_GSCN_REQ_T rCmdGscnReq;
	BOOLEAN fgNLOScnEnable;
	BOOLEAN fgBatchScnEnable;
	BOOLEAN fgGScnEnable;
	UINT_32 u4BasePeriod;
} CMD_SET_PSCAN_PARAM, *P_CMD_SET_PSCAN_PARAM;

typedef struct _CMD_SET_PSCAN_ADD_HOTLIST_BSSID {
	UINT_8 aucMacAddr[6];
	UINT_8 ucFlags;
	UINT_8 aucReserved[5];
} CMD_SET_PSCAN_ADD_HOTLIST_BSSID, *P_CMD_SET_PSCAN_ADD_HOTLIST_BSSID;

typedef struct _CMD_SET_PSCAN_ADD_SWC_BSSID {
	INT_32 i4RssiLowThreshold;
	INT_32 i4RssiHighThreshold;
	UINT_8 aucMacAddr[6];
	UINT_8 aucReserved[6];
} CMD_SET_PSCAN_ADD_SWC_BSSID, *P_CMD_SET_PSCAN_ADD_SWC_BSSID;

typedef struct _CMD_SET_PSCAN_MAC_ADDR {
	UINT_8 ucVersion;
	UINT_8 ucFlags;
	UINT_8 aucMacAddr[6];
	UINT_8 aucReserved[8];
} CMD_SET_PSCAN_MAC_ADDR, *P_CMD_SET_PSCAN_MAC_ADDR;




VOID scnInit(IN P_ADAPTER_T prAdapter);

VOID scnUninit(IN P_ADAPTER_T prAdapter);

P_BSS_DESC_T scanSearchBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

P_BSS_DESC_T
scanSearchBssDescByBssidAndSsid(IN P_ADAPTER_T prAdapter,
				IN UINT_8 aucBSSID[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

P_BSS_DESC_T scanSearchBssDescByTA(IN P_ADAPTER_T prAdapter, IN UINT_8 aucSrcAddr[]);

P_BSS_DESC_T
scanSearchBssDescByTAAndSsid(IN P_ADAPTER_T prAdapter,
			     IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

#if CFG_SUPPORT_HOTSPOT_2_0
P_BSS_DESC_T scanSearchBssDescByBssidAndLatestUpdateTime(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);
#endif

P_BSS_DESC_T
scanSearchExistingBssDesc(IN P_ADAPTER_T prAdapter,
				IN ENUM_BSS_TYPE_T eBSSType, IN UINT_8 aucBSSID[], IN UINT_8 aucSrcAddr[]);

P_BSS_DESC_T
scanSearchExistingBssDescWithSsid(IN P_ADAPTER_T prAdapter,
					IN ENUM_BSS_TYPE_T eBSSType,
					IN UINT_8 aucBSSID[],
					IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid);

P_BSS_DESC_T scanAllocateBssDesc(IN P_ADAPTER_T prAdapter);

VOID scanRemoveBssDescsByPolicy(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemovePolicy);

VOID scanRemoveBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

VOID
scanRemoveBssDescByBandAndNetwork(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BAND_T eBand, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex);

VOID scanRemoveConnFlagOfBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[]);

#if 0
P_BSS_DESC_T scanAddToInternalScanResult(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb, IN P_BSS_DESC_T prBssDesc);
#endif

P_BSS_DESC_T scanAddToBssDesc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

WLAN_STATUS scanProcessBeaconAndProbeResp(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb);

VOID
scanBuildProbeReqFrameCommonIEs(IN P_MSDU_INFO_T prMsduInfo,
				IN PUINT_8 pucDesiredSsid, IN UINT_32 u4DesiredSsidLen, IN UINT_16 u2SupportedRateSet);

WLAN_STATUS scanSendProbeReqFrames(IN P_ADAPTER_T prAdapter, IN P_SCAN_PARAM_T prScanParam);

VOID scanUpdateBssDescForSearch(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc);

P_BSS_DESC_T scanSearchBssDescByPolicy(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex);

WLAN_STATUS scanAddScanResult(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc, IN P_SW_RFB_T prSwRfb);

VOID scanReportBss2Cfg80211(IN P_ADAPTER_T prAdapter, IN ENUM_BSS_TYPE_T eBSSType, IN P_BSS_DESC_T SpecificprBssDesc);

VOID scnFsmSteps(IN P_ADAPTER_T prAdapter, IN ENUM_SCAN_STATE_T eNextState);

VOID scnSendScanReqExtCh(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReq(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReqV2ExtCh(IN P_ADAPTER_T prAdapter);

VOID scnSendScanReqV2(IN P_ADAPTER_T prAdapter);

VOID scnEventScanDone(IN P_ADAPTER_T prAdapter, IN P_EVENT_SCAN_DONE prScanDone);

VOID scnEventNloDone(IN P_ADAPTER_T	 prAdapter,	IN P_EVENT_NLO_DONE_T prNloDone);

VOID scnFsmMsgStart(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmMsgAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr);

VOID scnFsmHandleScanMsg(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ prScanReqMsg);

VOID scnFsmHandleScanMsgV2(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg);

VOID scnFsmRemovePendingMsg(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSeqNum, IN UINT_8 ucNetTypeIndex);

VOID
scnFsmGenerateScanDoneMsg(IN P_ADAPTER_T prAdapter,
			  IN UINT_8 ucSeqNum, IN UINT_8 ucNetTypeIndex, IN ENUM_SCAN_STATUS eScanStatus);

BOOLEAN scnQuerySparseChannel(IN P_ADAPTER_T prAdapter, P_ENUM_BAND_T prSparseBand, PUINT_8 pucSparseChannel);

BOOLEAN
scnFsmSchedScanRequest(IN P_ADAPTER_T prAdapter,
		       IN UINT_8 ucSsidNum,
		       IN P_PARAM_SSID_T prSsid, IN UINT_32 u4IeLength, IN PUINT_8 pucIe, IN UINT_16 u2Interval);

BOOLEAN scnFsmSchedScanStopRequest(IN P_ADAPTER_T prAdapter);

VOID scnFsmSchedScanResumeRequest(IN P_ADAPTER_T prAdapter);

BOOLEAN scnFsmPSCNAction(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPscanAct);

BOOLEAN scnFsmPSCNSetParam(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam);

BOOLEAN scnFsmGSCNSetHotlist(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam);

#if 0

BOOLEAN scnFsmGSCNSetRssiSignificatn(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_PARAM prCmdPscnParam);
#endif

BOOLEAN scnFsmPSCNAddSWCBssId(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_ADD_SWC_BSSID prCmdPscnAddSWCBssId);

BOOLEAN scnFsmPSCNSetMacAddr(IN P_ADAPTER_T prAdapter, IN P_CMD_SET_PSCAN_MAC_ADDR prCmdPscnSetMacAddr);

#if 1				
BOOLEAN scnSetGSCNParam(IN P_ADAPTER_T prAdapter, IN P_PARAM_WIFI_GSCAN_CMD_PARAMS prCmdGscnParam);

#else
BOOLEAN scnSetGSCNParam(IN P_ADAPTER_T prAdapter, IN P_CMD_GSCN_REQ_T prCmdGscnParam);

#endif

BOOLEAN
scnCombineParamsIntoPSCN(IN P_ADAPTER_T prAdapter,
			 IN P_CMD_NLO_REQ prCmdNloReq,
			 IN P_CMD_BATCH_REQ_T prCmdBatchReq,
			 IN P_CMD_GSCN_REQ_T prCmdGscnReq,
			 IN P_CMD_GSCN_SCN_COFIG_T prNewCmdGscnConfig,
			 IN BOOLEAN fgRemoveNLOfromPSCN,
			 IN BOOLEAN fgRemoveBatchSCNfromPSCN, IN BOOLEAN fgRemoveGSCNfromPSCN);

BOOLEAN scnFsmSetGSCNConfig(IN P_ADAPTER_T prAdapter, IN P_CMD_GSCN_SCN_COFIG_T prCmdGscnScnConfig);

BOOLEAN scnFsmGetGSCNResult(IN P_ADAPTER_T prAdapter, IN P_CMD_GET_GSCAN_RESULT_T prGetGscnScnResultCmd);

VOID
scnPSCNFsm(IN P_ADAPTER_T prAdapter,
	   ENUM_PSCAN_STATE_T eNextPSCNState,
	   IN P_CMD_NLO_REQ prCmdNloReq,
	   IN P_CMD_BATCH_REQ_T prCmdBatchReq,
	   IN P_CMD_GSCN_REQ_T prCmdGscnReq,
	   IN P_CMD_GSCN_SCN_COFIG_T prNewCmdGscnConfig,
	   IN BOOLEAN fgRemoveNLOfromPSCN,
	   IN BOOLEAN fgRemoveBatchSCNfromPSCN, IN BOOLEAN fgRemoveGSCNfromPSCN, IN BOOLEAN fgEnableGSCN);

#endif 

#if CFG_SUPPORT_AGPS_ASSIST
VOID scanReportScanResultToAgps(P_ADAPTER_T prAdapter);
#endif
