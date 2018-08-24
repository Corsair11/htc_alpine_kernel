



#include "precomp.h"

#define REPLICATED_BEACON_TIME_THRESHOLD        (3000)
#define REPLICATED_BEACON_FRESH_PERIOD          (10000)
#define REPLICATED_BEACON_STRENGTH_THRESHOLD    (32)

#define ROAMING_NO_SWING_RCPI_STEP              (10)

#define RSSI_HI_5GHZ	(-60)
#define RSSI_MED_5GHZ	(-70)
#define RSSI_LO_5GHZ	(-80)

#define PREF_HI_5GHZ	(20)
#define PREF_MED_5GHZ	(15)
#define PREF_LO_5GHZ	(10)

INT_32 rssiRangeHi = RSSI_HI_5GHZ;
INT_32 rssiRangeMed = RSSI_MED_5GHZ;
INT_32 rssiRangeLo = RSSI_LO_5GHZ;
UINT_8 pref5GhzHi = PREF_HI_5GHZ;
UINT_8 pref5GhzMed = PREF_MED_5GHZ;
UINT_8 pref5GhzLo = PREF_LO_5GHZ;






VOID scnInit(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_BSS_DESC_T prBSSDesc;
	PUINT_8 pucBSSBuff;
	UINT_32 i;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	pucBSSBuff = &prScanInfo->aucScanBuffer[0];

	DBGLOG(SCN, INFO, "->scnInit()\n");

	
	prScanInfo->eCurrentState = SCAN_STATE_IDLE;

	prScanInfo->rLastScanCompletedTime = (OS_SYSTIME) 0;

	LINK_INITIALIZE(&prScanInfo->rPendingMsgList);

	
	kalMemZero((PVOID) pucBSSBuff, SCN_MAX_BUFFER_SIZE);

	LINK_INITIALIZE(&prScanInfo->rFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rBSSDescList);

	for (i = 0; i < CFG_MAX_NUM_BSS_LIST; i++) {

		prBSSDesc = (P_BSS_DESC_T) pucBSSBuff;

		LINK_INSERT_TAIL(&prScanInfo->rFreeBSSDescList, &prBSSDesc->rLinkEntry);

		pucBSSBuff += ALIGN_4(sizeof(BSS_DESC_T));
	}
	
	ASSERT(((ULONG) pucBSSBuff - (ULONG)&prScanInfo->aucScanBuffer[0]) == SCN_MAX_BUFFER_SIZE);

	
	prScanInfo->fgIsSparseChannelValid = FALSE;

	
	prScanInfo->fgNloScanning = FALSE;
	prScanInfo->fgPscnOnnning = FALSE;

	
	prScanInfo->fgIsPostponeSchedScan = FALSE;

	prScanInfo->prPscnParam = kalMemAlloc(sizeof(PSCN_PARAM_T), VIR_MEM_TYPE);
	if (prScanInfo->prPscnParam)
		kalMemZero(prScanInfo->prPscnParam, sizeof(PSCN_PARAM_T));

	prScanInfo->fgRptDisconnectPostponedToScanDone = FALSE;

}				

VOID scnUninit(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	DBGLOG(SCN, INFO, "->scnUninit()\n");

	
	prScanInfo->eCurrentState = SCAN_STATE_IDLE;

	prScanInfo->rLastScanCompletedTime = (OS_SYSTIME) 0;

	

	
	LINK_INITIALIZE(&prScanInfo->rFreeBSSDescList);
	LINK_INITIALIZE(&prScanInfo->rBSSDescList);

	kalMemFree(prScanInfo->prPscnParam, VIR_MEM_TYPE, sizeof(PSCN_PARAM_T));

}				

P_BSS_DESC_T scanSearchBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	return scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID, FALSE, NULL);
}

P_BSS_DESC_T
scanSearchBssDescByBssidAndSsid(IN P_ADAPTER_T prAdapter,
				IN UINT_8 aucBSSID[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_DESC_T prDstBssDesc = (P_BSS_DESC_T) NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			if (fgCheckSsid == FALSE || prSsid == NULL)
				return prBssDesc;

			if (EQUAL_SSID(prBssDesc->aucSSID,
				       prBssDesc->ucSSIDLen, prSsid->aucSsid, prSsid->u4SsidLen)) {
				return prBssDesc;
			} else if (prDstBssDesc == NULL && prBssDesc->fgIsHiddenSSID == TRUE) {
				prDstBssDesc = prBssDesc;
			} else {
				COPY_SSID(prBssDesc->aucSSID,
					  prBssDesc->ucSSIDLen, prSsid->aucSsid, prSsid->u4SsidLen);
				return prBssDesc;
			}
		}
	}

	return prDstBssDesc;

}				

P_BSS_DESC_T scanSearchBssDescByTA(IN P_ADAPTER_T prAdapter, IN UINT_8 aucSrcAddr[])
{
	return scanSearchBssDescByTAAndSsid(prAdapter, aucSrcAddr, FALSE, NULL);
}

P_BSS_DESC_T
scanSearchBssDescByTAAndSsid(IN P_ADAPTER_T prAdapter,
			     IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_DESC_T prDstBssDesc = (P_BSS_DESC_T) NULL;

	ASSERT(prAdapter);
	ASSERT(aucSrcAddr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucSrcAddr, aucSrcAddr)) {
			if (fgCheckSsid == FALSE || prSsid == NULL)
				return prBssDesc;

			if (EQUAL_SSID(prBssDesc->aucSSID,
				       prBssDesc->ucSSIDLen, prSsid->aucSsid, prSsid->u4SsidLen)) {
				return prBssDesc;
			} else if (prDstBssDesc == NULL && prBssDesc->fgIsHiddenSSID == TRUE) {
				prDstBssDesc = prBssDesc;
			}

		}
	}

	return prDstBssDesc;

}				

#if CFG_SUPPORT_HOTSPOT_2_0
P_BSS_DESC_T scanSearchBssDescByBssidAndLatestUpdateTime(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_BSS_DESC_T prDstBssDesc = (P_BSS_DESC_T) NULL;
	OS_SYSTIME rLatestUpdateTime = 0;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			if (!rLatestUpdateTime || CHECK_FOR_EXPIRATION(prBssDesc->rUpdateTime, rLatestUpdateTime)) {
				prDstBssDesc = prBssDesc;
				COPY_SYSTIME(rLatestUpdateTime, prBssDesc->rUpdateTime);
			}
		}
	}

	return prDstBssDesc;

}				
#endif

P_BSS_DESC_T
scanSearchExistingBssDesc(IN P_ADAPTER_T prAdapter,
			  IN ENUM_BSS_TYPE_T eBSSType, IN UINT_8 aucBSSID[], IN UINT_8 aucSrcAddr[])
{
	return scanSearchExistingBssDescWithSsid(prAdapter, eBSSType, aucBSSID, aucSrcAddr, FALSE, NULL);
}

P_BSS_DESC_T
scanSearchExistingBssDescWithSsid(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BSS_TYPE_T eBSSType,
				  IN UINT_8 aucBSSID[],
				  IN UINT_8 aucSrcAddr[], IN BOOLEAN fgCheckSsid, IN P_PARAM_SSID_T prSsid)
{
	P_SCAN_INFO_T prScanInfo;
	P_BSS_DESC_T prBssDesc, prIBSSBssDesc;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;


	ASSERT(prAdapter);
	ASSERT(aucSrcAddr);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	switch (eBSSType) {
	case BSS_TYPE_P2P_DEVICE:
		fgCheckSsid = FALSE;
	case BSS_TYPE_INFRASTRUCTURE:
	case BSS_TYPE_BOW_DEVICE:
		{
			prBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID, fgCheckSsid, prSsid);

			

			return prBssDesc;
		}

	case BSS_TYPE_IBSS:
		{
			prIBSSBssDesc = scanSearchBssDescByBssidAndSsid(prAdapter, aucBSSID, fgCheckSsid, prSsid);
			prBssDesc = scanSearchBssDescByTAAndSsid(prAdapter, aucSrcAddr, fgCheckSsid, prSsid);

			if (prBssDesc) {
				if ((!prIBSSBssDesc) ||	
				    (prBssDesc == prIBSSBssDesc)) {	

					return prBssDesc;
				}	

				prBSSDescList = &prScanInfo->rBSSDescList;
				prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

				
				LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);

				
				LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);

				return prIBSSBssDesc;
			}

			if (prIBSSBssDesc) {	

				return prIBSSBssDesc;
			}
			
			break;	
		}

	default:
		break;
	}

	return (P_BSS_DESC_T) NULL;

}				

VOID scanRemoveBssDescsByPolicy(IN P_ADAPTER_T prAdapter, IN UINT_32 u4RemovePolicy)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc;

	ASSERT(prAdapter);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	
	

	if (u4RemovePolicy & SCN_RM_POLICY_TIMEOUT) {
		P_BSS_DESC_T prBSSDescNext;
		OS_SYSTIME rCurrentTime;

		GET_CURRENT_SYSTIME(&rCurrentTime);

		
		LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				
				continue;
			}

			if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
					      SEC_TO_SYSTIME(SCN_BSS_DESC_REMOVE_TIMEOUT_SEC))) {

				

				
				LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);

				
				LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);
			}
		}
	} else if (u4RemovePolicy & SCN_RM_POLICY_OLDEST_HIDDEN) {
		P_BSS_DESC_T prBssDescOldest = (P_BSS_DESC_T) NULL;

		
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				
				continue;
			}

			if (!prBssDesc->fgIsHiddenSSID)
				continue;

			if (!prBssDescOldest) {	
				prBssDescOldest = prBssDesc;
				continue;
			}

			if (TIME_BEFORE(prBssDesc->rUpdateTime, prBssDescOldest->rUpdateTime))
				prBssDescOldest = prBssDesc;
		}

		if (prBssDescOldest) {

			

			
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDescOldest);

			
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDescOldest->rLinkEntry);
		}
	} else if (u4RemovePolicy & SCN_RM_POLICY_SMART_WEAKEST) {
		P_BSS_DESC_T prBssDescWeakest = (P_BSS_DESC_T) NULL;
		P_BSS_DESC_T prBssDescWeakestSameSSID = (P_BSS_DESC_T) NULL;
		UINT_32 u4SameSSIDCount = 0;

		
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				
				continue;
			}

			if ((!prBssDesc->fgIsHiddenSSID) &&
			    (EQUAL_SSID(prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen))) {

				u4SameSSIDCount++;

				if (!prBssDescWeakestSameSSID)
					prBssDescWeakestSameSSID = prBssDesc;
				else if (prBssDesc->ucRCPI < prBssDescWeakestSameSSID->ucRCPI)
					prBssDescWeakestSameSSID = prBssDesc;
				if (u4SameSSIDCount < SCN_BSS_DESC_SAME_SSID_THRESHOLD)
					continue;
			}

			if (!prBssDescWeakest) {	
				prBssDescWeakest = prBssDesc;
				continue;
			}

			if (prBssDesc->ucRCPI < prBssDescWeakest->ucRCPI)
				prBssDescWeakest = prBssDesc;

		}

		if ((u4SameSSIDCount >= SCN_BSS_DESC_SAME_SSID_THRESHOLD) && (prBssDescWeakestSameSSID))
			prBssDescWeakest = prBssDescWeakestSameSSID;

		if (prBssDescWeakest) {

			
			

			
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDescWeakest);

			
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDescWeakest->rLinkEntry);
		}
	} else if (u4RemovePolicy & SCN_RM_POLICY_ENTIRE) {
		P_BSS_DESC_T prBSSDescNext;

		LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {

			if ((u4RemovePolicy & SCN_RM_POLICY_EXCLUDE_CONNECTED) &&
			    (prBssDesc->fgIsConnected || prBssDesc->fgIsConnecting)) {
				
				continue;
			}

			
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);

			
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);
		}

	}

	return;

}				

VOID scanRemoveBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prBSSDescNext;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {

			
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);

			
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);

			
		}
	}

}				

VOID
scanRemoveBssDescByBandAndNetwork(IN P_ADAPTER_T prAdapter,
				  IN ENUM_BAND_T eBand, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prBSSDescNext;
	BOOLEAN fgToRemove;
	DBGLOG(SCN, INFO, "scanRemoveBssDescByBandAndNetwork()\n");

	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(eNetTypeIndex <= NETWORK_TYPE_INDEX_NUM);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;
	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	if (eBand == BAND_NULL)
		return;		

	
	LINK_FOR_EACH_ENTRY_SAFE(prBssDesc, prBSSDescNext, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		fgToRemove = FALSE;

		if (prBssDesc->eBand == eBand) {
			switch (eNetTypeIndex) {
			case NETWORK_TYPE_AIS_INDEX:
				if ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE)
				    || (prBssDesc->eBSSType == BSS_TYPE_IBSS)) {
					fgToRemove = TRUE;
				}
				break;

			case NETWORK_TYPE_P2P_INDEX:
				if (prBssDesc->eBSSType == BSS_TYPE_P2P_DEVICE)
					fgToRemove = TRUE;
				break;

			case NETWORK_TYPE_BOW_INDEX:
				if (prBssDesc->eBSSType == BSS_TYPE_BOW_DEVICE)
					fgToRemove = TRUE;
				break;

			default:
				ASSERT(0);
				break;
			}
		}

		if (fgToRemove == TRUE) {
			
			LINK_REMOVE_KNOWN_ENTRY(prBSSDescList, prBssDesc);

			
			LINK_INSERT_TAIL(prFreeBSSDescList, &prBssDesc->rLinkEntry);
		}
	}

}				

VOID scanRemoveConnFlagOfBssDescByBssid(IN P_ADAPTER_T prAdapter, IN UINT_8 aucBSSID[])
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;

	ASSERT(prAdapter);
	ASSERT(aucBSSID);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, aucBSSID)) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;

			
		}
	}

	return;

}				

P_BSS_DESC_T scanAllocateBssDesc(IN P_ADAPTER_T prAdapter)
{
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prFreeBSSDescList;
	P_BSS_DESC_T prBssDesc;

	ASSERT(prAdapter);
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prFreeBSSDescList = &prScanInfo->rFreeBSSDescList;

	LINK_REMOVE_HEAD(prFreeBSSDescList, prBssDesc, P_BSS_DESC_T);

	if (prBssDesc) {
		P_LINK_T prBSSDescList;

		kalMemZero(prBssDesc, sizeof(BSS_DESC_T));

#if CFG_ENABLE_WIFI_DIRECT
		LINK_INITIALIZE(&(prBssDesc->rP2pDeviceList));
		prBssDesc->fgIsP2PPresent = FALSE;
#endif 

		prBSSDescList = &prScanInfo->rBSSDescList;

		LINK_INSERT_TAIL(prBSSDescList, &prBssDesc->rLinkEntry);
	}

	return prBssDesc;

}				

P_BSS_DESC_T scanAddToBssDesc(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_BSS_DESC_T prBssDesc = NULL;
	UINT_16 u2CapInfo;
	ENUM_BSS_TYPE_T eBSSType = BSS_TYPE_INFRASTRUCTURE;

	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;

	P_WLAN_BEACON_FRAME_T prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) NULL;
	P_IE_SSID_T prIeSsid = (P_IE_SSID_T) NULL;
	P_IE_SUPPORTED_RATE_T prIeSupportedRate = (P_IE_SUPPORTED_RATE_T) NULL;
	P_IE_EXT_SUPPORTED_RATE_T prIeExtSupportedRate = (P_IE_EXT_SUPPORTED_RATE_T) NULL;
	P_HIF_RX_HEADER_T prHifRxHdr;
	UINT_8 ucHwChannelNum = 0;
	UINT_8 ucIeDsChannelNum = 0;
	UINT_8 ucIeHtChannelNum = 0;
	BOOLEAN fgIsValidSsid = FALSE, fgEscape = FALSE;
	PARAM_SSID_T rSsid;
	UINT_64 u8Timestamp;
	BOOLEAN fgIsNewBssDesc = FALSE;

	UINT_32 i;
	UINT_8 ucSSIDChar;

	UINT_8 ucOuiType;
	UINT_16 u2SubTypeVersion;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) prSwRfb->pvHeader;

	WLAN_GET_FIELD_16(&prWlanBeaconFrame->u2CapInfo, &u2CapInfo);
	WLAN_GET_FIELD_64(&prWlanBeaconFrame->au4Timestamp[0], &u8Timestamp);

	
	switch (u2CapInfo & CAP_INFO_BSS_TYPE) {
	case CAP_INFO_ESS:
		
		eBSSType = BSS_TYPE_INFRASTRUCTURE;
		break;

	case CAP_INFO_IBSS:
		eBSSType = BSS_TYPE_IBSS;
		break;
	case 0:
		eBSSType = BSS_TYPE_P2P_DEVICE;
		break;

#if CFG_ENABLE_BT_OVER_WIFI
		
#endif

	default:
		 DBGLOG(SCN, ERROR, "wrong bss type %d\n", (INT_32)(u2CapInfo & CAP_INFO_BSS_TYPE));
		return NULL;
	}

	
	pucIE = prWlanBeaconFrame->aucInfoElem;
	u2IELength = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (UINT_16) OFFSET_OF(WLAN_BEACON_FRAME_BODY_T, aucInfoElem[0]);

	if (u2IELength > CFG_IE_BUFFER_SIZE)
		u2IELength = CFG_IE_BUFFER_SIZE;
	kalMemZero(&rSsid, sizeof(rSsid));
	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID) {
				ucSSIDChar = '\0';

				
				if (IE_LEN(pucIE) == 0)
					fgIsValidSsid = FALSE;
				
				else {
					for (i = 0; i < IE_LEN(pucIE); i++)
						ucSSIDChar |= SSID_IE(pucIE)->aucSSID[i];

					if (ucSSIDChar)
						fgIsValidSsid = TRUE;
				}

				
				if (fgIsValidSsid == TRUE) {
					COPY_SSID(rSsid.aucSsid,
						  rSsid.u4SsidLen, SSID_IE(pucIE)->aucSSID, SSID_IE(pucIE)->ucLength);
				}
			}
			fgEscape = TRUE;
			break;
		default:
			break;
		}

		if (fgEscape == TRUE)
			break;
	}
	if (fgIsValidSsid)
		DBGLOG(SCN, EVENT, "%s %pM channel %d\n", rSsid.aucSsid, prWlanBeaconFrame->aucBSSID,
				HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr));
	else
		DBGLOG(SCN, EVENT, "hidden ssid, %pM channel %d\n", prWlanBeaconFrame->aucBSSID,
				HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr));
	
	prBssDesc = scanSearchExistingBssDescWithSsid(prAdapter,
						      eBSSType,
						      (PUINT_8) prWlanBeaconFrame->aucBSSID,
						      (PUINT_8) prWlanBeaconFrame->aucSrcAddr,
						      fgIsValidSsid, fgIsValidSsid == TRUE ? &rSsid : NULL);

	if (prBssDesc == (P_BSS_DESC_T) NULL) {
		fgIsNewBssDesc = TRUE;

		do {
			
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			
			scanRemoveBssDescsByPolicy(prAdapter,
						   (SCN_RM_POLICY_EXCLUDE_CONNECTED | SCN_RM_POLICY_OLDEST_HIDDEN));

			
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			
			scanRemoveBssDescsByPolicy(prAdapter,
						   (SCN_RM_POLICY_EXCLUDE_CONNECTED | SCN_RM_POLICY_SMART_WEAKEST));

			
			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (prBssDesc)
				break;
			
			DBGLOG(SCN, ERROR, "no bss desc available after remove policy\n");
			return NULL;

		} while (FALSE);

	} else {
		OS_SYSTIME rCurrentTime;

		
		
		

		GET_CURRENT_SYSTIME(&rCurrentTime);

		if (prBssDesc->eBSSType != eBSSType) {
			prBssDesc->eBSSType = eBSSType;
		} else if (HIF_RX_HDR_GET_CHNL_NUM(prSwRfb->prHifRxHdr) != prBssDesc->ucChannelNum &&
			   prBssDesc->ucRCPI > prSwRfb->prHifRxHdr->ucRcpi) {
			
			if ((prBssDesc->ucRCPI - prSwRfb->prHifRxHdr->ucRcpi) >= REPLICATED_BEACON_STRENGTH_THRESHOLD &&
				(rCurrentTime - prBssDesc->rUpdateTime) <= REPLICATED_BEACON_FRESH_PERIOD) {
				DBGLOG(SCN, EVENT, "rssi is too much weaker and previous one is fresh\n");
				return prBssDesc;
			}
			
			else if (rCurrentTime - prBssDesc->rUpdateTime <= REPLICATED_BEACON_TIME_THRESHOLD) {
				DBGLOG(SCN, EVENT, "receive beacon/probe reponses too close\n");
				return prBssDesc;
			}
		}

		
		if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE && u8Timestamp < prBssDesc->u8TimeStamp.QuadPart) {
			BOOLEAN fgIsConnected, fgIsConnecting;

			
			fgIsNewBssDesc = TRUE;

			
			fgIsConnected = prBssDesc->fgIsConnected;
			fgIsConnecting = prBssDesc->fgIsConnecting;
			scanRemoveBssDescByBssid(prAdapter, prBssDesc->aucBSSID);

			prBssDesc = scanAllocateBssDesc(prAdapter);
			if (!prBssDesc)
				return NULL;

			
			prBssDesc->fgIsConnected = fgIsConnected;
			prBssDesc->fgIsConnecting = fgIsConnecting;
		}
	}
#if 1

	prBssDesc->u2RawLength = prSwRfb->u2PacketLen;
	if (prBssDesc->u2RawLength > CFG_RAW_BUFFER_SIZE)
		prBssDesc->u2RawLength = CFG_RAW_BUFFER_SIZE;
	kalMemCopy(prBssDesc->aucRawBuf, prWlanBeaconFrame, prBssDesc->u2RawLength);
#endif

	
	if ((fgIsNewBssDesc == FALSE) && prBssDesc->fgIsConnecting) {
		DBGLOG(SCN, INFO, "we're connecting this BSS(%pM) now, don't update it\n",
				prBssDesc->aucBSSID);
		return prBssDesc;
	}
	
	prBssDesc->eBSSType = eBSSType;	

	COPY_MAC_ADDR(prBssDesc->aucSrcAddr, prWlanBeaconFrame->aucSrcAddr);

	COPY_MAC_ADDR(prBssDesc->aucBSSID, prWlanBeaconFrame->aucBSSID);

	prBssDesc->u8TimeStamp.QuadPart = u8Timestamp;

	WLAN_GET_FIELD_16(&prWlanBeaconFrame->u2BeaconInterval, &prBssDesc->u2BeaconInterval);

	prBssDesc->u2CapInfo = u2CapInfo;

	
	u2IELength = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (UINT_16) OFFSET_OF(WLAN_BEACON_FRAME_BODY_T, aucInfoElem[0]);

	if (u2IELength > CFG_IE_BUFFER_SIZE) {
		u2IELength = CFG_IE_BUFFER_SIZE;
		prBssDesc->fgIsIEOverflow = TRUE;
	} else {
		prBssDesc->fgIsIEOverflow = FALSE;
	}
	prBssDesc->u2IELength = u2IELength;

	kalMemCopy(prBssDesc->aucIEBuf, prWlanBeaconFrame->aucInfoElem, u2IELength);

	
	prBssDesc->fgIsERPPresent = FALSE;
	prBssDesc->fgIsHTPresent = FALSE;
	prBssDesc->eSco = CHNL_EXT_SCN;
	prBssDesc->fgIEWAPI = FALSE;
#if CFG_RSN_MIGRATION
	prBssDesc->fgIERSN = FALSE;
#endif
#if CFG_PRIVACY_MIGRATION
	prBssDesc->fgIEWPA = FALSE;
#endif

	
	pucIE = prWlanBeaconFrame->aucInfoElem;

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {

		switch (IE_ID(pucIE)) {
		case ELEM_ID_SSID:
			if ((!prIeSsid) &&	
			    (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID)) {
				BOOLEAN fgIsHiddenSSID = FALSE;

				ucSSIDChar = '\0';

				prIeSsid = (P_IE_SSID_T) pucIE;

				
				if (IE_LEN(pucIE) == 0)
					fgIsHiddenSSID = TRUE;
				
				else {
					for (i = 0; i < IE_LEN(pucIE); i++)
						ucSSIDChar |= SSID_IE(pucIE)->aucSSID[i];

					if (!ucSSIDChar)
						fgIsHiddenSSID = TRUE;
				}

				
				if (!fgIsHiddenSSID) {
					COPY_SSID(prBssDesc->aucSSID,
						  prBssDesc->ucSSIDLen,
						  SSID_IE(pucIE)->aucSSID, SSID_IE(pucIE)->ucLength);
				}
#if 0
				else {
					prBssDesc->aucSSID[0] = '\0';
					prBssDesc->ucSSIDLen = 0;
				}
#endif

			}
			break;

		case ELEM_ID_SUP_RATES:
			
			if ((!prIeSupportedRate) && (IE_LEN(pucIE) <= RATE_NUM))
				prIeSupportedRate = SUP_RATES_IE(pucIE);
			break;

		case ELEM_ID_DS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_DS_PARAMETER_SET)
				ucIeDsChannelNum = DS_PARAM_IE(pucIE)->ucCurrChnl;
			break;

		case ELEM_ID_TIM:
			if (IE_LEN(pucIE) <= ELEM_MAX_LEN_TIM)
				prBssDesc->ucDTIMPeriod = TIM_IE(pucIE)->ucDTIMPeriod;
			break;

		case ELEM_ID_IBSS_PARAM_SET:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_IBSS_PARAMETER_SET)
				prBssDesc->u2ATIMWindow = IBSS_PARAM_IE(pucIE)->u2ATIMWindow;
			break;

#if 0				
		case ELEM_ID_COUNTRY_INFO:
			prBssDesc->prIECountry = (P_IE_COUNTRY_T) pucIE;
			break;
#endif

		case ELEM_ID_ERP_INFO:
			if (IE_LEN(pucIE) == ELEM_MAX_LEN_ERP)
				prBssDesc->fgIsERPPresent = TRUE;
			break;

		case ELEM_ID_EXTENDED_SUP_RATES:
			if (!prIeExtSupportedRate)
				prIeExtSupportedRate = EXT_SUP_RATES_IE(pucIE);
			break;

#if CFG_RSN_MIGRATION
		case ELEM_ID_RSN:
			if (rsnParseRsnIE(prAdapter, RSN_IE(pucIE), &prBssDesc->rRSNInfo)) {
				prBssDesc->fgIERSN = TRUE;
				prBssDesc->u2RsnCap = prBssDesc->rRSNInfo.u2RsnCap;
				if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2)
					rsnCheckPmkidCache(prAdapter, prBssDesc);
			}
			break;
#endif

		case ELEM_ID_HT_CAP:
			prBssDesc->fgIsHTPresent = TRUE;
			break;

		case ELEM_ID_HT_OP:
			if (IE_LEN(pucIE) != (sizeof(IE_HT_OP_T) - 2))
				break;

			if ((((P_IE_HT_OP_T) pucIE)->ucInfo1 & HT_OP_INFO1_SCO) != CHNL_EXT_RES) {
				prBssDesc->eSco = (ENUM_CHNL_EXT_T)
				    (((P_IE_HT_OP_T) pucIE)->ucInfo1 & HT_OP_INFO1_SCO);
			}
			ucIeHtChannelNum = ((P_IE_HT_OP_T) pucIE)->ucPrimaryChannel;

			break;

#if CFG_SUPPORT_WAPI
		case ELEM_ID_WAPI:
			if (wapiParseWapiIE(WAPI_IE(pucIE), &prBssDesc->rIEWAPI))
				prBssDesc->fgIEWAPI = TRUE;
			break;
#endif

		case ELEM_ID_VENDOR:	
#if CFG_PRIVACY_MIGRATION
			if (rsnParseCheckForWFAInfoElem(prAdapter, pucIE, &ucOuiType, &u2SubTypeVersion)) {
				if ((ucOuiType == VENDOR_OUI_TYPE_WPA) && (u2SubTypeVersion == VERSION_WPA)) {

					if (rsnParseWpaIE(prAdapter, WPA_IE(pucIE), &prBssDesc->rWPAInfo))
						prBssDesc->fgIEWPA = TRUE;
				}
			}
#endif

#if CFG_ENABLE_WIFI_DIRECT
			if (prAdapter->fgIsP2PRegistered) {
				if (p2pFuncParseCheckForP2PInfoElem(prAdapter, pucIE, &ucOuiType)) {
					if (ucOuiType == VENDOR_OUI_TYPE_P2P)
						prBssDesc->fgIsP2PPresent = TRUE;
				}
			}
#endif 
			break;

			
		}
	}

	
	

	if (prBssDesc->ucSSIDLen == 0)
		prBssDesc->fgIsHiddenSSID = TRUE;
	else
		prBssDesc->fgIsHiddenSSID = FALSE;

	
	if (prIeSupportedRate || prIeExtSupportedRate) {
		rateGetRateSetFromIEs(prIeSupportedRate,
				      prIeExtSupportedRate,
				      &prBssDesc->u2OperationalRateSet,
				      &prBssDesc->u2BSSBasicRateSet, &prBssDesc->fgIsUnknownBssBasicRate);
	}
	
	{
		prHifRxHdr = prSwRfb->prHifRxHdr;

		ASSERT(prHifRxHdr);

		
		prBssDesc->fgIsLargerTSF = HIF_RX_HDR_GET_TCL_FLAG(prHifRxHdr);

		
		prBssDesc->eBand = HIF_RX_HDR_GET_RF_BAND(prHifRxHdr);

		
		ucHwChannelNum = HIF_RX_HDR_GET_CHNL_NUM(prHifRxHdr);

		if (BAND_2G4 == prBssDesc->eBand) {

			
			if (ucIeDsChannelNum >= 1 && ucIeDsChannelNum <= 14) {

				
				if ((ucIeDsChannelNum == ucHwChannelNum) || (prHifRxHdr->ucRcpi > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;
				
				prBssDesc->ucChannelNum = ucIeDsChannelNum;
			} else if (ucIeHtChannelNum >= 1 && ucIeHtChannelNum <= 14) {
				
				if ((ucIeHtChannelNum == ucHwChannelNum) || (prHifRxHdr->ucRcpi > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;
				
				prBssDesc->ucChannelNum = ucIeHtChannelNum;
			} else {
				prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;

				prBssDesc->ucChannelNum = ucHwChannelNum;
			}
		}
		
		else {
			if (ucIeHtChannelNum >= 1 && ucIeHtChannelNum < 200) {
				
				if ((ucIeHtChannelNum == ucHwChannelNum) || (prHifRxHdr->ucRcpi > prBssDesc->ucRCPI))
					prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;
				
				prBssDesc->ucChannelNum = ucIeHtChannelNum;
			} else {
				
				prBssDesc->ucRCPI = prHifRxHdr->ucRcpi;

				prBssDesc->ucChannelNum = ucHwChannelNum;
			}
		}
	}

	
	prBssDesc->ucPhyTypeSet = 0;

	if (BAND_2G4 == prBssDesc->eBand) {
		
		if (prBssDesc->fgIsHTPresent)
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

		
		if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
			
			if ((prBssDesc->u2OperationalRateSet & RATE_SET_OFDM) || prBssDesc->fgIsERPPresent)
				prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_ERP;

			
			if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_OFDM)) {
				
				if ((prBssDesc->u2OperationalRateSet & RATE_SET_HR_DSSS))
					prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HR_DSSS;
			}
		}
	} else {		
		
		if (prBssDesc->fgIsHTPresent)
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_HT;

		
		if (!(prBssDesc->u2BSSBasicRateSet & RATE_SET_BIT_HT_PHY)) {
			
			prBssDesc->ucPhyTypeSet |= PHY_TYPE_BIT_OFDM;

			ASSERT(!(prBssDesc->u2OperationalRateSet & RATE_SET_HR_DSSS));
		}
	}

	
	GET_CURRENT_SYSTIME(&prBssDesc->rUpdateTime);

	return prBssDesc;
}

WLAN_STATUS scanAddScanResult(IN P_ADAPTER_T prAdapter, IN P_BSS_DESC_T prBssDesc, IN P_SW_RFB_T prSwRfb)
{
	P_SCAN_INFO_T prScanInfo;
	UINT_8 aucRatesEx[PARAM_MAX_LEN_RATES_EX];
	P_WLAN_BEACON_FRAME_T prWlanBeaconFrame;
	PARAM_MAC_ADDRESS rMacAddr;
	PARAM_SSID_T rSsid;
	ENUM_PARAM_NETWORK_TYPE_T eNetworkType;
	PARAM_802_11_CONFIG_T rConfiguration;
	ENUM_PARAM_OP_MODE_T eOpMode;
	UINT_8 ucRateLen = 0;
	UINT_32 i;

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	if (prBssDesc->eBand == BAND_2G4) {
		if ((prBssDesc->u2OperationalRateSet & RATE_SET_OFDM)
		    || prBssDesc->fgIsERPPresent) {
			eNetworkType = PARAM_NETWORK_TYPE_OFDM24;
		} else {
			eNetworkType = PARAM_NETWORK_TYPE_DS;
		}
	} else {
		ASSERT(prBssDesc->eBand == BAND_5G);
		eNetworkType = PARAM_NETWORK_TYPE_OFDM5;
	}

	if (prBssDesc->eBSSType == BSS_TYPE_P2P_DEVICE) {
		
		return WLAN_STATUS_FAILURE;
	}

	prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) prSwRfb->pvHeader;
	COPY_MAC_ADDR(rMacAddr, prWlanBeaconFrame->aucBSSID);
	COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);

	rConfiguration.u4Length = sizeof(PARAM_802_11_CONFIG_T);
	rConfiguration.u4BeaconPeriod = (UINT_32) prWlanBeaconFrame->u2BeaconInterval;
	rConfiguration.u4ATIMWindow = prBssDesc->u2ATIMWindow;
	rConfiguration.u4DSConfig = nicChannelNum2Freq(prBssDesc->ucChannelNum);
	rConfiguration.rFHConfig.u4Length = sizeof(PARAM_802_11_CONFIG_FH_T);

	rateGetDataRatesFromRateSet(prBssDesc->u2OperationalRateSet, 0, aucRatesEx, &ucRateLen);

	for (i = ucRateLen; i < sizeof(aucRatesEx) / sizeof(aucRatesEx[0]); i++)
		aucRatesEx[i] = 0;

	switch (prBssDesc->eBSSType) {
	case BSS_TYPE_IBSS:
		eOpMode = NET_TYPE_IBSS;
		break;

	case BSS_TYPE_INFRASTRUCTURE:
	case BSS_TYPE_P2P_DEVICE:
	case BSS_TYPE_BOW_DEVICE:
	default:
		eOpMode = NET_TYPE_INFRA;
		break;
	}

	DBGLOG(SCN, TRACE, "ind %s %d\n", prBssDesc->aucSSID, prBssDesc->ucChannelNum);

#if (CFG_SUPPORT_TDLS == 1)
	{
		if (flgTdlsTestExtCapElm == TRUE) {
			
			UINT8 *pucElm = (UINT8 *) (prSwRfb->pvHeader + prSwRfb->u2PacketLen);

			kalMemCopy(pucElm - 9, aucTdlsTestExtCapElm, 7);
			prSwRfb->u2PacketLen -= 2;

			DBGLOG(TDLS, INFO,
			       "<tdls> %s: append ext cap element to %pM\n",
				__func__, prBssDesc->aucBSSID);
		}
	}
#endif 

	if (prAdapter->rWifiVar.rScanInfo.fgNloScanning &&
				test_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME, &prAdapter->ulSuspendFlag)) {
			UINT_8 i = 0;
			P_BSS_DESC_T *pprPendBssDesc = &prScanInfo->rNloParam.aprPendingBssDescToInd[0];

			for (; i < SCN_SSID_MATCH_MAX_NUM; i++) {
				if (pprPendBssDesc[i])
					continue;
				DBGLOG(SCN, INFO,
					"indicate bss[%pM] before wiphy resume, need to indicate again after wiphy resume\n",
					prBssDesc->aucBSSID);
				pprPendBssDesc[i] = prBssDesc;
				break;
			}
	}

	kalIndicateBssInfo(prAdapter->prGlueInfo,
			   (PUINT_8) prSwRfb->pvHeader,
			   prSwRfb->u2PacketLen, prBssDesc->ucChannelNum, RCPI_TO_dBm(prBssDesc->ucRCPI));

	nicAddScanResult(prAdapter,
			 rMacAddr,
			 &rSsid,
			 prWlanBeaconFrame->u2CapInfo & CAP_INFO_PRIVACY ? 1 : 0,
			 RCPI_TO_dBm(prBssDesc->ucRCPI),
			 eNetworkType,
			 &rConfiguration,
			 eOpMode,
			 aucRatesEx,
			 prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen,
			 (PUINT_8) ((ULONG) (prSwRfb->pvHeader) + WLAN_MAC_MGMT_HEADER_LEN));

	return WLAN_STATUS_SUCCESS;

}				

#if 1

BOOLEAN scanCheckBssIsLegal(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	BOOLEAN fgAddToScanResult = FALSE;
	ENUM_BAND_T eBand = 0;
	UINT_8 ucChannel = 0;

	ASSERT(prAdapter);
	
	if (rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand, prBssDesc->ucChannelNum) == TRUE) {
		
		if (cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel) == TRUE &&
		    (eBand != prBssDesc->eBand || ucChannel != prBssDesc->ucChannelNum)) {
			fgAddToScanResult = FALSE;
		} else {
			fgAddToScanResult = TRUE;
		}
	}
	if (fgAddToScanResult && prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX] != BAND_NULL) {
		if (prBssDesc->eBand != prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX]) {
			DBGLOG(SCN, INFO, "setband for filter ap\n");
			fgAddToScanResult = FALSE;
		}
	}
	return fgAddToScanResult;

}

VOID scanReportBss2Cfg80211(IN P_ADAPTER_T prAdapter, IN ENUM_BSS_TYPE_T eBSSType, IN P_BSS_DESC_T SpecificprBssDesc)
{
	P_SCAN_INFO_T prScanInfo = (P_SCAN_INFO_T) NULL;
	P_LINK_T prBSSDescList = (P_LINK_T) NULL;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	RF_CHANNEL_INFO_T rChannelInfo;

	ASSERT(prAdapter);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	prBSSDescList = &prScanInfo->rBSSDescList;

	DBGLOG(SCN, TRACE, "scanReportBss2Cfg80211\n");

	if (SpecificprBssDesc) {
		{
			
			if (!scanCheckBssIsLegal(prAdapter, SpecificprBssDesc)) {
				DBGLOG(SCN, TRACE, "Remove specific SSID[%s %d]\n",
						    SpecificprBssDesc->aucSSID, SpecificprBssDesc->ucChannelNum);
				return;
			}

			DBGLOG(SCN, TRACE, "Report Specific SSID[%s]\n", SpecificprBssDesc->aucSSID);
			if (eBSSType == BSS_TYPE_INFRASTRUCTURE) {

				kalIndicateBssInfo(prAdapter->prGlueInfo,
						   (PUINT_8) SpecificprBssDesc->aucRawBuf,
						   SpecificprBssDesc->u2RawLength,
						   SpecificprBssDesc->ucChannelNum,
						   RCPI_TO_dBm(SpecificprBssDesc->ucRCPI));
			} else {

				rChannelInfo.ucChannelNum = SpecificprBssDesc->ucChannelNum;
				rChannelInfo.eBand = SpecificprBssDesc->eBand;
				kalP2PIndicateBssInfo(prAdapter->prGlueInfo,
						      (PUINT_8) SpecificprBssDesc->aucRawBuf,
						      SpecificprBssDesc->u2RawLength,
						      &rChannelInfo, RCPI_TO_dBm(SpecificprBssDesc->ucRCPI));

			}

#if CFG_ENABLE_WIFI_DIRECT
			SpecificprBssDesc->fgIsP2PReport = FALSE;
#endif
		}
	} else {
		
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
			
			P_PARAM_CHN_LOAD_INFO prChnLoad;
			UINT_8 ucIdx = 0;

			if (((prBssDesc->ucChannelNum > 0) && (prBssDesc->ucChannelNum <= 48))
			    || (prBssDesc->ucChannelNum >= 147) ) {
				if (prBssDesc->ucChannelNum <= HW_CHNL_NUM_MAX_2G4) {
					ucIdx = prBssDesc->ucChannelNum - 1;
				} else if (prBssDesc->ucChannelNum <= 48) {
					ucIdx = (UINT_8) (HW_CHNL_NUM_MAX_2G4 + (prBssDesc->ucChannelNum - 34) / 4);
				} else {
					ucIdx =
					    (UINT_8) (HW_CHNL_NUM_MAX_2G4 + 4 + (prBssDesc->ucChannelNum - 149) / 4);
				}

				if (ucIdx < MAX_AUTO_CHAL_NUM) {
					prChnLoad = (P_PARAM_CHN_LOAD_INFO) &
						(prAdapter->rWifiVar.rChnLoadInfo.rEachChnLoad[ucIdx]);
					prChnLoad->ucChannel = prBssDesc->ucChannelNum;
					prChnLoad->u2APNum++;
				} else {
					DBGLOG(SCN, WARN, "ACS: ChIdx > MAX_AUTO_CHAL_NUM\n");
				}

			}
#endif
			
			if (!scanCheckBssIsLegal(prAdapter, prBssDesc)) {
				DBGLOG(SCN, TRACE, "Remove SSID[%s %d]\n",
						    prBssDesc->aucSSID, prBssDesc->ucChannelNum);

				continue;
			}

			if ((prBssDesc->eBSSType == eBSSType)
#if CFG_ENABLE_WIFI_DIRECT
			    || ((eBSSType == BSS_TYPE_P2P_DEVICE) && (prBssDesc->fgIsP2PReport == TRUE))
#endif
			    ) {

				DBGLOG(SCN, TRACE, "Report ALL SSID[%s %d]\n",
						    prBssDesc->aucSSID, prBssDesc->ucChannelNum);

				if (eBSSType == BSS_TYPE_INFRASTRUCTURE) {
					if (prBssDesc->u2RawLength != 0) {
						kalIndicateBssInfo(prAdapter->prGlueInfo,
								   (PUINT_8) prBssDesc->aucRawBuf,
								   prBssDesc->u2RawLength,
								   prBssDesc->ucChannelNum,
								   RCPI_TO_dBm(prBssDesc->ucRCPI));
						kalMemZero(prBssDesc->aucRawBuf, CFG_RAW_BUFFER_SIZE);
						prBssDesc->u2RawLength = 0;

#if CFG_ENABLE_WIFI_DIRECT
						prBssDesc->fgIsP2PReport = FALSE;
#endif
					}
				} else {
#if CFG_ENABLE_WIFI_DIRECT
					if (prBssDesc->fgIsP2PReport == TRUE) {
#endif
						rChannelInfo.ucChannelNum = prBssDesc->ucChannelNum;
						rChannelInfo.eBand = prBssDesc->eBand;

						kalP2PIndicateBssInfo(prAdapter->prGlueInfo,
								      (PUINT_8) prBssDesc->aucRawBuf,
								      prBssDesc->u2RawLength,
								      &rChannelInfo, RCPI_TO_dBm(prBssDesc->ucRCPI));

						
						

						
#if CFG_ENABLE_WIFI_DIRECT
						prBssDesc->fgIsP2PReport = FALSE;
					}
#endif
				}
			}

		}
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
		prAdapter->rWifiVar.rChnLoadInfo.fgDataReadyBit = TRUE;
#endif

	}

}

#endif

UINT_8 nicChannelNum2Index(IN UINT_8 ucChannelNum)
{
	UINT_8 ucindex;

	
	if (ucChannelNum >= 1 && ucChannelNum <= 14)
		
		ucindex = ucChannelNum;
	else if (ucChannelNum >= 36 && ucChannelNum <= 64)
		
		ucindex = 6 + (ucChannelNum >> 2);
	else if (ucChannelNum >= 100 && ucChannelNum <= 144)
		
		ucindex = (ucChannelNum >> 2) - 2;
	else if (ucChannelNum >= 149 && ucChannelNum <= 165) {
		
		ucChannelNum = ucChannelNum - 1;
		ucindex = (ucChannelNum >> 2) - 2;
	} else
		ucindex = 0;

		return ucindex;
}

WLAN_STATUS scanProcessBeaconAndProbeResp(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_BSS_INFO_T prAisBssInfo;
	P_WLAN_BEACON_FRAME_T prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) NULL;
#if CFG_SLT_SUPPORT
	P_SLT_INFO_T prSltInfo = (P_SLT_INFO_T) NULL;
#endif

	ASSERT(prAdapter);
	ASSERT(prSwRfb);

	
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
		(TIMESTAMP_FIELD_LEN + BEACON_INTERVAL_FIELD_LEN + CAP_INFO_FIELD_LEN)) {
		
		UINT_32 u4MailBox0;

		nicGetMailbox(prAdapter, 0, &u4MailBox0);
		DBGLOG(SCN, WARN, "if conn sys also get less length (0x5a means yes) %x\n", (UINT_32) u4MailBox0);
		DBGLOG(SCN, WARN, "u2PacketLen %d, u2HeaderLen %d, payloadLen %d\n",
			prSwRfb->u2PacketLen, prSwRfb->u2HeaderLen,
			prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen);
		

#ifndef _lint
		ASSERT(0);
#endif 
		return rStatus;
	}
#if CFG_SLT_SUPPORT
	prSltInfo = &prAdapter->rWifiVar.rSltInfo;

	if (prSltInfo->fgIsDUT) {
		DBGLOG(SCN, INFO, "\n\rBCN: RX\n");
		prSltInfo->u4BeaconReceiveCnt++;
		return WLAN_STATUS_SUCCESS;
	} else {
		return WLAN_STATUS_SUCCESS;
	}
#endif

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prWlanBeaconFrame = (P_WLAN_BEACON_FRAME_T) prSwRfb->pvHeader;

	
	if (IS_BMCAST_MAC_ADDR(prWlanBeaconFrame->aucSrcAddr)) {
		DBGLOG(SCN, WARN, "received beacon/probe response from multicast AP\n");
		return rStatus;
	}

	
	prBssDesc = scanAddToBssDesc(prAdapter, prSwRfb);

	if (prBssDesc) {
		
		if (prAdapter->prGlueInfo->ucTrScanType == 1) {
			UINT_8	ucindex;

			ucindex = nicChannelNum2Index(prBssDesc->ucChannelNum);
			DBGLOG(SCN, TRACE, "Full2Partial ucChannelNum=%d, ucindex=%d\n",
				prBssDesc->ucChannelNum, ucindex);

			
			prAdapter->prGlueInfo->ucChannelNum[ucindex] = 1;
		}

		
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED &&
		    ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE && prConnSettings->eOPMode != NET_TYPE_IBSS)
		     || (prBssDesc->eBSSType == BSS_TYPE_IBSS && prConnSettings->eOPMode != NET_TYPE_INFRA)) &&
		    EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID) &&
		    EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen, prAisBssInfo->aucSSID,
			       prAisBssInfo->ucSSIDLen)) {
			BOOLEAN fgNeedDisconnect = FALSE;

#if CFG_SUPPORT_BEACON_CHANGE_DETECTION
			
			if (prAisBssInfo->u2OperationalRateSet != prBssDesc->u2OperationalRateSet)
				fgNeedDisconnect = TRUE;
#endif
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
			if (
#if CFG_SUPPORT_WAPI
			(prAdapter->rWifiVar.rConnSettings.fgWapiMode == TRUE &&
			!wapiPerformPolicySelection(prAdapter, prBssDesc)) ||
#endif
			rsnCheckSecurityModeChanged(prAdapter, prAisBssInfo, prBssDesc)) {
				DBGLOG(SCN, INFO, "Beacon security mode change detected\n");
				fgNeedDisconnect = FALSE;
				aisBssSecurityChanged(prAdapter);
			}
#endif

			
			if (fgNeedDisconnect == TRUE)
				aisBssBeaconTimeout(prAdapter);
		}
		
		if (((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE && prConnSettings->eOPMode != NET_TYPE_IBSS)
		     || (prBssDesc->eBSSType == BSS_TYPE_IBSS && prConnSettings->eOPMode != NET_TYPE_INFRA))) {
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				if ((!prAisBssInfo->ucDTIMPeriod) &&
				    EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID) &&
				    (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) &&
				    ((prWlanBeaconFrame->u2FrameCtrl & MASK_FRAME_TYPE) == MAC_FRAME_BEACON)) {

					prAisBssInfo->ucDTIMPeriod = prBssDesc->ucDTIMPeriod;

					
					nicPmIndicateBssConnected(prAdapter, NETWORK_TYPE_AIS_INDEX);
				}
			}
#if CFG_SUPPORT_ADHOC
			if (EQUAL_SSID(prBssDesc->aucSSID,
				prBssDesc->ucSSIDLen,
				prConnSettings->aucSSID,
				prConnSettings->ucSSIDLen) &&
			    (prBssDesc->eBSSType == BSS_TYPE_IBSS) && (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS)) {
				ibssProcessMatchedBeacon(prAdapter, prAisBssInfo, prBssDesc,
							 prSwRfb->prHifRxHdr->ucRcpi);
			}
#endif 

		}

		rlmProcessBcn(prAdapter,
			      prSwRfb,
			      ((P_WLAN_BEACON_FRAME_T) (prSwRfb->pvHeader))->aucInfoElem,
			      (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
			      (UINT_16) (OFFSET_OF(WLAN_BEACON_FRAME_BODY_T, aucInfoElem[0])));

		
		if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE || prBssDesc->eBSSType == BSS_TYPE_IBSS) {
			
			if (prConnSettings->fgIsScanReqIssued || prAdapter->rWifiVar.rScanInfo.fgNloScanning) {
				BOOLEAN fgAddToScanResult;

				fgAddToScanResult = scanCheckBssIsLegal(prAdapter, prBssDesc);

				if (fgAddToScanResult == TRUE)
					rStatus = scanAddScanResult(prAdapter, prBssDesc, prSwRfb);
			}
		}
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered)
			scanP2pProcessBeaconAndProbeResp(prAdapter, prSwRfb, &rStatus, prBssDesc, prWlanBeaconFrame);
#endif
	}

	return rStatus;

}				

INT_32 scanResultsAdjust5GPref(P_BSS_DESC_T prBssDesc)
{
	INT_32 rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
	INT_32 orgRssi = rssi;

	if (prBssDesc->eBand == BAND_5G) {
		if (rssi >= rssiRangeHi)
			rssi += pref5GhzHi;
		else if (rssi >= rssiRangeMed)
			rssi += pref5GhzMed;
		else if (rssi >= rssiRangeLo)
			rssi += pref5GhzLo;
	}
	
	if (prBssDesc->fgIsConnected)
		rssi += (ROAMING_NO_SWING_RCPI_STEP >> 1);

	if (prBssDesc->eBand == BAND_5G || prBssDesc->fgIsConnected)
		DBGLOG(SCN, TRACE, "Adjust 5G band RSSI: " MACSTR " band=%d, orgRssi=%d afterRssi=%d\n",
			MAC2STR(prBssDesc->aucBSSID), prBssDesc->eBand, orgRssi, rssi);
	return rssi;
}

P_BSS_DESC_T scanSearchBssDescByPolicy(IN P_ADAPTER_T prAdapter, IN ENUM_NETWORK_TYPE_INDEX_T eNetTypeIndex)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
	P_SCAN_INFO_T prScanInfo;

	P_LINK_T prBSSDescList;

	P_BSS_DESC_T prBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prPrimaryBssDesc = (P_BSS_DESC_T) NULL;
	P_BSS_DESC_T prCandidateBssDesc = (P_BSS_DESC_T) NULL;

	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;
	P_STA_RECORD_T prPrimaryStaRec;
	P_STA_RECORD_T prCandidateStaRec = (P_STA_RECORD_T) NULL;

	OS_SYSTIME rCurrentTime;

	
	BOOLEAN fgIsFindFirst = (BOOLEAN) FALSE;

	BOOLEAN fgIsFindBestRSSI = (BOOLEAN) FALSE;
	BOOLEAN fgIsFindBestEncryptionLevel = (BOOLEAN) FALSE;
	

	
	

	BOOLEAN fgIsFixedChannel;
	BOOLEAN fgIsAbsentCandidateBss = TRUE;
	ENUM_BAND_T eBand = 0;
	UINT_8 ucChannel = 0;

	ASSERT(prAdapter);

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prBssInfo = &(prAdapter->rWifiVar.arBssInfo[eNetTypeIndex]);

	prAisSpecBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	
	if (eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {
#if CFG_P2P_LEGACY_COEX_REVISE
		fgIsFixedChannel = cnmAisDetectP2PChannel(prAdapter, &eBand, &ucChannel);
#else
		fgIsFixedChannel = cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel);
#endif
	} else {
		fgIsFixedChannel = FALSE;
	}

#if DBG
	if (prConnSettings->ucSSIDLen < ELEM_MAX_LEN_SSID)
		prConnSettings->aucSSID[prConnSettings->ucSSIDLen] = '\0';
#endif

	DBGLOG(SCN, LOUD, "SEARCH: Bss Num: %d, Look for SSID: %s, %pM Band=%d, channel=%d\n",
			   (UINT_32) prBSSDescList->u4NumElem, prConnSettings->aucSSID,
			   (prConnSettings->aucBSSID), eBand, ucChannel);

	
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {

		

		DBGLOG(SCN, TRACE, "SEARCH: [ %pM ], SSID:%s\n",
				   prBssDesc->aucBSSID, prBssDesc->aucSSID);
		
		if (prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX] != BAND_NULL) {
			if (prBssDesc->eBand != prAdapter->aeSetBand[NETWORK_TYPE_AIS_INDEX]) {
				DBGLOG(SCN, INFO, "skip the bss band not match the set band\n");
				continue;
			}
		}

		
		
		if (!(prBssDesc->ucPhyTypeSet & (prAdapter->rWifiVar.ucAvailablePhyTypeSet))) {
			DBGLOG(SCN, TRACE, "SEARCH: Ignore unsupported ucPhyTypeSet = %x\n", prBssDesc->ucPhyTypeSet);
			continue;
		}
		
		if (prBssDesc->fgIsUnknownBssBasicRate)
			continue;
		
		if (fgIsFixedChannel == TRUE && (prBssDesc->eBand != eBand || prBssDesc->ucChannelNum != ucChannel))
			continue;
		
		if (rlmDomainIsLegalChannel(prAdapter, prBssDesc->eBand, prBssDesc->ucChannelNum) == FALSE)
			continue;
		
#if CFG_SUPPORT_RN
	if (prBssInfo->fgDisConnReassoc == FALSE)
#endif
		if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
				      SEC_TO_SYSTIME(SCN_BSS_DESC_REMOVE_TIMEOUT_SEC))) {

			BOOLEAN fgIsNeedToCheckTimeout = TRUE;

#if CFG_SUPPORT_ROAMING
			P_ROAMING_INFO_T prRoamingFsmInfo;

			prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);
			if ((prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) ||
			    (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_ROAM)) {
				if (++prRoamingFsmInfo->RoamingEntryTimeoutSkipCount <
				    ROAMING_ENTRY_TIMEOUT_SKIP_COUNT_MAX) {
					fgIsNeedToCheckTimeout = FALSE;
					DBGLOG(SCN, INFO, "SEARCH: Romaing skip SCN_BSS_DESC_REMOVE_TIMEOUT_SEC\n");
				}
			}
#endif

			if (fgIsNeedToCheckTimeout == TRUE) {
				DBGLOG(SCN, TRACE, "Ignore stale bss %pM\n", prBssDesc->aucBSSID);
				continue;
			}
		}
		
		
		prStaRec = cnmGetStaRecByAddress(prAdapter, (UINT_8) eNetTypeIndex, prBssDesc->aucSrcAddr);

		if (prStaRec) {
#if 0				
			if (prStaRec->u2ReasonCode != REASON_CODE_RESERVED) {
				DBGLOG(SCN, INFO, "SEARCH: Ignore BSS with previous Reason Code = %d\n",
						   prStaRec->u2ReasonCode);
				continue;
			} else
#endif
			if (prStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL) {
				if ((prStaRec->ucJoinFailureCount < JOIN_MAX_RETRY_FAILURE_COUNT) ||
				    (CHECK_FOR_TIMEOUT(rCurrentTime,
						       prStaRec->rLastJoinTime,
						       SEC_TO_SYSTIME(JOIN_RETRY_INTERVAL_SEC)))) {

					if (prStaRec->ucJoinFailureCount >= JOIN_MAX_RETRY_FAILURE_COUNT)
						prStaRec->ucJoinFailureCount = 0;
					DBGLOG(SCN, INFO,
					       "SEARCH: Try to join BSS again,Status Code=%d (Curr=%u/Last Join=%u)\n",
						prStaRec->u2StatusCode, rCurrentTime, prStaRec->rLastJoinTime);
				} else {
					DBGLOG(SCN, INFO,
					       "SEARCH: Ignore BSS which reach maximum Join Retry Count = %d\n",
						JOIN_MAX_RETRY_FAILURE_COUNT);
					continue;
				}

			}
		}
		
		if (eNetTypeIndex == NETWORK_TYPE_AIS_INDEX) {

			
			
			if (((prConnSettings->eOPMode == NET_TYPE_INFRA) &&
			     (prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE))
#if CFG_SUPPORT_ADHOC
			     || ((prConnSettings->eOPMode == NET_TYPE_IBSS
			      || prConnSettings->eOPMode == NET_TYPE_DEDICATED_IBSS)
			     && (prBssDesc->eBSSType != BSS_TYPE_IBSS))
#endif
			     ) {

				DBGLOG(SCN, TRACE, "Cur OPMode %d, Ignore eBSSType = %d\n",
						prConnSettings->eOPMode, prBssDesc->eBSSType);
				continue;
			}
			
			if ((prConnSettings->fgIsConnByBssidIssued) &&
				(prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE)) {

				if (UNEQUAL_MAC_ADDR(prConnSettings->aucBSSID, prBssDesc->aucBSSID)) {

					DBGLOG(SCN, TRACE, "SEARCH: Ignore due to BSSID was not matched!\n");
					continue;
				}
			}
#if CFG_SUPPORT_ADHOC
			
			if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {
				OS_SYSTIME rCurrentTime;

				
				GET_CURRENT_SYSTIME(&rCurrentTime);
				if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
						      SEC_TO_SYSTIME(SCN_ADHOC_BSS_DESC_TIMEOUT_SEC))) {
					DBGLOG(SCN, LOUD,
						"SEARCH: Skip old record of BSS Descriptor - BSSID:[%pM]\n\n",
						prBssDesc->aucBSSID);
					continue;
				}
				
				if (ibssCheckCapabilityForAdHocMode(prAdapter, prBssDesc) == WLAN_STATUS_FAILURE) {

					if (prPrimaryBssDesc)
						DBGLOG(SCN, INFO,
							"SEARCH: BSS DESC MAC: %pM, not supported AdHoc Mode.\n",
							prPrimaryBssDesc->aucBSSID);

					continue;
				}
				
				if (prBssInfo->fgIsBeaconActivated &&
				    UNEQUAL_MAC_ADDR(prBssInfo->aucBSSID, prBssDesc->aucBSSID)) {

					DBGLOG(SCN, LOUD,
					       "SEARCH: prBssDesc->fgIsLargerTSF = %d\n", prBssDesc->fgIsLargerTSF);

					if (!prBssDesc->fgIsLargerTSF) {
						DBGLOG(SCN, INFO,
						       "SEARCH: Ignore BSS DESC MAC: [ %pM ], Smaller TSF\n",
							prBssDesc->aucBSSID);
						continue;
					}
				}
			}
#endif 

		}
#if 0				
		
		if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {
			OS_SYSTIME rCurrentTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
					      SEC_TO_SYSTIME(BSS_DESC_TIMEOUT_SEC))) {
				DBGLOG(SCAN, TRACE, "Skip old record of BSS Descriptor - BSSID:[%pM]\n\n",
						     prBssDesc->aucBSSID);
				continue;
			}
		}

		if ((prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) &&
		    (prAdapter->eConnectionState == MEDIA_STATE_CONNECTED)) {
			OS_SYSTIME rCurrentTime;

			GET_CURRENT_SYSTIME(&rCurrentTime);
			if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rUpdateTime,
					      SEC_TO_SYSTIME(BSS_DESC_TIMEOUT_SEC))) {
				DBGLOG(SCAN, TRACE, "Skip old record of BSS Descriptor - BSSID:[%pM]\n\n",
						     (prBssDesc->aucBSSID));
				continue;
			}
		}
		
		
		if (prPrimaryBssDesc->eBSSType == BSS_TYPE_IBSS) {
			
			if (ibssCheckCapabilityForAdHocMode(prAdapter, prPrimaryBssDesc) == WLAN_STATUS_FAILURE) {

				DBGLOG(SCAN, TRACE,
				       "Ignore BSS DESC MAC: %pM, Capability not supported for AdHoc Mode.\n",
					prPrimaryBssDesc->aucBSSID);

				continue;
			}
			
			if (prAdapter->fgIsIBSSActive &&
			    UNEQUAL_MAC_ADDR(prBssInfo->aucBSSID, prPrimaryBssDesc->aucBSSID)) {

				if (!fgIsLocalTSFRead) {
					NIC_GET_CURRENT_TSF(prAdapter, &rCurrentTsf);

					DBGLOG(SCAN, TRACE,
					       "\n\nCurrent TSF : %08lx-%08lx\n\n",
						rCurrentTsf.u.HighPart, rCurrentTsf.u.LowPart);
				}

				if (rCurrentTsf.QuadPart > prPrimaryBssDesc->u8TimeStamp.QuadPart) {
					DBGLOG(SCAN, TRACE,
					       "Ignore BSS DESC MAC: [%pM], Current BSSID: [%pM].\n",
						prPrimaryBssDesc->aucBSSID, prBssInfo->aucBSSID);

					DBGLOG(SCAN, TRACE,
					       "\n\nBSS's TSF : %08lx-%08lx\n\n",
						prPrimaryBssDesc->u8TimeStamp.u.HighPart,
						prPrimaryBssDesc->u8TimeStamp.u.LowPart);

					prPrimaryBssDesc->fgIsLargerTSF = FALSE;
					continue;
				} else {
					prPrimaryBssDesc->fgIsLargerTSF = TRUE;
				}

			}
		}
		
		if (rsnPerformPolicySelection(prPrimaryBssDesc)) {

			if (prPrimaryBssDesc->ucEncLevel > 0) {
				fgIsFindBestEncryptionLevel = TRUE;

				fgIsFindFirst = FALSE;
			}
		} else {
			
			continue;
		}

		
		if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2) {
			rsnUpdatePmkidCandidateList(prPrimaryBssDesc);
			if (prAdapter->rWifiVar.rAisBssInfo.u4PmkidCandicateCount)
				prAdapter->rWifiVar.rAisBssInfo.fgIndicatePMKID = rsnCheckPmkidCandicate();
		}
#endif

		prPrimaryBssDesc = (P_BSS_DESC_T) NULL;

		
		switch (prConnSettings->eConnectionPolicy) {
		case CONNECT_BY_SSID_BEST_RSSI:
			
			if (prAdapter->rWifiVar.fgEnableJoinToHiddenSSID && prBssDesc->fgIsHiddenSSID) {
				if (prConnSettings->ucSSIDLen) {
					prPrimaryBssDesc = prBssDesc;
					fgIsFindBestRSSI = TRUE;
				}

			} else if (EQUAL_SSID(prBssDesc->aucSSID,
					      prBssDesc->ucSSIDLen,
					      prConnSettings->aucSSID, prConnSettings->ucSSIDLen)) {
				prPrimaryBssDesc = prBssDesc;
				fgIsFindBestRSSI = TRUE;

				DBGLOG(SCN, TRACE, "SEARCH: fgIsFindBestRSSI=TRUE, %d, prPrimaryBssDesc=[ %pM ]\n",
						   prBssDesc->ucRCPI, prPrimaryBssDesc->aucBSSID);
			}
			break;

		case CONNECT_BY_SSID_ANY:
			if (!prBssDesc->fgIsHiddenSSID) {
				prPrimaryBssDesc = prBssDesc;
				fgIsFindFirst = TRUE;
			}
			break;

		case CONNECT_BY_BSSID:
			if (EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prConnSettings->aucBSSID))
				prPrimaryBssDesc = prBssDesc;
			break;

		default:
			break;
		}

		
		if (prPrimaryBssDesc == NULL)
			continue;
		
		if (prPrimaryBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {
#if CFG_SUPPORT_WAPI
			if (prAdapter->rWifiVar.rConnSettings.fgWapiMode) {
				DBGLOG(SCN, TRACE, "SEARCH: fgWapiMode == 1\n");

				if (wapiPerformPolicySelection(prAdapter, prPrimaryBssDesc)) {
					fgIsFindFirst = TRUE;
				} else {
					
					DBGLOG(SCN, TRACE, "SEARCH: WAPI cannot pass the Encryption Status Check!\n");
					continue;
				}
			} else
#endif
#if CFG_RSN_MIGRATION
			if (rsnPerformPolicySelection(prAdapter, prPrimaryBssDesc)) {
				if (prAisSpecBssInfo->fgCounterMeasure) {
					DBGLOG(RSN, INFO, "Skip while at counter measure period!!!\n");
					continue;
				}

				if (prPrimaryBssDesc->ucEncLevel > 0) {
					fgIsFindBestEncryptionLevel = TRUE;
					fgIsFindFirst = FALSE;
				}
#if 0
			
			if (prAdapter->rWifiVar.rConnSettings.eAuthMode == AUTH_MODE_WPA2) {
				rsnUpdatePmkidCandidateList(prPrimaryBssDesc);
				if (prAisSpecBssInfo->u4PmkidCandicateCount) {
					if (rsnCheckPmkidCandicate()) {
						DBGLOG(RSN, WARN,
						       "Prepare a timer to indicate candidate %pM\n",
							(prAisSpecBssInfo->arPmkidCache
								[prAisSpecBssInfo->u4PmkidCacheCount].
								rBssidInfo.aucBssid)));
						cnmTimerStopTimer(&prAisSpecBssInfo->rPreauthenticationTimer);
						cnmTimerStartTimer(&prAisSpecBssInfo->rPreauthenticationTimer,
								   SEC_TO_MSEC
								   (WAIT_TIME_IND_PMKID_CANDICATE_SEC));
					}
				}
			}
#endif
			} else {
				
				continue;
			}
#endif
		} else {
			
		}

		prPrimaryStaRec = prStaRec;

		
		if (!prCandidateBssDesc) {
			prCandidateBssDesc = prPrimaryBssDesc;
			prCandidateStaRec = prPrimaryStaRec;

			
			if (fgIsFindFirst)
				break;
		} else {
#if 0				
	
	if (fgIsFindBestEncryptionLevel) {
		if (prCandidateBssDesc->ucEncLevel < prPrimaryBssDesc->ucEncLevel) {

			prCandidateBssDesc = prPrimaryBssDesc;
			prCandidateStaRec = prPrimaryStaRec;
			continue;
		}
	}


	
	
	if (!prCandidateBssDesc->fgIsConnected && !prPrimaryBssDesc->fgIsConnected) {
		if ((prCandidateStaRec != (P_STA_RECORD_T) NULL) &&
		    (prCandidateStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL)) {

			DBGLOG(SCAN, TRACE,
			       "So far -BSS DESC MAC: %pM has nonzero Status Code = %d\n",
				prCandidateBssDesc->aucBSSID,
				prCandidateStaRec->u2StatusCode);

			if (prPrimaryStaRec != (P_STA_RECORD_T) NULL) {
				if (prPrimaryStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL) {

					
					if (TIME_BEFORE(prCandidateStaRec->rLastJoinTime,
							prPrimaryStaRec->rLastJoinTime)) {
						continue;
					}
					else {
						prCandidateBssDesc = prPrimaryBssDesc;
						prCandidateStaRec = prPrimaryStaRec;
						continue;
					}
				}
				
				else {
					prCandidateBssDesc = prPrimaryBssDesc;
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			}
			
			else {
				prCandidateBssDesc = prPrimaryBssDesc;
				prCandidateStaRec = prPrimaryStaRec;
				continue;
			}
		} else {
			if ((prPrimaryStaRec != (P_STA_RECORD_T) NULL) &&
			    (prPrimaryStaRec->u2StatusCode != STATUS_CODE_SUCCESSFUL)) {
				continue;
			}
		}
	}
#endif

			
			if (prCandidateBssDesc->fgIsHiddenSSID) {
				if (!prPrimaryBssDesc->fgIsHiddenSSID) {
					prCandidateBssDesc = prPrimaryBssDesc;	
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			} else {
				if (prPrimaryBssDesc->fgIsHiddenSSID)
					continue;
			}

			
			if (fgIsFindBestRSSI) {
				INT_32 u4PrimAdjRssi = scanResultsAdjust5GPref(prPrimaryBssDesc);
				INT_32 u4CandAdjRssi = scanResultsAdjust5GPref(prCandidateBssDesc);

				DBGLOG(SCN, TRACE,
					"Candidate [" MACSTR "]: RCPI=%d, RSSI=%d, joinFailCnt=%d, Primary ["
					MACSTR "]: RCPI=%d, RSSI=%d, joinFailCnt=%d\n",
					MAC2STR(prCandidateBssDesc->aucBSSID),
					prCandidateBssDesc->ucRCPI, u4CandAdjRssi,
					prCandidateBssDesc->ucJoinFailureCount,
					MAC2STR(prPrimaryBssDesc->aucBSSID),
					prPrimaryBssDesc->ucRCPI, u4PrimAdjRssi,
					prPrimaryBssDesc->ucJoinFailureCount);

				ASSERT(!(prCandidateBssDesc->fgIsConnected && prPrimaryBssDesc->fgIsConnected));
				if (prPrimaryBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD) {
					if (CHECK_FOR_TIMEOUT(rCurrentTime, prBssDesc->rJoinFailTime,
							SEC_TO_SYSTIME(SCN_BSS_JOIN_FAIL_CNT_RESET_SEC))) {
						prBssDesc->ucJoinFailureCount = SCN_BSS_JOIN_FAIL_THRESOLD -
										SCN_BSS_JOIN_FAIL_RESET_STEP;
						DBGLOG(SCN, INFO,
						"decrease join fail count for Bss %pM to %u, timeout second %d\n",
							prBssDesc->aucBSSID, prBssDesc->ucJoinFailureCount,
							SCN_BSS_JOIN_FAIL_CNT_RESET_SEC);
					}
				}
				if (prCandidateBssDesc->fgIsConnected) {
					if (u4CandAdjRssi < u4PrimAdjRssi &&
						prPrimaryBssDesc->ucJoinFailureCount < SCN_BSS_JOIN_FAIL_THRESOLD) {
						prCandidateBssDesc = prPrimaryBssDesc;
						prCandidateStaRec = prPrimaryStaRec;
						continue;
					}
				} else if (prPrimaryBssDesc->fgIsConnected) {
					if (u4CandAdjRssi < u4PrimAdjRssi ||
						(prCandidateBssDesc->ucJoinFailureCount >=
						SCN_BSS_JOIN_FAIL_THRESOLD)) {
						prCandidateBssDesc = prPrimaryBssDesc;
						prCandidateStaRec = prPrimaryStaRec;
						continue;
					}
				} else if (prPrimaryBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD)
					continue;
				else if (prCandidateBssDesc->ucJoinFailureCount >= SCN_BSS_JOIN_FAIL_THRESOLD ||
					u4CandAdjRssi < u4PrimAdjRssi) {
					prCandidateBssDesc = prPrimaryBssDesc;
					prCandidateStaRec = prPrimaryStaRec;
					continue;
				}
			}
#if 0
			
			if (fgIsFindMinChannelLoad) {
				
				
			}
#endif
		}
	}

	if (prCandidateBssDesc != NULL) {
		DBGLOG(SCN, INFO,
		       "SEARCH: Candidate BSS: %pM, RSSI = %d\n", prCandidateBssDesc->aucBSSID,
		       RCPI_TO_dBm(prCandidateBssDesc->ucRCPI));
	} else {
		DBGLOG(SCN, WARN, "SEARCH: Candidate BSS is NULL\n");
		
		LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
			if (EQUAL_SSID(prBssDesc->aucSSID, prBssDesc->ucSSIDLen,
							prConnSettings->aucSSID, prConnSettings->ucSSIDLen)) {
				fgIsAbsentCandidateBss = FALSE;
				DBGLOG(SCN, INFO, "Find %s [%pM] in %d BSS!\n", prBssDesc->aucSSID
					, prBssDesc->aucBSSID
					, (UINT_32) prBSSDescList->u4NumElem);
			}
		}
		if (fgIsAbsentCandidateBss == TRUE)
			DBGLOG(SCN, WARN, "Driver can't find :%s in %d BSS list!\n", prConnSettings->aucSSID
					, (UINT_32) prBSSDescList->u4NumElem);
	}


	return prCandidateBssDesc;

} 

#if CFG_SUPPORT_AGPS_ASSIST
VOID scanReportScanResultToAgps(P_ADAPTER_T prAdapter)
{
	P_LINK_T prBSSDescList = &prAdapter->rWifiVar.rScanInfo.rBSSDescList;
	P_BSS_DESC_T prBssDesc = NULL;
	P_AGPS_AP_LIST_T prAgpsApList;
	P_AGPS_AP_INFO_T prAgpsInfo;
	P_SCAN_INFO_T prScanInfo = &prAdapter->rWifiVar.rScanInfo;
	UINT_8 ucIndex = 0;

	prAgpsApList = kalMemAlloc(sizeof(AGPS_AP_LIST_T), VIR_MEM_TYPE);
	if (!prAgpsApList)
		return;

	prAgpsInfo = &prAgpsApList->arApInfo[0];
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		if (prBssDesc->rUpdateTime < prScanInfo->rLastScanCompletedTime)
			continue;
		COPY_MAC_ADDR(prAgpsInfo->aucBSSID, prBssDesc->aucBSSID);
		prAgpsInfo->ePhyType = AGPS_PHY_G;
		prAgpsInfo->u2Channel = prBssDesc->ucChannelNum;
		prAgpsInfo->i2ApRssi = RCPI_TO_dBm(prBssDesc->ucRCPI);
		prAgpsInfo++;
		ucIndex++;
		if (ucIndex == 32)
			break;
	}
	prAgpsApList->ucNum = ucIndex;
	GET_CURRENT_SYSTIME(&prScanInfo->rLastScanCompletedTime);
	
	kalIndicateAgpsNotify(prAdapter, AGPS_EVENT_WLAN_AP_LIST, (PUINT_8) prAgpsApList, sizeof(AGPS_AP_LIST_T));
	kalMemFree(prAgpsApList, VIR_MEM_TYPE, sizeof(AGPS_AP_LIST_T));
}
#endif
