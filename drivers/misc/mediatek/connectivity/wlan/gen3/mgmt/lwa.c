#include "lwa_shm_def.h"
#include <mt-plat/mt_ccci_common.h>


static struct lwa_shm_info lwaShmInfo = {0, 0, 0, 0, 0, 0, NULL, 0, 0, 0x0ULL};

static UINT_32 getShmNumber(VOID);

static inline UINT_32 isLwaInited(VOID);

static inline UINT_32 isLwaShmFull(VOID);

static INT_32 mdMsgCallback(INT_32 md_id, INT_32 msg);

static PUINT_8 getNextShmBlock(VOID);

static UINT_16 getHeaderOffset(P_HW_MAC_RX_DESC_T pRxStatus);

#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
static int lwa_wlan_debug_init = 0;
static u64 lwa_sent_count = 0;

static int proc_lwa_smem_debug_dump_read(struct seq_file *m, void *v);

static int proc_lwa_smem_debug_open(struct inode *inode, struct file *file);

static const struct file_operations lwa_setting_fops = {
	.owner = THIS_MODULE,
	.open = proc_lwa_smem_debug_open,
	.read = seq_read,
};

static int proc_lwa_smem_debug_dump_read(struct seq_file *m, void *v)
{
	seq_printf(m, "ShmNumber=[%d]\n", getShmNumber());
	seq_printf(m, "isInited=[%d]\n", lwaShmInfo.isInited);
	seq_printf(m, "isLwaConnection=[%d]\n", lwaShmInfo.isLwaConnection);
	seq_printf(m, "isFull=[%d]\n", lwaShmInfo.isFull);
	seq_printf(m, "needToNotify=[%d]\n", lwaShmInfo.needToNotify);
	seq_printf(m, "descIndex=[%d]\n", lwaShmInfo.descIndex);
	seq_printf(m, "layoutDesicion=[%d]\n", lwaShmInfo.layoutDesicion);
	seq_printf(m, "startAddr=[0x%x]\n", lwaShmInfo.startAddr);
	seq_printf(m, "shmSize=[0x%x]\n", lwaShmInfo.shmSize);
	seq_printf(m, "lwa_sent_count=[%llu]\n", lwa_sent_count);
	seq_printf(m, "dropPktNum=[%d]\n", lwaShmInfo.dropPktNum);
	seq_printf(m, "notifyTimeStamp=[%lld]\n", lwaShmInfo.notifyTimeStamp);
	return 0;
}

static int proc_lwa_smem_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_lwa_smem_debug_dump_read, NULL);
}

int proc_lwa_smem_debug_init(void)
{
	int r = 0;
	if (lwa_wlan_debug_init == 0) {
		lwa_wlan_debug_init = 1;
		proc_create("lwa_setting_dump", 0440, NULL, &lwa_setting_fops);
	}
	return r;
}
EXPORT_SYMBOL(proc_lwa_smem_debug_init);
#endif
static UINT_32 getShmNumber(VOID)
{
	if (lwaShmInfo.startAddr)
		return lwaShmInfo.shmSize/(sizeof(struct _lwa_dl_desc) + sizeof(struct lwa_dl_buff));
	else
		return 0;
}

static inline UINT_32 isLwaInited(VOID)
{
	return lwaShmInfo.isInited;
}

static inline UINT_32 isLwaShmFull(VOID)
{
	return lwaShmInfo.isFull;
}

inline UINT_32 isLwaConnection(VOID)
{
	return lwaShmInfo.isLwaConnection;
}

VOID setLwaConnection(UINT_32 isLwa)
{
	lwaShmInfo.isLwaConnection = isLwa;
	DBGLOG(RX, INFO, "set LWA connection, islwa: %d\n", isLwa);
}

INT_32 genLwaDlDescAndNotify(INT_32 bypass, INT_32 payloadoffset, INT_32 len, INT_32 bearer, INT_32 count)
{
	INT_32 mdResult = 0;
	UINT_32 mdMsg = 0;
	struct _lwa_dl_desc *pLwaDesc = NULL;

	if (!lwaShmInfo.startAddr) {
		DBGLOG(RX, WARN, "shm startAddr is null?\n");
		return -1;
	}

	if (!lwaShmInfo.isInited) {
		DBGLOG(RX, WARN, "not inited yet?\n");
		return -1;
	}

	pLwaDesc = (struct _lwa_dl_desc *)lwaShmInfo.startAddr + lwaShmInfo.descIndex;
	
	pLwaDesc->payload_offset = (UINT_16)payloadoffset;
	pLwaDesc->bearer = (UINT_8)bearer;
	pLwaDesc->count = (UINT_32)count;
	pLwaDesc->data_len = (UINT_16)len;

	
	if (bypass) {
		pLwaDesc->flag |= 0x04;
		
		pLwaDesc->flag |= 0x01;
	} else {
		pLwaDesc->flag &= 0xFB;

		pLwaDesc->flag |= 0x01;
		if (lwaShmInfo.needToNotify) {
			UINT_64 now = sched_clock();

			if (now - lwaShmInfo.notifyTimeStamp >= LWA_NOTIFICATION_PERIOD * NSEC_PER_MSEC) {
				lwaShmInfo.notifyTimeStamp = now;
				mdMsg = 0;
				mdMsg = LWA_UP_CTRL_CMD_WLAN_DL_PKT_READY_NOTIFY << 16;
				mdResult = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_LWA_CONTROL_MSG, (char *)&mdMsg, 4);
				if (mdResult < 0) {
					DBGLOG(RX, WARN, "notify md failed\n");
					
				}
			}
		}
	}
	lwaShmInfo.descIndex++;
	if (getShmNumber()) {
		if (lwaShmInfo.descIndex == getShmNumber())
			lwaShmInfo.descIndex = 0;
	} else {
		DBGLOG(RX, WARN, "shm number is 0? !!!\n");
	}

	
	pLwaDesc = (struct _lwa_dl_desc *)lwaShmInfo.startAddr + lwaShmInfo.descIndex;

	if (pLwaDesc->flag & 0x01) {
		lwaShmInfo.isFull = 1;
		mdMsg = 0;
		mdMsg = LWA_UP_CTRL_CMD_DL_SHM_FULL << 16;
		mdResult = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_LWA_CONTROL_MSG, (char *)&mdMsg, 4);
		if (mdResult < 0) {
			DBGLOG(RX, WARN, "tell md next block full failed\n");
			
		}
	}
	return 0;
}

INT_32 lwaInit(UINT_8 rewind)
{
	INT_32 mdResult = 0;
	UINT_32 initMsg = 0;
	UINT_32 blockNum = 0;
	UINT_32 i = 0;
	struct _lwa_dl_desc *pDesc = NULL;

	lwaShmInfo.startAddr = (PUINT_8) get_smem_start_addr(MD_SYS1, SMEM_USER_RAW_LWA, &lwaShmInfo.shmSize);
	if (!lwaShmInfo.startAddr) {
		DBGLOG(RX, WARN, "get shm start addr failed, pls check it!\n");
		return -1;
	}
	DBGLOG(RX, INFO, "lwa shm start addri: %p!\n", lwaShmInfo.startAddr);

	blockNum = getShmNumber();
	pDesc = (struct _lwa_dl_desc *)lwaShmInfo.startAddr;

	if (rewind) {
		for (; i < blockNum; i++) {
			pDesc->flag &= 0xFA;
			pDesc++;
		}
		lwaShmInfo.descIndex = 0;
	}
	
	register_ccci_sys_call_back(MD_SYS1, LWA_CONTROL_MSG, &mdMsgCallback);

	initMsg = LWA_UP_CTRL_CMD_DL_SHM_LAYOUT_SUPPORT_LIST << 16 | 0x0001;
	mdResult = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_LWA_CONTROL_MSG, (char *)&initMsg, 4);
	if (mdResult < 0) {
		DBGLOG(RX, WARN, "send LAYOUT_SUPPORTLIST fail, reslut: %d\n", mdResult);
		
		return mdResult;
	}
	return 0;
}

int mdMsgCallback(int md_id, int msg)
{
	if (md_id != MD_SYS1) {
		DBGLOG(RX, WARN, "LWA received msg from wrong md id\n");
		return -1;
	}

	switch (msg >> 16) {
	case LWA_UP_CTRL_CMD_DL_SHM_LAYOUT_DESICION:
		lwaShmInfo.layoutDesicion = msg & 0x0000FFFF;
		lwaShmInfo.isInited = 1;
		DBGLOG(RX, INFO, "LWA INIT Done\n");
		break;
	case LWA_UP_CTRL_CMD_DL_SHM_NOT_FULL:
		lwaShmInfo.isFull = 0;
		break;
	case LWA_UP_CTRL_CMD_WLAN_DL_PKT_READY_NOTIFY_SETTING:
		if ((msg & 0x0000FFFF) == LWA_WIFI_DL_PKT_READY_NOTIFY_ON) {
			
			INT_32 mdResult = 0;
			UINT_32 notifyMsg = 0;

			lwaShmInfo.needToNotify = 1;
			lwaShmInfo.notifyTimeStamp = sched_clock();
			notifyMsg = LWA_UP_CTRL_CMD_WLAN_DL_PKT_READY_NOTIFY << 16;
			mdResult = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_LWA_CONTROL_MSG, (char *)&notifyMsg, 4);
			if (mdResult < 0) {
				DBGLOG(RX, WARN, "notify md failed\n");
				
			}
		} else {
			lwaShmInfo.needToNotify = msg & 0x0000FFFF;
			lwaShmInfo.notifyTimeStamp = 0x0ULL;
		}
		break;
	case LWA_UP_CTRL_CMD_DL_HDR_LOOK_AHEAD_SETTING:
		break;
	default:
		DBGLOG(RX, INFO, "LWA received msg that can't be parsed!\n");
		break;
	}
	return 0;
}

UINT_32 isLwaDataPkt(PUINT_8 data, UINT_32 dataLen)
{
	UINT_8 pktType = 0xFF;
	UINT_32 etherType = 0;
	PUINT_8 pSkbData = NULL;

	pktType = (UINT_8)HAL_RX_STATUS_GET_PKT_TYPE((P_HW_MAC_RX_DESC_T)data);
	if (pktType == RX_PKT_TYPE_RX_DATA) {
		pSkbData = data + getHeaderOffset((P_HW_MAC_RX_DESC_T)data);
		etherType = pSkbData[ETH_TYPE_LEN_OFFSET] << 8 | pSkbData[ETH_TYPE_LEN_OFFSET + 1];
		if (etherType == ETH_P_LTE)
			return 1;
	}
	return 0;
}

INT_32 processLwaDataPkt(PUINT_8 data, UINT_32 dataLen)
{
	UINT_32 headerOffset = 0;
	UINT_32 pktLen = 0;
	PUINT_8 pDataBody = NULL;
	PUINT_8 pNextBlock = NULL;

	UINT_16 count = 0;
	UINT_8 bearer = 0;
	INT_32 payloadOffset = 0;

	INT_16 seqnum = -1;
	UINT_8 groupVLD = 0;

	groupVLD = (UINT_8) HAL_RX_STATUS_GET_GROUP_VLD((P_HW_MAC_RX_DESC_T)data);
	if (groupVLD & BIT(RX_GROUP_VLD_4))
		seqnum = (((P_HW_MAC_RX_STS_GROUP_4_T)((PUINT_8)data + sizeof(HW_MAC_RX_DESC_T)))->u2SeqFrag
			& BITS(4, 15)) >> 4;

	headerOffset = getHeaderOffset((P_HW_MAC_RX_DESC_T)data);
	pktLen = (UINT_32) HAL_RX_STATUS_GET_RX_BYTE_CNT((P_HW_MAC_RX_DESC_T)data);
	pDataBody = data + headerOffset + ETHER_HEADER_LEN;

	pNextBlock = getNextShmBlock();
	if (pNextBlock) {
		count = pDataBody[1] << 8 | pDataBody[2];
                
                
                
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
		if ( lwa_sent_count == U64_MAX )
			lwa_sent_count = 0;
		lwa_sent_count++;
#endif
		kalMemCopy(pNextBlock, (PUINT_8)((UINT_64)pDataBody & ~7ull),
			       ALIGN_8(dataLen - headerOffset - ETHER_HEADER_LEN));
	} else {
		lwaShmInfo.dropPktNum++;
		DBGLOG(RX, WARN, "LWA drop packet, count: %d!\n", lwaShmInfo.dropPktNum);
		return -1;
	}
	payloadOffset = (INT_32)(pDataBody - (PUINT_8)((UINT_64)pDataBody & ~7ull));

	bearer = pDataBody[0];
	count = pDataBody[1] << 8 | pDataBody[2];

	genLwaDlDescAndNotify(0, payloadOffset, pktLen - headerOffset - ETHER_HEADER_LEN, bearer, count);
	return 0;
}

static PUINT_8 getNextShmBlock(VOID)
{
	struct _lwa_dl_desc *pLwaDesc = (struct _lwa_dl_desc *)lwaShmInfo.startAddr + lwaShmInfo.descIndex;

	if (!lwaShmInfo.isInited) {
		DBGLOG(RX, WARN, "LWA shm sitll not inited, when try to write!\n");
		return 0;
	}

	if (pLwaDesc->flag & 0x01) {
		UINT_32 fullMsg = 0;
		INT_32 mdResult = 0;

		lwaShmInfo.isFull = 1;
		fullMsg = LWA_UP_CTRL_CMD_DL_SHM_FULL << 16;
		mdResult = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_LWA_CONTROL_MSG, (char *)&fullMsg, 4);
		if (mdResult < 0) {
			DBGLOG(RX, WARN, "notify md failed in getNextShmBlock\n");
			
		}
		return NULL;
	}
	return lwaShmInfo.startAddr + getShmNumber() * sizeof(struct _lwa_dl_desc) +
	       lwaShmInfo.descIndex * sizeof(struct lwa_dl_buff);
}

static UINT_16 getHeaderOffset(P_HW_MAC_RX_DESC_T pRxStatus)
{
	UINT_8 groupVLD = 0;
	UINT_16 rxStatusOffset = sizeof(HW_MAC_RX_DESC_T);

	rxStatusOffset += (UINT_8) HAL_RX_STATUS_GET_HEADER_OFFSET(pRxStatus);

	groupVLD = (UINT_8) HAL_RX_STATUS_GET_GROUP_VLD(pRxStatus);
	if (groupVLD & BIT(RX_GROUP_VLD_4))
		rxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_4_T);
	if (groupVLD & BIT(RX_GROUP_VLD_1))
		rxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_1_T);
	if (groupVLD & BIT(RX_GROUP_VLD_2))
		rxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_2_T);
	if (groupVLD & BIT(RX_GROUP_VLD_3))
		rxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_3_T);

	return rxStatusOffset;
}

UINT_32 validateRxPkt(PUINT_8 data, PUINT_32 reorder, PUINT_32 fragment)
{
	UINT_32 isDrop = 0;
	P_HW_MAC_RX_DESC_T pRxStatus = (P_HW_MAC_RX_DESC_T)data;

	if (pRxStatus->u2StatusFlag == RXS_DW2_AMPDU_nERR_VALUE)
		*reorder = 1;
	else if ((pRxStatus->u2StatusFlag & RXS_DW2_RX_nERR_BITMAP) == RXS_DW2_RX_nERR_VALUE) {
		if ((pRxStatus->u2StatusFlag & RXS_DW2_RX_FRAG_BITMAP) == RXS_DW2_RX_FRAG_VALUE) {
			DBGLOG(RX, WARN, "pkt need to be defraged\n");
			*fragment = 1;
		}
	} else {
		isDrop = 1;
		if (!HAL_RX_STATUS_IS_ICV_ERROR(pRxStatus) && HAL_RX_STATUS_IS_TKIP_MIC_ERROR(pRxStatus))
			DBGLOG(RX, WARN, "MIC_ERR_PKT\n");
		else if (HAL_RX_STATUS_IS_LLC_MIS(pRxStatus)) {
			DBGLOG(RX, WARN, "LLC_MIS_PKT\n");
			isDrop = 0;
		}
	}
	return isDrop;
}
