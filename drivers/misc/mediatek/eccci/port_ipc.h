/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __PORT_IPC_H__
#define __PORT_IPC_H__

#include <linux/wait.h>
#include "ccci_core.h"
#include "ccci_config.h"

#define MAX_NUM_IPC_TASKS 10
#define CCCI_TASK_PENDING 0x01
#define IPC_MSGSVC_RVC_DONE 0x12344321
#define CCCI_IPC_MINOR_BASE 100

typedef enum {
	IPC_L4C_MSG_ID_BEGIN = 0x80000000,
#if defined(IPC_L4C_MSG_ID_LEN)
	IPC_L4C_MSG_ID_RANGE = IPC_L4C_MSG_ID_LEN,
#else
	IPC_L4C_MSG_ID_RANGE = 0x80,
#endif
	IPC_EL1_MSG_ID_BEGIN = IPC_L4C_MSG_ID_BEGIN + IPC_L4C_MSG_ID_RANGE,
	IPC_EL1_MSG_ID_RANGE = 0x20,
	IPC_CCCIIPC_MSG_ID_BEGIN = IPC_EL1_MSG_ID_BEGIN + IPC_EL1_MSG_ID_RANGE,
	IPC_CCCIIPC_MSG_ID_RANGE = 0x10,
	IPC_IPCORE_MSG_ID_BEGIN = IPC_CCCIIPC_MSG_ID_BEGIN + IPC_CCCIIPC_MSG_ID_RANGE,
	IPC_IPCORE_MSG_ID_RANGE = 0x8,
	IPC_MDT_MSG_ID_BEGIN = IPC_IPCORE_MSG_ID_BEGIN + IPC_IPCORE_MSG_ID_RANGE,
	IPC_MDT_MSG_ID_RANGE = 0x8,
	IPC_UFPM_MSG_ID_BEGIN = IPC_MDT_MSG_ID_BEGIN + IPC_MDT_MSG_ID_RANGE,
	IPC_UFPM_MSG_ID_RANGE = 0x18,
#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
	IPC_WAAL_MSG_ID_BEGIN = IPC_UFPM_MSG_ID_BEGIN + IPC_UFPM_MSG_ID_RANGE,
	IPC_WAAL_MSG_ID_RANGE = 0x20,
#endif
} CCCI_IPC_MSG_ID_RANGE;

#ifdef CONFIG_HTC_FEATURES_RIL_PCN0007_LWA
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;

#define LOCAL_PARA_HDR \
   uint8    ref_count; \
   uint8    lp_reserved; \
   uint16    msg_len;

typedef enum
{
    MSG_ID_IPC_INVALID_TYPE = IPC_L4C_MSG_ID_BEGIN,
    MSG_ID_IPC_WAAL_INVALID = IPC_WAAL_MSG_ID_BEGIN,
    MSG_ID_LWAD_WAAL_WLAN_CAPABILITY_REQ,
    MSG_ID_LWAD_WAAL_WLAN_CAPABILITY_IND,
    MSG_ID_LWAD_WAAL_WLAN_LWA_CONNECT_REQ,
    MSG_ID_LWAD_WAAL_WLAN_LWA_CONNECT_CNF,
    MSG_ID_LWAD_WAAL_WLAN_LWA_DISCONNECT_REQ,
    MSG_ID_LWAD_WAAL_WLAN_LWA_DISCONNECT_CNF,
    MSG_ID_LWAD_WAAL_WLAN_LWA_LOST_IND,
    MSG_ID_LWAD_WAAL_WLAN_MEASUREMENT_REQ,
    MSG_ID_LWAD_WAAL_WLAN_MEASUREMENT_IND,
    MSG_ID_LWAD_WAAL_WLAN_MEASUREMENT_STOP_REQ,
    MSG_ID_IPC_WAAL_END,
    MSG_ID_IPC_END,
}LWA_IPC_MSG_ID;

typedef enum {
    WAAL_ERRC_WLAN_LWA_CONNECTION_SUCCESS,
    WAAL_ERRC_WLAN_LWA_CONNECTION_FAILURE
} waal_errc_wlan_lwa_connect_result_enum;

typedef struct ilm_struct {
	uint	src_mod_id;      
	uint	dest_mod_id;     
	uint	sap_id;          
	uint	msg_id;          
	int	local_para_ptr; 
	int	peer_buff_ptr;  
} ilm_struct;

typedef struct {
    LOCAL_PARA_HDR
    uint8 result;
    uint8 failure_cause;
} waal_errc_wlan_lwa_connect_cnf_struct;
#endif

struct ccci_ipc_ctrl {
	unsigned char task_id;
	unsigned char md_is_ready;
	unsigned long flag;
	wait_queue_head_t tx_wq;
	wait_queue_head_t md_rdy_wq;
	struct ccci_port *port;
};

struct ipc_task_id_map {
	u32 extq_id;		
	u32 task_id;		
};

typedef struct local_para {
	u8 ref_count;
	u8 _stub;		
	u16 msg_len;
	u8 data[0];
} __packed local_para_struct;

typedef struct peer_buff {
	u16 pdu_len;
	u8 ref_count;
	u8 pb_resvered;
	u16 free_header_space;
	u16 free_tail_space;
	u8 data[0];
} __packed peer_buff_struct;

typedef struct {
	u32 src_mod_id;
	u32 dest_mod_id;
	u32 sap_id;
	u32 msg_id;
	struct local_para *local_para_ptr;
	struct peer_buff *peer_buff_ptr;
} ipc_ilm_t;			

struct ccci_ipc_ilm {
	u32 src_mod_id;
	u32 dest_mod_id;
	u32 sap_id;
	u32 msg_id;
	u32 local_para_ptr;
	u32 peer_buff_ptr;
} __packed;	

struct garbage_filter_header {
	u32 filter_set_id;
	u32 filter_cnt;
	u32 uplink;
} __packed;

struct garbage_filter_item {
	u32 filter_id;
	u8 ip_type;
	u8 protocol;
	u16 dst_port;
	u32 magic_code;
} __packed;

struct ccci_emi_info {
	u8 ap_domain_id;
	u8 md_domain_id;
	u8 reserve[6];
	u64 ap_view_bank0_base;
	u64 bank0_size;
	u64 ap_view_bank4_base;
	u64 bank4_size;
} __packed;

typedef enum {
	GF_IPV4V6 = 0,
	GF_IPV4,
	GF_IPV6
} GF_IP_TYPE;

typedef enum {
	GF_TCP = 6,
	GF_UDP = 17
} GF_PROTOCOL_TYPE;

int port_ipc_init(struct ccci_port *port);
int port_ipc_req_match(struct ccci_port *port, struct ccci_request *req);
int port_ipc_tx_wait(struct ccci_port *port);
int port_ipc_rx_ack(struct ccci_port *port);
int port_ipc_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg);
void port_ipc_md_state_notice(struct ccci_port *port, MD_STATE state);
int port_ipc_write_check_id(struct ccci_port *port, struct ccci_request *req);
unsigned int port_ipc_poll(struct file *fp, struct poll_table_struct *poll);

int ccci_ipc_send_ilm(int md_id, ipc_ilm_t *in_ilm);
int ccci_get_emi_info(int md_id, struct ccci_emi_info *emi_info);

#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT) || defined(CONFIG_MTK_MD_DIRECT_LOGGING_SUPPORT)
extern int musb_md_msg_hdlr(ipc_ilm_t *ilm);
#endif
#if defined(CONFIG_MTK_MD_DIRECT_TETHERING_SUPPORT)
extern int pkt_track_md_msg_hdlr(ipc_ilm_t *ilm);
#endif

#endif				
