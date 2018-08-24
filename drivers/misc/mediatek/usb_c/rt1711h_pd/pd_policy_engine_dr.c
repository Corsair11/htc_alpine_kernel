/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Power Delivery Policy Engine for DR
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

#include <linux/usb/pd_core.h>
#include <linux/usb/pd_dpm_core.h>
#include <linux/usb/tcpci.h>
#include <linux/usb/pd_policy_engine.h>


void pe_dr_src_get_source_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_GET_SOURCE_CAP);
}

void pe_dr_src_get_source_cap_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
	pd_dpm_dr_inform_source_cap(pd_port, pd_event);
}

void pe_dr_src_give_sink_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_send_sink_caps(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dr_snk_get_sink_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_send_ctrl_msg(pd_port, TCPC_TX_SOP, PD_CTRL_GET_SINK_CAP);
}

void pe_dr_snk_get_sink_cap_exit(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
	pd_dpm_dr_inform_sink_cap(pd_port, pd_event);
}

void pe_dr_snk_give_source_cap_entry(pd_port_t *pd_port, pd_event_t *pd_event)
{
	pd_dpm_send_source_caps(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}