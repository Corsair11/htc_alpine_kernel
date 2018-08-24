


#ifndef _CONFIG_H
#define _CONFIG_H


#ifdef MT6620
#undef MT6620
#endif

#ifndef MT6628
#define MT6628
#endif


#define MTK_WCN_SINGLE_MODULE		0	

#ifdef LINUX
#ifdef CONFIG_X86
#define MTK_WCN_HIF_SDIO        0
#else
#define MTK_WCN_HIF_SDIO        0	
#endif
#else
#define MTK_WCN_HIF_SDIO            0
#endif

#if (CFG_SUPPORT_AEE == 1)
#define CFG_ENABLE_AEE_MSG          1
#else
#define CFG_ENABLE_AEE_MSG          0
#endif

#if CFG_ENABLE_AEE_MSG
#include <mt-plat/aee.h>
#endif

#define CFG_TX_FRAGMENT                 1	
#define CFG_SUPPORT_PERFORMANCE_TEST    0	

#define CFG_COUNTRY_CODE                NULL	

#ifndef LINUX
#define CFG_FW_FILENAME             L"WIFI_RAM_CODE"
#define CFG_FW_FILENAME_E6          L"WIFI_RAM_CODE_E6"
#else
#define CFG_FW_FILENAME             "WIFI_RAM_CODE"
#endif
#ifndef LINUX
#define CFG_SUPPORT_CFG_FILE     0
#else
#define CFG_SUPPORT_CFG_FILE     1
#endif

#define CFG_SUPPORT_CE_FCC_TXPWR_LIMIT 0 

#define CFG_SUPPORT_802_11D             1	

#define CFG_SUPPORT_RRM             0	
#define CFG_SUPPORT_DFS             1	

#if (CFG_SUPPORT_DFS == 1)	
#define CFG_SUPPORT_QUIET           1	
#define CFG_SUPPORT_SPEC_MGMT       1	
#else
#define CFG_SUPPORT_QUIET           0	
#define CFG_SUPPORT_SPEC_MGMT       0	
#endif

#define CFG_SUPPORT_RX_RDG          0	
#define CFG_SUPPORT_MFB             0	
#define CFG_SUPPORT_RX_STBC         1	
#define CFG_SUPPORT_RX_SGI          1	
#define CFG_SUPPORT_RX_HT_GF        1	

#define CFG_SUPPORT_ROAMING_ENC		0	

#define CFG_SUPPORT_TDLS			1	
#define CFG_SUPPORT_TDLS_DBG		0	
#define CFG_SUPPORT_STATISTICS		1
#define CFG_SUPPORT_DBG_POWERMODE	1	

#define CFG_SUPPORT_GSCN   1

#define CFG_SUPPORT_TXR_ENC			0	

#define CFG_SUPPORT_PERSIST_NETDEV		0	

#define CFG_FORCE_USE_20BW			0

#define CFG_SUPPORT_RN				1

#define CFG_SLT_SUPPORT                             0

#define	MTK_AUTO_CHANNEL_SEL_SUPPORT_ENABLE				0

#if defined(MTK_AUTO_CHANNEL_SEL_SUPPORT_ENABLE)
#define CFG_AUTO_CHANNEL_SEL_SUPPORT				1
#else
#define CFG_AUTO_CHANNEL_SEL_SUPPORT				0
#endif

#ifdef NDIS60_MINIPORT
#define CFG_NATIVE_802_11                       1

#define CFG_TX_MAX_PKT_SIZE                     2304
#define CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60       0	
#define CFG_TCP_IP_CHKSUM_OFFLOAD               0
#define CFG_WHQL_DOT11_STATISTICS               1
#define CFG_WHQL_ADD_REMOVE_KEY                 1
#define CFG_WHQL_CUSTOM_IE                      1
#define CFG_WHQL_SAFE_MODE_ENABLED              1

#else
#define CFG_TCP_IP_CHKSUM_OFFLOAD               1	
#define CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60       0
#define CFG_TX_MAX_PKT_SIZE                     1600
#define CFG_NATIVE_802_11                       0
#endif

#define CFG_EHPI_FASTER_BUS_TIMING                  0	

#ifdef _lint
#define _HIF_SDIO   0		
#endif

#define CFG_SDIO_INTR_ENHANCE                        1	
#define CFG_SDIO_RX_ENHANCE                          0	
#define CFG_SDIO_TX_AGG                              1	

#define CFG_SDIO_RX_AGG                              1	

#if (CFG_SDIO_RX_AGG == 1) && (CFG_SDIO_INTR_ENHANCE == 0)
#error "CFG_SDIO_INTR_ENHANCE should be 1 once CFG_SDIO_RX_AGG equals to 1"
#elif (CFG_SDIO_INTR_ENHANCE == 1 || CFG_SDIO_RX_ENHANCE == 1) && (CFG_SDIO_RX_AGG == 0)
#error "CFG_SDIO_RX_AGG should be 1 once CFG_SDIO_INTR_ENHANCE and/or CFG_SDIO_RX_ENHANCE equals to 1"
#endif

#define CFG_SDIO_MAX_RX_AGG_NUM                     0	

#ifdef WINDOWS_CE
#define CFG_SDIO_PATHRU_MODE                    1	
#else
#define CFG_SDIO_PATHRU_MODE                    0	
#endif

#define CFG_MAX_RX_ENHANCE_LOOP_COUNT               3

#if defined(MT6620)
#define MT6620_FPGA_BWCS    0
#define MT6620_FPGA_V5      0

#if (MT6620_FPGA_BWCS == 1) && (MT6620_FPGA_V5 == 1)
#error
#endif

#if (MTK_WCN_HIF_SDIO == 1)
#define CFG_MULTI_ECOVER_SUPPORT    1
#elif !defined(LINUX)
#define CFG_MULTI_ECOVER_SUPPORT    1
#else
#define CFG_MULTI_ECOVER_SUPPORT    0
#endif

#define CFG_ENABLE_CAL_LOG      0
#define CFG_REPORT_RFBB_VERSION       0

#elif defined(MT6628)

#define CFG_MULTI_ECOVER_SUPPORT    0

#define CFG_ENABLE_CAL_LOG      1
#define CFG_REPORT_RFBB_VERSION       1

#endif

#define CFG_CHIP_RESET_SUPPORT                      1

#if defined(MT6628)
#define CFG_EMBED_FIRMWARE_BUILD_DATE_CODE  1
#endif

#if defined(MT6620) && (MT6620_FPGA_BWCS == 0) && (MT6620_FPGA_V5 == 0)
#define MT6620_E1_ASIC_HIFSYS_WORKAROUND            0
#else
#define MT6620_E1_ASIC_HIFSYS_WORKAROUND            0
#endif

#define CFG_SPM_WORKAROUND_FOR_HOTSPOT                  1

#define CFG_DRV_OWN_VERSION \
	((UINT_16)((NIC_DRIVER_MAJOR_VERSION << 8) | (NIC_DRIVER_MINOR_VERSION)))
#define CFG_DRV_PEER_VERSION                    ((UINT_16)0x0000)

#if defined(LINUX)
#define CFG_TX_BUFFER_IS_SCATTER_LIST       1	
#else 
#define CFG_TX_BUFFER_IS_SCATTER_LIST       1
#endif 

#if CFG_SDIO_TX_AGG || CFG_TX_BUFFER_IS_SCATTER_LIST
#define CFG_COALESCING_BUFFER_SIZE          (CFG_TX_MAX_PKT_SIZE * NIC_TX_BUFF_SUM)
#else
#define CFG_COALESCING_BUFFER_SIZE          (CFG_TX_MAX_PKT_SIZE)
#endif 


#define CFG_TX_MAX_PKT_NUM                      512	

#define CFG_TX_MAX_CMD_PKT_NUM                  32

#define CFG_NUM_OF_STA_RECORD                   20


#if CFG_SLT_SUPPORT
#define CFG_NUM_OF_RX0_HIF_DESC                 42
#else
#define CFG_NUM_OF_RX0_HIF_DESC                 16
#endif
#define CFG_NUM_OF_RX1_HIF_DESC                 2

#define CFG_NUM_OF_QM_RX_PKT_NUM                120

#define CFG_RX_MAX_PKT_NUM                      ((CFG_NUM_OF_RX0_HIF_DESC + CFG_NUM_OF_RX1_HIF_DESC) * 3 \
						+ CFG_NUM_OF_QM_RX_PKT_NUM)

#define CFG_RX_REORDER_Q_THRESHOLD              8

#ifndef LINUX
#define CFG_RX_RETAINED_PKT_THRESHOLD \
	(CFG_NUM_OF_RX0_HIF_DESC + CFG_NUM_OF_RX1_HIF_DESC + CFG_NUM_OF_QM_RX_PKT_NUM)
#else
#define CFG_RX_RETAINED_PKT_THRESHOLD           0
#endif

#define CFG_RX_MAX_PKT_SIZE   (28 + 2312 + 12 )	

#define CFG_RX_MIN_PKT_SIZE                     10	

#if CFG_SDIO_RX_AGG
    
#define CFG_RX_COALESCING_BUFFER_SIZE       ((CFG_NUM_OF_RX0_HIF_DESC  + 1) \
						* CFG_RX_MAX_PKT_SIZE)
#else
#define CFG_RX_COALESCING_BUFFER_SIZE       (CFG_RX_MAX_PKT_SIZE)
#endif

#define CFG_NUM_OF_RX_BA_AGREEMENTS             8
#define CFG_RX_BA_MAX_WINSIZE                   16
#define CFG_RX_BA_INC_SIZE                      4
#define CFG_RX_MAX_BA_TID_NUM                   8
#define CFG_RX_REORDERING_ENABLED               1

#define CFG_RESPONSE_POLLING_TIMEOUT            512

#define CFG_MAX_NUM_BSS_LIST                    128

#define CFG_MAX_COMMON_IE_BUF_LEN         ((1500 * CFG_MAX_NUM_BSS_LIST) / 3)

#define CFG_RAW_BUFFER_SIZE                      1024

#define CFG_IE_BUFFER_SIZE                      512

#define CFG_MAX_NUM_STA_RECORD                  32

#define CFG_ENABLE_FULL_PM                      1
#define CFG_ENABLE_WAKEUP_ON_LAN                0
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || \
	defined(CONFIG_ARCH_MT6753) || defined(CONFIG_ARCH_MT6580)
#define CFG_SUPPORT_WAKEUP_REASON_DEBUG			1	
#else
#define CFG_SUPPORT_WAKEUP_REASON_DEBUG			0	
#endif
#define CFG_INIT_POWER_SAVE_PROF                    ENUM_PSP_FAST_SWITCH

#define CFG_INIT_ENABLE_PATTERN_FILTER_ARP                    0

#define CFG_INIT_UAPSD_AC_BMP                    0	

#define CFG_SUPPORT_WPS                          1
#define CFG_SUPPORT_WPS2                         1

#define CFG_MAX_PMKID_CACHE                     16	

#define MAX_AUTO_CHAL_NUM						23	
#define CFG_ENABLE_FAST_SCAN                    0
#define CFG_CN_SUPPORT_CLASS121                 0        
#if CFG_ENABLE_FAST_SCAN
	#define CFG_FAST_SCAN_DWELL_TIME                  40
	#define CFG_FAST_SCAN_REG_DOMAIN_DEF_IDX          10
#endif
#define CFG_INIT_ADHOC_FREQ                     (2462000)
#define CFG_INIT_ADHOC_MODE                     AD_HOC_MODE_MIXED_11BG
#define CFG_INIT_ADHOC_BEACON_INTERVAL          (100)
#define CFG_INIT_ADHOC_ATIM_WINDOW              (0)

#define CFG_SCAN_SSID_MAX_NUM                   (4)
#define CFG_SCAN_SSID_MATCH_MAX_NUM             (16)



#if defined(_HIF_SDIO) && defined(WINDOWS_CE)
#define CFG_IST_LOOP_COUNT                  8
#else
#define CFG_IST_LOOP_COUNT                  8
#endif 

#define CFG_INT_WRITE_CLEAR                     0

#if defined(LINUX)
#define CFG_DBG_GPIO_PINS                       0	
#endif

#define CFG_DBG_MGT_BUF                         1	

#define CFG_HIF_STATISTICS                      0

#define CFG_HIF_RX_STARVATION_WARNING           0

#define CFG_STARTUP_DEBUG                       0

#define CFG_RX_PKTS_DUMP                        1

#define CFG_ENABLE_FW_DOWNLOAD                  1

#define CFG_ENABLE_FW_DOWNLOAD_ACK              1
#define CFG_ENABLE_FW_ENCRYPTION                1

#if defined(MT6628)
#define CFG_ENABLE_FW_DOWNLOAD_AGGREGATION  0
#define CFG_ENABLE_FW_DIVIDED_DOWNLOAD      1
#endif

#if defined(MT6620)
#if MT6620_FPGA_BWCS
#define CFG_FW_LOAD_ADDRESS                     0x10014000
#define CFG_OVERRIDE_FW_START_ADDRESS           0
#define CFG_FW_START_ADDRESS                    0x10014001
#elif MT6620_FPGA_V5
#define CFG_FW_LOAD_ADDRESS                     0x10008000
#define CFG_OVERRIDE_FW_START_ADDRESS           0
#define CFG_FW_START_ADDRESS                    0x10008001
#else
#define CFG_FW_LOAD_ADDRESS                     0x10008000
#define CFG_OVERRIDE_FW_START_ADDRESS           0
#define CFG_FW_START_ADDRESS                    0x10008001
#endif
#elif defined(MT6628)
#define CFG_FW_LOAD_ADDRESS                     0x00060000
#define CFG_OVERRIDE_FW_START_ADDRESS           1
#define CFG_FW_START_ADDRESS                    0x00060000
#define CFG_START_ADDRESS_IS_1ST_SECTION_ADDR   1
#endif


#ifdef LINUX
#ifdef CONFIG_X86
#define CFG_ENABLE_BT_OVER_WIFI         0
#else
#define CFG_ENABLE_BT_OVER_WIFI         1
#endif
#else
#define CFG_ENABLE_BT_OVER_WIFI             0
#endif

#define CFG_BOW_SEPARATE_DATA_PATH              1

#define CFG_BOW_PHYSICAL_LINK_NUM               4

#define CFG_BOW_TEST                            0

#define CFG_BOW_LIMIT_AIS_CHNL                  1

#define CFG_BOW_SUPPORT_11N                     0

#define CFG_BOW_RATE_LIMITATION                 1

#ifdef LINUX
#ifdef CONFIG_X86
#define CFG_ENABLE_WIFI_DIRECT          0
#define CFG_SUPPORT_802_11W             0
#else
#define CFG_ENABLE_WIFI_DIRECT          1
#define CFG_SUPPORT_802_11W             0	
#endif
#else
#define CFG_ENABLE_WIFI_DIRECT              0
#define CFG_SUPPORT_802_11W                 0	
#endif

#define CFG_SUPPORT_PERSISTENT_GROUP     0

#define CFG_TEST_WIFI_DIRECT_GO                 0

#define CFG_TEST_ANDROID_DIRECT_GO              0

#define CFG_UNITEST_P2P                         0

#define CFG_ENABLE_WIFI_DIRECT_CFG_80211        1

#define CFG_SUPPORT_HOTSPOT_OPTIMIZATION                1
#define     CFG_HOTSPOT_OPTIMIZATION_BEACON_INTERVAL    300
#define     CFG_HOTSPOT_OPTIMIZATION_DTIM               1

#define CFG_SUPPORT_EXT_CONFIG                  0

#if CFG_SUPPORT_PERFORMANCE_TEST
#define CFG_ENABLE_STATISTICS_BUFFERING         1
#else
#define CFG_ENABLE_STATISTICS_BUFFERING         0
#endif
#define CFG_STATISTICS_VALID_CYCLE              2000
#define CFG_LINK_QUALITY_VALID_PERIOD           5000

#define CFG_SUPPORT_ADHOC                       0
#define CFG_SUPPORT_AAA                         1

#define CFG_SUPPORT_BCM                         0
#define CFG_SUPPORT_BCM_BWCS                    0
#define CFG_SUPPORT_BCM_BWCS_DEBUG              0

#define CFG_SUPPORT_RDD_TEST_MODE       0

#define CFG_SUPPORT_PWR_MGT                     1

#define CFG_RSN_MIGRATION                       1

#define CFG_PRIVACY_MIGRATION                   1

#define CFG_ENABLE_HOTSPOT_PRIVACY_CHECK        1

#define CFG_MGMT_FRAME_HANDLING                 1

#define CFG_MGMT_HW_ACCESS_REPLACEMENT          0

#if CFG_SUPPORT_PERFORMANCE_TEST

#else

#endif

#define CFG_SUPPORT_AIS_5GHZ                    1
#define CFG_SUPPORT_BEACON_CHANGE_DETECTION     0

#define CFG_SUPPORT_NVRAM                       1
#define CFG_NVRAM_EXISTENCE_CHECK               1
#define CFG_SW_NVRAM_VERSION_CHECK              1
#define CFG_SUPPORT_NIC_CAPABILITY              1

#define CFG_STRESS_TEST_SUPPORT                 0

#define LINT_SAVE_AND_DISABLE	

#define LINT_RESTORE		

#define LINT_EXT_HEADER_BEGIN                   LINT_SAVE_AND_DISABLE

#define LINT_EXT_HEADER_END                     LINT_RESTORE


#define CFG_SUPPORT_QOS             1	
#define CFG_SUPPORT_AMPDU_TX        1
#define CFG_SUPPORT_AMPDU_RX        1
#define CFG_SUPPORT_TSPEC           0	
#define CFG_SUPPORT_UAPSD           1
#define CFG_SUPPORT_UL_PSMP         0

#define CFG_SUPPORT_ROAMING         1	
#define CFG_SUPPORT_SWCR            1

#define CFG_SUPPORT_ANTI_PIRACY     1

#define CFG_SUPPORT_OSC_SETTING     1

#define CFG_SUPPORT_P2P_RSSI_QUERY        0

#if 0 
#define CFG_SHOW_MACADDR_SOURCE     1
#else
#define CFG_SHOW_MACADDR_SOURCE     0
#endif

#define CFG_SUPPORT_802_11V                    0	
#define CFG_SUPPORT_802_11V_TIMING_MEASUREMENT 0
#if (CFG_SUPPORT_802_11V_TIMING_MEASUREMENT == 1) && (CFG_SUPPORT_802_11V == 0)
#error "CFG_SUPPORT_802_11V should be 1 once CFG_SUPPORT_802_11V_TIMING_MEASUREMENT equals to 1"
#endif
#if (CFG_SUPPORT_802_11V == 0)
#define WNM_UNIT_TEST 0
#endif

#define CFG_DRIVER_COMPOSE_ASSOC_REQ   1

#define CFG_STRICT_CHECK_CAPINFO_PRIVACY    0

#define CFG_SUPPORT_WFD                     1

#define CFG_SUPPORT_WFD_COMPOSE_IE          1

#define CFG_ENABLE_PKT_LIFETIME_PROFILE     1

#define CFG_ENABLE_PER_STA_STATISTICS       1

#define CFG_PRINT_RTP_PROFILE               0	
#define CFG_PRINT_RTP_SN_SKIP               0

#define CFG_SUPPORT_PWR_LIMIT_COUNTRY       1
#define CFG_FORCE_RESET_UNDER_BUS_ERROR     0

#define CFG_SUPPORT_BUILD_DATE_CODE 1

#define COMPILE_FLAG0_GET_STA_LINK_STATUS     (1<<0)
#define COMPILE_FLAG0_WFD_ENHANCEMENT_PROTECT (1<<1)

#define CFG_SUPPORT_BATCH_SCAN              0
#define CFG_BATCH_MAX_MSCAN                 2

#define CFG_SUPPORT_GET_CH_ENV              1


#define CFG_SUPPORT_THERMO_THROTTLING       1
#define WLAN_INCLUDE_PROC                   1

#define CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE 0
#define CFG_SUPPORT_DROP_MC_PACKET		1





#endif 
