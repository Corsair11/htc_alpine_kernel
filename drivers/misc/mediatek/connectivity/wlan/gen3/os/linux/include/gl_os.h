


#ifndef _GL_OS_H
#define _GL_OS_H

#define CFG_MAX_WLAN_DEVICES                1	

#define CFG_MAX_TXQ_NUM                     4	

#define CFG_USE_SPIN_LOCK_BOTTOM_HALF       0	

#define CFG_TX_PADDING_SMALL_ETH_PACKET     0	

#define CFG_TX_STOP_NETIF_QUEUE_THRESHOLD   256	

#define CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD   512	
#define CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD  128	

#if defined(MT6630)
#define CHIP_NAME    "MT6630"
#endif

#define DRV_NAME "["CHIP_NAME"]: "

#ifndef CONFIG_ANDROID
#define CONFIG_ANDROID      0
#endif

#define CFG_CFG80211_IE_BUF_LEN     (512)
#define GLUE_INFO_WSCIE_LENGTH		500

#include <linux/version.h>	

#include <linux/kernel.h>	

#include <linux/timer.h>	
#include <linux/jiffies.h>	
#include <linux/delay.h>	

#if CONFIG_ANDROID
#include <linux/wakelock.h>
#endif

#include <linux/irq.h>		

#include <linux/netdevice.h>	
#include <linux/etherdevice.h>	
#include <linux/wireless.h>	
#include <linux/if_arp.h>
#include <linux/inetdevice.h>	

#include <linux/ip.h>		

#include <linux/string.h>	
#include <linux/stddef.h>	

#include <linux/proc_fs.h>	

#include <linux/rtnetlink.h>	
#include <linux/kthread.h>	
#include <asm/uaccess.h>	
#include <linux/fs.h>		
#include <linux/vmalloc.h>

#include <linux/kfifo.h>	
#include <linux/cdev.h>		

#include <linux/firmware.h>	

#if defined(_HIF_SDIO)
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#endif

#include <linux/random.h>

#include <asm/io.h>		

#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif

#include "version.h"
#include "config.h"

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/wireless.h>
#include <net/cfg80211.h>
#endif

#include <linux/module.h>
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <linux/fb.h>

#ifdef CONFIG_IPV6
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <net/if_inet6.h>
#endif

#if CFG_SUPPORT_PASSPOINT
#include <net/addrconf.h>
#endif 

#include "gl_typedef.h"
#include "typedef.h"
#include "queue.h"
#include "gl_kal.h"
#if CFG_CHIP_RESET_SUPPORT
#include "gl_rst.h"
#endif
#include "hif.h"

#if CFG_SUPPORT_TDLS
#include "tdls.h"
#endif

#include "debug.h"

#include "wlan_lib.h"
#include "wlan_oid.h"

#if CFG_ENABLE_AEE_MSG
#include <mt-plat/aee.h>
#endif

extern BOOLEAN fgIsBusAccessFailed;

#define GLUE_FLAG_HALT                  BIT(0)
#define GLUE_FLAG_INT                   BIT(1)
#define GLUE_FLAG_OID                   BIT(2)
#define GLUE_FLAG_TIMEOUT               BIT(3)
#define GLUE_FLAG_TXREQ                 BIT(4)
#define GLUE_FLAG_SUB_MOD_MULTICAST     BIT(7)
#define GLUE_FLAG_FRAME_FILTER          BIT(8)
#define GLUE_FLAG_FRAME_FILTER_AIS      BIT(9)

#define GLUE_FLAG_HALT_BIT              (0)
#define GLUE_FLAG_INT_BIT               (1)
#define GLUE_FLAG_OID_BIT               (2)
#define GLUE_FLAG_TIMEOUT_BIT           (3)
#define GLUE_FLAG_TXREQ_BIT             (4)
#define GLUE_FLAG_SUB_MOD_MULTICAST_BIT (7)
#define GLUE_FLAG_FRAME_FILTER_BIT      (8)
#define GLUE_FLAG_FRAME_FILTER_AIS_BIT  (9)

#if CFG_SUPPORT_MULTITHREAD
#define GLUE_FLAG_RX            BIT(10)
#define GLUE_FLAG_TX_CMD_DONE   BIT(11)
#define GLUE_FLAG_HIF_TX        BIT(12)
#define GLUE_FLAG_HIF_TX_CMD    BIT(13)
#define GLUE_FLAG_RX_TO_OS      BIT(14)
#define GLUE_FLAG_HIF_FW_OWN    BIT(15)

#define GLUE_FLAG_RX_BIT            (10)
#define GLUE_FLAG_TX_CMD_DONE_BIT   (11)
#define GLUE_FLAG_HIF_TX_BIT        (12)
#define GLUE_FLAG_HIF_TX_CMD_BIT    (13)
#define GLUE_FLAG_RX_TO_OS_BIT      (14)
#define GLUE_FLAG_HIF_FW_OWN_BIT    (15)
#endif

#define GLUE_BOW_KFIFO_DEPTH        (1024)
#define GLUE_BOW_DEVICE_NAME        "ampc0"

#define WAKE_LOCK_RX_TIMEOUT                            300	
#define WAKE_LOCK_THREAD_WAKEUP_TIMEOUT                 50	
#define UPDATE_FULL_TO_PARTIAL_SCAN_TIMEOUT             60    

#define FULL_SCAN_MAX_CHANNEL_NUM                       40
typedef struct _GL_WPA_INFO_T {
	UINT_32 u4WpaVersion;
	UINT_32 u4KeyMgmt;
	UINT_32 u4CipherGroup;
	UINT_32 u4CipherPairwise;
	UINT_32 u4AuthAlg;
	BOOLEAN fgPrivacyInvoke;
#if CFG_SUPPORT_802_11W
	UINT_32 u4Mfp;
#endif
} GL_WPA_INFO_T, *P_GL_WPA_INFO_T;

typedef enum _ENUM_NET_DEV_IDX_T {
	NET_DEV_WLAN_IDX = 0,
	NET_DEV_P2P_IDX,
	NET_DEV_BOW_IDX,
	NET_DEV_NUM
} ENUM_NET_DEV_IDX_T;

typedef enum _ENUM_RSSI_TRIGGER_TYPE {
	ENUM_RSSI_TRIGGER_NONE,
	ENUM_RSSI_TRIGGER_GREATER,
	ENUM_RSSI_TRIGGER_LESS,
	ENUM_RSSI_TRIGGER_TRIGGERED,
	ENUM_RSSI_TRIGGER_NUM
} ENUM_RSSI_TRIGGER_TYPE;

#if CFG_ENABLE_WIFI_DIRECT
typedef enum _ENUM_NET_REG_STATE_T {
	ENUM_NET_REG_STATE_UNREGISTERED,
	ENUM_NET_REG_STATE_REGISTERING,
	ENUM_NET_REG_STATE_REGISTERED,
	ENUM_NET_REG_STATE_UNREGISTERING,
	ENUM_NET_REG_STATE_NUM
} ENUM_NET_REG_STATE_T;
#endif

typedef enum _ENUM_PKT_FLAG_T {
	ENUM_PKT_802_11,	
	ENUM_PKT_802_3,		
	ENUM_PKT_1X,		
	ENUM_PKT_PROTECTED_1X,	
	ENUM_PKT_VLAN_EXIST,	
	ENUM_PKT_DHCP,		
	ENUM_PKT_ARP,		
	ENUM_PKT_ICMP,		
	ENUM_PKT_TDLS,		

	ENUM_PKT_FLAG_NUM
} ENUM_PKT_FLAG_T;

typedef struct _GL_IO_REQ_T {
	QUE_ENTRY_T rQueEntry;
	
	BOOL fgRead;
	BOOL fgWaitResp;
	P_ADAPTER_T prAdapter;
	PFN_OID_HANDLER_FUNC pfnOidHandler;
	PVOID pvInfoBuf;
	UINT_32 u4InfoBufLen;
	PUINT_32 pu4QryInfoLen;
	WLAN_STATUS rStatus;
	UINT_32 u4Flag;
} GL_IO_REQ_T, *P_GL_IO_REQ_T;

#if CFG_ENABLE_BT_OVER_WIFI
typedef struct _GL_BOW_INFO {
	BOOLEAN fgIsRegistered;
	dev_t u4DeviceNumber;	
	
	struct kfifo rKfifo;	
	spinlock_t rSpinLock;	
	struct cdev cdev;
	UINT_32 u4FreqInKHz;	

	UINT_8 aucRole[CFG_BOW_PHYSICAL_LINK_NUM];	
	ENUM_BOW_DEVICE_STATE aeState[CFG_BOW_PHYSICAL_LINK_NUM];
	PARAM_MAC_ADDRESS arPeerAddr[CFG_BOW_PHYSICAL_LINK_NUM];

	wait_queue_head_t outq;

#if CFG_BOW_SEPARATE_DATA_PATH
	
	struct net_device *prDevHandler;
	BOOLEAN fgIsNetRegistered;
#endif

} GL_BOW_INFO, *P_GL_BOW_INFO;
#endif

typedef struct _GL_P2P_INFO_T GL_P2P_INFO_T, *P_GL_P2P_INFO_T;

struct _GLUE_INFO_T {
	
	struct net_device *prDevHandler;

	
	INT_32 i4DevIdx;

	
	

	
	struct iw_statistics rIwStats;

	
	spinlock_t rSpinLock[SPIN_LOCK_NUM];

	
	struct mutex arMutex[MUTEX_NUM];

	
	struct semaphore ioctl_sem;

	UINT_64 u8Cookie;

	ULONG ulFlag;		
	UINT_32 u4PendFlag;
	
	UINT_32 u4OidCompleteFlag;
	UINT_32 u4ReadyFlag;	

	UINT_32 u4OsMgmtFrameFilter;

	INT_32 ai4TxPendingFrameNumPerQueue[HW_BSSID_NUM][CFG_MAX_TXQ_NUM];
	INT_32 i4TxPendingFrameNum;
	INT_32 i4TxPendingSecurityFrameNum;

	
	NET_INTERFACE_INFO_T arNetInterfaceInfo[HW_BSSID_NUM];

	
	

	
	GL_IO_REQ_T OidEntry;

	
	REG_INFO_T rRegInfo;

	
	struct firmware *prFw;

	
	
	GL_HIF_INFO_T rHifInfo;

	
	GL_WPA_INFO_T rWpaInfo;

	
	P_ADAPTER_T prAdapter;

#ifdef WLAN_INCLUDE_PROC
	struct proc_dir_entry *pProcRoot;
#endif				

	
	ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicated;

	
	PARAM_DEVICE_POWER_STATE ePowerState;

	struct completion rScanComp;	
	struct completion rHaltComp;	
	struct completion rPendComp;	
#if CFG_SUPPORT_MULTITHREAD
	struct completion rHifHaltComp;	
	struct completion rRxHaltComp;	

	UINT_32 u4TxThreadPid;
	UINT_32 u4RxThreadPid;
	UINT_32 u4HifThreadPid;
#endif
	WLAN_STATUS rPendStatus;

	QUE_T rTxQueue;

	
	QUE_T rCmdQueue;
	
	
	
	
	
	

	wait_queue_head_t waitq;
	struct task_struct *main_thread;

#if CFG_SUPPORT_MULTITHREAD
	wait_queue_head_t waitq_hif;
	struct task_struct *hif_thread;

	wait_queue_head_t waitq_rx;
	struct task_struct *rx_thread;

#endif

	struct timer_list tickfn;

#if CFG_SUPPORT_EXT_CONFIG
	UINT_16 au2ExtCfg[256];	
	UINT_32 u4ExtCfgLength;	
#endif

#if 1				
	
	UINT_8 aucWapiAssocInfoIEs[42];
	UINT_16 u2WapiAssocInfoIESz;
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	GL_BOW_INFO rBowInfo;
#endif

#if CFG_ENABLE_WIFI_DIRECT
	P_GL_P2P_INFO_T prP2PInfo;
#if CFG_SUPPORT_P2P_RSSI_QUERY
	
	struct iw_statistics rP2pIwStats;
#endif
#endif
	BOOLEAN fgWpsActive;
	UINT_8 aucWSCIE[GLUE_INFO_WSCIE_LENGTH];	
	UINT_16 u2WSCIELen;
	UINT_8 aucWSCAssocInfoIE[200];	
	UINT_16 u2WSCAssocInfoIELen;

	
	BOOLEAN fgNvramAvailable;

	BOOLEAN fgMcrAccessAllowed;

	
	BOOLEAN fgIsMacAddrOverride;
	PARAM_MAC_ADDRESS rMacAddrOverride;

	SET_TXPWR_CTRL_T rTxPwr;

	
	struct cfg80211_scan_request *prScanRequest;

	
	struct cfg80211_sched_scan_request *prSchedScanRequest;

	
	BOOLEAN fgIsRegistered;

	
	UINT_32 u4RspIeLength;
	UINT_8 aucRspIe[CFG_CFG80211_IE_BUF_LEN];

	UINT_32 u4ReqIeLength;
	UINT_8 aucReqIe[CFG_CFG80211_IE_BUF_LEN];

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
	BOOLEAN fgEnSdioTestPattern;
	BOOLEAN fgSdioReadWriteMode;
	BOOLEAN fgIsSdioTestInitialized;
	UINT_8 aucSdioTestBuffer[256];
#endif

	BOOLEAN fgIsInSuspendMode;

#if CFG_SUPPORT_PASSPOINT
	UINT_8 aucHS20AssocInfoIE[200];	
	UINT_16 u2HS20AssocInfoIELen;
	UINT_8 ucHotspotConfig;
	BOOLEAN fgConnectHS20AP;

	BOOLEAN fgIsDad;
	UINT_8 aucDADipv4[4];
	BOOLEAN fgIs6Dad;
	UINT_8 aucDADipv6[16];
#endif				

	KAL_WAKE_LOCK_T rIntrWakeLock;
	KAL_WAKE_LOCK_T rTimeoutWakeLock;

#if CFG_MET_PACKET_TRACE_SUPPORT
	BOOLEAN fgMetProfilingEn;
	UINT_16 u2MetUdpPort;
#endif

#if CFG_SUPPORT_SNIFFER
	BOOLEAN fgIsEnableMon;
	struct net_device *prMonDevHandler;
	struct work_struct monWork;
#endif

	INT_32 i4RssiCache;
	UINT_32 u4LinkSpeedCache;

	UINT_32 IsrCnt;
	UINT_32 IsrPassCnt;
	UINT_32 TaskIsrCnt;

	UINT_32 IsrAbnormalCnt;
	UINT_32 IsrSoftWareCnt;
	UINT_32 IsrTxCnt;
	UINT_32 IsrRxCnt;

	
	PARTIAL_SCAN_INFO rScanChannelInfo;
	PUINT_8	pucScanChannel;

	
	
	OS_SYSTIME u4LastFullScanTime;
	
	UINT_8 ucTrScanType;
	
	UINT_8 ucChannelNum[FULL_SCAN_MAX_CHANNEL_NUM];
	PUINT_8 puFullScan2PartialChannel;
};

typedef irqreturn_t(*PFN_WLANISR) (int irq, void *dev_id, struct pt_regs *regs);

typedef void (*PFN_LINUX_TIMER_FUNC) (unsigned long);

#if CFG_ENABLE_WIFI_DIRECT
typedef BOOLEAN(*SUB_MODULE_INIT) (P_GLUE_INFO_T prGlueInfo);
typedef BOOLEAN(*SUB_MODULE_EXIT) (P_GLUE_INFO_T prGlueInfo);

typedef struct _SUB_MODULE_HANDLER {
	SUB_MODULE_INIT subModInit;
	SUB_MODULE_EXIT subModExit;
	BOOLEAN fgIsInited;
} SUB_MODULE_HANDLER, *P_SUB_MODULE_HANDLER;

#endif

#if CONFIG_NL80211_TESTMODE

enum TestModeCmdType {
	TESTMODE_CMD_ID_SW_CMD = 1,
	TESTMODE_CMD_ID_WAPI = 2,
	TESTMODE_CMD_ID_HS20 = 3,
	TESTMODE_CMD_ID_STR_CMD = 102,
	TESTMODE_CMD_ID_LWA_CONNECTION = 104,
	NUM_OF_TESTMODE_CMD_ID
};

#if CFG_SUPPORT_PASSPOINT
enum Hs20CmdType {
	HS20_CMD_ID_SET_BSSID_POOL = 0,
	NUM_OF_HS20_CMD_ID
};
#endif 

typedef struct _NL80211_DRIVER_TEST_MODE_PARAMS {
	UINT_32 index;
	UINT_32 buflen;
} NL80211_DRIVER_TEST_MODE_PARAMS, *P_NL80211_DRIVER_TEST_MODE_PARAMS;

typedef struct _NL80211_DRIVER_SW_CMD_PARAMS {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	UINT_8 set;
	UINT_32 adr;
	UINT_32 data;
} NL80211_DRIVER_SW_CMD_PARAMS, *P_NL80211_DRIVER_SW_CMD_PARAMS;

struct _LWA_CONNECTION {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	INT_32 value;
};

struct iw_encode_exts {
	__u32 ext_flags;	
	__u8 tx_seq[IW_ENCODE_SEQ_MAX_SIZE];	
	__u8 rx_seq[IW_ENCODE_SEQ_MAX_SIZE];	
	__u8 addr[MAC_ADDR_LEN];	
	__u16 alg;		
	__u16 key_len;
	__u8 key[32];
};

typedef struct _NL80211_DRIVER_SET_KEY_EXTS {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	UINT_8 key_index;
	UINT_8 key_len;
	struct iw_encode_exts ext;
} NL80211_DRIVER_SET_KEY_EXTS, *P_NL80211_DRIVER_SET_KEY_EXTS;

#if CFG_SUPPORT_PASSPOINT

struct param_hs20_set_bssid_pool {
	UINT_8 fgBssidPoolIsEnable;
	UINT_8 ucNumBssidPool;
	UINT_8 arBssidPool[8][ETH_ALEN];
};

struct wpa_driver_hs20_data_s {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	enum Hs20CmdType CmdType;
	struct param_hs20_set_bssid_pool hs20_set_bssid_pool;
};

#endif 

#endif

typedef struct _NETDEV_PRIVATE_GLUE_INFO {
	P_GLUE_INFO_T prGlueInfo;
	UINT_8 ucBssIdx;
} NETDEV_PRIVATE_GLUE_INFO, *P_NETDEV_PRIVATE_GLUE_INFO;

typedef struct _PACKET_PRIVATE_DATA {
	QUE_ENTRY_T rQueEntry;
	UINT_16 u2Flag;
	UINT_8 ucTid;
	UINT_8 ucBssIdx;

	UINT_8 ucHeaderLen;
	UINT_16 u2FrameLen;

	UINT_8 ucProfilingFlag;
	OS_SYSTIME rArrivalTime;
	UINT_16 u2IpId;
	
	UINT_8 ucSeqNo;
} PACKET_PRIVATE_DATA, *P_PACKET_PRIVATE_DATA;



#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
#define GLUE_SPIN_LOCK_DECLARATION()
#define GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_lock_bh(&(prGlueInfo->rSpinLock[rLockCategory])); \
	    }
#define GLUE_RELEASE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_unlock_bh(&(prGlueInfo->rSpinLock[rLockCategory])); \
	    }
#else 
#define GLUE_SPIN_LOCK_DECLARATION()                        ULONG __ulFlags = 0
#define GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_lock_irqsave(&(prGlueInfo)->rSpinLock[rLockCategory], __ulFlags); \
	    }
#define GLUE_RELEASE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_unlock_irqrestore(&(prGlueInfo->rSpinLock[rLockCategory]), __ulFlags); \
	    }
#endif 


#define GLUE_GET_PKT_PRIVATE_DATA(_p) \
	((P_PACKET_PRIVATE_DATA)(&(((struct sk_buff *)(_p))->cb[0])))

#define GLUE_GET_PKT_QUEUE_ENTRY(_p)    \
	    (&(GLUE_GET_PKT_PRIVATE_DATA(_p)->rQueEntry))

#define GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)  \
	    ((P_NATIVE_PACKET) (((ULONG)_prQueueEntry) - offsetof(struct sk_buff, cb[0])))

#define GLUE_SET_PKT_TID(_p, _tid) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucTid = (UINT_8)(_tid))

#define GLUE_GET_PKT_TID(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucTid)

#define GLUE_SET_PKT_FLAG(_p, _flag) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag |= BIT(_flag))

#define GLUE_TEST_PKT_FLAG(_p, _flag) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag & BIT(_flag))

#define GLUE_IS_PKT_FLAG_SET(_p) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag)

#define GLUE_SET_PKT_BSS_IDX(_p, _ucBssIndex) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucBssIdx = (UINT_8)(_ucBssIndex))

#define GLUE_GET_PKT_BSS_IDX(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucBssIdx)

#define GLUE_SET_PKT_HEADER_LEN(_p, _ucMacHeaderLen) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucHeaderLen = (UINT_8)(_ucMacHeaderLen))

#define GLUE_GET_PKT_HEADER_LEN(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucHeaderLen)

#define GLUE_SET_PKT_FRAME_LEN(_p, _u2PayloadLen) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2FrameLen = (UINT_16)(_u2PayloadLen))

#define GLUE_GET_PKT_FRAME_LEN(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->u2FrameLen)

#define GLUE_SET_PKT_ARRIVAL_TIME(_p, _rSysTime) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->rArrivalTime = (OS_SYSTIME)(_rSysTime))

#define GLUE_GET_PKT_ARRIVAL_TIME(_p)    \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->rArrivalTime)

#define GLUE_SET_PKT_IP_ID(_p, _u2IpId) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2IpId = (UINT_16)(_u2IpId))

#define GLUE_GET_PKT_IP_ID(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->u2IpId)

#define GLUE_SET_PKT_SEQ_NO(_p, _ucSeqNo) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->ucSeqNo = (UINT_8)(_ucSeqNo))

#define GLUE_GET_PKT_SEQ_NO(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucSeqNo)

#define GLUE_SET_PKT_FLAG_PROF_MET(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucProfilingFlag |= BIT(0))

#define GLUE_GET_PKT_IS_PROF_MET(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucProfilingFlag & BIT(0))

#define GLUE_GET_PKT_ETHER_DEST_ADDR(_p)    \
	    ((PUINT_8)&(((struct sk_buff *)(_p))->data))

#define GLUE_CHK_DEV(prDev) \
	((prDev && *((P_GLUE_INFO_T *) netdev_priv(prDev))) ? TRUE : FALSE)

#define GLUE_CHK_PR2(prDev, pr2) \
	((GLUE_CHK_DEV(prDev) && pr2) ? TRUE : FALSE)

#define GLUE_CHK_PR3(prDev, pr2, pr3) \
	((GLUE_CHK_PR2(prDev, pr2) && pr3) ? TRUE : FALSE)

#define GLUE_CHK_PR4(prDev, pr2, pr3, pr4) \
	((GLUE_CHK_PR3(prDev, pr2, pr3) && pr4) ? TRUE : FALSE)

#define GLUE_SET_EVENT(pr) \
	kalSetEvent(pr)

#define GLUE_INC_REF_CNT(_refCount)     atomic_inc((atomic_t *)&(_refCount))
#define GLUE_DEC_REF_CNT(_refCount)     atomic_dec((atomic_t *)&(_refCount))
#define GLUE_GET_REF_CNT(_refCount)     atomic_read((atomic_t *)&(_refCount))

#define DbgPrint(...)

static __KAL_INLINE__ VOID glPacketDataTypeCheck(VOID)
{

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(PACKET_PRIVATE_DATA) <= sizeof(((struct sk_buff *) 0)->cb));

}

#ifdef WLAN_INCLUDE_PROC
INT_32 procRemoveProcfs(VOID);

INT_32 procCreateFsEntry(P_GLUE_INFO_T prGlueInfo);
INT_32 procInitFs(VOID);
INT_32 procUninitProcFs(VOID);
#endif 

#if CFG_ENABLE_BT_OVER_WIFI
BOOLEAN glRegisterAmpc(P_GLUE_INFO_T prGlueInfo);

BOOLEAN glUnregisterAmpc(P_GLUE_INFO_T prGlueInfo);
#endif

P_GLUE_INFO_T wlanGetGlueInfo(VOID);

UINT_16 wlanSelectQueue(struct net_device *dev, struct sk_buff *skb,
			void *accel_priv, select_queue_fallback_t fallback);

VOID wlanDebugInit(VOID);

WLAN_STATUS wlanSetDebugLevel(IN UINT_32 u4DbgIdx, IN UINT_32 u4DbgMask);

WLAN_STATUS wlanGetDebugLevel(IN UINT_32 u4DbgIdx, OUT PUINT_32 pu4DbgMask);

VOID wlanSetSuspendMode(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgEnable);


extern void wlanRegisterNotifier(void);
extern void wlanUnregisterNotifier(void);


typedef int (*set_p2p_mode) (struct net_device *netdev, PARAM_CUSTOM_P2P_SET_STRUCT_T p2pmode);
typedef void (*set_dbg_level) (unsigned char modules[DBG_MODULE_NUM]);
extern void register_set_p2p_mode_handler(set_p2p_mode handler);
extern void register_set_dbg_level_handler(set_dbg_level handler);

#if CFG_ENABLE_EARLY_SUSPEND
extern int glRegisterEarlySuspend(struct early_suspend *prDesc,
				  early_suspend_callback wlanSuspend, late_resume_callback wlanResume);

extern int glUnregisterEarlySuspend(struct early_suspend *prDesc);
#endif

#if CFG_MET_PACKET_TRACE_SUPPORT
VOID kalMetTagPacket(IN P_GLUE_INFO_T prGlueInfo, IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag);

VOID kalMetInit(IN P_GLUE_INFO_T prGlueInfo);
#endif

VOID wlanUpdateChannelTable(P_GLUE_INFO_T prGlueInfo);

#ifdef FW_CFG_SUPPORT
INT_32 cfgCreateProcEntry(P_GLUE_INFO_T prGlueInfo);
INT_32 cfgRemoveProcEntry(void);
#endif
#endif 
