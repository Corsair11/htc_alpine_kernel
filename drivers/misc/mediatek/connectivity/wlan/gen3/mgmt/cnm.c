



#include "precomp.h"








VOID cnmInit(P_ADAPTER_T prAdapter)
{
	P_CNM_INFO_T prCnmInfo;

	ASSERT(prAdapter);

	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->fgChGranted = FALSE;
	prCnmInfo->ucReqChnPrivilegeCnt = 0;
	prCnmInfo->rReqChnTimeoutTimer.ulDataPtr = 0;

	cnmTimerInitTimer(prAdapter, &prCnmInfo->rReqChnlUtilTimer,
				  (PFN_MGMT_TIMEOUT_FUNC)cnmRunEventReqChnlUtilTimeout, (ULONG) NULL);

}				

VOID cnmUninit(P_ADAPTER_T prAdapter)
{
	cnmTimerStopTimer(prAdapter, &prAdapter->rCnmInfo.rReqChnlUtilTimer);
	cnmTimerStopTimer(prAdapter, &prAdapter->rCnmInfo.rReqChnTimeoutTimer);
}				

VOID cnmChMngrRequestPrivilege(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_MSG_CH_REQ_T prMsgChReq;
	P_CMD_CH_PRIVILEGE_T prCmdBody;
	WLAN_STATUS rStatus;
	P_CNM_INFO_T prCnmInfo;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChReq = (P_MSG_CH_REQ_T) prMsgHdr;
	prCnmInfo = &prAdapter->rCnmInfo;

	prCmdBody = (P_CMD_CH_PRIVILEGE_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_CH_PRIVILEGE_T));
	ASSERT(prCmdBody);

	
	if (!prCmdBody) {
		DBGLOG(CNM, ERROR, "ChReq: fail to get buf (net=%d, token=%d)\n",
				    prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID);

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	DBGLOG(CNM, INFO, "ChReq net=%d token=%d b=%d c=%d s=%d w=%d s1=%d s2=%d\n",
			   prMsgChReq->ucBssIndex, prMsgChReq->ucTokenID,
			   prMsgChReq->eRfBand, prMsgChReq->ucPrimaryChannel,
			   prMsgChReq->eRfSco, prMsgChReq->eRfChannelWidth,
			   prMsgChReq->ucRfCenterFreqSeg1, prMsgChReq->ucRfCenterFreqSeg2);

	prCmdBody->ucBssIndex = prMsgChReq->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChReq->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_REQ;	
	prCmdBody->ucPrimaryChannel = prMsgChReq->ucPrimaryChannel;
	prCmdBody->ucRfSco = (UINT_8) prMsgChReq->eRfSco;
	prCmdBody->ucRfBand = (UINT_8) prMsgChReq->eRfBand;
	prCmdBody->ucRfChannelWidth = (UINT_8) prMsgChReq->eRfChannelWidth;
	prCmdBody->ucRfCenterFreqSeg1 = (UINT_8) prMsgChReq->ucRfCenterFreqSeg1;
	prCmdBody->ucRfCenterFreqSeg2 = (UINT_8) prMsgChReq->ucRfCenterFreqSeg2;
	prCmdBody->ucReqType = (UINT_8) prMsgChReq->eReqType;
	prCmdBody->aucReserved[0] = 0;
	prCmdBody->aucReserved[1] = 0;
	prCmdBody->u4MaxInterval = prMsgChReq->u4MaxInterval;

	ASSERT(prCmdBody->ucBssIndex <= MAX_BSS_INDEX);

	
	if (prCmdBody->ucBssIndex > MAX_BSS_INDEX)
		DBGLOG(CNM, ERROR, "CNM: ChReq with wrong netIdx=%d\n\n", prCmdBody->ucBssIndex);

	cnmTimerStopTimer(prAdapter, &prCnmInfo->rReqChnTimeoutTimer);
	if (prCnmInfo->rReqChnTimeoutTimer.ulDataPtr &&
		((P_MSG_CH_REQ_T)prCnmInfo->rReqChnTimeoutTimer.ulDataPtr)->ucTokenID != prMsgChReq->ucTokenID)
		cnmMemFree(prAdapter, (PVOID)prCnmInfo->rReqChnTimeoutTimer.ulDataPtr);

	cnmTimerInitTimer(prAdapter,
		  &prCnmInfo->rReqChnTimeoutTimer,
		  (PFN_MGMT_TIMEOUT_FUNC) cnmChReqPrivilegeTimeout, (ULONG)prMsgHdr);
	cnmTimerStartTimer(prAdapter, &prCnmInfo->rReqChnTimeoutTimer, 4000);

	rStatus = wlanSendSetQueryCmd(prAdapter,	
				      CMD_ID_CH_PRIVILEGE,	
				      TRUE,	
				      FALSE,	
				      FALSE,	
				      NULL,	
				      NULL,	
				      sizeof(CMD_CH_PRIVILEGE_T),	
				      (PUINT_8) prCmdBody,	
				      NULL,	
				      0	
	    );

	

	cnmMemFree(prAdapter, prCmdBody);
}				

VOID cnmChMngrAbortPrivilege(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	P_MSG_CH_ABORT_T prMsgChAbort;
	P_CMD_CH_PRIVILEGE_T prCmdBody;
	P_CNM_INFO_T prCnmInfo;
	WLAN_STATUS rStatus;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prMsgChAbort = (P_MSG_CH_ABORT_T) prMsgHdr;

	
	prCnmInfo = &prAdapter->rCnmInfo;
	if (prCnmInfo->fgChGranted &&
	    prCnmInfo->ucBssIndex == prMsgChAbort->ucBssIndex && prCnmInfo->ucTokenID == prMsgChAbort->ucTokenID) {

		prCnmInfo->fgChGranted = FALSE;
	}

	cnmTimerStopTimer(prAdapter, &prCnmInfo->rReqChnTimeoutTimer);
	cnmMemFree(prAdapter, (PVOID)prCnmInfo->rReqChnTimeoutTimer.ulDataPtr);
	prCnmInfo->rReqChnTimeoutTimer.ulDataPtr = 0;
	prCnmInfo->ucReqChnPrivilegeCnt = 0;

	prCmdBody = (P_CMD_CH_PRIVILEGE_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_BUF, sizeof(CMD_CH_PRIVILEGE_T));
	ASSERT(prCmdBody);

	
	if (!prCmdBody) {
		DBGLOG(CNM, ERROR, "ChAbort: fail to get buf (net=%d, token=%d)\n",
				    prMsgChAbort->ucBssIndex, prMsgChAbort->ucTokenID);

		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prCmdBody->ucBssIndex = prMsgChAbort->ucBssIndex;
	prCmdBody->ucTokenID = prMsgChAbort->ucTokenID;
	prCmdBody->ucAction = CMD_CH_ACTION_ABORT;	

	DBGLOG(CNM, INFO, "ChAbort net=%d token=%d\n", prCmdBody->ucBssIndex, prCmdBody->ucTokenID);

	ASSERT(prCmdBody->ucBssIndex <= MAX_BSS_INDEX);

	
	if (prCmdBody->ucBssIndex > MAX_BSS_INDEX)
		DBGLOG(CNM, ERROR, "CNM: ChAbort with wrong netIdx=%d\n\n", prCmdBody->ucBssIndex);

	rStatus = wlanSendSetQueryCmd(prAdapter,	
				      CMD_ID_CH_PRIVILEGE,	
				      TRUE,	
				      FALSE,	
				      FALSE,	
				      NULL,	
				      NULL,	
				      sizeof(CMD_CH_PRIVILEGE_T),	
				      (PUINT_8) prCmdBody,	
				      NULL,	
				      0	
	    );

	

	cnmMemFree(prAdapter, prCmdBody);
	cnmMemFree(prAdapter, prMsgHdr);

}				

VOID cnmChMngrHandleChEvent(P_ADAPTER_T prAdapter, P_WIFI_EVENT_T prEvent)
{
	P_EVENT_CH_PRIVILEGE_T prEventBody;
	P_MSG_CH_GRANT_T prChResp;
	P_BSS_INFO_T prBssInfo;
	P_CNM_INFO_T prCnmInfo;

	ASSERT(prAdapter);
	ASSERT(prEvent);

	prCnmInfo = &prAdapter->rCnmInfo;
	prEventBody = (P_EVENT_CH_PRIVILEGE_T) (prEvent->aucBuffer);

	
#if 0
	prChResp = (P_MSG_CH_GRANT_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_GRANT_T));
	ASSERT(prChResp);

	
	if (!prChResp) {
		DBGLOG(CNM, ERROR, "ChGrant: fail to get buf (net=%d, token=%d)\n",
				    prEventBody->ucBssIndex, prEventBody->ucTokenID);

		return;
	}
#endif

	DBGLOG(CNM, INFO, "ChGrant net=%d token=%d ch=%d sco=%d\n",
			   prEventBody->ucBssIndex, prEventBody->ucTokenID,
			   prEventBody->ucPrimaryChannel, prEventBody->ucRfSco);

	ASSERT(prEventBody->ucBssIndex <= MAX_BSS_INDEX);
	ASSERT(prEventBody->ucStatus == EVENT_CH_STATUS_GRANT);

	prBssInfo = prAdapter->aprBssInfo[prEventBody->ucBssIndex];
	if (prCnmInfo->rReqChnTimeoutTimer.ulDataPtr &&
		((P_MSG_CH_REQ_T)prCnmInfo->rReqChnTimeoutTimer.ulDataPtr)->ucTokenID == prEventBody->ucTokenID) {
		cnmTimerStopTimer(prAdapter, &prCnmInfo->rReqChnTimeoutTimer);
		cnmMemFree(prAdapter, (PVOID)prCnmInfo->rReqChnTimeoutTimer.ulDataPtr);
		prCnmInfo->rReqChnTimeoutTimer.ulDataPtr = 0;
		prCnmInfo->ucReqChnPrivilegeCnt = 0;
	}
	
	
	if (IS_BSS_AIS(prBssInfo))
		;
#if CFG_ENABLE_WIFI_DIRECT
	else if (prAdapter->fgIsP2PRegistered && IS_BSS_P2P(prBssInfo))
		;
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	else if (IS_BSS_BOW(prBssInfo))
		;
#endif
	else {
		
		DBGLOG(CNM, INFO, "prBssInfo->eNetworkType = %d\n", prBssInfo->eNetworkType);
		return;
	}

	
	prChResp = (P_MSG_CH_GRANT_T)
	    cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_GRANT_T));
	ASSERT(prChResp);

	
	if (!prChResp) {
		DBGLOG(CNM, ERROR, "ChGrant: fail to get buf (net=%d, token=%d)\n",
				    prEventBody->ucBssIndex, prEventBody->ucTokenID);

		return;
	}

	
	if (IS_BSS_AIS(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_AIS_CH_GRANT;
#if CFG_ENABLE_WIFI_DIRECT
	else if (prAdapter->fgIsP2PRegistered && IS_BSS_P2P(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_P2P_CH_GRANT;
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	else if (IS_BSS_BOW(prBssInfo))
		prChResp->rMsgHdr.eMsgId = MID_CNM_BOW_CH_GRANT;
#endif
	else {
		cnmMemFree(prAdapter, prChResp);
		return;
	}

	prChResp->ucBssIndex = prEventBody->ucBssIndex;
	prChResp->ucTokenID = prEventBody->ucTokenID;
	prChResp->ucPrimaryChannel = prEventBody->ucPrimaryChannel;
	prChResp->eRfSco = (ENUM_CHNL_EXT_T) prEventBody->ucRfSco;
	prChResp->eRfBand = (ENUM_BAND_T) prEventBody->ucRfBand;
	prChResp->eRfChannelWidth = (ENUM_CHANNEL_WIDTH_T) prEventBody->ucRfChannelWidth;
	prChResp->ucRfCenterFreqSeg1 = prEventBody->ucRfCenterFreqSeg1;
	prChResp->ucRfCenterFreqSeg2 = prEventBody->ucRfCenterFreqSeg2;
	prChResp->eReqType = (ENUM_CH_REQ_TYPE_T) prEventBody->ucReqType;
	prChResp->u4GrantInterval = prEventBody->u4GrantInterval;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prChResp, MSG_SEND_METHOD_BUF);

	
	prCnmInfo = &prAdapter->rCnmInfo;
	prCnmInfo->ucBssIndex = prEventBody->ucBssIndex;
	prCnmInfo->ucTokenID = prEventBody->ucTokenID;
	prCnmInfo->fgChGranted = TRUE;
}

BOOLEAN
cnmPreferredChannel(P_ADAPTER_T prAdapter, P_ENUM_BAND_T prBand, PUINT_8 pucPrimaryChannel, P_ENUM_CHNL_EXT_T prBssSCO)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);
	ASSERT(prBand);
	ASSERT(pucPrimaryChannel);
	ASSERT(prBssSCO);

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

		if (prBssInfo) {
			if (IS_BSS_AIS(prBssInfo) && RLM_NET_PARAM_VALID(prBssInfo)) {
				*prBand = prBssInfo->eBand;
				*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;
				*prBssSCO = prBssInfo->eBssSCO;

				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOLEAN cnmAisInfraChannelFixed(P_ADAPTER_T prAdapter, P_ENUM_BAND_T prBand, PUINT_8 pucPrimaryChannel)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

#if 0
		DBGLOG(INIT, INFO, "%s BSS[%u] active[%u] netType[%u]\n",
				    __func__, i, prBssInfo->fgIsNetActive, prBssInfo->eNetworkType;
#endif

		if (!IS_NET_ACTIVE(prAdapter, i))
			continue;

#if CFG_ENABLE_WIFI_DIRECT
		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			BOOLEAN fgFixedChannel = p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings);

			if (fgFixedChannel) {

				*prBand = prBssInfo->eBand;
				*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;

				return TRUE;

			}
		}
#endif

#if CFG_ENABLE_BT_OVER_WIFI && CFG_BOW_LIMIT_AIS_CHNL
		if (prBssInfo->eNetworkType == NETWORK_TYPE_BOW) {
			*prBand = prBssInfo->eBand;
			*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;

			return TRUE;
		}
#endif

	}

	return FALSE;
}

#if CFG_SUPPORT_CHNL_CONFLICT_REVISE
BOOLEAN cnmAisDetectP2PChannel(P_ADAPTER_T prAdapter, P_ENUM_BAND_T prBand, PUINT_8 pucPrimaryChannel)
{
	UINT_8 i = 0;
	P_BSS_INFO_T prBssInfo;
#if CFG_ENABLE_WIFI_DIRECT
	for (; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];
		if (prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
			continue;
		if (prBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED ||
		    (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT && prBssInfo->eIntendOPMode == OP_MODE_NUM)) {
			*prBand = prBssInfo->eBand;
			*pucPrimaryChannel = prBssInfo->ucPrimaryChannel;
			return TRUE;
		}
	}
#endif
	return FALSE;
}
#endif

VOID cnmAisInfraConnectNotify(P_ADAPTER_T prAdapter)
{
#if CFG_ENABLE_BT_OVER_WIFI
	P_BSS_INFO_T prBssInfo, prAisBssInfo, prBowBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	prAisBssInfo = NULL;
	prBowBssInfo = NULL;

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)) {
			if (IS_BSS_AIS(prBssInfo))
				prAisBssInfo = prBssInfo;
			else if (IS_BSS_BOW(prBssInfo))
				prBowBssInfo = prBssInfo;
		}
	}

	if (prAisBssInfo && prBowBssInfo && RLM_NET_PARAM_VALID(prAisBssInfo) && RLM_NET_PARAM_VALID(prBowBssInfo)) {
		if (prAisBssInfo->eBand != prBowBssInfo->eBand ||
		    prAisBssInfo->ucPrimaryChannel != prBowBssInfo->ucPrimaryChannel) {

			
			bowNotifyAllLinkDisconnected(prAdapter);
		}
	}
#endif
}

BOOLEAN cnmAisIbssIsPermitted(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	
	for (i = 0; i <= BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) && !IS_BSS_AIS(prBssInfo))
			return FALSE;
	}

	return TRUE;
}

BOOLEAN cnmP2PIsPermitted(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;
	BOOLEAN fgBowIsActive;

	ASSERT(prAdapter);

	fgBowIsActive = FALSE;

	for (i = 0; i < BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo)) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS)
				return FALSE;
			else if (IS_BSS_BOW(prBssInfo))
				fgBowIsActive = TRUE;
		}
	}

#if CFG_ENABLE_BT_OVER_WIFI
	if (fgBowIsActive) {
		
		bowNotifyAllLinkDisconnected(prAdapter);
	}
#endif

	return TRUE;
}

BOOLEAN cnmBowIsPermitted(P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 i;

	ASSERT(prAdapter);

	
	for (i = 0; i <= BSS_INFO_NUM; i++) {
		prBssInfo = prAdapter->aprBssInfo[i];

		if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) &&
		    (IS_BSS_P2P(prBssInfo) || prBssInfo->eCurrentOPMode == OP_MODE_IBSS)) {
			return FALSE;
		}
	}

	return TRUE;
}

static UINT_8 cnmGetAPBwPermitted(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucAPBandwidth = MAX_BW_80MHZ;
	P_BSS_DESC_T    prBssDesc = NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	
	prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[0];

	if (IS_BSS_AIS(prBssInfo)) {
		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
	} else if (IS_BSS_P2P(prBssInfo)) {
		
		if (!p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			if (prP2pRoleFsmInfo)
				prBssDesc = prP2pRoleFsmInfo->rJoinInfo.prTargetBssDesc;
		}
	}

	if (prBssDesc) {
		if (prBssDesc->eChannelWidth == CW_20_40MHZ) {
			if ((prBssDesc->eSco == CHNL_EXT_SCA) || (prBssDesc->eSco == CHNL_EXT_SCB))
				ucAPBandwidth = MAX_BW_40MHZ;
			else
				ucAPBandwidth = MAX_BW_20MHZ;
		}
#if (CFG_FORCE_USE_20BW == 1)
		if (prBssDesc->eBand == BAND_2G4)
			ucAPBandwidth = MAX_BW_20MHZ;
#endif
	}

	return ucAPBandwidth;
}

BOOLEAN cnmBss40mBwPermitted(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	UINT_8 ucAPBandwidth;

	ASSERT(prAdapter);


	ucAPBandwidth = cnmGetAPBwPermitted(prAdapter, ucBssIndex);

	
	if ((cnmGetBssMaxBw(prAdapter, ucBssIndex) < MAX_BW_40MHZ) || (ucAPBandwidth < MAX_BW_40MHZ))
		return FALSE;
#if 0
	
	for (i = 0; i < BSS_INFO_NUM; i++) {
		if (i != ucBssIndex) {
			prBssInfo = prAdapter->aprBssInfo[i];

			if (prBssInfo && IS_BSS_ACTIVE(prBssInfo) &&
			    (prBssInfo->fg40mBwAllowed || prBssInfo->fgAssoc40mBwAllowed))
				return FALSE;
		}
	}
#endif

	return TRUE;
}

BOOLEAN cnmBss40mBwPermittedForJoin(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	UINT_8 ucAPBandwidth;
	P_BSS_DESC_T prBssDesc = NULL;
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucMaxBandwidth = MAX_BW_80MHZ;

	ASSERT(prAdapter);

	ucAPBandwidth = cnmGetAPBwPermitted(prAdapter, ucBssIndex);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo)) {
		
		prBssDesc = prAdapter->rWifiVar.rAisFsmInfo.prTargetBssDesc;
		if (prBssDesc->eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		
		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings))
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp5gBandwidth;
		else {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p5gBandwidth;
		}
	}

	
	if ((ucMaxBandwidth < MAX_BW_40MHZ) || (ucAPBandwidth < MAX_BW_40MHZ))
		return FALSE;

	return TRUE;
}

BOOLEAN cnmBss80mBwPermitted(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	UINT_8 ucAPBandwidth;

	ASSERT(prAdapter);


	ucAPBandwidth = cnmGetAPBwPermitted(prAdapter, ucBssIndex);

	
	if ((cnmGetBssMaxBw(prAdapter, ucBssIndex) < MAX_BW_80MHZ) || (ucAPBandwidth < MAX_BW_80MHZ))
		return FALSE;

	return TRUE;
}

UINT_8 cnmGetBssMaxBw(P_ADAPTER_T prAdapter, UINT_8 ucBssIndex)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucMaxBandwidth = MAX_BW_80MHZ;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (IS_BSS_AIS(prBssInfo)) {
		
		if (prBssInfo->eBand == BAND_2G4)
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta2gBandwidth;
		else
			ucMaxBandwidth = prAdapter->rWifiVar.ucSta5gBandwidth;

		if (ucMaxBandwidth > prAdapter->rWifiVar.ucStaBandwidth)
			ucMaxBandwidth = prAdapter->rWifiVar.ucStaBandwidth;
	} else if (IS_BSS_P2P(prBssInfo)) {
		
		if (p2pFuncIsAPMode(prAdapter->rWifiVar.prP2PConnSettings)) {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucAp5gBandwidth;
		}
		
		else {
			if (prBssInfo->eBand == BAND_2G4)
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p2gBandwidth;
			else
				ucMaxBandwidth = prAdapter->rWifiVar.ucP2p5gBandwidth;
		}
	}

	return ucMaxBandwidth;
}

P_BSS_INFO_T cnmGetBssInfoAndInit(P_ADAPTER_T prAdapter, ENUM_NETWORK_TYPE_T eNetworkType, BOOLEAN fgIsP2pDevice)
{
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucBssIndex, ucOwnMacIdx;

	ASSERT(prAdapter);

	if (eNetworkType == NETWORK_TYPE_P2P && fgIsP2pDevice) {
		prBssInfo = prAdapter->aprBssInfo[P2P_DEV_BSS_INDEX];

		prBssInfo->fgIsInUse = TRUE;
		prBssInfo->ucBssIndex = P2P_DEV_BSS_INDEX;
		prBssInfo->eNetworkType = eNetworkType;
		prBssInfo->ucOwnMacIndex = HW_BSSID_NUM;
#if CFG_SUPPORT_PNO
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
#endif
		prBssInfo->ucKeyCmdAction = SEC_TX_KEY_COMMAND;
		return prBssInfo;
	}

	ucOwnMacIdx = (eNetworkType == NETWORK_TYPE_MBSS) ? 0 : 1;

	
	do {
		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

			if (prBssInfo && prBssInfo->fgIsInUse && ucOwnMacIdx == prBssInfo->ucOwnMacIndex)
				break;
		}

		if (ucBssIndex >= BSS_INFO_NUM)
			break;	
	} while (++ucOwnMacIdx < HW_BSSID_NUM);

	
	for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
		prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

		if (prBssInfo && !prBssInfo->fgIsInUse) {
			prBssInfo->fgIsInUse = TRUE;
			prBssInfo->ucBssIndex = ucBssIndex;
			prBssInfo->eNetworkType = eNetworkType;
			prBssInfo->ucOwnMacIndex = ucOwnMacIdx;
			break;
		}
	}

	if (ucOwnMacIdx >= HW_BSSID_NUM || ucBssIndex >= BSS_INFO_NUM)
		prBssInfo = NULL;
#if CFG_SUPPORT_PNO
	if (prBssInfo) {
		prBssInfo->fgIsPNOEnable = FALSE;
		prBssInfo->fgIsNetRequestInActive = FALSE;
	}
#endif
	if (prBssInfo)
		prBssInfo->ucKeyCmdAction = SEC_TX_KEY_COMMAND;
	return prBssInfo;
}

VOID cnmFreeBssInfo(P_ADAPTER_T prAdapter, P_BSS_INFO_T prBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prBssInfo);

	prBssInfo->fgIsInUse = FALSE;
}

VOID cnmRunEventReqChnlUtilTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_CNM_INFO_T prCnmInfo = &prAdapter->rCnmInfo;
	struct MSG_CH_UTIL_RSP *prMsgChUtil = NULL;
	P_MSG_SCN_SCAN_REQ prScanReqMsg = NULL;

	DBGLOG(CNM, INFO, "Request Channel Utilization timeout\n");
	wlanReleasePendingCmdById(prAdapter, CMD_ID_REQ_CHNL_UTILIZATION);
	prMsgChUtil = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prMsgChUtil));
	kalMemZero(prMsgChUtil, sizeof(*prMsgChUtil));
	prMsgChUtil->rMsgHdr.eMsgId = prCnmInfo->u2ReturnMID;
	prMsgChUtil->ucChnlNum = 0;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgChUtil, MSG_SEND_METHOD_BUF);
	
	prScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prScanReqMsg));
	kalMemZero(prScanReqMsg, sizeof(*prScanReqMsg));
	prScanReqMsg->rMsgHdr.eMsgId = MID_MNY_CNM_SCAN_CONTINUE;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prScanReqMsg, MSG_SEND_METHOD_BUF);
}

VOID cnmHandleChannelUtilization(P_ADAPTER_T prAdapter,
	struct EVENT_RSP_CHNL_UTILIZATION *prChnlUtil)
{
	P_CNM_INFO_T prCnmInfo = &prAdapter->rCnmInfo;
	struct MSG_CH_UTIL_RSP *prMsgChUtil = NULL;
	P_MSG_SCN_SCAN_REQ prScanReqMsg = NULL;

	if (!timerPendingTimer(&prCnmInfo->rReqChnlUtilTimer))
		return;
	prMsgChUtil = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prMsgChUtil));
	if (!prMsgChUtil) {
		DBGLOG(CNM, ERROR, "No memory!");
		return;
	}
	DBGLOG(CNM, INFO, "Receive Channel Utilization response\n");
	cnmTimerStopTimer(prAdapter, &prCnmInfo->rReqChnlUtilTimer);
	kalMemZero(prMsgChUtil, sizeof(*prMsgChUtil));
	prMsgChUtil->rMsgHdr.eMsgId = prCnmInfo->u2ReturnMID;
	prMsgChUtil->ucChnlNum = prChnlUtil->ucChannelNum;
	kalMemCopy(prMsgChUtil->aucChnlList, prChnlUtil->aucChannelMeasureList, prChnlUtil->ucChannelNum);
	kalMemCopy(prMsgChUtil->aucChUtil, prChnlUtil->aucChannelUtilization, prChnlUtil->ucChannelNum);
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgChUtil, MSG_SEND_METHOD_BUF);
	prScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(*prScanReqMsg));
	kalMemZero(prScanReqMsg, sizeof(*prScanReqMsg));
	prScanReqMsg->rMsgHdr.eMsgId = MID_MNY_CNM_SCAN_CONTINUE;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prScanReqMsg, MSG_SEND_METHOD_BUF);
}

VOID cnmRequestChannelUtilization(P_ADAPTER_T prAdapter, P_MSG_HDR_T prMsgHdr)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_CNM_INFO_T prCnmInfo = &prAdapter->rCnmInfo;
	struct MSG_REQ_CH_UTIL *prMsgReqChUtil = (struct MSG_REQ_CH_UTIL *)prMsgHdr;
	struct CMD_REQ_CHNL_UTILIZATION rChnlUtilCmd;

	if (!prMsgReqChUtil)
		return;
	if (timerPendingTimer(&prCnmInfo->rReqChnlUtilTimer)) {
		cnmMemFree(prAdapter, prMsgReqChUtil);
		return;
	}
	DBGLOG(CNM, INFO, "Request Channel Utilization, channel count %d\n", prMsgReqChUtil->ucChnlNum);
	kalMemZero(&rChnlUtilCmd, sizeof(rChnlUtilCmd));
	prCnmInfo->u2ReturnMID = prMsgReqChUtil->u2ReturnMID;
	rChnlUtilCmd.u2MeasureDuration = prMsgReqChUtil->u2Duration;
	if (prMsgReqChUtil->ucChnlNum > 9)
		prMsgReqChUtil->ucChnlNum = 9;
	rChnlUtilCmd.ucChannelNum = prMsgReqChUtil->ucChnlNum;
	kalMemCopy(rChnlUtilCmd.aucChannelList, prMsgReqChUtil->aucChnlList, rChnlUtilCmd.ucChannelNum);
	cnmMemFree(prAdapter, prMsgReqChUtil);
	rStatus = wlanSendSetQueryCmd(
				prAdapter,                  
				CMD_ID_REQ_CHNL_UTILIZATION,
				TRUE,                       
				FALSE,                      
				FALSE,                       
				nicCmdEventSetCommon,		
				nicOidCmdTimeoutCommon,		
				sizeof(rChnlUtilCmd),
				(PUINT_8)&rChnlUtilCmd,      
				NULL,                       
				0                           
				);
	cnmTimerStartTimer(prAdapter, &prCnmInfo->rReqChnlUtilTimer, 1000);
}

BOOLEAN cnmChUtilIsRunning(P_ADAPTER_T prAdapter)
{
	return timerPendingTimer(&prAdapter->rCnmInfo.rReqChnlUtilTimer);
}

VOID cnmChReqPrivilegeTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_MSG_CH_REQ_T prMsgChReq;
	P_CNM_INFO_T prCnmInfo;

	DBGLOG(CNM, INFO, "Request channel privilege timeout\n");

	prMsgChReq = (P_MSG_CH_REQ_T) ulParamPtr;
	prCnmInfo = &prAdapter->rCnmInfo;

	if (prCnmInfo->ucReqChnPrivilegeCnt < 2) {
		prCnmInfo->ucReqChnPrivilegeCnt++;
		cnmChMngrRequestPrivilege(prAdapter, (P_MSG_HDR_T)prMsgChReq);
	} else {
		P_BSS_INFO_T prBssInfo;
		P_MSG_CH_GRANT_T prChResp;

		prChResp = (P_MSG_CH_GRANT_T)
			cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_GRANT_T));

		
		if (!prChResp) {
			DBGLOG(CNM, ERROR, "ChGrant: fail to get buf (net=%d, token=%d)\n");
			cnmMemFree(prAdapter, prMsgChReq);
			return;
		}

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsgChReq->ucBssIndex);
		prCnmInfo->ucReqChnPrivilegeCnt = 0;

		if (IS_BSS_AIS(prBssInfo))
			prChResp->rMsgHdr.eMsgId = MID_CNM_AIS_CH_GRANT_FAIL;
		#if CFG_ENABLE_WIFI_DIRECT
		else if (prAdapter->fgIsP2PRegistered && IS_BSS_P2P(prBssInfo))
			prChResp->rMsgHdr.eMsgId = MID_CNM_P2P_CH_GRANT_FAIL;
		#endif
		#if CFG_ENABLE_BT_OVER_WIFI
		else if (IS_BSS_BOW(prBssInfo))
			prChResp->rMsgHdr.eMsgId = MID_CNM_BOW_CH_GRANT_FAIL;
		#endif
		else {
			cnmMemFree(prAdapter, prChResp);
			cnmMemFree(prAdapter, prMsgChReq);
			return;
		}

		prChResp->ucBssIndex = prMsgChReq->ucBssIndex;
		prChResp->ucTokenID = prMsgChReq->ucTokenID;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prChResp, MSG_SEND_METHOD_BUF);
		cnmMemFree(prAdapter, prMsgChReq);
		prCnmInfo->rReqChnTimeoutTimer.ulDataPtr = 0;
	}
}
