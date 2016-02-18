/*
 *  M-Stack USB Device Stack Implementation
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  Initial version for PIC18, 2008-02-24
 *  PIC24 port, 2013-08-13
 *
 *  M-Stack is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License as published by the
 *  Free Software Foundation, version 3; or the Apache License, version 2.0
 *  as published by the Apache Software Foundation.  If you have purchased a
 *  commercial license for this software from Signal 11 Software, your
 *  commerical license superceeds the information in this header.
 *
 *  M-Stack is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this software.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  You should have received a copy of the Apache License, verion 2.0 along
 *  with this software.  If not, see <http://www.apache.org/licenses/>.
 */

#include <usb_config.h>

#include <usb_ch9.h>
#include <usb.h>
#include <usb_hid.h>

#define MIN(x,y) (((x)<(y))?(x):(y))

STATIC_SIZE_CHECK_EQUAL(sizeof(struct hid_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct hid_optional_descriptor), 3);

#ifdef MULTI_CLASS_DEVICE
static uint8_t *hid_interfaces;
static uint8_t num_hid_interfaces;

void hid_set_interface_list(uint8_t *interfaces, uint8_t num_interfaces)
{
	hid_interfaces = interfaces;
	num_hid_interfaces = num_interfaces;
}
#endif

uint8_t process_hid_setup_request(const struct setup_packet *setup)
{
	/* The following comes from the HID spec 1.11, section 7.1.1 */

	uint8_t interface = setup->wIndex;

#ifdef MULTI_CLASS_DEVICE
	/* Check the interface first to make sure the destination is a
	 * HID interface. Composite devices will need to call
	 * hid_set_interface_list() first.
	 */
	uint8_t i;
	for (i = 0; i < num_hid_interfaces; i++) {
		if (interface == hid_interfaces[i])
			break;
	}

	/* Return if interface is not in the list of HID interfaces. */
	if (i == num_hid_interfaces)
		return -1;
#endif

	if (setup->bRequest == GET_DESCRIPTOR &&
	    setup->REQUEST.bmRequestType == 0x81) {
		uint8_t descriptor = ((setup->wValue >> 8) & 0x00ff);

		const void *desc;
		int16_t len = -1;

		if (descriptor == DESC_HID) {
			len = USB_HID_DESCRIPTOR_FUNC(interface, &desc);
		}
		else if (descriptor == DESC_REPORT) {
			len = USB_HID_REPORT_DESCRIPTOR_FUNC(interface, &desc);
		}
#ifdef USB_HID_PHYSICAL_DESCRIPTOR_FUNC
		else if (descriptor == DESC_PHYSICAL) {
			uint8_t descriptor_index = setup->wValue & 0x00ff;
			len = USB_HID_PHYSICAL_DESCRIPTOR_FUNC(interface, descriptor_index, &desc);
		}
#endif
		if (len < 0)
			return -1;

		usb_send_data_stage((void*) desc, min(len, setup->wLength), NULL, NULL);
		return 0;
	}

	/* No support for Set_Descriptor */

#ifdef HID_GET_REPORT_CALLBACK
	const void *desc;
	int16_t len = -1;
	usb_ep0_data_stage_callback callback;
	void *context;
	if (setup->bRequest == HID_GET_REPORT &&
	    setup->REQUEST.bmRequestType == 0xa1) {
		uint8_t report_type = (setup->wValue >> 8) & 0x00ff;
		uint8_t report_id = setup->wValue & 0x00ff;
		len = HID_GET_REPORT_CALLBACK(interface/*interface*/,
		                              report_type, report_id,
		                              &desc, &callback, &context);
		if (len < 0)
			return -1;

		usb_send_data_stage((void*)desc, min(len, setup->wLength), callback, context);
		return 0;
	}
#endif

#ifdef HID_SET_REPORT_CALLBACK
	if (setup->bRequest == HID_SET_REPORT &&
	    setup->REQUEST.bmRequestType == 0x21) {
		uint8_t report_type = (setup->wValue >> 8) & 0x00ff;
		uint8_t report_id = setup->wValue & 0x00ff;
		int8_t res = HID_SET_REPORT_CALLBACK(interface,
		                                     report_type, report_id);
		return res;
	}
#endif

#ifdef HID_GET_IDLE_CALLBACK
	if (setup->bRequest == HID_GET_IDLE &&
	    setup->REQUEST.bmRequestType == 0xa1) {
		uint8_t report_id = setup->wValue & 0x00ff;
		uint8_t res = HID_GET_IDLE_CALLBACK(interface, report_id);

		usb_send_data_stage((char*)&res, 1, NULL, NULL);
		return 0;
	}
#endif

#ifdef HID_SET_IDLE_CALLBACK
	if (setup->bRequest == HID_SET_IDLE &&
	    setup->REQUEST.bmRequestType == 0x21) {
		uint8_t duration = (setup->wValue >> 8) & 0x00ff;
		uint8_t report_id = setup->wValue & 0x00ff;
		uint8_t res = HID_SET_IDLE_CALLBACK(interface, report_id,
		                                    duration);

		return res;
	}
#endif

#ifdef HID_GET_PROTOCOL_CALLBACK
	if (setup->bRequest == HID_GET_PROTOCOL &&
	    setup->REQUEST.bmRequestType == 0xa1) {
		int8_t res = HID_GET_PROTOCOL_CALLBACK(interface);
		if (res < 0)
			return -1;

		usb_send_data_stage((char*)&res, 1, NULL, NULL);
		return 0;
	}
#endif

#ifdef HID_SET_PROTOCOL_CALLBACK
	if (setup->bRequest == HID_SET_PROTOCOL &&
	    setup->REQUEST.bmRequestType == 0x21) {
		int8_t res = HID_SET_PROTOCOL_CALLBACK(interface,
		                                       setup->wValue);
		return res;
	}
#endif

	return -1;
}
