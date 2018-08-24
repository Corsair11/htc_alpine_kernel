


#ifndef _ADAPTER_H
#define _ADAPTER_H


#if CFG_SUPPORT_HOTSPOT_2_0
#include "hs20.h"
#endif


typedef struct _ENHANCE_MODE_DATA_STRUCT_T SDIO_CTRL_T, *P_SDIO_CTRL_T;

typedef struct _WLAN_INFO_T {
	PARAM_BSSID_EX_T rCurrBssId;

	
	PARAM_BSSID_EX_T arScanResult[CFG_MAX_NUM_BSS_LIST];
	PUINT_8 apucScanResultIEs[CFG_MAX_NUM_BSS_LIST];
	UINT_32 u4ScanResultNum;

	
	UINT_8 aucScanIEBuf[CFG_MAX_COMMON_IE_BUF_LEN];
	UINT_32 u4ScanIEBufferUsage;

	OS_SYSTIME u4SysTime;

	
	UINT_16 u2BeaconPeriod;
	UINT_16 u2AtimWindow;

	PARAM_RATES eDesiredRates;
	CMD_LINK_ATTRIB eLinkAttr;
	CMD_PS_PROFILE_T arPowerSaveMode[NETWORK_TYPE_INDEX_NUM];

	
	ENUM_RSSI_TRIGGER_TYPE eRssiTriggerType;
	PARAM_RSSI rRssiTriggerValue;

	
	ENUM_PARAM_PRIVACY_FILTER_T ePrivacyFilter;

	
	PARAM_RTS_THRESHOLD eRtsThreshold;

	
	UINT_8 ucNetworkType;

	
	UINT_8 ucNetworkTypeInUse;

} WLAN_INFO_T, *P_WLAN_INFO_T;

typedef struct _CONNECTION_SETTINGS_T {

	UINT_8 aucMacAddress[MAC_ADDR_LEN];

	UINT_8 ucDelayTimeOfDisconnectEvent;

	BOOLEAN fgIsConnByBssidIssued;
	UINT_8 aucBSSID[MAC_ADDR_LEN];

	BOOLEAN fgIsConnReqIssued;
	BOOLEAN fgIsDisconnectedByNonRequest;
	ENUM_RECONNECT_LEVEL_T eReConnectLevel;

	UINT_8 ucSSIDLen;
	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];

	ENUM_PARAM_OP_MODE_T eOPMode;

	ENUM_PARAM_CONNECTION_POLICY_T eConnectionPolicy;

	ENUM_PARAM_AD_HOC_MODE_T eAdHocMode;

	ENUM_PARAM_AUTH_MODE_T eAuthMode;

	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;

	BOOLEAN fgIsScanReqIssued;

	
	UINT_16 u2BeaconPeriod;

	UINT_16 u2RTSThreshold;	

	UINT_16 u2DesiredNonHTRateSet;	

	UINT_8 ucAdHocChannelNum;	

	ENUM_BAND_T eAdHocBand;	

	UINT_32 u4FreqInKHz;	

	
	UINT_16 u2AtimWindow;

	
	BOOLEAN fgIsEnableRoaming;

	BOOLEAN fgIsAdHocQoSEnable;

	ENUM_PARAM_PHY_CONFIG_T eDesiredPhyConfig;

	
	UINT_16 u2CountryCode;
	UINT_16 u2CountryCodeBakup;
	UINT_8 uc2G4BandwidthMode;	
	UINT_8 uc5GBandwidthMode;	

	BOOLEAN fgTxShortGIDisabled;
	BOOLEAN fgRxShortGIDisabled;

#if CFG_SUPPORT_802_11D
	BOOLEAN fgMultiDomainCapabilityEnabled;
#endif				

#if 1				
	BOOLEAN fgWapiMode;
	UINT_32 u4WapiSelectedGroupCipher;
	UINT_32 u4WapiSelectedPairwiseCipher;
	UINT_32 u4WapiSelectedAKMSuite;
#endif

	
	
	BOOLEAN fgPrivacyCheckDisable;

	
	UINT_8 bmfgApsdEnAc;

	
	RSN_INFO_T rRsnInfo;

} CONNECTION_SETTINGS_T, *P_CONNECTION_SETTINGS_T;

struct _BSS_INFO_T {

	ENUM_PARAM_MEDIA_STATE_T eConnectionState;	
	ENUM_PARAM_MEDIA_STATE_T eConnectionStateIndicated;	

	ENUM_OP_MODE_T eCurrentOPMode;	
#if CFG_ENABLE_WIFI_DIRECT
	ENUM_OP_MODE_T eIntendOPMode;
#endif

	BOOLEAN fgIsNetActive;	

	UINT_8 ucNetTypeIndex;	

	UINT_8 ucReasonOfDisconnect;	

	UINT_8 ucSSIDLen;	

#if CFG_ENABLE_WIFI_DIRECT
	ENUM_HIDDEN_SSID_TYPE_T eHiddenSsidType;	
#endif

	UINT_8 aucSSID[ELEM_MAX_LEN_SSID];	

	UINT_8 aucBSSID[MAC_ADDR_LEN];	

	UINT_8 aucOwnMacAddr[MAC_ADDR_LEN];	

	P_STA_RECORD_T prStaRecOfAP;	
	LINK_T rStaRecOfClientList;	

	UINT_16 u2CapInfo;	

	UINT_16 u2BeaconInterval;	

	UINT_16 u2ATIMWindow;	

	UINT_16 u2AssocId;	

	UINT_8 ucDTIMPeriod;	

	UINT_8 ucDTIMCount;	

	UINT_8 ucPhyTypeSet;	

	UINT_8 ucNonHTBasicPhyType;	

	UINT_8 ucConfigAdHocAPMode;	

	UINT_8 ucBeaconTimeoutCount;	

	BOOLEAN fgHoldSameBssidForIBSS;	

	BOOLEAN fgIsBeaconActivated;	

	P_MSDU_INFO_T prBeacon;	

	BOOLEAN fgIsIBSSMaster;	

	BOOLEAN fgIsShortPreambleAllowed;	
	BOOLEAN fgUseShortPreamble;	
	BOOLEAN fgUseShortSlotTime;	

	UINT_16 u2OperationalRateSet;	
	UINT_16 u2BSSBasicRateSet;	

	UINT_8 ucAllSupportedRatesLen;	
	UINT_8 aucAllSupportedRates[RATE_NUM];

	UINT_8 ucAssocClientCnt;	

	BOOLEAN fgIsProtection;
	BOOLEAN fgIsQBSS;	
	BOOLEAN fgIsNetAbsent;	

	UINT_32 u4RsnSelectedGroupCipher;
	UINT_32 u4RsnSelectedPairwiseCipher;
	UINT_32 u4RsnSelectedAKMSuite;
	UINT_16 u2RsnSelectedCapInfo;

    
	
    
	PM_PROFILE_SETUP_INFO_T rPmProfSetupInfo;

    
	
    
	UINT_8 ucWmmParamSetCount;	

	AC_QUE_PARMS_T arACQueParms[WMM_AC_INDEX_NUM];

	UINT_8 aucCWminLog2ForBcast[WMM_AC_INDEX_NUM];	
	UINT_8 aucCWmaxLog2ForBcast[WMM_AC_INDEX_NUM];	
	AC_QUE_PARMS_T arACQueParmsForBcast[WMM_AC_INDEX_NUM];	

    
	
	
	
    
	ENUM_BAND_T eBand;
	UINT_8 ucPrimaryChannel;
	UINT_8 ucHtOpInfo1;
	UINT_16 u2HtOpInfo2;
	UINT_16 u2HtOpInfo3;

    
	
    
	BOOLEAN fgErpProtectMode;
	ENUM_HT_PROTECT_MODE_T eHtProtectMode;
	ENUM_GF_MODE_T eGfOperationMode;
	ENUM_RIFS_MODE_T eRifsOperationMode;

	BOOLEAN fgObssErpProtectMode;	
	ENUM_HT_PROTECT_MODE_T eObssHtProtectMode;	
	ENUM_GF_MODE_T eObssGfOperationMode;	
	BOOLEAN fgObssRifsOperationMode;	

    
	
	
    
	BOOLEAN fgAssoc40mBwAllowed;
	BOOLEAN fg40mBwAllowed;
	ENUM_CHNL_EXT_T eBssSCO;	
	UINT_8 auc2G_20mReqChnlList[CHNL_LIST_SZ_2G + 1];
	UINT_8 auc2G_NonHtChnlList[CHNL_LIST_SZ_2G + 1];
	UINT_8 auc2G_PriChnlList[CHNL_LIST_SZ_2G + 1];
	UINT_8 auc2G_SecChnlList[CHNL_LIST_SZ_2G + 1];

	UINT_8 auc5G_20mReqChnlList[CHNL_LIST_SZ_5G + 1];
	UINT_8 auc5G_NonHtChnlList[CHNL_LIST_SZ_5G + 1];
	UINT_8 auc5G_PriChnlList[CHNL_LIST_SZ_5G + 1];
	UINT_8 auc5G_SecChnlList[CHNL_LIST_SZ_5G + 1];

	TIMER_T rObssScanTimer;
	UINT_16 u2ObssScanInterval;	

	BOOLEAN fgObssActionForcedTo20M;	
	BOOLEAN fgObssBeaconForcedTo20M;	

    
	
    
	UINT_8 ucHwDefaultFixedRateCode;	
	UINT_16 u2HwLPWakeupGuardTimeUsec;

	UINT_8 ucBssFreeQuota;	
#if CFG_ENABLE_GTK_FRAME_FILTER
	P_IPV4_NETWORK_ADDRESS_LIST prIpV4NetAddrList;
#endif
	UINT_16 u2DeauthReason;

#if (CFG_SUPPORT_TDLS == 1)
	BOOLEAN fgTdlsIsProhibited;	
	BOOLEAN fgTdlsIsChSwProhibited;	
#endif				

#if CFG_SUPPORT_RN
	OS_SYSTIME	rConnTime;
	BOOLEAN		fgDisConnReassoc;
#endif
};

struct _AIS_SPECIFIC_BSS_INFO_T {
	UINT_8 ucRoamingAuthTypes;	

	BOOLEAN fgIsIBSSActive;

	
	BOOLEAN fgCounterMeasure;
	UINT_8 ucWEPDefaultKeyID;
	BOOLEAN fgTransmitKeyExist;	

	
	BOOLEAN fgCheckEAPoLTxDone;

	UINT_32 u4RsnaLastMICFailTime;

	
	
	TIMER_T rPreauthenticationTimer;

	TIMER_T rRsnaBlockTrafficTimer;
	TIMER_T rRsnaEAPoLReportTimeoutTimer;

	
	
	UINT_8 aucTxMicKey[8];
	UINT_8 aucRxMicKey[8];

	
	
	UINT_32 u4PmkidCandicateCount;
	PMKID_CANDICATE_T arPmkidCandicate[CFG_MAX_PMKID_CACHE];
	UINT_32 u4PmkidCacheCount;
	PMKID_ENTRY_T arPmkidCache[CFG_MAX_PMKID_CACHE];
	BOOLEAN fgIndicatePMKID;
#if CFG_SUPPORT_802_11W
	BOOLEAN fgMgmtProtection;
	UINT_32 u4SaQueryStart;
	UINT_32 u4SaQueryCount;
	UINT_8 ucSaQueryTimedOut;
	PUINT_8 pucSaQueryTransId;
	TIMER_T rSaQueryTimer;
	BOOLEAN fgBipKeyInstalled;
#endif
};

struct _BOW_SPECIFIC_BSS_INFO_T {
	UINT_16 u2Reserved;	
};

#if CFG_SLT_SUPPORT
typedef struct _SLT_INFO_T {

	P_BSS_DESC_T prPseudoBssDesc;
	UINT_16 u2SiteID;
	UINT_8 ucChannel2G4;
	UINT_8 ucChannel5G;
	BOOLEAN fgIsDUT;
	UINT_32 u4BeaconReceiveCnt;
	
	P_STA_RECORD_T prPseudoStaRec;
} SLT_INFO_T, *P_SLT_INFO_T;
#endif

typedef struct _WIFI_VAR_T {
	BOOLEAN fgIsRadioOff;

	BOOLEAN fgIsEnterD3ReqIssued;

	BOOLEAN fgDebugCmdResp;

	CONNECTION_SETTINGS_T rConnSettings;

	SCAN_INFO_T rScanInfo;

#if CFG_SUPPORT_ROAMING
	ROAMING_INFO_T rRoamingInfo;
#endif				

	AIS_FSM_INFO_T rAisFsmInfo;

	ENUM_PWR_STATE_T aePwrState[NETWORK_TYPE_INDEX_NUM];

	BSS_INFO_T arBssInfo[NETWORK_TYPE_INDEX_NUM];

	AIS_SPECIFIC_BSS_INFO_T rAisSpecificBssInfo;

#if CFG_ENABLE_WIFI_DIRECT
	P_P2P_CONNECTION_SETTINGS_T prP2PConnSettings;

	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo;

	P_P2P_FSM_INFO_T prP2pFsmInfo;
#endif				

#if CFG_ENABLE_BT_OVER_WIFI
	BOW_SPECIFIC_BSS_INFO_T rBowSpecificBssInfo;
	BOW_FSM_INFO_T rBowFsmInfo;
#endif				

	DEAUTH_INFO_T arDeauthInfo[MAX_DEAUTH_INFO_COUNT];

	
	UINT_8 aucPermanentAddress[MAC_ADDR_LEN];
	UINT_8 aucMacAddress[MAC_ADDR_LEN];
	UINT_8 aucDeviceAddress[MAC_ADDR_LEN];
	UINT_8 aucInterfaceAddress[MAC_ADDR_LEN];

	UINT_8 ucAvailablePhyTypeSet;

	ENUM_PHY_TYPE_INDEX_T eNonHTBasicPhyType2G4;	

	ENUM_PARAM_PREAMBLE_TYPE_T ePreambleType;
	ENUM_REGISTRY_FIXED_RATE_T eRateSetting;

	BOOLEAN fgIsShortSlotTimeOptionEnable;
	

	BOOLEAN fgEnableJoinToHiddenSSID;
	BOOLEAN fgSupportWZCDisassociation;

	BOOLEAN fgSupportQoS;
	BOOLEAN fgSupportAmpduTx;
	BOOLEAN fgSupportAmpduRx;
	BOOLEAN fgSupportTspec;
	BOOLEAN fgSupportUAPSD;
	BOOLEAN fgSupportULPSMP;
	UINT_8 u8SupportRxSgi20;	
	UINT_8 u8SupportRxSgi40;
	UINT_8 u8SupportRxGf;
	UINT_8 u8SupportRxSTBC;
#if CFG_SUPPORT_CFG_FILE
	UINT_8 ucApWpsMode;
	UINT_8 ucCert11nMode;
#endif
#if CFG_SUPPORT_CE_FCC_TXPWR_LIMIT
	UINT_8 ucCeFccTxPwrLimit;
	UINT_8 ucCeFccTxPwrLimitCck;
	UINT_8 ucCeFccTxPwrLimitOfdmHt20;
	UINT_8 ucCeFccTxPwrLimitHt40;
#endif

#if CFG_SLT_SUPPORT
	SLT_INFO_T rSltInfo;
#endif

#if CFG_SUPPORT_HOTSPOT_2_0
	HS20_INFO_T rHS20Info;
#endif
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
	PARAM_GET_CHN_LOAD rChnLoadInfo;
#endif

} WIFI_VAR_T, *P_WIFI_VAR_T;	

typedef struct {
	LINK_T rLinkHead;
	OS_SYSTIME rNextExpiredSysTime;
	KAL_WAKE_LOCK_T rWakeLock;
	BOOLEAN fgWakeLocked;
} ROOT_TIMER, *P_ROOT_TIMER;

typedef struct {

	
	UINT_16 u2Part1CfgOwnVersion;
	UINT_16 u2Part1CfgPeerVersion;
	UINT_16 u2Part2CfgOwnVersion;
	UINT_16 u2Part2CfgPeerVersion;

	
	UINT_16 u2FwProductID;
	UINT_16 u2FwOwnVersion;
	UINT_16 u2FwPeerVersion;

} WIFI_VER_INFO_T, *P_WIFI_VER_INFO_T;

#if CFG_ENABLE_WIFI_DIRECT

typedef struct _P2P_FUNCTION_LINKER {
	P2P_REMOVE prP2pRemove;
	P2P_GENERATE_P2P_IE prP2pGenerateWSC_IEForBeacon;
	P2P_NET_REGISTER prP2pNetRegister;
	P2P_NET_UNREGISTER prP2pNetUnregister;
	P2P_CALCULATE_P2P_IE_LEN prP2pCalculateP2p_IELenForAssocReq;	
	P2P_GENERATE_P2P_IE prP2pGenerateP2p_IEForAssocReq;	
} P2P_FUNCTION_LINKER, *P_P2P_FUNCTION_LINKER;

#endif

struct PERF_MONITOR_T {
	TIMER_T rPerfMonTimer;
	ULONG ulPerfMonFlag;
	ULONG ulLastTxBytes;
	ULONG ulLastRxBytes;
	ULONG ulP2PLastTxBytes;
	ULONG ulP2PLastRxBytes;
	ULONG ulThroughput; 
	UINT32 u4UpdatePeriod; 
	UINT32 u4TarPerfLevel;
	UINT32 u4CurrPerfLevel;
};

struct _ADAPTER_T {
	UINT_8 ucRevID;

	UINT_16 u2NicOpChnlNum;

	BOOLEAN fgIsEnableWMM;
	BOOLEAN fgIsWmmAssoc;	

	UINT_32 u4OsPacketFilter;	

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	UINT_32 u4CSUMFlags;
#endif				

	ENUM_BAND_T aePreferBand[NETWORK_TYPE_INDEX_NUM];

	ENUM_BAND_T aeSetBand[NETWORK_TYPE_INDEX_NUM];

	
	UINT_32 u4Flags;
	UINT_32 u4HwFlags;

	BOOLEAN fgIsRadioOff;

	BOOLEAN fgIsEnterD3ReqIssued;

	UINT_8 aucMacAddress[MAC_ADDR_LEN];

	ENUM_PHY_TYPE_INDEX_T eCurrentPhyType;	

#if CFG_COALESCING_BUFFER_SIZE || CFG_SDIO_RX_AGG
	UINT_32 u4CoalescingBufCachedSize;
	PUINT_8 pucCoalescingBufCached;
#endif				

	
	BUF_INFO_T rMgtBufInfo;
	BUF_INFO_T rMsgBufInfo;
	PUINT_8 pucMgtBufCached;
	UINT_32 u4MgtBufCachedSize;
	UINT_8 aucMsgBuf[MSG_BUFFER_SIZE];
#if CFG_DBG_MGT_BUF
	UINT_32 u4MemAllocDynamicCount;	
	UINT_32 u4MemFreeDynamicCount;	
#endif

	STA_RECORD_T arStaRec[CFG_STA_REC_NUM];

	
	TX_CTRL_T rTxCtrl;
	QUE_T rFreeCmdList;
	CMD_INFO_T arHifCmdDesc[CFG_TX_MAX_CMD_PKT_NUM];

	
	RX_CTRL_T rRxCtrl;

	P_SDIO_CTRL_T prSDIOCtrl;

#if (MT6620_E1_ASIC_HIFSYS_WORKAROUND == 1)
	
	BOOLEAN fgIsClockGatingEnabled;
#endif

	
	
	
	UINT_8 aucIndicationEventBuffer[(CFG_MAX_PMKID_CACHE * 20) + 8];

	UINT_32 u4IntStatus;

	ENUM_ACPI_STATE_T rAcpiState;

	BOOLEAN fgIsIntEnable;
	BOOLEAN fgIsIntEnableWithLPOwnSet;

	BOOLEAN fgIsFwOwn;
	BOOLEAN fgWiFiInSleepyState;

	UINT_32 u4PwrCtrlBlockCnt;

	QUE_T rPendingCmdQueue;

	P_GLUE_INFO_T prGlueInfo;

	UINT_8 ucCmdSeqNum;
	UINT_8 ucTxSeqNum;

#if 1				
	BOOLEAN fgUseWapi;
#endif

	
	BOOLEAN fgTestMode;

	
	WLAN_INFO_T rWlanInfo;

#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2PRegistered;
	ENUM_NET_REG_STATE_T rP2PNetRegState;
	BOOLEAN fgIsWlanLaunched;
	P_P2P_INFO_T prP2pInfo;
#if CFG_SUPPORT_P2P_RSSI_QUERY
	OS_SYSTIME rP2pLinkQualityUpdateTime;
	BOOLEAN fgIsP2pLinkQualityValid;
	EVENT_LINK_QUALITY rP2pLinkQuality;
#endif

	
	TIMER_T rP2pFsmTimeoutTimer;
#endif

	
	BOOLEAN fgEnOnlineScan;

	
	BOOLEAN fgDisBcnLostDetection;

	
	PARAM_MAC_ADDRESS rMyMacAddr;

	
	UINT_32 u4WakeupEventEnable;

	
	EVENT_STATISTICS rStatStruct;
	OS_SYSTIME rStatUpdateTime;
	BOOLEAN fgIsStatValid;

	EVENT_LINK_QUALITY rLinkQuality;
	OS_SYSTIME rLinkQualityUpdateTime;
	BOOLEAN fgIsLinkQualityValid;
	OS_SYSTIME rLinkRateUpdateTime;
	BOOLEAN fgIsLinkRateValid;

	
	WIFI_VAR_T rWifiVar;

	
	IEEE_802_11_MIB_T rMib;

	
	MBOX_T arMbox[MBOX_ID_TOTAL_NUM];

	
	TIMER_T rOidTimeoutTimer;

	TIMER_T rReturnIndicatedRfbListTimer;

	
	ROOT_TIMER rRootTimer;

	
	ENUM_CHNL_EXT_T eRfSco;
	ENUM_SYS_PROTECT_MODE_T eSysProtectMode;
	ENUM_GF_MODE_T eSysHtGfMode;
	ENUM_RIFS_MODE_T eSysTxRifsMode;
	ENUM_SYS_PCO_PHASE_T eSysPcoPhase;

	P_DOMAIN_INFO_ENTRY prDomainInfo;

	
	QUE_MGT_T rQM;

	CNM_INFO_T rCnmInfo;

	UINT_32 u4PowerMode;

	UINT_32 u4CtiaPowerMode;
	BOOLEAN fgEnCtiaPowerMode;

	UINT_32 fgEnArpFilter;

	UINT_32 u4UapsdAcBmp;

	UINT_32 u4MaxSpLen;

	UINT_32 u4PsCurrentMeasureEn;

	
	WIFI_VER_INFO_T rVerInfo;

	
	BOOLEAN fgIsHw5GBandDisabled;
	BOOLEAN fgEnable5GBand;
	BOOLEAN fgIsEepromUsed;
	BOOLEAN fgIsEfuseValid;
	BOOLEAN fgIsEmbbededMacAddrValid;

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
	BOOLEAN fgIsPowerLimitTableValid;
#endif

	
	INT_32 i4PendingFwdFrameCount;

#if CFG_SUPPORT_RDD_TEST_MODE
	UINT_8 ucRddStatus;
#endif

	BOOLEAN fgDisStaAgingTimeoutDetection;
#if CFG_SUPPORT_CFG_FILE
	P_WLAN_CFG_T prWlanCfg;
	WLAN_CFG_T rWlanCfg;
#endif
#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
	KAL_WAKE_LOCK_T rApWakeLock;
#endif
	UINT_32 u4FwCompileFlag0;
	UINT_32 u4FwCompileFlag1;
	KAL_WAKE_LOCK_T rTxThreadWakeLock;
	KAL_WAKE_LOCK_T rAhbIsrWakeLock;

#if CFG_SUPPORT_ROAMING_ENC
	BOOLEAN fgIsRoamingEncEnabled;
#endif				

#if (CFG_SUPPORT_TDLS == 1)
	BOOLEAN fgTdlsIsSup;
#endif				

	UINT_8 ucScanTime;

#if CFG_SUPPORT_DBG_POWERMODE
	BOOLEAN fgEnDbgPowerMode;	
#endif

	UINT_32 u4AirDelayTotal;	
	ULONG	ulSuspendFlag;
	UINT_8 ucFlushCount;	
	struct PERF_MONITOR_T rPerMonitor;
};				



#define SUSPEND_FLAG_FOR_WAKEUP_REASON	(0)
#define SUSPEND_FLAG_CLEAR_WHEN_RESUME	(1)

#define IS_NET_ACTIVE(_prAdapter, _NetTypeIndex) \
		(_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)].fgIsNetActive)
#define IS_BSS_ACTIVE(_prBssInfo)     ((_prBssInfo)->fgIsNetActive)

#define IS_AIS_ACTIVE(_prAdapter)     IS_NET_ACTIVE(_prAdapter, NETWORK_TYPE_AIS_INDEX)
#define IS_P2P_ACTIVE(_prAdapter)     IS_NET_ACTIVE(_prAdapter, NETWORK_TYPE_P2P_INDEX)
#define IS_BOW_ACTIVE(_prAdapter)     IS_NET_ACTIVE(_prAdapter, NETWORK_TYPE_BOW_INDEX)

#define SET_NET_ACTIVE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)].fgIsNetActive = TRUE; }

#define UNSET_NET_ACTIVE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)].fgIsNetActive = FALSE; }

#define BSS_INFO_INIT(_prAdapter, _NetTypeIndex) \
		{   UINT_8 _aucZeroMacAddr[] = NULL_MAC_ADDR; \
		    P_BSS_INFO_T _prBssInfo = &(_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)]); \
		    \
		    _prBssInfo->eConnectionState = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eConnectionStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE; \
		    _prBssInfo->fgIsNetActive = FALSE; \
		    _prBssInfo->ucNetTypeIndex = (_NetTypeIndex); \
		    _prBssInfo->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED; \
		    COPY_MAC_ADDR(_prBssInfo->aucBSSID, _aucZeroMacAddr); \
		    LINK_INITIALIZE(&_prBssInfo->rStaRecOfClientList); \
		    _prBssInfo->fgIsBeaconActivated = FALSE; \
		    _prBssInfo->ucHwDefaultFixedRateCode = RATE_CCK_1M_LONG; \
		    _prBssInfo->fgIsNetAbsent = FALSE; \
		}

#if CFG_ENABLE_BT_OVER_WIFI
#define BOW_BSS_INFO_INIT(_prAdapter, _NetTypeIndex) \
		{  \
		    P_BSS_INFO_T _prBssInfo = &(_prAdapter->rWifiVar.arBssInfo[(_NetTypeIndex)]); \
		    \
		    _prBssInfo->eConnectionState = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eConnectionStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED; \
		    _prBssInfo->eCurrentOPMode = OP_MODE_BOW; \
		    _prBssInfo->ucNetTypeIndex = (_NetTypeIndex); \
		    _prBssInfo->ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED; \
		    LINK_INITIALIZE(&_prBssInfo->rStaRecOfClientList); \
		    _prBssInfo->fgIsBeaconActivated = TRUE; \
		    _prBssInfo->ucHwDefaultFixedRateCode = RATE_CCK_1M_LONG; \
		    _prBssInfo->fgIsNetAbsent = FALSE; \
		}
#endif

#define PERF_MON_DISABLE_BIT    (0)
#define PERF_MON_STOP_BIT       (1)
#define PERF_MON_RUNNING_BIT    (2)

#define THROUGHPUT_L1_THRESHOLD		(20*1024*1024)
#define THROUGHPUT_L2_THRESHOLD		(60*1024*1024)
#define THROUGHPUT_L3_THRESHOLD		(135*1024*1024)

#define SET_NET_PWR_STATE_IDLE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] = PWR_STATE_IDLE; }

#define SET_NET_PWR_STATE_ACTIVE(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] = PWR_STATE_ACTIVE; }

#define SET_NET_PWR_STATE_PS(_prAdapter, _NetTypeIndex) \
	{_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] = PWR_STATE_PS; }

#define IS_NET_PWR_STATE_ACTIVE(_prAdapter, _NetTypeIndex) \
	(_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] == PWR_STATE_ACTIVE)

#define IS_NET_PWR_STATE_IDLE(_prAdapter, _NetTypeIndex) \
	(_prAdapter->rWifiVar.aePwrState[(_NetTypeIndex)] == PWR_STATE_IDLE)

#define IS_SCN_PWR_STATE_ACTIVE(_prAdapter) \
	(_prAdapter->rWifiVar.rScanInfo.eScanPwrState == SCAN_PWR_STATE_ACTIVE)

#define IS_SCN_PWR_STATE_IDLE(_prAdapter) \
	(_prAdapter->rWifiVar.rScanInfo.eScanPwrState == SCAN_PWR_STATE_IDLE)



#endif 
