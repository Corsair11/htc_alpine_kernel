



#include "precomp.h"

#define AIS_ROAMING_CONNECTION_TRIAL_LIMIT  2
#define AIS_JOIN_TIMEOUT                    7

#define CTIA_MAGIC_SSID                     "no_use_ctia_ssid"	
#define CTIA_MAGIC_SSID_LEN                 30

#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_0	0
#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_1	1
#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_2	2



#if DBG
static PUINT_8 apucDebugAisState[AIS_STATE_NUM] = {
	(PUINT_8) DISP_STRING("AIS_STATE_IDLE"),
	(PUINT_8) DISP_STRING("AIS_STATE_SEARCH"),
	(PUINT_8) DISP_STRING("AIS_STATE_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_ONLINE_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_LOOKING_FOR"),
	(PUINT_8) DISP_STRING("AIS_STATE_WAIT_FOR_NEXT_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_REQ_CHANNEL_JOIN"),
	(PUINT_8) DISP_STRING("AIS_STATE_JOIN"),
	(PUINT_8) DISP_STRING("AIS_STATE_JOIN_FAILURE"),
	(PUINT_8) DISP_STRING("AIS_STATE_IBSS_ALONE"),
	(PUINT_8) DISP_STRING("AIS_STATE_IBSS_MERGE"),
	(PUINT_8) DISP_STRING("AIS_STATE_NORMAL_TR"),
	(PUINT_8) DISP_STRING("AIS_STATE_DISCONNECTING"),
	(PUINT_8) DISP_STRING("AIS_STATE_REQ_REMAIN_ON_CHANNEL"),
	(PUINT_8) DISP_STRING("AIS_STATE_REMAIN_ON_CHANNEL")
};

#endif 


static VOID aisFsmRunEventScanDoneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParam);
static VOID aisRemoveOldestBcnTimeout(P_AIS_FSM_INFO_T prAisFsmInfo);
static VOID aisRemoveDisappearedBlacklist(P_ADAPTER_T prAdapter);
#if CFG_SUPPORT_802_11K
static VOID
aisSendNeighborRequest(P_ADAPTER_T prAdapter);
#endif
VOID aisInitializeConnectionSettings(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_8 aucAnyBSSID[] = BC_BSSID;
	UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	int i = 0;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	
	COPY_MAC_ADDR(prConnSettings->aucMacAddress, aucZeroMacAddr);

	prConnSettings->ucDelayTimeOfDisconnectEvent = AIS_DELAY_TIME_OF_DISCONNECT_SEC;

	COPY_MAC_ADDR(prConnSettings->aucBSSID, aucAnyBSSID);
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	prConnSettings->eReConnectLevel = RECONNECT_LEVEL_MIN;
	prConnSettings->fgIsConnReqIssued = FALSE;
	prConnSettings->fgIsDisconnectedByNonRequest = FALSE;

	prConnSettings->ucSSIDLen = 0;

	prConnSettings->eOPMode = NET_TYPE_INFRA;

	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	if (prRegInfo) {
		prConnSettings->ucAdHocChannelNum = (UINT_8) nicFreq2ChannelNum(prRegInfo->u4StartFreq);
		prConnSettings->eAdHocBand = prRegInfo->u4StartFreq < 5000000 ? BAND_2G4 : BAND_5G;
		prConnSettings->eAdHocMode = (ENUM_PARAM_AD_HOC_MODE_T) (prRegInfo->u4AdhocMode);
	}

	prConnSettings->eAuthMode = AUTH_MODE_OPEN;

	prConnSettings->eEncStatus = ENUM_ENCRYPTION_DISABLED;

	prConnSettings->fgIsScanReqIssued = FALSE;

	
	prConnSettings->u2BeaconPeriod = DOT11_BEACON_PERIOD_DEFAULT;

	prConnSettings->u2RTSThreshold = DOT11_RTS_THRESHOLD_DEFAULT;

	prConnSettings->u2DesiredNonHTRateSet = RATE_SET_ALL_ABG;

	

	
	prConnSettings->bmfgApsdEnAc = PM_UAPSD_NONE;

	secInit(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	prConnSettings->fgIsEnableRoaming = FALSE;

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	prConnSettings->fgSecModeChangeStartTimer = FALSE;
#endif

#if CFG_SUPPORT_ROAMING
	if (prRegInfo)
		prConnSettings->fgIsEnableRoaming = ((prRegInfo->fgDisRoaming > 0) ? (FALSE) : (TRUE));
#endif 

	prConnSettings->fgIsAdHocQoSEnable = FALSE;

#if CFG_SUPPORT_802_11AC
	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGNAC;
#else
	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGN;
#endif

	
	prConnSettings->uc2G4BandwidthMode = CONFIG_BW_20M;
	prConnSettings->uc5GBandwidthMode = CONFIG_BW_20_40M;

	prConnSettings->rRsnInfo.ucElemId = 0x30;
	prConnSettings->rRsnInfo.u2Version = 0x0001;
	prConnSettings->rRsnInfo.u4GroupKeyCipherSuite = 0;
	prConnSettings->rRsnInfo.u4PairwiseKeyCipherSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++)
		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[i] = 0;
	prConnSettings->rRsnInfo.u4AuthKeyMgtSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++)
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[i] = 0;
	prConnSettings->rRsnInfo.u2RsnCap = 0;
	prConnSettings->rRsnInfo.fgRsnCapPresent = FALSE;

}				

VOID aisFsmInit(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmInit()");
	DBGLOG(SW1, INFO, "->aisFsmInit()\n");

	prAdapter->prAisBssInfo = prAisBssInfo = cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_AIS, FALSE);
	ASSERT(prAisBssInfo);

	
	COPY_MAC_ADDR(prAdapter->prAisBssInfo->aucOwnMacAddr, prAdapter->rMyMacAddr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	
	prAisFsmInfo->ePreviousState = AIS_STATE_IDLE;
	prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;

	prAisFsmInfo->ucAvailableAuthTypes = 0;

	prAisFsmInfo->prTargetBssDesc = (P_BSS_DESC_T) NULL;

	prAisFsmInfo->ucSeqNumOfReqMsg = 0;
	prAisFsmInfo->ucSeqNumOfChReq = 0;
	prAisFsmInfo->ucSeqNumOfScanReq = 0;

	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
#if CFG_SUPPORT_ROAMING
	prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif 
	prAisFsmInfo->fgIsChannelRequested = FALSE;
	prAisFsmInfo->fgIsChannelGranted = FALSE;
	prAisFsmInfo->u4PostponeIndStartTime = 0;
	prAisFsmInfo->ucJoinFailCntAfterScan = 0;

	
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rBGScanTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventBGSleepTimeOut, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rIbssAloneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventIbssAloneTimeOut, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rScanDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventScanDoneTimeOut, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rJoinTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventJoinTimeout, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rDeauthDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventDeauthTimeout, (ULONG) NULL);

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rSecModeChangeTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventSecModeChangeTimeout, (ULONG) NULL);
#endif

	
	SET_NET_PWR_STATE_IDLE(prAdapter, prAisBssInfo->ucBssIndex);

	
	BSS_INFO_INIT(prAdapter, prAisBssInfo);
	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr, prAdapter->rWifiVar.aucMacAddress);

	
	
	prAisBssInfo->eBand = BAND_2G4;
	prAisBssInfo->ucPrimaryChannel = 1;
	prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;

	
	prAisBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
						OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem[0]) + MAX_IE_LENGTH);

	if (prAisBssInfo->prBeacon) {
		prAisBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
		prAisBssInfo->prBeacon->ucStaRecIndex = 0xFF;	
	} else {
		ASSERT(0);
	}
	
	prAisBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;

#if 0
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
#else
	if (prAdapter->u4UapsdAcBmp == 0) {
		prAdapter->u4UapsdAcBmp = CFG_INIT_UAPSD_AC_BMP;
		
	}
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = (UINT_8) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = (UINT_8) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp = (UINT_8) prAdapter->u4MaxSpLen;
#endif

	
	LINK_INITIALIZE(&prAisFsmInfo->rPendingReqList);

	LINK_MGMT_INIT(&prAdapter->rWifiVar.rConnSettings.rBlackList);
	LINK_MGMT_INIT(&prAisFsmInfo->rBcnTimeout);
	kalMemZero(&prAisSpecificBssInfo->arCurEssChnlInfo[0], sizeof(prAisSpecificBssInfo->arCurEssChnlInfo));
	LINK_INITIALIZE(&prAisSpecificBssInfo->rCurEssLink);
	
	
	
	

	
	

}				

VOID aisFsmUninit(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmUninit()");
	DBGLOG(SW1, INFO, "->aisFsmUninit()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rSecModeChangeTimer);
#endif

	
	aisFsmFlushRequest(prAdapter);

	
	if ((prAisBssInfo != NULL) && (prAisBssInfo->prBeacon != NULL)) {
		cnmMgtPktFree(prAdapter, prAisBssInfo->prBeacon);
		prAisBssInfo->prBeacon = NULL;
	}
#if CFG_SUPPORT_802_11W
	rsnStopSaQuery(prAdapter);
#endif

	if (prAisBssInfo) {
		cnmFreeBssInfo(prAdapter, prAisBssInfo);
		prAdapter->prAisBssInfo = NULL;
	}
	LINK_MGMT_UNINIT(&prAdapter->rWifiVar.rConnSettings.rBlackList, struct AIS_BLACKLIST_ITEM, VIR_MEM_TYPE);
	LINK_MGMT_UNINIT(&prAisFsmInfo->rBcnTimeout, struct AIS_BEACON_TIMEOUT_BSS, VIR_MEM_TYPE);
}				

VOID aisFsmStateInit_JOIN(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_STA_RECORD_T prStaRec;
	P_MSG_JOIN_REQ_T prJoinReqMsg;

	DEBUGFUNC("aisFsmStateInit_JOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	ASSERT(prBssDesc);

	
	prBssDesc->fgIsConnecting = TRUE;

	
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_LEGACY_AP, prAdapter->prAisBssInfo->ucBssIndex, prBssDesc);

	prAisFsmInfo->prTargetStaRec = prStaRec;

	
	if (prStaRec->ucStaState == STA_STATE_1)
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

		prStaRec->fgIsReAssoc = FALSE;

		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_OPEN:	
		case AUTH_MODE_WPA:
		case AUTH_MODE_WPA_PSK:
		case AUTH_MODE_WPA2:
		case AUTH_MODE_WPA2_PSK:
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_OPEN_SYSTEM;
			break;

		case AUTH_MODE_SHARED:
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_SHARED_KEY;
			break;

		case AUTH_MODE_AUTO_SWITCH:
			DBGLOG(AIS, LOUD, "JOIN INIT: eAuthMode == AUTH_MODE_AUTO_SWITCH\n");
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) (AUTH_TYPE_OPEN_SYSTEM | AUTH_TYPE_SHARED_KEY);
			break;

		default:
			ASSERT(!(prConnSettings->eAuthMode == AUTH_MODE_WPA_NONE));
			DBGLOG(AIS, ERROR,
			       "JOIN INIT: Auth Algorithm : %d was not supported by JOIN\n",
				prConnSettings->eAuthMode);
			
			return;
		}

		
		prAisSpecificBssInfo->ucRoamingAuthTypes = prAisFsmInfo->ucAvailableAuthTypes;

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;

	} else {
		ASSERT(prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE);
		ASSERT(!prBssDesc->fgIsConnected);

		DBGLOG(AIS, LOUD, "JOIN INIT: AUTH TYPE = %d for Roaming\n",
				   prAisSpecificBssInfo->ucRoamingAuthTypes);

		prStaRec->fgIsReAssoc = TRUE;	

		
		prAisFsmInfo->ucAvailableAuthTypes = prAisSpecificBssInfo->ucRoamingAuthTypes;

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT_FOR_ROAMING;
	}

	
	if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, LOUD, "JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n");
		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_SHARED_KEY) {

		DBGLOG(AIS, LOUD, "JOIN INIT: Try to do Authentication with AuthType == SHARED_KEY.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SHARED_KEY;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_FAST_BSS_TRANSITION) {

		DBGLOG(AIS, LOUD, "JOIN INIT: Try to do Authentication with AuthType == FAST_BSS_TRANSITION.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_FAST_BSS_TRANSITION;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION;
	} else {
		ASSERT(0);
	}

	
	if (prBssDesc->ucSSIDLen)
		COPY_SSID(prConnSettings->aucSSID, prConnSettings->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	
	prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
	if (!prJoinReqMsg) {

		ASSERT(0);	
		return;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	if (1) {
		int j;
		P_FRAG_INFO_T prFragInfo;

		for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
			prFragInfo = &prStaRec->rFragInfo[j];

			if (prFragInfo->pr1stFrag) {
				
				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
			}
		}
	}

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

}				

BOOLEAN aisFsmStateInit_RetryJOIN(IN P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_JOIN_REQ_T prJoinReqMsg;

	DEBUGFUNC("aisFsmStateInit_RetryJOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	
	if (!prAisFsmInfo->ucAvailableAuthTypes)
		return FALSE;

	if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_SHARED_KEY) {

		DBGLOG(AIS, INFO, "RETRY JOIN INIT: Retry Authentication with AuthType == SHARED_KEY.\n");

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SHARED_KEY;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY;
	} else {
		DBGLOG(AIS, ERROR, "RETRY JOIN INIT: Retry Authentication with Unexpected AuthType.\n");
		ASSERT(0);
	}

	prAisFsmInfo->ucAvailableAuthTypes = 0;	

	
	prJoinReqMsg = (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
	if (!prJoinReqMsg) {

		ASSERT(0);	
		return FALSE;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

	return TRUE;

}				

#if CFG_SUPPORT_ADHOC
VOID aisFsmStateInit_IBSS_ALONE(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;

	
	if (prAisBssInfo->fgIsBeaconActivated) {

		
#if !CFG_SLT_SUPPORT
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer, SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));
#endif
	}

	aisFsmCreateIBSS(prAdapter);

}				

VOID aisFsmStateInit_IBSS_MERGE(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	ASSERT(prBssDesc);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;

	
	prBssDesc->fgIsConnecting = FALSE;
	prBssDesc->fgIsConnected = TRUE;

	
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_ADHOC_PEER, prAdapter->prAisBssInfo->ucBssIndex, prBssDesc);

	prStaRec->fgIsMerging = TRUE;

	prAisFsmInfo->prTargetStaRec = prStaRec;

	
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	
	aisFsmMergeIBSS(prAdapter, prStaRec);

}				

#endif 

VOID aisFsmStateAbort_JOIN(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_JOIN_ABORT_T prJoinAbortMsg;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	
	prJoinAbortMsg = (P_MSG_JOIN_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_ABORT_T));
	if (!prJoinAbortMsg) {

		ASSERT(0);	
		return;
	}

	prJoinAbortMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_ABORT;
	prJoinAbortMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinAbortMsg->prStaRec = prAisFsmInfo->prTargetStaRec;

	scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisFsmInfo->prTargetStaRec->aucMacAddr);

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinAbortMsg, MSG_SEND_METHOD_BUF);

	
	aisFsmReleaseCh(prAdapter);

	
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

}				

VOID aisFsmStateAbort_SCAN(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_SCN_SCAN_CANCEL prScanCancelMsg;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	DBGLOG(AIS, STATE, "aisFsmStateAbort_SCAN\n");

	
	prScanCancelMsg = (P_MSG_SCN_SCAN_CANCEL) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_CANCEL));
	if (!prScanCancelMsg) {

		ASSERT(0);	
		return;
	}

	prScanCancelMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_CANCEL;
	prScanCancelMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfScanReq;
	prScanCancelMsg->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	prScanCancelMsg->fgIsChannelExt = FALSE;

	
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanCancelMsg, MSG_SEND_METHOD_UNBUF);

}				

VOID aisFsmStateAbort_NORMAL_TR(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	

	
	aisFsmReleaseCh(prAdapter);

	
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
}				

#if CFG_SUPPORT_ADHOC
VOID aisFsmStateAbort_IBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_DESC_T prBssDesc;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	
	if (prAisFsmInfo->prTargetStaRec) {
		prBssDesc = scanSearchBssDescByTA(prAdapter, prAisFsmInfo->prTargetStaRec->aucMacAddr);

		if (prBssDesc) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;
		}
	}
	
	aisFsmReleaseCh(prAdapter);

}
#endif 

UINT_8 aisIndex2ChannelNum(IN UINT_8 ucIndex)
{
	UINT_8 ucChannel;

	
	if (ucIndex >= 1 && ucIndex <= 14)
		
		ucChannel = ucIndex;
		
	else if (ucIndex >= 15 && ucIndex <= 22)
		
		ucChannel = (ucIndex - 6) << 2;
		
	else if (ucIndex >= 23 && ucIndex <= 34)
		
		ucChannel = (ucIndex + 2) << 2;
		
	else if (ucIndex >= 35 && ucIndex <= 39) {
		
		ucIndex = ucIndex + 2;
		ucChannel = (ucIndex << 2) + 1;
		
	} else {
		
		ucChannel = 0;
	}
	return ucChannel;
}


VOID aisGetAndSetScanChannel(IN P_ADAPTER_T prAdapter)
{
	P_PARTIAL_SCAN_INFO PartialScanChannel = NULL;
	UINT_8	*ucChannelp;
	UINT_8	ucChannelNum;
	int i = 1;
	int t = 0;
	

	if (prAdapter->prGlueInfo->u4LastFullScanTime == 0) {
		
		DBGLOG(AIS, INFO, "Full2Partial u4LastFullScanTime=0\n");
		return;
	}
	if (prAdapter->prGlueInfo->puFullScan2PartialChannel != NULL) {
		DBGLOG(AIS, TRACE, "Full2Partial puFullScan2PartialChannel not null\n");
		return;
	}

	
	PartialScanChannel = (P_PARTIAL_SCAN_INFO) kalMemAlloc(sizeof(PARTIAL_SCAN_INFO), VIR_MEM_TYPE);
	if (PartialScanChannel == NULL) {
		DBGLOG(AIS, INFO, "Full2Partial alloc PartialScanChannel fail\n");
		return;
	}
	kalMemSet(PartialScanChannel, 0, sizeof(PARTIAL_SCAN_INFO));

	ucChannelp = prAdapter->prGlueInfo->ucChannelNum;
	while (i < FULL_SCAN_MAX_CHANNEL_NUM) {
		if (ucChannelp[i] != 0) {
			ucChannelNum = aisIndex2ChannelNum(i);
			DBGLOG(AIS, TRACE, "Full2Partial i=%d, channel value=%d\n", i, ucChannelNum);
			if (ucChannelNum != 0) {
				if ((ucChannelNum >= 1) && (ucChannelNum <= 14))
					PartialScanChannel->arChnlInfoList[t].eBand = BAND_2G4;
				else
					PartialScanChannel->arChnlInfoList[t].eBand = BAND_5G;

				PartialScanChannel->arChnlInfoList[t].ucChannelNum = ucChannelNum;
				t++;
			}
		}
		i++;
	}
	DBGLOG(AIS, INFO, "Full2Partial channel num=%d\n", t);
	if ((t > 0) && (t <= MAXIMUM_OPERATION_CHANNEL_LIST)) {
		PartialScanChannel->ucChannelListNum = t;
		prAdapter->prGlueInfo->puFullScan2PartialChannel = (PUINT_8)PartialScanChannel;
	} else {
		DBGLOG(AIS, INFO, "Full2Partial channel num great %d max channel number\n",
			MAXIMUM_OPERATION_CHANNEL_LIST);
		kalMemFree(PartialScanChannel, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
	}
}

VOID aisFsmSteps(IN P_ADAPTER_T prAdapter, ENUM_AIS_STATE_T eNextState)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc;
	P_MSG_CH_REQ_T prMsgChReq;
	P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg;
	P_AIS_REQ_HDR_T prAisReq;
	ENUM_BAND_T eBand;
	UINT_8 ucChannel;
	UINT_16 u2ScanIELen;

	BOOLEAN fgIsTransition = (BOOLEAN) FALSE;
	OS_SYSTIME rCurrentTime;

	DEBUGFUNC("aisFsmSteps()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	do {

		
		prAisFsmInfo->ePreviousState = prAisFsmInfo->eCurrentState;

#if DBG
		DBGLOG(AIS, STATE, "TRANSITION: [%s] -> [%s]\n",
				    apucDebugAisState[prAisFsmInfo->eCurrentState], apucDebugAisState[eNextState]);
#else
		DBGLOG(AIS, STATE, "[%d] TRANSITION: [%d] -> [%d]\n",
				    DBG_AIS_IDX, prAisFsmInfo->eCurrentState, eNextState);
#endif
		
		prAisFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (BOOLEAN) FALSE;

		aisCheckPostponedDisconnTimeout(prAdapter, prAisFsmInfo);

		GET_CURRENT_SYSTIME(&rCurrentTime);

		
		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_IDLE:

			prAisReq = aisFsmGetNextRequest(prAdapter);
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);

			if (prAisReq == NULL || prAisReq->eReqType == AIS_REQUEST_RECONNECT) {
				if (prConnSettings->fgIsConnReqIssued == TRUE &&
				    prConnSettings->fgIsDisconnectedByNonRequest == FALSE) {

					prAisFsmInfo->fgTryScan = TRUE;
					if (!IS_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex)) {
						SET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
						
						nicActivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
					}
					SET_NET_PWR_STATE_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
					prAisBssInfo->fgIsNetRequestInActive = FALSE;

					
					prAisFsmInfo->ucConnTrialCount = 0;

					eNextState = AIS_STATE_COLLECT_ESS_INFO;
					fgIsTransition = TRUE;
				} else {
					SET_NET_PWR_STATE_IDLE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

					
#if CFG_SUPPORT_PNO
					prAisBssInfo->fgIsNetRequestInActive = TRUE;
					if (prAisBssInfo->fgIsPNOEnable) {
						DBGLOG(BSS, INFO,
						"[BSSidx][Network]=%d PNOEnable&&OP_MODE_INFRASTRUCTURE,KEEP ACTIVE\n",
							prAisBssInfo->ucBssIndex);
					} else
#endif
					{
						UNSET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
						nicDeactivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
					}

					
					if (prAisReq && (aisFsmIsRequestPending
							 (prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE)) {
						wlanClearScanningResult(prAdapter);
						eNextState = AIS_STATE_SCAN;

						fgIsTransition = TRUE;
					}
				}

				if (prAisReq) {
					
					cnmMemFree(prAdapter, prAisReq);
				}
			} else if (prAisReq->eReqType == AIS_REQUEST_SCAN) {
#if CFG_SUPPORT_ROAMING
				prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif 
				wlanClearScanningResult(prAdapter);

				eNextState = AIS_STATE_SCAN;
				fgIsTransition = TRUE;

				
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType == AIS_REQUEST_ROAMING_CONNECT
				   || prAisReq->eReqType == AIS_REQUEST_ROAMING_SEARCH) {
				
				
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType == AIS_REQUEST_REMAIN_ON_CHANNEL) {
				eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
				fgIsTransition = TRUE;

				
				cnmMemFree(prAdapter, prAisReq);
			}

			prAisFsmInfo->u4SleepInterval = AIS_BG_SCAN_INTERVAL_MIN_SEC;

			break;

		case AIS_STATE_SEARCH:
			
#if CFG_SLT_SUPPORT
			prBssDesc = prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
			if (prAisFsmInfo->ucJoinFailCntAfterScan >= 5) {
				prBssDesc = NULL;
				DBGLOG(AIS, STATE,
					"Failed to connect %s more than 5 times after last scan, scan again\n",
					prConnSettings->aucSSID);
			} else {
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
				prBssDesc = scanSearchBssDescByScoreForAis(prAdapter);
#else
				prBssDesc = scanSearchBssDescByPolicy(prAdapter, prAisBssInfo->ucBssIndex);
#endif
			}
#endif

			
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				if (prAisFsmInfo->ucConnTrialCount > AIS_ROAMING_CONNECTION_TRIAL_LIMIT) {
#if CFG_SUPPORT_ROAMING
					roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_CONNLIMIT);
#endif 
					
					prAisFsmInfo->ucConnTrialCount = 0;

					
					if (prConnSettings->eReConnectLevel < RECONNECT_LEVEL_INTER_DRIVER) {
						prConnSettings->eReConnectLevel = RECONNECT_LEVEL_INTER_OTHER;
						prConnSettings->fgIsConnReqIssued = FALSE;
					} else {
						DBGLOG(AIS, INFO,
							   "do not set fgIsConnReqIssued, Level is %d\n",
							   prConnSettings->eReConnectLevel);
					}

					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;

					break;
				}
			}
			
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

				
				if (prBssDesc) {

					
					
					prAisBssInfo->u4RsnSelectedGroupCipher = prBssDesc->u4RsnSelectedGroupCipher;
					prAisBssInfo->u4RsnSelectedPairwiseCipher =
					    prBssDesc->u4RsnSelectedPairwiseCipher;
					prAisBssInfo->u4RsnSelectedAKMSuite = prBssDesc->u4RsnSelectedAKMSuite;

					
					if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;

						
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						
						eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
						fgIsTransition = TRUE;

						
						prAisFsmInfo->ucConnTrialCount++;
					}
#if CFG_SUPPORT_ADHOC
					else if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

						
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						eNextState = AIS_STATE_IBSS_MERGE;
						fgIsTransition = TRUE;
					}
#endif 
					else {
						ASSERT(0);
						eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
						fgIsTransition = TRUE;
					}
				}
				
				else {
					
					if (prConnSettings->eOPMode == NET_TYPE_INFRA)
						prAisFsmInfo->ucConnTrialCount++;
#if CFG_SUPPORT_RN
					if (prAisBssInfo->fgDisConnReassoc == TRUE) {
						eNextState = AIS_STATE_JOIN_FAILURE;
						fgIsTransition = TRUE;
						break;
					}
#endif
					if (prAisFsmInfo->rJoinReqTime != 0 &&
						CHECK_FOR_TIMEOUT(kalGetTimeTick(),
								  prAisFsmInfo->rJoinReqTime,
								  SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
						eNextState = AIS_STATE_JOIN_FAILURE;
						fgIsTransition = TRUE;
						break;
					}

					
					if (prAisFsmInfo->fgTryScan) {
						eNextState = AIS_STATE_LOOKING_FOR;

						fgIsTransition = TRUE;
					}
					
					else {
						eNextState = aisFsmStateSearchAction(prAdapter,
									AIS_FSM_STATE_SEARCH_ACTION_PHASE_0);
						fgIsTransition = TRUE;
					}
				}
			}
			
			else {	

				
				if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION &&
					((!prBssDesc) ||	
					(prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE) ||	
					(prBssDesc->fgIsConnected) ||	
					(EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID))) ) {
#if DBG
					if ((prBssDesc) && (prBssDesc->fgIsConnected))
						ASSERT(EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID));
#endif 
					
					
#if CFG_SUPPORT_ROAMING
					roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_NOCANDIDATE);
#endif 

					
					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;
				}
				
				else {
					if (prBssDesc == NULL) {
						fgIsTransition = TRUE;
						eNextState = aisFsmStateSearchAction(prAdapter,
								     AIS_FSM_STATE_SEARCH_ACTION_PHASE_1);
					} else {
						aisFsmStateSearchAction(prAdapter, AIS_FSM_STATE_SEARCH_ACTION_PHASE_2);
						
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						
						prAisFsmInfo->ucConnTrialCount++;

						
						eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
						fgIsTransition = TRUE;
					}
				}
			}

			break;

		case AIS_STATE_WAIT_FOR_NEXT_SCAN:

			DBGLOG(AIS, LOUD, "SCAN: Idle Begin - Current Time = %u\n", kalGetTimeTick());

			cnmTimerStartTimer(prAdapter,
					   &prAisFsmInfo->rBGScanTimer, SEC_TO_MSEC(prAisFsmInfo->u4SleepInterval));

			SET_NET_PWR_STATE_IDLE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

			if (prAisFsmInfo->u4SleepInterval < AIS_BG_SCAN_INTERVAL_MAX_SEC)
				prAisFsmInfo->u4SleepInterval <<= 1;

			break;

		case AIS_STATE_SCAN:
		case AIS_STATE_ONLINE_SCAN:
		case AIS_STATE_LOOKING_FOR:

			if (!IS_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

				
				nicActivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
				prAisBssInfo->fgIsNetRequestInActive = FALSE;
			}

			
			if (prAisFsmInfo->u4ScanIELength > 0) {
				u2ScanIELen = (UINT_16) prAisFsmInfo->u4ScanIELength;
			} else {
#if CFG_SUPPORT_WPS2
				u2ScanIELen = prAdapter->prGlueInfo->u2WSCIELen;
#else
				u2ScanIELen = 0;
#endif
			}

			prScanReqMsg = (P_MSG_SCN_SCAN_REQ_V2) cnmMemAlloc(prAdapter,
									   RAM_TYPE_MSG,
									   OFFSET_OF
									   (MSG_SCN_SCAN_REQ_V2, aucIE) + u2ScanIELen);
			if (!prScanReqMsg) {
				ASSERT(0);	
				return;
			}

			prScanReqMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_REQ_V2;
			prScanReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfScanReq;
			prScanReqMsg->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;

#if CFG_SUPPORT_RDD_TEST_MODE
			prScanReqMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;
#else
			if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN
			    || prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
				if (prAisFsmInfo->ucScanSSIDNum == 0) {
#if CFG_SUPPORT_AIS_PASSIVE_SCAN
					prScanReqMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;

					prScanReqMsg->ucSSIDType = 0;
					prScanReqMsg->ucSSIDNum = 0;
#else
					prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
#endif
				} else if (prAisFsmInfo->ucScanSSIDNum == 1
					   && prAisFsmInfo->arScanSSID[0].u4SsidLen == 0) {
					prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
				} else {
					prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
					prScanReqMsg->ucSSIDNum = prAisFsmInfo->ucScanSSIDNum;
					prScanReqMsg->prSsid = prAisFsmInfo->arScanSSID;
				}
			} else {
				prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

				COPY_SSID(prAisFsmInfo->rRoamingSSID.aucSsid,
					  prAisFsmInfo->rRoamingSSID.u4SsidLen,
					  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

				
				prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
				prScanReqMsg->ucSSIDNum = 1;
				prScanReqMsg->prSsid = &(prAisFsmInfo->rRoamingSSID);
			}
#endif

			
			prScanReqMsg->u2ProbeDelay = 0;
			prScanReqMsg->u2ChannelDwellTime = 0;
			prScanReqMsg->u2TimeoutValue = 0;

			
			if (cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel) == TRUE) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
				prScanReqMsg->ucChannelListNum = 1;
				prScanReqMsg->arChnlInfoList[0].eBand = eBand;
				prScanReqMsg->arChnlInfoList[0].ucChannelNum = ucChannel;
			} else if ((prAisFsmInfo->eCurrentState != AIS_STATE_LOOKING_FOR) &&
				(prAdapter->prGlueInfo != NULL) &&
				(prAdapter->prGlueInfo->pucScanChannel != NULL)) {
				
				P_PARTIAL_SCAN_INFO channel_t;
				UINT_32 u4size;

				channel_t = (P_PARTIAL_SCAN_INFO)prAdapter->prGlueInfo->pucScanChannel;

				
				prScanReqMsg->ucChannelListNum = channel_t->ucChannelListNum;
				u4size = sizeof(channel_t->arChnlInfoList);

				DBGLOG(AIS, TRACE, "SCAN: channel num = %d, total size=%d\n",
						prScanReqMsg->ucChannelListNum, u4size);

				kalMemCopy(prScanReqMsg->arChnlInfoList, channel_t->arChnlInfoList,
						u4size);

				
				kalMemSet(channel_t, 0, sizeof(PARTIAL_SCAN_INFO));
				prAdapter->prGlueInfo->pucScanChannel = NULL;
				DBGLOG(AIS, TRACE, "partial scan clear and reset puScanChannel\n");

				
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;

			} else if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR &&
					prAisFsmInfo->aucNeighborAPChnl[0] != 0) {
				PUINT_8 pucChnl = &prAisFsmInfo->aucNeighborAPChnl[0];
				P_RF_CHANNEL_INFO_T prChnlInfo = &prScanReqMsg->arChnlInfoList[0];
				UINT_8 ucChnlNum = 0;

				while (pucChnl[ucChnlNum] > 0 && ucChnlNum < CFG_NEIGHBOR_AP_CHANNEL_NUM) {
					prChnlInfo[ucChnlNum].ucChannelNum = pucChnl[ucChnlNum];
					prChnlInfo[ucChnlNum].eBand = pucChnl[ucChnlNum] > 14 ? BAND_5G:BAND_2G4;
					ucChnlNum++;
				}
				prScanReqMsg->ucChannelListNum = ucChnlNum;
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
			} else {
#if 0
				aisFsmSetChannelInfo(prAdapter, prScanReqMsg, prAisFsmInfo->eCurrentState);
#endif
				if (prAdapter->aePreferBand[prAdapter->prAisBssInfo->ucBssIndex] == BAND_NULL) {
					if (prAdapter->fgEnable5GBand == TRUE)
						prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
					else
						prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
				} else if (prAdapter->aePreferBand[prAdapter->prAisBssInfo->ucBssIndex]
						== BAND_2G4) {
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
				} else if (prAdapter->aePreferBand[prAdapter->prAisBssInfo->ucBssIndex]
						== BAND_5G) {
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
				} else {
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
					ASSERT(0);
				}

			}

			
			if ((prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) &&
				(prScanReqMsg->eScanChannel == SCAN_CHANNEL_FULL) &&
				(prScanReqMsg->ucSSIDType == SCAN_REQ_SSID_WILDCARD) &&
				(prScanReqMsg->ucSSIDNum == 0)) {
				
				OS_SYSTIME rCurrentTime;
				P_PARTIAL_SCAN_INFO channel_t;
				P_GLUE_INFO_T pGlinfo;
				UINT_32 u4size;

				DBGLOG(AIS, INFO, "Full2Partial eScanChannel = %d, ucSSIDNum=%d\n",
					prScanReqMsg->eScanChannel, prScanReqMsg->ucSSIDNum);
				pGlinfo = prAdapter->prGlueInfo;
				GET_CURRENT_SYSTIME(&rCurrentTime);
				DBGLOG(AIS, TRACE, "Full2Partial LastFullST= %lld,CurrentT=%lld\n",
					pGlinfo->u4LastFullScanTime, rCurrentTime);
				if ((pGlinfo->u4LastFullScanTime == 0) ||
					(CHECK_FOR_TIMEOUT(rCurrentTime, pGlinfo->u4LastFullScanTime,
						SEC_TO_SYSTIME(UPDATE_FULL_TO_PARTIAL_SCAN_TIMEOUT)))) {
					
					
					DBGLOG(AIS, INFO, "Full2Partial not update full scan\n");
					pGlinfo->u4LastFullScanTime = rCurrentTime;
					pGlinfo->ucTrScanType = 1;
					kalMemSet(pGlinfo->ucChannelNum, 0, FULL_SCAN_MAX_CHANNEL_NUM);
					if (pGlinfo->puFullScan2PartialChannel != NULL) {
						kalMemFree(pGlinfo->puFullScan2PartialChannel,
							VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
						pGlinfo->puFullScan2PartialChannel = NULL;
					}
				} else {
					DBGLOG(AIS, INFO, "Full2Partial update full scan to partial scan\n");

					
					aisGetAndSetScanChannel(prAdapter);

					if (pGlinfo->puFullScan2PartialChannel != NULL) {
						PUINT_8 pChanneltmp;
						
						pChanneltmp = pGlinfo->puFullScan2PartialChannel;
						channel_t = (P_PARTIAL_SCAN_INFO)pChanneltmp;

						
						prScanReqMsg->ucChannelListNum = channel_t->ucChannelListNum;
						u4size = sizeof(channel_t->arChnlInfoList);

						DBGLOG(AIS, TRACE, "Full2Partial ChList=%d,u4size=%d\n",
							channel_t->ucChannelListNum, u4size);

						kalMemCopy(&(prScanReqMsg->arChnlInfoList),
							&(channel_t->arChnlInfoList), u4size);
						
						prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
					}
				}
			}

			if (prAisFsmInfo->u4ScanIELength > 0) {
				kalMemCopy(prScanReqMsg->aucIE, prAisFsmInfo->aucScanIEBuf,
					   prAisFsmInfo->u4ScanIELength);
			} else {
#if CFG_SUPPORT_WPS2
				if (prAdapter->prGlueInfo->u2WSCIELen > 0) {
					kalMemCopy(prScanReqMsg->aucIE,
						   &prAdapter->prGlueInfo->aucWSCIE, prAdapter->prGlueInfo->u2WSCIELen);
				}
			}
#endif

			prScanReqMsg->u2IELen = u2ScanIELen;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanReqMsg, MSG_SEND_METHOD_BUF);
			prAisFsmInfo->fgTryScan = FALSE;	
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;
			break;

		case AIS_STATE_REQ_CHANNEL_JOIN:
			
			prMsgChReq = (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_REQ_T));
			if (!prMsgChReq) {
				ASSERT(0);	
				return;
			}

			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;
			prMsgChReq->ucPrimaryChannel = prAisFsmInfo->prTargetBssDesc->ucChannelNum;
			prMsgChReq->eRfSco = prAisFsmInfo->prTargetBssDesc->eSco;
			prMsgChReq->eRfBand = prAisFsmInfo->prTargetBssDesc->eBand;

			
			prMsgChReq->eRfChannelWidth = prAisFsmInfo->prTargetBssDesc->eChannelWidth;
			prMsgChReq->ucRfCenterFreqSeg1 = prAisFsmInfo->prTargetBssDesc->ucCenterFreqS1;
			prMsgChReq->ucRfCenterFreqSeg2 = prAisFsmInfo->prTargetBssDesc->ucCenterFreqS2;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;
			break;

		case AIS_STATE_JOIN:
			aisFsmStateInit_JOIN(prAdapter, prAisFsmInfo->prTargetBssDesc);
			break;

		case AIS_STATE_JOIN_FAILURE:
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
			prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			prConnSettings->ucSSIDLen = 0;
#endif
#if CFG_SUPPORT_RN
			if (prAisBssInfo->fgDisConnReassoc == TRUE) {
				nicMediaJoinFailure(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
							WLAN_STATUS_MEDIA_DISCONNECT);
				prAisBssInfo->fgDisConnReassoc = FALSE;
			} else
#endif
			{
				nicMediaJoinFailure(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
					WLAN_STATUS_JOIN_TIMEOUT);
			}
			eNextState = AIS_STATE_IDLE;
			fgIsTransition = TRUE;

			break;

#if CFG_SUPPORT_ADHOC
		case AIS_STATE_IBSS_ALONE:
			aisFsmStateInit_IBSS_ALONE(prAdapter);
			break;

		case AIS_STATE_IBSS_MERGE:
			aisFsmStateInit_IBSS_MERGE(prAdapter, prAisFsmInfo->prTargetBssDesc);
			break;
#endif 

		case AIS_STATE_NORMAL_TR:
			if (prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				
			} else {
				
				if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE) {
					wlanClearScanningResult(prAdapter);
					eNextState = AIS_STATE_ONLINE_SCAN;
					fgIsTransition = TRUE;
				}
				
				else if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE) == TRUE) {
					eNextState = AIS_STATE_LOOKING_FOR;
					fgIsTransition = TRUE;
				}
				
				else if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE) == TRUE) {
					eNextState = AIS_STATE_COLLECT_ESS_INFO;
					fgIsTransition = TRUE;
				} else
				    if (aisFsmIsRequestPending
					(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE) == TRUE) {
					eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
					fgIsTransition = TRUE;
				}
			}

			break;

		case AIS_STATE_DISCONNECTING:
			
			authSendDeauthFrame(prAdapter,
					    prAisBssInfo,
					    prAisBssInfo->prStaRecOfAP,
					    (P_SW_RFB_T) NULL, REASON_CODE_DEAUTH_LEAVING_BSS, aisDeauthXmitComplete);
			cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer, 100);
			break;

		case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
			
			prMsgChReq = (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_REQ_T));
			if (!prMsgChReq) {
				ASSERT(0);	
				return;
			}

			
			aisFsmReleaseCh(prAdapter);

			
			kalMemZero(prMsgChReq, sizeof(MSG_CH_REQ_T));

			
			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval = prAisFsmInfo->rChReqInfo.u4DurationMs;
			prMsgChReq->ucPrimaryChannel = prAisFsmInfo->rChReqInfo.ucChannelNum;
			prMsgChReq->eRfSco = prAisFsmInfo->rChReqInfo.eSco;
			prMsgChReq->eRfBand = prAisFsmInfo->rChReqInfo.eBand;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;

			break;

		case AIS_STATE_REMAIN_ON_CHANNEL:
			SET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

			
			nicActivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
			prAisBssInfo->fgIsNetRequestInActive = FALSE;
			break;
		case AIS_STATE_COLLECT_ESS_INFO:
		{
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM && 0 
			UINT_8 i = 0;
			P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo;
			struct MSG_REQ_CH_UTIL *prMsgReqChUtil = NULL;

			
			if (prConnSettings->eConnectionPolicy == CONNECT_BY_BSSID) {
				eNextState = AIS_STATE_SEARCH;
				fgIsTransition = TRUE;
				break;
			}
			prMsgReqChUtil = (struct MSG_REQ_CH_UTIL *)
				cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(struct MSG_REQ_CH_UTIL));
			if (!prMsgReqChUtil) {
				DBGLOG(AIS, ERROR, "No memory!");
				return;
			}
			kalMemZero(prMsgReqChUtil, sizeof(*prMsgReqChUtil));
			prMsgReqChUtil->rMsgHdr.eMsgId = MID_MNY_CNM_REQ_CH_UTIL;
			prMsgReqChUtil->u2ReturnMID = MID_CNM_AIS_RSP_CH_UTIL;
			prMsgReqChUtil->u2Duration = 100; 
			prMsgReqChUtil->ucChnlNum = prAisSpecBssInfo->ucCurEssChnlInfoNum;
			for (; i < prMsgReqChUtil->ucChnlNum && i < sizeof(prMsgReqChUtil->aucChnlList); i++)
				prMsgReqChUtil->aucChnlList[i] = prAisSpecBssInfo->arCurEssChnlInfo[i].ucChannel;

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgReqChUtil, MSG_SEND_METHOD_BUF);
#else
			eNextState = AIS_STATE_SEARCH;
			fgIsTransition = TRUE;
#endif
			break;
		}
		default:
			ASSERT(0);	
			break;

		}
	} while (fgIsTransition);

	return;

}				

enum _ENUM_AIS_STATE_T aisFsmStateSearchAction(IN struct _ADAPTER_T *prAdapter, UINT_8 ucPhase)
{
	struct _CONNECTION_SETTINGS_T *prConnSettings;
	struct _BSS_INFO_T *prAisBssInfo;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
#if !CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	struct _BSS_DESC_T *prBssDesc;
#endif
	enum _ENUM_AIS_STATE_T eState = AIS_STATE_IDLE;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

#if CFG_SLT_SUPPORT
	prBssDesc = prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
#if !CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	prBssDesc = scanSearchBssDescByPolicy(prAdapter, prAisBssInfo->ucBssIndex);
#endif
#endif

	if (ucPhase == AIS_FSM_STATE_SEARCH_ACTION_PHASE_0) {
		if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

			
			
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);
			eState = AIS_STATE_IDLE;
		}
#if CFG_SUPPORT_ADHOC
		else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
			 || (prConnSettings->eOPMode == NET_TYPE_AUTO_SWITCH)
			 || (prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS)) {
			prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
			prAisFsmInfo->prTargetBssDesc = NULL;
			eState = AIS_STATE_IBSS_ALONE;
		}
#endif 
		else {
			ASSERT(0);
			eState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		}
	} else if (ucPhase == AIS_FSM_STATE_SEARCH_ACTION_PHASE_1) {
		
		if (prConnSettings->eOPMode == NET_TYPE_INFRA)
			prAisFsmInfo->ucConnTrialCount++;
		
		if (prAisFsmInfo->fgTryScan)
			eState = AIS_STATE_LOOKING_FOR;

		
		else {
			if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

				
				
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

				eState = AIS_STATE_IDLE;
			}
#if CFG_SUPPORT_ADHOC
			else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
				 || (prConnSettings->eOPMode == NET_TYPE_AUTO_SWITCH)
				 || (prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS)) {

				prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
				prAisFsmInfo->prTargetBssDesc = NULL;

				eState = AIS_STATE_IBSS_ALONE;
			}
#endif 
			else {
				ASSERT(0);
				eState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
			}
		}
	} else {
#if DBG
		if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION)
			ASSERT(UNEQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID));
#endif 
	}
	return eState;
}
#if 0
VOID aisFsmSetChannelInfo(IN P_ADAPTER_T prAdapter, IN P_MSG_SCN_SCAN_REQ_V2 ScanReqMsg,
			IN ENUM_AIS_STATE_T CurrentState)

{
	
	struct cfg80211_scan_request *scan_req_t = NULL;
	struct ieee80211_channel *channel_tmp = NULL;
	int i = 0;
	int j = 0;
	UINT_8 channel_num = 0;
	UINT_8 channel_counts = 0;
	UINT_8 BssIndex_t = 0;

	if ((prAdapter == NULL) || (ScanReqMsg == NULL))
		return;
	if ((CurrentState == AIS_STATE_SCAN) || (CurrentState == AIS_STATE_ONLINE_SCAN)) {
		if (prAdapter->prGlueInfo->prScanRequest != NULL) {
			scan_req_t = prAdapter->prGlueInfo->prScanRequest;
			if ((scan_req_t != NULL) && (scan_req_t->n_channels != 0) &&
				(scan_req_t->channels != NULL)) {
				channel_counts = scan_req_t->n_channels;
				DBGLOG(AIS, TRACE, "channel_counts=%d\n", channel_counts);

				while (j < channel_counts) {
					channel_tmp = scan_req_t->channels[j];
					if (channel_tmp == NULL)
						break;

					DBGLOG(AIS, TRACE, "set channel band=%d\n", channel_tmp->band);
					if (channel_tmp->band >= IEEE80211_BAND_60GHZ) {
						j++;
						continue;
					}
					if (i >= MAXIMUM_OPERATION_CHANNEL_LIST)
						break;
					if (channel_tmp->band == IEEE80211_BAND_2GHZ)
						ScanReqMsg->arChnlInfoList[i].eBand = BAND_2G4;
					else if (channel_tmp->band == IEEE80211_BAND_5GHZ)
						ScanReqMsg->arChnlInfoList[i].eBand = BAND_5G;

					DBGLOG(AIS, TRACE, "set channel channel_rer =%d\n",
						channel_tmp->center_freq);

					channel_num = (UINT_8)nicFreq2ChannelNum(
						channel_tmp->center_freq * 1000);

					DBGLOG(AIS, TRACE, "set channel channel_num=%d\n",
						channel_num);
					ScanReqMsg->arChnlInfoList[i].ucChannelNum = channel_num;

					j++;
					i++;
				}
			}

		}
	}

	DBGLOG(AIS, INFO, "set channel i=%d\n", i);
	if (i > 0) {
		ScanReqMsg->ucChannelListNum = i;
		ScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;

		return;
	}

	BssIndex_t = prAdapter->prAisBssInfo->ucBssIndex;
	if (prAdapter->aePreferBand[BssIndex_t] == BAND_NULL) {
		if (prAdapter->fgEnable5GBand == TRUE)
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
		else
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
		} else if (prAdapter->aePreferBand[BssIndex_t]
				== BAND_2G4) {
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
		} else if (prAdapter->aePreferBand[BssIndex_t]
				== BAND_5G) {
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
		} else {
			ScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
			ASSERT(0);
		}

}
#endif
VOID aisFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	UINT_8 ucSeqNumOfCompMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventScanDone()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	DBGLOG(AIS, LOUD, "EVENT-SCAN DONE: Current Time = %u\n", kalGetTimeTick());

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;
	ASSERT(prScanDoneMsg->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex);

	ucSeqNumOfCompMsg = prScanDoneMsg->ucSeqNum;
	cnmMemFree(prAdapter, prMsgHdr);

	eNextState = prAisFsmInfo->eCurrentState;

	if (ucSeqNumOfCompMsg != prAisFsmInfo->ucSeqNumOfScanReq) {
		DBGLOG(AIS, WARN, "SEQ NO of AIS SCN DONE MSG is not matched.\n");
	} else {
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_SCAN:
			prConnSettings->fgIsScanReqIssued = FALSE;

			
			prAisFsmInfo->u4ScanIELength = 0;

			kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
			eNextState = AIS_STATE_IDLE;
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
			break;

		case AIS_STATE_ONLINE_SCAN:
			prConnSettings->fgIsScanReqIssued = FALSE;

			
			prAisFsmInfo->u4ScanIELength = 0;

			kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_NORMAL_TR;
#endif 
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			scanGetCurrentEssChnlList(prAdapter);
#endif
			break;

		case AIS_STATE_LOOKING_FOR:
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_COLLECT_ESS_INFO;
#endif 
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			scanGetCurrentEssChnlList(prAdapter);
#endif
			break;

		default:
			DBGLOG(AIS, WARN, "Wrong AIS state %d\n", prAisFsmInfo->eCurrentState);
			break;

		}
	}
	aisRemoveOldestBcnTimeout(prAisFsmInfo);
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				

VOID aisFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	UINT_8 ucReasonOfDisconnect;
	BOOLEAN fgDelayIndication;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventAbort()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) prMsgHdr;
	ucReasonOfDisconnect = prAisAbortMsg->ucReasonOfDisconnect;
	fgDelayIndication = prAisAbortMsg->fgDelayIndication;

	cnmMemFree(prAdapter, prMsgHdr);

#if DBG
	DBGLOG(AIS, LOUD, "EVENT-ABORT: Current State %s\n", apucDebugAisState[prAisFsmInfo->eCurrentState]);
#else
	DBGLOG(AIS, LOUD, "[%d] EVENT-ABORT: Current State [%d]\n", DBG_AIS_IDX, prAisFsmInfo->eCurrentState);
#endif

	
	GET_CURRENT_SYSTIME(&(prAisFsmInfo->rJoinReqTime));

	
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED ||
		ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED) {
		P_STA_RECORD_T prSta = prAisFsmInfo->prTargetStaRec;
		P_BSS_DESC_T prBss = prAisFsmInfo->prTargetBssDesc;

	    if (prSta && prBss && prSta->u2ReasonCode == REASON_CODE_DISASSOC_AP_OVERLOAD) {
			struct AIS_BLACKLIST_ITEM *prBlackList = aisAddBlacklist(prAdapter, prBss);

			if (prBlackList)
				prBlackList->u2DeauthReason = prSta->u2ReasonCode;
	    }
		prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
		if (prAisFsmInfo->prTargetBssDesc)
			prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = TRUE;
	} else {
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
	}

	
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_ROAMING &&
	    prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR &&
		    prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
			aisFsmSteps(prAdapter, AIS_STATE_COLLECT_ESS_INFO);
		} else {
			aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
			aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_ROAMING_CONNECT);
		}
		return;
	}
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	scanGetCurrentEssChnlList(prAdapter);
#endif
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
	aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

	if (prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
		
		aisFsmStateAbort(prAdapter, ucReasonOfDisconnect, fgDelayIndication);
	}

}				

VOID aisFsmStateAbort(IN P_ADAPTER_T prAdapter, UINT_8 ucReasonOfDisconnect, BOOLEAN fgDelayIndication)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	BOOLEAN fgIsCheckConnected;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	fgIsCheckConnected = FALSE;

	
	prAisBssInfo->ucReasonOfDisconnect = ucReasonOfDisconnect;

	
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IDLE:
	case AIS_STATE_SEARCH:
	case AIS_STATE_JOIN_FAILURE:
		break;
	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);

		
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_SCAN:
		
		aisFsmStateAbort_SCAN(prAdapter);

		
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == FALSE)
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);

		break;

	case AIS_STATE_LOOKING_FOR:
		
		aisFsmStateAbort_SCAN(prAdapter);

		
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_REQ_CHANNEL_JOIN:
		
		aisFsmReleaseCh(prAdapter);

		
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_JOIN:
		
		aisFsmStateAbort_JOIN(prAdapter);

		
		fgIsCheckConnected = TRUE;
		break;

#if CFG_SUPPORT_ADHOC
	case AIS_STATE_IBSS_ALONE:
	case AIS_STATE_IBSS_MERGE:
		aisFsmStateAbort_IBSS(prAdapter);
		break;
#endif 

	case AIS_STATE_ONLINE_SCAN:
		
		aisFsmStateAbort_SCAN(prAdapter);

		
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == FALSE)
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);

		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_NORMAL_TR:
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_DISCONNECTING:
		
		aisFsmStateAbort_NORMAL_TR(prAdapter);

		break;

	case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
		
		aisFsmReleaseCh(prAdapter);
		break;

	case AIS_STATE_REMAIN_ON_CHANNEL:
		
		aisFsmReleaseCh(prAdapter);

		
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		break;

	default:
		break;
	}

	if (fgIsCheckConnected && (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState)) {

		
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
		    prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_NEW_CONNECTION &&
		    prAisBssInfo->prStaRecOfAP && prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			aisFsmSteps(prAdapter, AIS_STATE_DISCONNECTING);

			return;
		}

			
			aisFsmStateAbort_NORMAL_TR(prAdapter);
		}
#if CFG_SUPPORT_802_11K
	if (!fgDelayIndication)
		kalMemZero(prAisFsmInfo->aucNeighborAPChnl, CFG_NEIGHBOR_AP_CHANNEL_NUM);
#endif

	aisFsmDisconnect(prAdapter, fgDelayIndication);

}				

VOID aisFsmRunEventJoinComplete(IN struct _ADAPTER_T *prAdapter, IN struct _MSG_HDR_T *prMsgHdr)
{
	struct _MSG_SAA_FSM_COMP_T *prJoinCompMsg;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
	enum _ENUM_AIS_STATE_T eNextState;
	struct _SW_RFB_T *prAssocRspSwRfb;

	DEBUGFUNC("aisFsmRunEventJoinComplete()");
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (struct _MSG_SAA_FSM_COMP_T *)prMsgHdr;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

	eNextState = prAisFsmInfo->eCurrentState;

	
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN) {
		
		if (prJoinCompMsg->ucSeqNum == prAisFsmInfo->ucSeqNumOfReqMsg)
			eNextState = aisFsmJoinCompleteAction(prAdapter, prMsgHdr);
		
		aisRemoveDisappearedBlacklist(prAdapter);
#if DBG
		else
			DBGLOG(AIS, WARN, "SEQ NO of AIS JOIN COMP MSG is not matched.\n");
#endif 
	}
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);

}				

enum _ENUM_AIS_STATE_T aisFsmJoinCompleteAction(IN struct _ADAPTER_T *prAdapter, IN struct _MSG_HDR_T *prMsgHdr)
{
	struct _MSG_SAA_FSM_COMP_T *prJoinCompMsg;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
	enum _ENUM_AIS_STATE_T eNextState;
	struct _STA_RECORD_T *prStaRec;
	struct _SW_RFB_T *prAssocRspSwRfb;
	struct _BSS_INFO_T *prAisBssInfo;
	OS_SYSTIME rCurrentTime;
	UINT_8 aucP2pSsid[] = CTIA_MAGIC_SSID;

	DEBUGFUNC("aisFsmJoinCompleteAction()");

	ASSERT(prMsgHdr);

	GET_CURRENT_SYSTIME(&rCurrentTime);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (struct _MSG_SAA_FSM_COMP_T *)prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
	prAisBssInfo = prAdapter->prAisBssInfo;
	eNextState = prAisFsmInfo->eCurrentState;

#if CFG_SUPPORT_RN
	GET_CURRENT_SYSTIME(&prAisBssInfo->rConnTime);
#endif

	do {
		
		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {

#if CFG_SUPPORT_RN
			prAisBssInfo->fgDisConnReassoc = FALSE;
#endif
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
			prAdapter->rWifiVar.rConnSettings.fgSecModeChangeStartTimer = FALSE;
#endif
			
			prAisFsmInfo->ucConnTrialCount = 0;
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;

			
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {

#if CFG_SUPPORT_ROAMING
				
				aisFsmRoamingDisconnectPrevAP(prAdapter, prStaRec);

				
				aisUpdateBssInfoForRoamingAP(prAdapter, prStaRec, prAssocRspSwRfb);
#endif 
			} else {
				
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				
				if ((prAisBssInfo->prStaRecOfAP) &&
				    (prAisBssInfo->prStaRecOfAP != prStaRec) &&
				    (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {

					cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1);
					cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP);
				}

				
				
				aisUpdateBssInfoForJOIN(prAdapter, prStaRec, prAssocRspSwRfb);

				
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

				
				nicUpdateRSSI(prAdapter,
					      prAdapter->prAisBssInfo->ucBssIndex,
					      (INT_8) (RCPI_TO_dBm(prStaRec->ucRCPI)), 0);

				
				
				
				aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

				if (EQUAL_SSID
				    (aucP2pSsid, CTIA_MAGIC_SSID_LEN, prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen)) {
					nicEnterCtiaMode(prAdapter, TRUE, FALSE);
				}
			}

#if CFG_SUPPORT_ROAMING
			
			if (prAdapter->rWifiVar.rConnSettings.eConnectionPolicy != CONNECT_BY_BSSID)
				roamingFsmRunEventStart(prAdapter);
#endif 
			if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, FALSE) == FALSE)
					prAisFsmInfo->rJoinReqTime = 0;
#if CFG_SUPPORT_802_11K
			kalMemZero(prAisFsmInfo->aucNeighborAPChnl, CFG_NEIGHBOR_AP_CHANNEL_NUM);
			aisSendNeighborRequest(prAdapter);
#endif
			prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = FALSE;
			prAisFsmInfo->ucJoinFailCntAfterScan = 0;
			
			eNextState = AIS_STATE_NORMAL_TR;
		}
		
		else {
			
			if (aisFsmStateInit_RetryJOIN(prAdapter, prStaRec) == FALSE) {
				struct _BSS_DESC_T *prBssDesc;

				
				prStaRec->ucJoinFailureCount++;

				
				aisFsmReleaseCh(prAdapter);

				
				cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

				
				prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
				prAisFsmInfo->ucJoinFailCntAfterScan++;

				prBssDesc = scanSearchBssDescByBssid(prAdapter, prStaRec->aucMacAddr);

				if (prBssDesc == NULL)
					break;

				
				
				prBssDesc->ucJoinFailureCount++;
				if (prBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD) {
					aisAddBlacklist(prAdapter, prBssDesc);
					GET_CURRENT_SYSTIME(&prBssDesc->rJoinFailTime);
					DBGLOG(AIS, TRACE,
					       "Bss " MACSTR " join fail %d times, temp disable it at time: %u\n",
						MAC2STR(prBssDesc->aucBSSID),
						prBssDesc->ucJoinFailureCount,
						prBssDesc->rJoinFailTime);
				}

				if (prBssDesc->prBlack)
					prBssDesc->prBlack->u2AuthStatus = prStaRec->u2StatusCode;

				if (prBssDesc)
					prBssDesc->fgIsConnecting = FALSE;

				
				if (prStaRec != prAisBssInfo->prStaRecOfAP)
					cnmStaRecFree(prAdapter, prStaRec);

				if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
#if CFG_SUPPORT_ROAMING
					eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
#endif 
#if CFG_SUPPORT_RN
				} else if (prAisBssInfo->fgDisConnReassoc == TRUE) {
					eNextState = AIS_STATE_JOIN_FAILURE;
#endif
				} else
				    if (prAisFsmInfo->rJoinReqTime != 0 &&
						CHECK_FOR_TIMEOUT(rCurrentTime,
								  prAisFsmInfo->rJoinReqTime,
								  SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
					
					eNextState = AIS_STATE_JOIN_FAILURE;
				} else {
					
					aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

					eNextState = AIS_STATE_IDLE;
				}
			}
		}
	} while (0);
	return eNextState;
}

#if CFG_SUPPORT_ADHOC
VOID aisFsmCreateIBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	do {
		
		if (prAisFsmInfo->eCurrentState == AIS_STATE_IBSS_ALONE)
			aisUpdateBssInfoForCreateIBSS(prAdapter);

	} while (FALSE);

}				

VOID aisFsmMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_BSS_INFO_T prAisBssInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	do {

		eNextState = prAisFsmInfo->eCurrentState;

		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_IBSS_MERGE:
			{
				P_BSS_DESC_T prBssDesc;

				
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				
				bssInitializeClientList(prAdapter, prAisBssInfo);

				
				prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = FALSE;
				}
				
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

				
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				
				aisUpdateBssInfoForMergeIBSS(prAdapter, prStaRec);

				

				
				aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

				
				eNextState = AIS_STATE_NORMAL_TR;

				
				aisFsmReleaseCh(prAdapter);

#if CFG_SLT_SUPPORT
				prAdapter->rWifiVar.rSltInfo.prPseudoStaRec = prStaRec;
#endif
			}
			break;

		default:
			break;
		}

		if (eNextState != prAisFsmInfo->eCurrentState)
			aisFsmSteps(prAdapter, eNextState);

	} while (FALSE);

}				

VOID aisFsmRunEventFoundIBSSPeer(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_AIS_IBSS_PEER_FOUND_T prAisIbssPeerFoundMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc;
	BOOLEAN fgIsMergeIn;

	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	prAisIbssPeerFoundMsg = (P_MSG_AIS_IBSS_PEER_FOUND_T) prMsgHdr;

	ASSERT(prAisIbssPeerFoundMsg->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex);

	prStaRec = prAisIbssPeerFoundMsg->prStaRec;
	ASSERT(prStaRec);

	fgIsMergeIn = prAisIbssPeerFoundMsg->fgIsMergeIn;

	cnmMemFree(prAdapter, prMsgHdr);

	eNextState = prAisFsmInfo->eCurrentState;
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:
		{
			
			if (fgIsMergeIn) {

				
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					ASSERT(0);	
				}

				
				prStaRec->fgIsQoS = TRUE;	
#else
				
				prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					ASSERT(0);	
				}

				
				prStaRec->fgIsQoS = FALSE;	

#endif

				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				
				nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

				
				aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

				
				nicPmIndicateBssConnected(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

				
				eNextState = AIS_STATE_NORMAL_TR;

				
				aisFsmReleaseCh(prAdapter);
			}
			
			else {

				
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				prAisFsmInfo->prTargetBssDesc = prBssDesc;

				
				eNextState = AIS_STATE_IBSS_MERGE;
			}
		}
		break;

	case AIS_STATE_NORMAL_TR:
		{

			
			if (fgIsMergeIn) {

				
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				
				prStaRec->fgIsQoS = TRUE;	
#else
				
				prStaRec->fgIsQoS = FALSE;	
#endif

				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

			}
			
			else {

				
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				prAisFsmInfo->prTargetBssDesc = prBssDesc;

				
				eNextState = AIS_STATE_IBSS_MERGE;

			}
		}
		break;

	default:
		break;
	}

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				
#endif 

VOID
aisIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter,
				ENUM_PARAM_MEDIA_STATE_T eConnectionState, BOOLEAN fgDelayIndication)
{
	EVENT_CONNECTION_STATUS rEventConnStatus;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisIndicationOfMediaStateToHost()");

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	
	

	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {
		if (prAisBssInfo->eConnectionStateIndicated == eConnectionState)
			return;

	}

	if (!fgDelayIndication) {
		
		prAisFsmInfo->u4PostponeIndStartTime = 0;

		
		rEventConnStatus.ucMediaStatus = (UINT_8) eConnectionState;

		if (eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			rEventConnStatus.ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED;

			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_INFRA;
				rEventConnStatus.u2AID = prAisBssInfo->u2AssocId;
				rEventConnStatus.u2ATIMWindow = 0;
			} else if (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_IBSS;
				rEventConnStatus.u2AID = 0;
				rEventConnStatus.u2ATIMWindow = prAisBssInfo->u2ATIMWindow;
			} else {
				ASSERT(0);
			}

			COPY_SSID(rEventConnStatus.aucSsid,
				  rEventConnStatus.ucSsidLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

			COPY_MAC_ADDR(rEventConnStatus.aucBssid, prAisBssInfo->aucBSSID);

			rEventConnStatus.u2BeaconPeriod = prAisBssInfo->u2BeaconInterval;
			rEventConnStatus.u4FreqInKHz = nicChannelNum2Freq(prAisBssInfo->ucPrimaryChannel);

			switch (prAisBssInfo->ucNonHTBasicPhyType) {
			case PHY_TYPE_HR_DSSS_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;

			case PHY_TYPE_ERP_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM24;
				break;

			case PHY_TYPE_OFDM_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM5;
				break;

			default:
				ASSERT(0);
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;
			}
		} else {
			
			secClearPmkid(prAdapter);
			rEventConnStatus.ucReasonOfDisconnect = prAisBssInfo->ucReasonOfDisconnect;
		}

		
		nicMediaStateChange(prAdapter, prAdapter->prAisBssInfo->ucBssIndex, &rEventConnStatus);
		prAisBssInfo->eConnectionStateIndicated = eConnectionState;
	} else {
		
		ASSERT(eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED);

#if CFG_SUPPORT_RN
		if (prAisBssInfo->fgDisConnReassoc) {
			DBGLOG(AIS, INFO, "Reassoc the AP once beacause of receive deauth/deassoc\n");
		} else
#endif
		{
			DBGLOG(AIS, INFO, "Postpone the indication of Disconnect for %d seconds\n",
					prConnSettings->ucDelayTimeOfDisconnectEvent);

			prAisFsmInfo->u4PostponeIndStartTime = kalGetTimeTick();
		}
	}

}				

VOID aisCheckPostponedDisconnTimeout(IN P_ADAPTER_T prAdapter, P_AIS_FSM_INFO_T prAisFsmInfo)
{
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	BOOLEAN fgFound = TRUE;

	if (prAisFsmInfo->u4PostponeIndStartTime == 0)
		return;

	
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_SEARCH ||
		prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN ||
		prAisFsmInfo->eCurrentState == AIS_STATE_COLLECT_ESS_INFO)
		return;

	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (!CHECK_FOR_TIMEOUT(kalGetTimeTick(), prAisFsmInfo->u4PostponeIndStartTime,
			SEC_TO_MSEC(prConnSettings->ucDelayTimeOfDisconnectEvent)))
		return;

	
	if (prAisBssInfo->prStaRecOfAP) {
		

		prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
	}
	
	while (fgFound)
		fgFound = aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
	if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR)
		prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;
	prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
	prAisBssInfo->u2DeauthReason = 100 * REASON_CODE_BEACON_TIMEOUT + prAisBssInfo->u2DeauthReason;
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
	prConnSettings->ucSSIDLen = 0;
#endif
	
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED, FALSE);

}				

VOID aisUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec, P_SW_RFB_T prAssocRspSwRfb)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame;
	P_BSS_DESC_T prBssDesc;
	UINT_16 u2IELength;
	PUINT_8 pucIE;

	DEBUGFUNC("aisUpdateBssInfoForJOIN()");

	ASSERT(prStaRec);
	ASSERT(prAssocRspSwRfb);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prAssocRspSwRfb->pvHeader;

	DBGLOG(AIS, TRACE, "Update AIS_BSS_INFO_T and apply settings to MAC\n");

	
	
	prAisBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;

	
	COPY_SSID(prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	
	prAisBssInfo->ucPrimaryChannel = prAisFsmInfo->prTargetBssDesc->ucChannelNum;
	prAisBssInfo->eBand = prAisFsmInfo->prTargetBssDesc->eBand;

	
	
	prAisBssInfo->prStaRecOfAP = prStaRec;
	prAisBssInfo->u2AssocId = prStaRec->u2AssocId;

	
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;	

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
	else
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;

#if CFG_SUPPORT_TDLS
	prAisBssInfo->fgTdlsIsProhibited = prStaRec->fgTdlsIsProhibited;
	prAisBssInfo->fgTdlsIsChSwProhibited = prStaRec->fgTdlsIsChSwProhibited;
#endif 

	
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	nicTxUpdateBssDefaultRate(prAisBssInfo);

	
	
	COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prAssocRspFrame->aucBSSID);

	u2IELength = (UINT_16) ((prAssocRspSwRfb->u2PacketLen - prAssocRspSwRfb->u2HeaderLen) -
				(OFFSET_OF(WLAN_ASSOC_RSP_FRAME_T, aucInfoElem[0]) - WLAN_MAC_MGMT_HEADER_LEN));
	pucIE = prAssocRspFrame->aucInfoElem;

	
	
	mqmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	prAisBssInfo->fgIsQBSS = prStaRec->fgIsQoS;

	
	prBssDesc = scanSearchBssDescByBssid(prAdapter, prAssocRspFrame->aucBSSID);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;
		aisRemoveBlackList(prAdapter, prBssDesc);
		prBssDesc->ucJoinFailureCount = 0;
		
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
	} else {
		
		ASSERT(0);
	}

	
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = 0;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_INFRA;
	prAisBssInfo->ucRoamSkipTimes = 3;
	prAisBssInfo->fgGoodRcpiArea = FALSE;
	prAisBssInfo->fgPoorRcpiArea = FALSE;

	
	rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	

}				

#if CFG_SUPPORT_ADHOC
VOID aisUpdateBssInfoForCreateIBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (prAisBssInfo->fgIsBeaconActivated)
		return;

	
	
	prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

	
	COPY_SSID(prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	
	prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
	prAisBssInfo->u2AssocId = 0;

	
	prAisBssInfo->ucPrimaryChannel = prConnSettings->ucAdHocChannelNum;
	prAisBssInfo->eBand = prConnSettings->eAdHocBand;

	if (prAisBssInfo->eBand == BAND_2G4) {
		
		prAisBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN;
		
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_MIXED_11BG;
	} else {
		
		prAisBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11ANAC;
		
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_11A;
	}

	
	prAisBssInfo->u2BeaconInterval = prConnSettings->u2BeaconPeriod;
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = prConnSettings->u2AtimWindow;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;

	if (prConnSettings->eEncStatus == ENUM_ENCRYPTION1_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION2_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION3_ENABLED) {
		prAisBssInfo->fgIsProtection = TRUE;
	} else {
		prAisBssInfo->fgIsProtection = FALSE;
	}

	
	ibssInitForAdHoc(prAdapter, prAisBssInfo);
	
	bssInitializeClientList(prAdapter, prAisBssInfo);

	
	
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	bssUpdateBeaconContent(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	nicPmIndicateBssCreated(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

	
	cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer, SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));

	return;

}				

VOID aisUpdateBssInfoForMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc;
	
	

	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);

	if (!prAisBssInfo->fgIsBeaconActivated) {

		
		
		prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

		
		COPY_SSID(prAisBssInfo->aucSSID,
			  prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

		
		prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		prAisBssInfo->u2AssocId = 0;
	}
	
	
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;	

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE) {
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
		prAisBssInfo->fgUseShortPreamble = TRUE;
	} else {
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;
		prAisBssInfo->fgUseShortPreamble = FALSE;
	}

	
	prAisBssInfo->fgUseShortSlotTime = FALSE;	
	prAisBssInfo->u2CapInfo &= ~CAP_INFO_SHORT_SLOT_TIME;

	if (prAisBssInfo->u2CapInfo & CAP_INFO_PRIVACY)
		prAisBssInfo->fgIsProtection = TRUE;
	else
		prAisBssInfo->fgIsProtection = FALSE;

	
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	rateGetDataRatesFromRateSet(prAisBssInfo->u2OperationalRateSet,
				    prAisBssInfo->u2BSSBasicRateSet,
				    prAisBssInfo->aucAllSupportedRates, &prAisBssInfo->ucAllSupportedRatesLen);

	

	
	prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;

		
		COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prBssDesc->aucBSSID);

		
		prAisBssInfo->ucPrimaryChannel = prBssDesc->ucChannelNum;
		prAisBssInfo->eBand = prBssDesc->eBand;

		
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
		prAisBssInfo->ucDTIMPeriod = 0;
		prAisBssInfo->u2ATIMWindow = 0;	

		prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;
	} else {
		
		ASSERT(0);
	}

	
	
	nicTxUpdateBssDefaultRate(prAisBssInfo);

	
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	bssUpdateBeaconContent(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	nicPmIndicateBssConnected(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

}				

BOOLEAN aisValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags)
{
	P_WLAN_MAC_MGMT_HEADER_T prMgtHdr;
	P_BSS_INFO_T prBssInfo;
	P_IE_SSID_T prIeSsid = (P_IE_SSID_T) NULL;
	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;
	BOOLEAN fgReplyProbeResp = FALSE;

	ASSERT(prSwRfb);
	ASSERT(pu4ControlFlags);

	prBssInfo = prAdapter->prAisBssInfo;

	
	prMgtHdr = (P_WLAN_MAC_MGMT_HEADER_T) prSwRfb->pvHeader;

	u2IELength = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen;
	pucIE = (PUINT_8) ((ULONG) prSwRfb->pvHeader + prSwRfb->u2HeaderLen);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (ELEM_ID_SSID == IE_ID(pucIE)) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (P_IE_SSID_T) pucIE;

			break;
		}
	}			

	

	if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {

		if ((prIeSsid) && ((prIeSsid->ucLength == BC_SSID_LEN) ||	
				   EQUAL_SSID(prBssInfo->aucSSID, prBssInfo->ucSSIDLen,	
					      prIeSsid->aucSSID, prIeSsid->ucLength))) {
			fgReplyProbeResp = TRUE;
		}
	}

	return fgReplyProbeResp;

}				

#endif 

VOID aisFsmDisconnect(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgDelayIndication)
{
	P_BSS_INFO_T prAisBssInfo;
	UINT_8 aucP2pSsid[] = CTIA_MAGIC_SSID;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	nicPmIndicateBssAbort(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

#if CFG_SUPPORT_ADHOC
	if (prAisBssInfo->fgIsBeaconActivated) {
		nicUpdateBeaconIETemplate(prAdapter,
					  IE_UPD_METHOD_DELETE_ALL, prAdapter->prAisBssInfo->ucBssIndex, 0, NULL, 0);

		prAisBssInfo->fgIsBeaconActivated = FALSE;
	}
#endif

	rlmBssAborted(prAdapter, prAisBssInfo);

	
	if (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState) {

		
		{

			if (EQUAL_SSID(aucP2pSsid, CTIA_MAGIC_SSID_LEN, prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen))
				nicEnterCtiaMode(prAdapter, FALSE, FALSE);
		}

		if (prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST) {
#if CFG_SUPPORT_RN
			if (prAisBssInfo->fgDisConnReassoc == FALSE)
#endif
				{
					scanRemoveBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
					
					wlanClearBssInScanningResult(prAdapter, prAisBssInfo->aucBSSID);
				}

			
			if (fgDelayIndication) {
				aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);
			}
		} else {
			scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
		}

		if (fgDelayIndication) {
			if (OP_MODE_IBSS != prAisBssInfo->eCurrentOPMode)
				prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		} else {
			prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		}
	} else {
		prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
	}

	
	if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION) {
		aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

		
		nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
	}

	if (!fgDelayIndication) {
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
		prAdapter->rWifiVar.rConnSettings.ucSSIDLen = 0;
#endif
		
		if (prAisBssInfo->prStaRecOfAP) {
			
			prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		}
	}
#if CFG_SUPPORT_ROAMING
	roamingFsmRunEventAbort(prAdapter);

	
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
#endif 

	
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED, fgDelayIndication);

	
	aisFsmSteps(prAdapter, AIS_STATE_IDLE);

}				

static VOID aisFsmRunEventScanDoneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParam)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventScanDoneTimeOut()");

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	DBGLOG(AIS, STATE, "aisFsmRunEventScanDoneTimeOut Current[%d]\n", prAisFsmInfo->eCurrentState);

	
	

	prConnSettings->fgIsScanReqIssued = FALSE;
	kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_SCAN:
		prAisFsmInfo->u4ScanIELength = 0;
		eNextState = AIS_STATE_IDLE;
		break;
	case AIS_STATE_ONLINE_SCAN:
		
		prAisFsmInfo->u4ScanIELength = 0;
#if CFG_SUPPORT_ROAMING
		eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
		eNextState = AIS_STATE_NORMAL_TR;
#endif 
#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
		scanGetCurrentEssChnlList(prAdapter);
#endif
		break;
	default:
		break;
	}

	
	aisFsmStateAbort_SCAN(prAdapter);
	aisRemoveOldestBcnTimeout(prAisFsmInfo);

 

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				

VOID aisFsmRunEventBGSleepTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DEBUGFUNC("aisFsmRunEventBGSleepTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		DBGLOG(AIS, LOUD, "EVENT - SCAN TIMER: Idle End - Current Time = %u\n", kalGetTimeTick());

		eNextState = AIS_STATE_LOOKING_FOR;

		SET_NET_PWR_STATE_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

		break;

	default:
		break;
	}

	
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				

VOID aisFsmRunEventIbssAloneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DEBUGFUNC("aisFsmRunEventIbssAloneTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:


		DBGLOG(AIS, LOUD, "EVENT-IBSS ALONE TIMER: Start pairing\n");

		prAisFsmInfo->fgTryScan = TRUE;

		
		aisFsmReleaseCh(prAdapter);

		
		eNextState = AIS_STATE_SEARCH;

		break;

	default:
		break;
	}

	
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				

VOID aisFsmRunEventJoinTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	OS_SYSTIME rCurrentTime;

	DEBUGFUNC("aisFsmRunEventJoinTimeout()");

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_JOIN:
		DBGLOG(AIS, LOUD, "EVENT- JOIN TIMEOUT\n");

		
		aisFsmStateAbort_JOIN(prAdapter);

		
		aisAddBlacklist(prAdapter, prAisFsmInfo->prTargetBssDesc);
		prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount++;

		if (prAisFsmInfo->prTargetBssDesc->ucJoinFailureCount < JOIN_MAX_RETRY_FAILURE_COUNT) {
			
			eNextState = AIS_STATE_SEARCH;
		} else if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			
			
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else
		    if (prAisFsmInfo->rJoinReqTime != 0 &&
					!CHECK_FOR_TIMEOUT(rCurrentTime,
									   prAisFsmInfo->rJoinReqTime,
									   SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
			
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else {
			
			eNextState = AIS_STATE_JOIN_FAILURE;
		}

		break;

	case AIS_STATE_NORMAL_TR:
		
		aisFsmReleaseCh(prAdapter);
		prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

		
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE) {
			wlanClearScanningResult(prAdapter);
			eNextState = AIS_STATE_ONLINE_SCAN;
		}

		break;

	default:
		
		aisFsmReleaseCh(prAdapter);
		break;

	}

	
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

}				

VOID aisFsmRunEventDeauthTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	aisDeauthXmitComplete(prAdapter, NULL, TX_RESULT_LIFE_TIMEOUT);
}

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisFsmRunEventSecModeChangeTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	DBGLOG(AIS, INFO, "Beacon security mode change timeout, trigger disconnect!\n");
	aisBssSecurityChanged(prAdapter);
}
#endif
#if defined(CFG_TEST_MGMT_FSM) && (CFG_TEST_MGMT_FSM != 0)
VOID aisTest(VOID)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_8 aucSSID[] = "pci-11n";
	UINT_8 ucSSIDLen = 7;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	
	prConnSettings->fgIsConnReqIssued = TRUE;
	prConnSettings->ucSSIDLen = ucSSIDLen;
	kalMemCopy(prConnSettings->aucSSID, aucSSID, ucSSIDLen);

	prAisAbortMsg = (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {

		ASSERT(0);	
		return;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_HEM_AIS_FSM_ABORT;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	wifi_send_msg(INDX_WIFI, MSG_ID_WIFI_IST, 0);

}
#endif 

VOID aisFsmScanRequest(IN P_ADAPTER_T prAdapter, IN P_PARAM_SSID_T prSsid, IN PUINT_8 pucIe, IN UINT_32 u4IeLength)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisFsmScanRequest()");

	ASSERT(prAdapter);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
		scanInitEssResult(prAdapter);

		if (prSsid == NULL) {
			prAisFsmInfo->ucScanSSIDNum = 0;
		} else {
			prAisFsmInfo->ucScanSSIDNum = 1;

			COPY_SSID(prAisFsmInfo->arScanSSID[0].aucSsid,
				  prAisFsmInfo->arScanSSID[0].u4SsidLen, prSsid->aucSsid, prSsid->u4SsidLen);
		}

		if (u4IeLength > 0) {
			prAisFsmInfo->u4ScanIELength = u4IeLength;
			kalMemCopy(prAisFsmInfo->aucScanIEBuf, pucIe, u4IeLength);
		} else {
			prAisFsmInfo->u4ScanIELength = 0;
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(AIS, WARN,
					       "Scan Request with channel granted for join operation: %d, %d",
						prAisFsmInfo->fgIsChannelGranted, prAisFsmInfo->fgIsChannelRequested);
				}

				
				wlanClearScanningResult(prAdapter);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
		}
	} else {
		DBGLOG(AIS, WARN, "Scan Request dropped. (state: %d)\n", prAisFsmInfo->eCurrentState);
	}

}				

VOID
aisFsmScanRequestAdv(IN P_ADAPTER_T prAdapter, IN UINT_8 ucSsidNum, IN P_PARAM_SSID_T prSsid, IN PUINT_8 pucIe,
			IN UINT_32 u4IeLength,  IN UINT_8 ucSetChannel)
{
	UINT_32 i;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisFsmScanRequestAdv()");

	ASSERT(prAdapter);
	ASSERT(ucSsidNum <= SCN_SSID_MAX_NUM);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;
		scanInitEssResult(prAdapter);

		if (ucSsidNum == 0) {
			prAisFsmInfo->ucScanSSIDNum = 0;
		} else {
			prAisFsmInfo->ucScanSSIDNum = ucSsidNum;

			for (i = 0; i < ucSsidNum; i++) {
				COPY_SSID(prAisFsmInfo->arScanSSID[i].aucSsid,
					  prAisFsmInfo->arScanSSID[i].u4SsidLen,
					  prSsid[i].aucSsid, prSsid[i].u4SsidLen);
			}
		}

		if (u4IeLength > 0) {
			prAisFsmInfo->u4ScanIELength = u4IeLength;
			kalMemCopy(prAisFsmInfo->aucScanIEBuf, pucIe, u4IeLength);
		} else {
			prAisFsmInfo->u4ScanIELength = 0;
		}

		
		if (ucSetChannel) {
			prAdapter->prGlueInfo->pucScanChannel = (PUINT_8)&(prAdapter->prGlueInfo->rScanChannelInfo);
			DBGLOG(AIS, TRACE, "partial scan set puScanChannel\n");
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(AIS, WARN,
					       "Scan Request with channel granted for join operation: %d, %d",
						prAisFsmInfo->fgIsChannelGranted, prAisFsmInfo->fgIsChannelRequested);
				}

				
				wlanClearScanningResult(prAdapter);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
		}
	} else {
		if (ucSetChannel) {
			
			kalMemSet(&(prAdapter->prGlueInfo->rScanChannelInfo), 0, sizeof(PARTIAL_SCAN_INFO));
			prAdapter->prGlueInfo->pucScanChannel = NULL;
		}

		DBGLOG(AIS, WARN, "Scan Request dropped. (state: %d)\n", prAisFsmInfo->eCurrentState);
	}

}				

VOID aisFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_CH_GRANT_T prMsgChGrant;
	UINT_8 ucTokenID;
	UINT_32 u4GrantInterval;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;

	ucTokenID = prMsgChGrant->ucTokenID;
	u4GrantInterval = prMsgChGrant->u4GrantInterval;

	
	cnmMemFree(prAdapter, prMsgHdr);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN && prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

		
		
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rJoinTimeoutTimer,
				   prAisFsmInfo->u4ChGrantedInterval - AIS_JOIN_CH_GRANT_THRESHOLD);
		
		prAisFsmInfo->fgIsInfraChannelFinished = FALSE;

		
		aisFsmSteps(prAdapter, AIS_STATE_JOIN);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL &&
		   prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

		
		cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer, prAisFsmInfo->u4ChGrantedInterval);

		
		aisFsmSteps(prAdapter, AIS_STATE_REMAIN_ON_CHANNEL);

		
		kalReadyOnChannel(prAdapter->prGlueInfo,
				  prAisFsmInfo->rChReqInfo.u8Cookie,
				  prAisFsmInfo->rChReqInfo.eBand,
				  prAisFsmInfo->rChReqInfo.eSco,
				  prAisFsmInfo->rChReqInfo.ucChannelNum, prAisFsmInfo->rChReqInfo.u4DurationMs);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else {		
		
		aisFsmReleaseCh(prAdapter);
	}

}				

VOID aisFsmRunEventChGrantFail(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prAisBssInfo;
	P_MSG_CH_GRANT_T prMsgChGrant;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	BOOLEAN fgFound = TRUE;
	UINT_8 ucTokenID;

	prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;
	ucTokenID = prMsgChGrant->ucTokenID;
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	cnmMemFree(prAdapter, prMsgHdr);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN &&
		prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		
		if (prAisFsmInfo->u4PostponeIndStartTime == 0 &&
			prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED){
			aisFsmSteps(prAdapter, AIS_STATE_JOIN_FAILURE);
			return;
		}
		
		if (prAisFsmInfo->u4PostponeIndStartTime != 0 &&
			prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED){
			
			if (prAisBssInfo->prStaRecOfAP)
				prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;

			
			while (fgFound)
				fgFound = aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);

			prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;
			prConnSettings->fgIsDisconnectedByNonRequest = TRUE;

			prAisBssInfo->u2DeauthReason = 100 * REASON_CODE_BEACON_TIMEOUT
					+ prAisBssInfo->u2DeauthReason;
			#if CFG_SELECT_BSS_BASE_ON_MULTI_PARAM
			prConnSettings->ucSSIDLen = 0;
			#endif
			
			aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED, FALSE);
		} else if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_NOCANDIDATE);
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		}
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL &&
		   prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE);

		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);

		kalRemainOnChannelExpired(prAdapter->prGlueInfo,
				prAisFsmInfo->rChReqInfo.u8Cookie,
				prAisFsmInfo->rChReqInfo.eBand,
				prAisFsmInfo->rChReqInfo.eSco, prAisFsmInfo->rChReqInfo.ucChannelNum);

	}
}
VOID aisFsmReleaseCh(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_CH_ABORT_T prMsgChAbort;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	if (prAisFsmInfo->fgIsChannelGranted == TRUE || prAisFsmInfo->fgIsChannelRequested == TRUE) {

		prAisFsmInfo->fgIsChannelRequested = FALSE;
		prAisFsmInfo->fgIsChannelGranted = FALSE;

		
		prMsgChAbort = (P_MSG_CH_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_ABORT_T));
		if (!prMsgChAbort) {
			ASSERT(0);	
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
		prMsgChAbort->ucTokenID = prAisFsmInfo->ucSeqNumOfChReq;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChAbort, MSG_SEND_METHOD_BUF);
	}

}				

VOID aisBssBeaconTimeout(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prAisBssInfo;
	BOOLEAN fgDoAbortIndication = FALSE;
	P_CONNECTION_SETTINGS_T prConnSettings;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	
	if (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState) {
		if (OP_MODE_INFRASTRUCTURE == prAisBssInfo->eCurrentOPMode) {
			P_STA_RECORD_T prStaRec = prAisBssInfo->prStaRecOfAP;

			if (prStaRec)
				fgDoAbortIndication = TRUE;

		} else if (OP_MODE_IBSS == prAisBssInfo->eCurrentOPMode) {
			fgDoAbortIndication = TRUE;
		}
	}
	
	if (fgDoAbortIndication) {
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
		if (prConnSettings->eReConnectLevel < RECONNECT_LEVEL_INTER_SET) {
			prConnSettings->eReConnectLevel = RECONNECT_LEVEL_INTER_DRIVER;
			prConnSettings->fgIsConnReqIssued = TRUE;
		}
		aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_RADIO_LOST, TRUE);
	}

}				

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisBssSecurityChanged(P_ADAPTER_T prAdapter)
{
	prAdapter->rWifiVar.rConnSettings.fgIsDisconnectedByNonRequest = TRUE;
	aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_DEAUTHENTICATED, FALSE);
}
#endif

WLAN_STATUS
aisDeauthXmitComplete(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_DISCONNECTING) {
		if (rTxDoneStatus != TX_RESULT_DROPPED_IN_DRIVER && rTxDoneStatus != TX_RESULT_QUEUE_CLEARANCE)
			aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_NEW_CONNECTION, FALSE);
	} else {
		DBGLOG(AIS, WARN, "DEAUTH frame transmitted without further handling");
	}

	return WLAN_STATUS_SUCCESS;

}				

#if CFG_SUPPORT_ROAMING
VOID aisFsmRunEventRoamingDiscovery(IN P_ADAPTER_T prAdapter, UINT_32 u4ReqScan)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	ENUM_AIS_REQUEST_TYPE_T eAisRequest;

	DBGLOG(AIS, LOUD, "aisFsmRunEventRoamingDiscovery()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	
#if CFG_SUPPORT_WFD
#if CFG_ENABLE_WIFI_DIRECT
	{
		
		P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;

		prWfdCfgSettings = &(prAdapter->rWifiVar.rWfdConfigureSettings);
		if ((prWfdCfgSettings->ucWfdEnable != 0)) {
			DBGLOG(ROAMING, INFO, "WFD is running. Stop roaming[WfdEnable:%u]\n",
				prWfdCfgSettings->ucWfdEnable);
			roamingFsmRunEventRoam(prAdapter);
			roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_NOCANDIDATE);
			return;
		}
	}
#endif
#endif

	
	if (!u4ReqScan) {
		roamingFsmRunEventRoam(prAdapter);
		eAisRequest = AIS_REQUEST_ROAMING_CONNECT;
	} else {
		if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN
		    || prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
			eAisRequest = AIS_REQUEST_ROAMING_CONNECT;
		} else {
			eAisRequest = AIS_REQUEST_ROAMING_SEARCH;
		}
	}

	if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR && prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
		if (eAisRequest == AIS_REQUEST_ROAMING_SEARCH)
			aisFsmSteps(prAdapter, AIS_STATE_LOOKING_FOR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_COLLECT_ESS_INFO);
	} else {
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);

		aisFsmInsertRequest(prAdapter, eAisRequest);
	}

}				

ENUM_AIS_STATE_T aisFsmRoamingScanResultsUpdate(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DBGLOG(AIS, LOUD, "->aisFsmRoamingScanResultsUpdate()\n");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	roamingFsmScanResultsUpdate(prAdapter);

	eNextState = prAisFsmInfo->eCurrentState;
	if (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) {
		roamingFsmRunEventRoam(prAdapter);
		eNextState = AIS_STATE_COLLECT_ESS_INFO;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
		eNextState = AIS_STATE_COLLECT_ESS_INFO;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
		eNextState = AIS_STATE_NORMAL_TR;
	}

	return eNextState;
}				

VOID aisFsmRoamingDisconnectPrevAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prTargetStaRec)
{
	P_BSS_INFO_T prAisBssInfo;

	DBGLOG(AIS, LOUD, "aisFsmRoamingDisconnectPrevAP()");

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	nicPmIndicateBssAbort(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	

	
	if (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState)
		scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

	
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

	
	prTargetStaRec->ucBssIndex = (MAX_BSS_INDEX + 1);	
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
	prTargetStaRec->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;

}				

VOID aisUpdateBssInfoForRoamingAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prAssocRspSwRfb)
{
	P_BSS_INFO_T prAisBssInfo;

	DBGLOG(AIS, LOUD, "aisUpdateBssInfoForRoamingAP()");

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

	
	if ((prAisBssInfo->prStaRecOfAP) &&
	    (prAisBssInfo->prStaRecOfAP != prStaRec) && (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {
		
		cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP);
	}

	
	aisUpdateBssInfoForJOIN(prAdapter, prStaRec, prAssocRspSwRfb);

	
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

	
	
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

}				

#endif 

BOOLEAN aisFsmIsRequestPending(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType, IN BOOLEAN bRemove)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_REQ_HDR_T prPendingReqHdr, prPendingReqHdrNext;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	
	LINK_FOR_EACH_ENTRY_SAFE(prPendingReqHdr,
				 prPendingReqHdrNext, &(prAisFsmInfo->rPendingReqList), rLinkEntry, AIS_REQ_HDR_T) {
		
		if (prPendingReqHdr->eReqType == eReqType) {
			
			if (bRemove == TRUE) {
				LINK_REMOVE_KNOWN_ENTRY(&(prAisFsmInfo->rPendingReqList),
							&(prPendingReqHdr->rLinkEntry));
				if (eReqType == AIS_REQUEST_SCAN) {
					if (prPendingReqHdr->pucChannelInfo != NULL) {
						DBGLOG(AIS, INFO, "partial scan req pu8ChannelInfo no NULL\n");
						prAdapter->prGlueInfo->pucScanChannel = prPendingReqHdr->pucChannelInfo;
						prPendingReqHdr->pucChannelInfo = NULL;
					}
				}

				cnmMemFree(prAdapter, prPendingReqHdr);
			}

			return TRUE;
		}
	}

	return FALSE;
}

P_AIS_REQ_HDR_T aisFsmGetNextRequest(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_REQ_HDR_T prPendingReqHdr;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	LINK_REMOVE_HEAD(&(prAisFsmInfo->rPendingReqList), prPendingReqHdr, P_AIS_REQ_HDR_T);
	
	if ((prPendingReqHdr != NULL) && (prAdapter->prGlueInfo != NULL)) {
		if (prAdapter->prGlueInfo->pucScanChannel != NULL)
			DBGLOG(INIT, TRACE, "partial scan prGlueInfo error puScanChannel=%p",
				prAdapter->prGlueInfo->pucScanChannel);

		if (prPendingReqHdr->pucChannelInfo != NULL) {
			prAdapter->prGlueInfo->pucScanChannel = prPendingReqHdr->pucChannelInfo;
			prPendingReqHdr->pucChannelInfo = NULL;
		}
	}

	return prPendingReqHdr;
}

BOOLEAN aisFsmInsertRequest(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType)
{
	P_AIS_REQ_HDR_T prAisReq;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	prAisReq = (P_AIS_REQ_HDR_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(AIS_REQ_HDR_T));

	if (!prAisReq) {
		ASSERT(0);	
		return FALSE;
	}

	prAisReq->eReqType = eReqType;
	prAisReq->pucChannelInfo = NULL;
	
	if ((prAdapter->prGlueInfo != NULL) &&
		(prAdapter->prGlueInfo->pucScanChannel != NULL)) {
		DBGLOG(AIS, TRACE, "partial scan insert req\n");
		prAisReq->pucChannelInfo = prAdapter->prGlueInfo->pucScanChannel;
		prAdapter->prGlueInfo->pucScanChannel = NULL;
	}

	
	LINK_INSERT_TAIL(&prAisFsmInfo->rPendingReqList, &prAisReq->rLinkEntry);

	return TRUE;
}

VOID aisFsmFlushRequest(IN P_ADAPTER_T prAdapter)
{
	P_AIS_REQ_HDR_T prAisReq;

	ASSERT(prAdapter);

	while ((prAisReq = aisFsmGetNextRequest(prAdapter)) != NULL) {
		
		if (prAisReq->pucChannelInfo != NULL) {
			DBGLOG(AIS, TRACE, "partial scan flush req\n");
			kalMemSet(prAisReq->pucChannelInfo, 0, sizeof(PARTIAL_SCAN_INFO));
		}
		cnmMemFree(prAdapter, prAisReq);
	}

}

VOID aisFsmRunEventRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_REMAIN_ON_CHANNEL_T prRemainOnChannel;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prRemainOnChannel = (P_MSG_REMAIN_ON_CHANNEL_T) prMsgHdr;

	
	prAisFsmInfo->rChReqInfo.eBand = prRemainOnChannel->eBand;
	prAisFsmInfo->rChReqInfo.eSco = prRemainOnChannel->eSco;
	prAisFsmInfo->rChReqInfo.ucChannelNum = prRemainOnChannel->ucChannelNum;
	prAisFsmInfo->rChReqInfo.u4DurationMs = prRemainOnChannel->u4DurationMs;
	prAisFsmInfo->rChReqInfo.u8Cookie = prRemainOnChannel->u8Cookie;

	if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE || prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
		
		aisFsmSteps(prAdapter, AIS_STATE_REQ_REMAIN_ON_CHANNEL);
	} else {
		aisFsmInsertRequest(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL);
	}

	
	cnmMemFree(prAdapter, prMsgHdr);

}

VOID aisFsmRunEventCancelRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_MSG_CANCEL_REMAIN_ON_CHANNEL_T prCancelRemainOnChannel;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	prCancelRemainOnChannel = (P_MSG_CANCEL_REMAIN_ON_CHANNEL_T) prMsgHdr;

	
	if (prCancelRemainOnChannel->u8Cookie == prAisFsmInfo->rChReqInfo.u8Cookie) {

		
		if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL) {
			
			aisFsmReleaseCh(prAdapter);
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
			
			aisFsmReleaseCh(prAdapter);

			
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
		}

		
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE);

		
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	}

	
	cnmMemFree(prAdapter, prMsgHdr);

}

VOID aisFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_MGMT_TX_REQUEST_T prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMsgHdr != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (prAisFsmInfo == NULL)
			break;

		prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) prMsgHdr;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo, prMgmtTxMsg->prMgmtMsduInfo, prMgmtTxMsg->u8Cookie);

	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

}				

VOID aisFsmRunEventChannelTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
		
		aisFsmReleaseCh(prAdapter);

		
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		
		kalRemainOnChannelExpired(prAdapter->prGlueInfo,
					  prAisFsmInfo->rChReqInfo.u8Cookie,
					  prAisFsmInfo->rChReqInfo.eBand,
					  prAisFsmInfo->rChReqInfo.eSco, prAisFsmInfo->rChReqInfo.ucChannelNum);

		
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	} else {
		DBGLOG(AIS, WARN, "Unexpected remain_on_channel timeout event\n");
#if DBG
		DBGLOG(AIS, STATE, "CURRENT State: [%s]\n", apucDebugAisState[prAisFsmInfo->eCurrentState]);
#else
		DBGLOG(AIS, STATE, "[%d] CURRENT State: [%d]\n", DBG_AIS_IDX, prAisFsmInfo->eCurrentState);
#endif
	}

}

WLAN_STATUS
aisFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter,
			      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo = (P_AIS_MGMT_TX_REQ_INFO_T) NULL;
	BOOLEAN fgIsSuccess = FALSE;

	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prMgmtTxReqInfo = &(prAisFsmInfo->rMgmtTxInfo);

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			fgIsSuccess = TRUE;

		if (prMgmtTxReqInfo->prMgmtTxMsdu == prMsduInfo) {
			kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
						prMgmtTxReqInfo->u8Cookie,
						fgIsSuccess, prMsduInfo->prPacket, (UINT_32) prMsduInfo->u2FrameLength);

			prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
		}

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				

WLAN_STATUS
aisFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter,
		   IN P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo, IN P_MSDU_INFO_T prMgmtTxMsdu, IN UINT_64 u8Cookie)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_MSDU_INFO_T prTxMsduInfo = (P_MSDU_INFO_T) NULL;
	P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMgmtTxReqInfo != NULL));

		if (prMgmtTxReqInfo->fgIsMgmtTxRequested) {

			
			
			prTxMsduInfo = prMgmtTxReqInfo->prMgmtTxMsdu;
			if (prTxMsduInfo != NULL) {

				kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
							prMgmtTxReqInfo->u8Cookie,
							FALSE,
							prTxMsduInfo->prPacket, (UINT_32) prTxMsduInfo->u2FrameLength);

				
				
				prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
			}
			
			
		}

		ASSERT(prMgmtTxReqInfo->prMgmtTxMsdu == NULL);

		prWlanHdr = (P_WLAN_MAC_HEADER_T) ((ULONG) prMgmtTxMsdu->prPacket + MAC_TX_RESERVED_FIELD);
		prStaRec = cnmGetStaRecByAddress(prAdapter, prAdapter->prAisBssInfo->ucBssIndex, prWlanHdr->aucAddr1);

		TX_SET_MMPDU(prAdapter,
			     prMgmtTxMsdu,
			     (prStaRec !=
			      NULL) ? (prStaRec->ucBssIndex) : (prAdapter->prAisBssInfo->ucBssIndex),
			     (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
			     WLAN_MAC_MGMT_HEADER_LEN, prMgmtTxMsdu->u2FrameLength,
			     aisFsmRunEventMgmtFrameTxDone, MSDU_RATE_MODE_AUTO);
		prMgmtTxReqInfo->u8Cookie = u8Cookie;
		prMgmtTxReqInfo->prMgmtTxMsdu = prMgmtTxMsdu;
		prMgmtTxReqInfo->fgIsMgmtTxRequested = TRUE;

		nicTxConfigPktControlFlag(prMgmtTxMsdu, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

		
		nicTxEnqueueMsdu(prAdapter, prMgmtTxMsdu);

	} while (FALSE);

	return rWlanStatus;
}				

VOID aisFuncValidateRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = (P_AIS_FSM_INFO_T) NULL;

	DEBUGFUNC("aisFuncValidateRxActionFrame");

	do {

		ASSERT((prAdapter != NULL) && (prSwRfb != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (1 ) {
			
			kalIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb);
		}

	} while (FALSE);

	return;

}				

struct AIS_BLACKLIST_ITEM *
aisAddBlacklist(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	if (!prBssDesc) {
		DBGLOG(AIS, ERROR, "bss descriptor is NULL\n");
		return NULL;
	}
	if (prBssDesc->prBlack) {
		GET_CURRENT_SYSTIME(&prBssDesc->prBlack->rAddTime);
		prBssDesc->prBlack->ucCount++;
		DBGLOG(AIS, INFO, "update blacklist for %pM, count %d\n",
			prBssDesc->aucBSSID, prBssDesc->prBlack->ucCount);
		return prBssDesc->prBlack;
	}

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);

	if (prEntry) {
		GET_CURRENT_SYSTIME(&prEntry->rAddTime);
		prBssDesc->prBlack = prEntry;
		prEntry->ucCount++;
		DBGLOG(AIS, INFO, "update blacklist for %pM, count %d\n",
			prBssDesc->aucBSSID, prEntry->ucCount);
		return prEntry;
	}

	LINK_REMOVE_HEAD(prFreeList, prEntry, struct AIS_BLACKLIST_ITEM *);
	if (!prEntry)
		prEntry = kalMemAlloc(sizeof(struct AIS_BLACKLIST_ITEM), VIR_MEM_TYPE);
	if (!prEntry) {
		DBGLOG(AIS, WARN, "No memory to allocate\n");
		return NULL;
	}
	kalMemZero(prEntry, sizeof(*prEntry));
	prEntry->ucCount = 1;
	COPY_MAC_ADDR(prEntry->aucBSSID, prBssDesc->aucBSSID);
	COPY_SSID(prEntry->aucSSID, prEntry->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	GET_CURRENT_SYSTIME(&prEntry->rAddTime);
	LINK_INSERT_HEAD(prBlackList, &prEntry->rLinkEntry);
	prBssDesc->prBlack = prEntry;

	DBGLOG(AIS, INFO, "Add %pM to black List\n", prBssDesc->aucBSSID);
	return prEntry;
}

VOID aisRemoveBlackList(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	prEntry = aisQueryBlackList(prAdapter, prBssDesc);
	if (!prEntry)
		return;
	LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
	LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	prBssDesc->prBlack = NULL;
	DBGLOG(AIS, INFO, "Remove %pM from blacklist\n", prBssDesc->aucBSSID);
}

struct AIS_BLACKLIST_ITEM *
aisQueryBlackList(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;

	if (!prBssDesc)
		return NULL;
	else if (prBssDesc->prBlack)
		return prBssDesc->prBlack;

	LINK_FOR_EACH_ENTRY(prEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry->aucBSSID) &&
			EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			prEntry->aucSSID, prEntry->ucSSIDLen)) {
			prBssDesc->prBlack = prEntry;
			return prEntry;
		}
	}
	DBGLOG(AIS, TRACE, "%pM is not in blacklist\n", prBssDesc->aucBSSID);
	return NULL;
}

VOID aisRemoveTimeoutBlacklist(P_ADAPTER_T prAdapter)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	OS_SYSTIME rCurrent;

	GET_CURRENT_SYSTIME(&rCurrent);

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		if (!CHECK_FOR_TIMEOUT(rCurrent, prEntry->rAddTime, SEC_TO_MSEC(AIS_BLACKLIST_TIMEOUT)))
			continue;
		LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
		LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	}
}

static VOID aisRemoveDisappearedBlacklist(P_ADAPTER_T prAdapter)
{
	P_CONNECTION_SETTINGS_T prConnSettings = &prAdapter->rWifiVar.rConnSettings;
	struct AIS_BLACKLIST_ITEM *prEntry = NULL;
	struct AIS_BLACKLIST_ITEM *prNextEntry = NULL;
	P_LINK_T prBlackList = &prConnSettings->rBlackList.rUsingLink;
	P_LINK_T prFreeList = &prConnSettings->rBlackList.rFreeLink;
	P_BSS_DESC_T prBssDesc = NULL;
	P_LINK_T prBSSDescList = &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	UINT_64 u8Current = kalGetBootTime();
	BOOLEAN fgDisappeared = TRUE;

	LINK_FOR_EACH_ENTRY_SAFE(prEntry, prNextEntry, prBlackList, rLinkEntry, struct AIS_BLACKLIST_ITEM) {
		fgDisappeared = TRUE;
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
			if (prBssDesc->prBlack == prEntry || (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry->aucBSSID) &&
				EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
				prEntry->aucSSID, prEntry->ucSSIDLen))) {
				fgDisappeared = FALSE;
				break;
			}
		}
		if (!fgDisappeared || (u8Current - prEntry->u8DisapperTime) < 600 * USEC_PER_SEC)
			continue;
		DBGLOG(AIS, INFO, "Remove disappeared blacklist %s %pM\n",
			prEntry->aucSSID, prEntry->aucBSSID);
		LINK_REMOVE_KNOWN_ENTRY(prBlackList, &prEntry->rLinkEntry);
		LINK_INSERT_HEAD(prFreeList, &prEntry->rLinkEntry);
	}
}

BOOLEAN aisApOverload(struct AIS_BLACKLIST_ITEM *prBlack)
{
	switch (prBlack->u2AuthStatus) {
	case STATUS_CODE_ASSOC_DENIED_AP_OVERLOAD:
	case STATUS_CODE_ASSOC_DENIED_BANDWIDTH:
		return TRUE;
	}
	switch (prBlack->u2DeauthReason) {
	case REASON_CODE_DISASSOC_LACK_OF_BANDWIDTH:
	case REASON_CODE_DISASSOC_AP_OVERLOAD:
		return TRUE;
	}
	return FALSE;
}

UINT_16 aisCalculateBlackListScore(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	if (!prBssDesc->prBlack)
		prBssDesc->prBlack = aisQueryBlackList(prAdapter, prBssDesc);

	if (!prBssDesc->prBlack)
		return 100;
	else if (aisApOverload(prBssDesc->prBlack) || prBssDesc->prBlack->ucCount >= 10)
		return 0;
	return 100 - prBssDesc->prBlack->ucCount * 10;
}

VOID aisRecordBeaconTimeout(P_ADAPTER_T prAdapter, P_BSS_INFO_T prAisBssInfo)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;
	struct LINK_MGMT *prBcnTimeout = &prAisFsmInfo->rBcnTimeout;
	struct AIS_BEACON_TIMEOUT_BSS *prEntry = NULL;

	LINK_MGMT_GET_ENTRY(prBcnTimeout, prEntry, struct AIS_BEACON_TIMEOUT_BSS, VIR_MEM_TYPE);
	if (!prEntry) {
		DBGLOG(CNM, WARN, "No memory to allocate\n");
		return;
	}
	COPY_MAC_ADDR(prEntry->aucBSSID, prAisBssInfo->aucBSSID);
	COPY_SSID(prEntry->aucSSID, prEntry->ucSSIDLen,
			prAisBssInfo->aucSSID, prAisBssInfo->ucSSIDLen);
	prEntry->u8Tsf = prAisFsmInfo->prTargetBssDesc->u8TimeStamp.QuadPart;
	prEntry->u8AddTime = kalGetBootTime();
	LINK_INSERT_TAIL(&prBcnTimeout->rUsingLink, &prEntry->rLinkEntry);
}

VOID aisRemoveBeaconTimeoutEntry(P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
#if 0 
	UINT_64 u8Tsf = 0;
	P_AIS_FSM_INFO_T prAisFsmInfo = &prAdapter->rWifiVar.rAisFsmInfo;
	struct LINK_MGMT *prBcnTimeout = &prAisFsmInfo->rBcnTimeout;
	struct AIS_BEACON_TIMEOUT_BSS *prEntry = NULL;

	LINK_FOR_EACH_ENTRY(prEntry, &prBcnTimeout->rUsingLink,
		rLinkEntry, struct AIS_BEACON_TIMEOUT_BSS) {
		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prEntry->aucBSSID) &&
			EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
			prEntry->aucSSID, prEntry->ucSSIDLen)) {
			u8Tsf = prBssDesc->u8TimeStamp.QuadPart;
			if (u8Tsf < prEntry->u8Tsf)
				DBGLOG(AIS, INFO, "%pM %s may reboot %llu seconds ago\n",
					prBssDesc->aucBSSID, prBssDesc->aucSSID, u8Tsf/USEC_PER_SEC);
			else
				DBGLOG(AIS, INFO, "%pM %s\n", prBssDesc->aucBSSID, prBssDesc->aucSSID);
			LINK_REMOVE_KNOWN_ENTRY(&prBcnTimeout->rUsingLink, prEntry);
			LINK_INSERT_HEAD(&prBcnTimeout->rFreeLink, &prEntry->rLinkEntry);
			return;
		}
	}
#endif
}

static VOID aisRemoveOldestBcnTimeout(P_AIS_FSM_INFO_T prAisFsmInfo)
{
#if 0 
	struct AIS_BEACON_TIMEOUT_BSS *prEntry = NULL;
	P_LINK_T prLink = &prAisFsmInfo->rBcnTimeout.rUsingLink;
	UINT_64 u8Current = kalGetBootTime();

	while (TRUE) {
		prEntry = LINK_PEEK_HEAD(prLink, struct AIS_BEACON_TIMEOUT_BSS, rLinkEntry);
		if (!prEntry || (u8Current - prEntry->u8AddTime < CFG_BSS_DISAPPEAR_THRESOLD * USEC_PER_SEC))
			break;
		DBGLOG(AIS, INFO, "%pM %s has disappeard about %llu seconds\n",
			prEntry->aucBSSID, prEntry->aucSSID, (u8Current - prEntry->u8AddTime)/USEC_PER_SEC);
		LINK_REMOVE_HEAD(prLink, prEntry, struct AIS_BEACON_TIMEOUT_BSS *);
	}
#endif
}

#if CFG_SUPPORT_802_11K
static VOID
aisSendNeighborRequest(P_ADAPTER_T prAdapter)
{
	struct SUB_ELEMENT_LIST rSSIDIE;
	P_BSS_INFO_T prBssInfo = prAdapter->prAisBssInfo;

	kalMemZero(&rSSIDIE, sizeof(rSSIDIE));
	rSSIDIE.rSubIE.ucSubID = ELEM_ID_SSID;
	rSSIDIE.rSubIE.ucLength = prBssInfo->ucSSIDLen;
	kalMemCopy(&rSSIDIE.rSubIE.aucOptInfo[0], prBssInfo->aucSSID, prBssInfo->ucSSIDLen);
	rlmTxNeighborReportRequest(prAdapter, prBssInfo->prStaRecOfAP, &rSSIDIE);
}

VOID aisCollectNeighborAPChannel(P_ADAPTER_T prAdapter,
	struct IE_NEIGHBOR_REPORT_T *prNeiRep, UINT_16 u2Length)
{
	PUINT_8 pucChnlList = &prAdapter->rWifiVar.rAisFsmInfo.aucNeighborAPChnl[0];
	UINT_8 i = 0;
	BOOLEAN fgValidChannel = FALSE;

	kalMemZero(pucChnlList, CFG_NEIGHBOR_AP_CHANNEL_NUM);
	while (u2Length > ELEM_HDR_LEN && i < CFG_NEIGHBOR_AP_CHANNEL_NUM) {
		fgValidChannel = rlmDomainIsLegalChannel(prAdapter,
			prNeiRep->ucChnlNumber <= 14 ? BAND_2G4:BAND_5G, prNeiRep->ucChnlNumber);
		if (fgValidChannel) {
			*pucChnlList++ = prNeiRep->ucChnlNumber;
			i++;
		}
		u2Length -= IE_SIZE(prNeiRep);
		prNeiRep = (struct IE_NEIGHBOR_REPORT_T *)((PUINT_8)prNeiRep + IE_SIZE(prNeiRep));
	}
	pucChnlList = &prAdapter->rWifiVar.rAisFsmInfo.aucNeighborAPChnl[0];
	DBGLOG(AIS, INFO, "Neighbor AP channel cnt %d, list %d %d %d %d %d %d\n", i, pucChnlList[0],
		pucChnlList[1], pucChnlList[2], pucChnlList[3], pucChnlList[4], pucChnlList[5]);
}
#endif

VOID aisRunEventChnlUtilRsp(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	struct MSG_CH_UTIL_RSP *prChUtilRsp = (struct MSG_CH_UTIL_RSP *)prMsgHdr;
	struct ESS_CHNL_INFO *prEssChnlInfo = &prAdapter->rWifiVar.rAisSpecificBssInfo.arCurEssChnlInfo[0];
	PUINT_8 pucChnlList = NULL;
	PUINT_8 pucUtilization = NULL;
	UINT_8 i = 0;
	UINT_8 j = 0;

	if (!prChUtilRsp)
		return;
	if (prAdapter->rWifiVar.rAisFsmInfo.eCurrentState != AIS_STATE_COLLECT_ESS_INFO) {
		cnmMemFree(prAdapter, prChUtilRsp);
		return;
	}
	pucChnlList = prChUtilRsp->aucChnlList;
	pucUtilization = prChUtilRsp->aucChUtil;
	for (i = 0; i < prChUtilRsp->ucChnlNum; i++) {
		DBGLOG(AIS, INFO, "channel %d, utilization %d\n", pucChnlList[i], pucUtilization[i]);
		for (j = 0; j < prAdapter->rWifiVar.rAisSpecificBssInfo.ucCurEssChnlInfoNum; j++) {
			if (prEssChnlInfo[j].ucChannel != pucChnlList[i])
				continue;
			if (prEssChnlInfo[j].ucUtilization >= pucUtilization[i])
				continue;
			prEssChnlInfo[j].ucUtilization = pucUtilization[i];
			break;
		}
	}
	cnmMemFree(prAdapter, prChUtilRsp);
	aisFsmSteps(prAdapter, AIS_STATE_SEARCH);
}
