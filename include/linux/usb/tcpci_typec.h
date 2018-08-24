/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Author: TH <tsunghan_tsai@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_TCPCI_TYPEC_H
#define __LINUX_TCPCI_TYPEC_H
#include <linux/usb/tcpci.h>

struct tcpc_device;


extern int tcpc_typec_handle_ra_detach(
	struct tcpc_device *tcpc_dev);

extern int tcpc_typec_handle_cc_change(
	struct tcpc_device *tcpc_dev);

extern int tcpc_typec_handle_ps_change(
		struct tcpc_device *tcpc_dev, int vbus_level);

extern int tcpc_typec_handle_timeout(
		struct tcpc_device *tcpc_dev, uint32_t timer_id);

extern int tcpc_typec_handle_vsafe0v(struct tcpc_device *tcpc_dev);

extern int tcpc_typec_set_rp_level(struct tcpc_device *tcpc_dev, uint8_t res);

extern int tcpc_typec_change_role(
	struct tcpc_device *tcpc_dev, uint8_t typec_role);

#ifdef CONFIG_USB_POWER_DELIVERY
extern int tcpc_typec_advertise_explicit_contract(struct tcpc_device *tcpc_dev);
extern int tcpc_typec_handle_pe_pr_swap(struct tcpc_device *tcpc_dev);
#endif 

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
extern int tcpc_typec_swap_role(struct tcpc_device *tcpc_dev);
#endif 

#endif 
