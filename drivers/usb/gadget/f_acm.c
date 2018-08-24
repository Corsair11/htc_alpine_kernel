/*
 * f_acm.c -- USB CDC serial (ACM) function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 * Copyright (C) 2009 by Samsung Electronics
 * Author: Michal Nazarewicz (mina86@mina86.com)
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/printk.h>
#include "u_serial.h"
#include "gadget_chips.h"


#define ACM_LOG "USB_ACM"


struct f_acm {
	struct gserial			port;
	u8				ctrl_id, data_id;
	u8				port_num;

	u8				pending;

	spinlock_t			lock;

	struct usb_ep			*notify;
	struct usb_request		*notify_req;

	struct usb_cdc_line_coding	port_line_coding;	

	
	u16				port_handshake_bits;
#define ACM_CTRL_RTS	(1 << 1)	
#define ACM_CTRL_DTR	(1 << 0)	

	
	u16				serial_state;
#define ACM_CTRL_OVERRUN	(1 << 6)
#define ACM_CTRL_PARITY		(1 << 5)
#define ACM_CTRL_FRAMING	(1 << 4)
#define ACM_CTRL_RI		(1 << 3)
#define ACM_CTRL_BRK		(1 << 2)
#define ACM_CTRL_DSR		(1 << 1)
#define ACM_CTRL_DCD		(1 << 0)
};

static inline struct f_acm *func_to_acm(struct usb_function *f)
{
	return container_of(f, struct f_acm, port.func);
}

static inline struct f_acm *port_to_acm(struct gserial *p)
{
	return container_of(p, struct f_acm, port);
}



#define GS_NOTIFY_INTERVAL_MS		32
#define GS_NOTIFY_MAXPACKET		10	


static struct usb_interface_assoc_descriptor
acm_iad_descriptor = {
	.bLength =		sizeof acm_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	
	.bInterfaceCount = 	2,	
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	
};


static struct usb_interface_descriptor acm_control_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =	USB_CDC_ACM_PROTO_AT_V25TER,
	
};

static struct usb_interface_descriptor acm_data_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	
};

static struct usb_cdc_header_desc acm_header_desc = {
	.bLength =		sizeof(acm_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
acm_call_mgmt_descriptor = {
	.bLength =		sizeof(acm_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	0,
	
};

static struct usb_cdc_acm_descriptor acm_descriptor = {
	.bLength =		sizeof(acm_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_LINE,
};

static struct usb_cdc_union_desc acm_union_desc = {
	.bLength =		sizeof(acm_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	
	
};


static struct usb_endpoint_descriptor acm_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_NOTIFY_INTERVAL_MS,
};

static struct usb_endpoint_descriptor acm_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor acm_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *acm_fs_function[] = {
	(struct usb_descriptor_header *) &acm_iad_descriptor,
	(struct usb_descriptor_header *) &acm_control_interface_desc,
	(struct usb_descriptor_header *) &acm_header_desc,
	(struct usb_descriptor_header *) &acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &acm_descriptor,
	(struct usb_descriptor_header *) &acm_union_desc,
	(struct usb_descriptor_header *) &acm_fs_notify_desc,
	(struct usb_descriptor_header *) &acm_data_interface_desc,
	(struct usb_descriptor_header *) &acm_fs_in_desc,
	(struct usb_descriptor_header *) &acm_fs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor acm_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		USB_MS_TO_HS_INTERVAL(GS_NOTIFY_INTERVAL_MS),
};

static struct usb_endpoint_descriptor acm_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor acm_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *acm_hs_function[] = {
	(struct usb_descriptor_header *) &acm_iad_descriptor,
	(struct usb_descriptor_header *) &acm_control_interface_desc,
	(struct usb_descriptor_header *) &acm_header_desc,
	(struct usb_descriptor_header *) &acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &acm_descriptor,
	(struct usb_descriptor_header *) &acm_union_desc,
	(struct usb_descriptor_header *) &acm_hs_notify_desc,
	(struct usb_descriptor_header *) &acm_data_interface_desc,
	(struct usb_descriptor_header *) &acm_hs_in_desc,
	(struct usb_descriptor_header *) &acm_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor acm_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor acm_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor acm_ss_bulk_comp_desc = {
	.bLength =              sizeof acm_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *acm_ss_function[] = {
	(struct usb_descriptor_header *) &acm_iad_descriptor,
	(struct usb_descriptor_header *) &acm_control_interface_desc,
	(struct usb_descriptor_header *) &acm_header_desc,
	(struct usb_descriptor_header *) &acm_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &acm_descriptor,
	(struct usb_descriptor_header *) &acm_union_desc,
	(struct usb_descriptor_header *) &acm_hs_notify_desc,
	(struct usb_descriptor_header *) &acm_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &acm_data_interface_desc,
	(struct usb_descriptor_header *) &acm_ss_in_desc,
	(struct usb_descriptor_header *) &acm_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &acm_ss_out_desc,
	(struct usb_descriptor_header *) &acm_ss_bulk_comp_desc,
	NULL,
};


#define ACM_CTRL_IDX	0
#define ACM_DATA_IDX	1
#define ACM_IAD_IDX	2

static struct usb_string acm_string_defs[] = {
	[ACM_CTRL_IDX].s = "CDC Abstract Control Model (ACM)",
	[ACM_DATA_IDX].s = "CDC ACM Data",
	[ACM_IAD_IDX ].s = "CDC Serial",
	{  } 
};

static struct usb_gadget_strings acm_string_table = {
	.language =		0x0409,	
	.strings =		acm_string_defs,
};

static struct usb_gadget_strings *acm_strings[] = {
	&acm_string_table,
	NULL,
};



static void acm_complete_set_line_coding(struct usb_ep *ep,
		struct usb_request *req)
{
	struct f_acm	*acm = ep->driver_data;
	struct usb_composite_dev *cdev = acm->port.func.config->cdev;

	if (req->status != 0) {
		DBG(cdev, "acm ttyGS%d completion, err %d\n",
				acm->port_num, req->status);
		return;
	}

	
	if (req->actual != sizeof(acm->port_line_coding)) {
		DBG(cdev, "acm ttyGS%d short resp, len %d\n",
				acm->port_num, req->actual);
		usb_ep_set_halt(ep);
	} else {
		struct usb_cdc_line_coding	*value = req->buf;

		acm->port_line_coding = *value;

		pr_debug("[XLOG_INFO][USB_ACM] %s: rate=%d, stop=%d, parity=%d, data=%d\n", __func__, \
			acm->port_line_coding.dwDTERate, acm->port_line_coding.bCharFormat, \
			acm->port_line_coding.bParityType, acm->port_line_coding.bDataBits);
	}
}

static int acm_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_acm		*acm = func_to_acm(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);


	pr_debug("[XLOG_INFO][USB_ACM]%s: ttyGS%d req%02x.%02x v%04x i%04x len=%d\n", __func__, \
		acm->port_num, ctrl->bRequestType, ctrl->bRequest, w_value, w_index, w_length);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_LINE_CODING:
		if (w_length != sizeof(struct usb_cdc_line_coding)
				|| w_index != acm->ctrl_id)
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = acm;
		req->complete = acm_complete_set_line_coding;
		break;

	
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_GET_LINE_CODING:
		if (w_index != acm->ctrl_id)
			goto invalid;

		value = min_t(unsigned, w_length,
				sizeof(struct usb_cdc_line_coding));
		memcpy(req->buf, &acm->port_line_coding, value);

		pr_debug("[XLOG_INFO][USB_ACM]%s: rate=%d,stop=%d,parity=%d,data=%d\n", __func__, \
			acm->port_line_coding.dwDTERate, acm->port_line_coding.bCharFormat, \
			acm->port_line_coding.bParityType, acm->port_line_coding.bDataBits);

		break;

	
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		if (w_index != acm->ctrl_id)
			goto invalid;

		value = 0;

		acm->port_handshake_bits = w_value;
		break;

	default:
invalid:
		VDBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	
	if (value >= 0) {
		DBG(cdev, "acm ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
			acm->port_num, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "acm response on ttyGS%d, err %d\n",
					acm->port_num, value);
	}

	
	return value;
}

static int acm_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_acm		*acm = func_to_acm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	

	if (intf == acm->ctrl_id) {
		if (acm->notify->driver_data) {
			VDBG(cdev, "reset acm control interface %d\n", intf);
			usb_ep_disable(acm->notify);
		}

		if (!acm->notify->desc)
			if (config_ep_by_speed(cdev->gadget, f, acm->notify))
				return -EINVAL;

		usb_ep_enable(acm->notify);
		acm->notify->driver_data = acm;

	} else if (intf == acm->data_id) {
		if (acm->port.in->driver_data) {
			DBG(cdev, "reset acm ttyGS%d\n", acm->port_num);
			gserial_disconnect(&acm->port);
		}
		if (!acm->port.in->desc || !acm->port.out->desc) {
			DBG(cdev, "activate acm ttyGS%d\n", acm->port_num);
			if (config_ep_by_speed(cdev->gadget, f,
					       acm->port.in) ||
			    config_ep_by_speed(cdev->gadget, f,
					       acm->port.out)) {
				acm->port.in->desc = NULL;
				acm->port.out->desc = NULL;
				return -EINVAL;
			}
		}
		gserial_connect(&acm->port, acm->port_num);

	} else
		return -EINVAL;

	return 0;
}

static void acm_disable(struct usb_function *f)
{
	struct f_acm	*acm = func_to_acm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	INFO(cdev, "acm ttyGS%d deactivated\n", acm->port_num);
	gserial_disconnect(&acm->port);
	usb_ep_disable(acm->notify);
	acm->notify->driver_data = NULL;
}


static int acm_cdc_notify(struct f_acm *acm, u8 type, u16 value,
		void *data, unsigned length)
{
	struct usb_ep			*ep = acm->notify;
	struct usb_request		*req;
	struct usb_cdc_notification	*notify;
	const unsigned			len = sizeof(*notify) + length;
	void				*buf;
	int				status;

	req = acm->notify_req;
	acm->notify_req = NULL;
	acm->pending = false;

	req->length = len;
	notify = req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = cpu_to_le16(value);
	notify->wIndex = cpu_to_le16(acm->ctrl_id);
	notify->wLength = cpu_to_le16(length);
	memcpy(buf, data, length);

	
	spin_unlock(&acm->lock);
	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	spin_lock(&acm->lock);

	if (status < 0) {
		ERROR(acm->port.func.config->cdev,
				"acm ttyGS%d can't notify serial state, %d\n",
				acm->port_num, status);
		acm->notify_req = req;
	}

	return status;
}

static int acm_notify_serial_state(struct f_acm *acm)
{
	struct usb_composite_dev *cdev = acm->port.func.config->cdev;
	int			status;

	spin_lock(&acm->lock);
	if (acm->notify_req) {
		DBG(cdev, "acm ttyGS%d serial state %04x\n",
				acm->port_num, acm->serial_state);
		status = acm_cdc_notify(acm, USB_CDC_NOTIFY_SERIAL_STATE,
				0, &acm->serial_state, sizeof(acm->serial_state));
	} else {
		acm->pending = true;
		status = 0;
	}
	spin_unlock(&acm->lock);
	return status;
}

static void acm_cdc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_acm		*acm = req->context;
	u8			doit = false;

	spin_lock(&acm->lock);
	if (req->status != -ESHUTDOWN)
		doit = acm->pending;
	acm->notify_req = req;
	spin_unlock(&acm->lock);

	if (doit)
		acm_notify_serial_state(acm);
}


static void acm_connect(struct gserial *port)
{
	struct f_acm		*acm = port_to_acm(port);

	acm->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
	acm_notify_serial_state(acm);
}

static void acm_disconnect(struct gserial *port)
{
	struct f_acm		*acm = port_to_acm(port);

	acm->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
	acm_notify_serial_state(acm);
}

static int acm_send_break(struct gserial *port, int duration)
{
	struct f_acm		*acm = port_to_acm(port);
	u16			state;

	state = acm->serial_state;
	state &= ~ACM_CTRL_BRK;
	if (duration)
		state |= ACM_CTRL_BRK;

	acm->serial_state = state;
	return acm_notify_serial_state(acm);
}


static int
acm_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_acm		*acm = func_to_acm(f);
	int			status;
	struct usb_ep		*ep;


	
	
	if (acm_string_defs[0].id == 0) {
		status = usb_string_ids_tab(c->cdev, acm_string_defs);
		if (status < 0)
		      return status;
		acm_control_interface_desc.iInterface =
		      acm_string_defs[ACM_CTRL_IDX].id;
		acm_data_interface_desc.iInterface =
		      acm_string_defs[ACM_DATA_IDX].id;
		acm_iad_descriptor.iFunction = acm_string_defs[ACM_IAD_IDX].id;
	}
	

	
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	acm->ctrl_id = status;
	acm_iad_descriptor.bFirstInterface = status;

	acm_control_interface_desc.bInterfaceNumber = status;
	acm_union_desc .bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	acm->data_id = status;

	acm_data_interface_desc.bInterfaceNumber = status;
	acm_union_desc.bSlaveInterface0 = status;
	acm_call_mgmt_descriptor.bDataInterface = status;

	status = -ENODEV;

	
	ep = usb_ep_autoconfig(cdev->gadget, &acm_fs_in_desc);
	if (!ep)
		goto fail;
	acm->port.in = ep;
	ep->driver_data = cdev;	

	ep = usb_ep_autoconfig(cdev->gadget, &acm_fs_out_desc);
	if (!ep)
		goto fail;
	acm->port.out = ep;
	ep->driver_data = cdev;	

	ep = usb_ep_autoconfig(cdev->gadget, &acm_fs_notify_desc);
	if (!ep)
		goto fail;
	acm->notify = ep;
	ep->driver_data = cdev;	

	
	acm->notify_req = gs_alloc_req(ep,
			sizeof(struct usb_cdc_notification) + 2,
			GFP_KERNEL);
	if (!acm->notify_req)
		goto fail;

	acm->notify_req->complete = acm_cdc_notify_complete;
	acm->notify_req->context = acm;

	acm_hs_in_desc.bEndpointAddress = acm_fs_in_desc.bEndpointAddress;
	acm_hs_out_desc.bEndpointAddress = acm_fs_out_desc.bEndpointAddress;
	acm_hs_notify_desc.bEndpointAddress =
		acm_fs_notify_desc.bEndpointAddress;

	acm_ss_in_desc.bEndpointAddress = acm_fs_in_desc.bEndpointAddress;
	acm_ss_out_desc.bEndpointAddress = acm_fs_out_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, acm_fs_function, acm_hs_function,
			acm_ss_function);
	if (status)
		goto fail;

	pr_debug("[XLOG_INFO][USB_ACM]%s: ttyGS%d: %s speed IN/%s OUT/%s NOTIFY/%s\n", \
			__func__, acm->port_num, \
			gadget_is_superspeed(c->cdev->gadget) ? "super" : \
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full", \
			acm->port.in->name, acm->port.out->name, acm->notify->name);

	DBG(cdev, "acm ttyGS%d: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			acm->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			acm->port.in->name, acm->port.out->name,
			acm->notify->name);
	return 0;

fail:
	if (acm->notify_req)
		gs_free_req(acm->notify, acm->notify_req);

	
	if (acm->notify)
		acm->notify->driver_data = NULL;
	if (acm->port.out)
		acm->port.out->driver_data = NULL;
	if (acm->port.in)
		acm->port.in->driver_data = NULL;

	ERROR(cdev, "%s/%p: can't bind, err %d\n", f->name, f, status);

	return status;
}

static void acm_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_acm		*acm = func_to_acm(f);

	acm_string_defs[0].id = 0;
	usb_free_all_descriptors(f);
	if (acm->notify_req)
		gs_free_req(acm->notify, acm->notify_req);
}

static void acm_free_func(struct usb_function *f)
{
	struct f_acm		*acm = func_to_acm(f);

	kfree(acm);
}

static struct usb_function *acm_alloc_func(struct usb_function_instance *fi)
{
	struct f_serial_opts *opts;
	struct f_acm *acm;

	acm = kzalloc(sizeof(*acm), GFP_KERNEL);
	if (!acm)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&acm->lock);

	acm->port.connect = acm_connect;
	acm->port.disconnect = acm_disconnect;
	acm->port.send_break = acm_send_break;

	acm->port.func.name = "acm";
	acm->port.func.strings = acm_strings;
	
	acm->port.func.bind = acm_bind;
	acm->port.func.set_alt = acm_set_alt;
	acm->port.func.setup = acm_setup;
	acm->port.func.disable = acm_disable;

	opts = container_of(fi, struct f_serial_opts, func_inst);
	acm->port_num = opts->port_num;
	acm->port.func.unbind = acm_unbind;
	acm->port.func.free_func = acm_free_func;

	return &acm->port.func;
}

static inline struct f_serial_opts *to_f_serial_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_serial_opts,
			func_inst.group);
}

CONFIGFS_ATTR_STRUCT(f_serial_opts);
static ssize_t f_acm_attr_show(struct config_item *item,
				 struct configfs_attribute *attr,
				 char *page)
{
	struct f_serial_opts *opts = to_f_serial_opts(item);
	struct f_serial_opts_attribute *f_serial_opts_attr =
		container_of(attr, struct f_serial_opts_attribute, attr);
	ssize_t ret = 0;

	if (f_serial_opts_attr->show)
		ret = f_serial_opts_attr->show(opts, page);
	return ret;
}

static void acm_attr_release(struct config_item *item)
{
	struct f_serial_opts *opts = to_f_serial_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations acm_item_ops = {
	.release                = acm_attr_release,
	.show_attribute		= f_acm_attr_show,
};

static ssize_t f_acm_port_num_show(struct f_serial_opts *opts, char *page)
{
	return sprintf(page, "%u\n", opts->port_num);
}

static struct f_serial_opts_attribute f_acm_port_num =
	__CONFIGFS_ATTR_RO(port_num, f_acm_port_num_show);


static struct configfs_attribute *acm_attrs[] = {
	&f_acm_port_num.attr,
	NULL,
};

static struct config_item_type acm_func_type = {
	.ct_item_ops    = &acm_item_ops,
	.ct_attrs	= acm_attrs,
	.ct_owner       = THIS_MODULE,
};

static void acm_free_instance(struct usb_function_instance *fi)
{
	struct f_serial_opts *opts;

	opts = container_of(fi, struct f_serial_opts, func_inst);
	gserial_free_line(opts->port_num);
	kfree(opts);
}

static struct usb_function_instance *acm_alloc_instance(void)
{
	struct f_serial_opts *opts;
	int ret;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	opts->func_inst.free_func_inst = acm_free_instance;
	ret = gserial_alloc_line(&opts->port_num);
	if (ret) {
		kfree(opts);
		return ERR_PTR(ret);
	}
	config_group_init_type_name(&opts->func_inst.group, "",
			&acm_func_type);
	return &opts->func_inst;
}
DECLARE_USB_FUNCTION_INIT(acm, acm_alloc_instance, acm_alloc_func);
MODULE_LICENSE("GPL");
