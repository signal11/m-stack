/*
 *  M-Stack USB Device Stack Implementation
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2014-05-12
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
#include <usb_cdc.h>

#define MIN(x,y) (((x)<(y))?(x):(y))

STATIC_SIZE_CHECK_EQUAL(sizeof(struct cdc_functional_descriptor_header), 5);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct cdc_acm_functional_descriptor), 4);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct cdc_union_functional_descriptor), 5);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct cdc_line_coding), 7);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct cdc_notification_header), 8);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct cdc_serial_state_notification), 10);


#ifdef MULTI_CLASS_DEVICE
static uint8_t *cdc_interfaces;
static uint8_t num_cdc_interfaces;

void cdc_set_interface_list(uint8_t *interfaces, uint8_t num_interfaces)
{
	cdc_interfaces = interfaces;
	num_cdc_interfaces = num_interfaces;
}

static bool interface_is_cdc(uint8_t interface)
{
	uint8_t i;
	for (i = 0; i < num_cdc_interfaces; i++) {
		if (interface == cdc_interfaces[i])
			break;
	}

	/* Return if interface is not in the list of CDC interfaces. */
	if (i == num_cdc_interfaces)
		return false;

	return true;
}
#endif

static uint8_t transfer_interface;
static union transfer_data {
	#if defined(CDC_SET_COMM_FEATURE_CALLBACK) || \
	    defined(CDC_CLEAR_COMM_FEATURE_CALLBACK) || \
	    defined(CDC_GET_COMM_FEATURE_CALLBACK)
	uint16_t comm_feature;
	#endif

	#if defined(CDC_SET_LINE_CODING_CALLBACK) || defined(CDC_GET_LINE_CODING_CALLBACK)
	struct cdc_line_coding line_coding;
	#endif
} transfer_data;

#if defined(CDC_SET_COMM_FEATURE_CALLBACK) || defined(CDC_CLEAR_COMM_FEATURE_CALLBACK)
static uint8_t set_or_clear_request;
static int8_t set_or_clear_comm_feature_callback(bool transfer_ok, void *context)
{
	/* Only ABSTRACT_STATE is supported here. */

	if (!transfer_ok)
		return -1;

	bool idle_setting = (transfer_data.comm_feature & 1) != 0;
	bool data_multiplexed_state = (transfer_data.comm_feature & 2) != 0;

	if (set_or_clear_request == CDC_SET_COMM_FEATURE) {
		return CDC_SET_COMM_FEATURE_CALLBACK(transfer_interface,
		                                     idle_setting,
		                                     data_multiplexed_state);
	}
	else {
		/* request == CDC_CLEAR_COMM_FEATURE */
		return CDC_CLEAR_COMM_FEATURE_CALLBACK(transfer_interface,
		                                       idle_setting,
		                                       data_multiplexed_state);
	}
	return 0;
}
#endif

#if defined(CDC_SET_LINE_CODING_CALLBACK)
static int8_t set_line_coding(bool transfer_ok, void *context) {
	if (!transfer_ok)
		return -1;

	return CDC_SET_LINE_CODING_CALLBACK(transfer_interface,
	                                    &transfer_data.line_coding);
}
#endif


uint8_t process_cdc_setup_request(const struct setup_packet *setup)
{
	/* The following comes from the CDC spec 1.1, chapter 6. */

	uint8_t interface = setup->wIndex;

#ifdef MULTI_CLASS_DEVICE
	/* Check the interface first to make sure the destination is a
	 * CDC interface. Multi-class devices will need to call
	 * cdc_set_interface_list() first.
	 */
	if (!interface_is_cdc(interface))
		return -1;
#endif

#ifdef CDC_SEND_ENCAPSULATED_COMMAND_CALLBACK
	if (setup->bRequest == CDC_SEND_ENCAPSULATED_COMMAND &&
	    setup->REQUEST.bmRequestType == 0x21) {
		int8_t res;
		res = CDC_SEND_ENCAPSULATED_COMMAND_CALLBACK(interface,
                                                             setup->wLength);
		if (res < 0)
			return -1;
		return 0;
	}
#endif

#ifdef CDC_GET_ENCAPSULATED_RESPONSE_CALLBACK
	if (setup->bRequest == CDC_GET_ENCAPSULATED_RESPONSE &&
	    setup->REQUEST.bmRequestType == 0xa1) {
		const void *response;
		int16_t len;
		usb_ep0_data_stage_callback callback;
		void *context;

		len = CDC_GET_ENCAPSULATED_RESPONSE_CALLBACK(
		                                interface, setup->wLength,
		                                &response, &callback,
		                                &context);
		if (len < 0)
			return -1;

		usb_send_data_stage((void*)response,
		                    MIN(len, setup->wLength),
		                    callback, context);
		return 0;
	}
#endif

#ifdef CDC_SET_COMM_FEATURE_CALLBACK
	if (setup->bRequest == CDC_SET_COMM_FEATURE &&
	    setup->REQUEST.bmRequestType == 0x21) {

		/* Only ABSTRACT_STATE feature is supported. If you need
		 * something else here, get in contact with Signal 11. */
		if (setup->wValue != CDC_FEATURE_ABSTRACT_STATE)
			return -1;

		transfer_interface = interface;
		set_or_clear_request = setup->bRequest;
		usb_start_receive_ep0_data_stage((char*) &transfer_data.comm_feature,
		                                 sizeof(transfer_data.comm_feature),
		                                 set_or_clear_comm_feature_callback,
		                                 NULL);
		return 0;
	}
#endif

#ifdef CDC_CLEAR_COMM_FEATURE_CALLBACK
	if (setup->bRequest == CDC_CLEAR_COMM_FEATURE &&
	    setup->REQUEST.bmRequestType == 0x21) {

		/* Only ABSTRACT_STATE feature is supported. If you need
		 * something else here, get in contact with Signal 11. */
		if (setup->wValue != CDC_FEATURE_ABSTRACT_STATE)
			return -1;

		transfer_interface = interface;
		set_or_clear_request = setup->bRequest;
		usb_start_receive_ep0_data_stage((char*)&transfer_data.comm_feature,
		                                 sizeof(transfer_data.comm_feature),
		                                 set_or_clear_comm_feature_callback,
		                                 NULL);
		return 0;
	}
#endif

#ifdef CDC_GET_COMM_FEATURE_CALLBACK
	if (setup->bRequest == CDC_GET_COMM_FEATURE &&
	    setup->REQUEST.bmRequestType == 0xa1) {
		bool idle_setting;
		bool data_multiplexed_state;
		int8_t res;

		/* Only ABSTRACT_STATE feature is supported. If you need
		 * something else here, get in contact with Signal 11. */
		if (setup->wValue != CDC_FEATURE_ABSTRACT_STATE)
			return -1;

		res = CDC_GET_COMM_FEATURE_CALLBACK(
		                                interface,
		                                &idle_setting,
		                                &data_multiplexed_state);
		if (res < 0)
			return -1;

		transfer_data.comm_feature =
			(uint16_t) idle_setting |
				(uint16_t) data_multiplexed_state << 1;

		usb_send_data_stage((char*)&transfer_data.comm_feature,
		                    MIN(setup->wLength,
		                        sizeof(transfer_data.comm_feature)),
		                    NULL/*callback*/, NULL);
		return 0;
	}
#endif

#ifdef CDC_SET_LINE_CODING_CALLBACK
	if (setup->bRequest == CDC_SET_LINE_CODING &&
	    setup->REQUEST.bmRequestType == 0x21) {

		transfer_interface = interface;
		usb_start_receive_ep0_data_stage(
		                      (char*)&transfer_data.line_coding,
		                      MIN(setup->wLength,
		                          sizeof(transfer_data.line_coding)),
		                      set_line_coding, NULL);
		return 0;
	}
#endif

#ifdef CDC_GET_LINE_CODING_CALLBACK
	if (setup->bRequest == CDC_GET_LINE_CODING &&
	    setup->REQUEST.bmRequestType == 0xa1) {
		int8_t res;

		res = CDC_GET_LINE_CODING_CALLBACK(
		                                interface,
		                                &transfer_data.line_coding);
		if (res < 0)
			return -1;

		usb_send_data_stage((char*)&transfer_data.line_coding,
		                    MIN(setup->wLength,
		                        sizeof(transfer_data.line_coding)),
		                    /*callback*/NULL, NULL);
		return 0;
	}
#endif

#ifdef CDC_SET_CONTROL_LINE_STATE_CALLBACK
	if (setup->bRequest == CDC_SET_CONTROL_LINE_STATE &&
	    setup->REQUEST.bmRequestType == 0x21) {
		int8_t res;
		bool dtr = (setup->wValue & 0x1) != 0;
		bool rts = (setup->wValue & 0x2) != 0;

		res = CDC_SET_CONTROL_LINE_STATE_CALLBACK(interface, dtr, rts);
		if (res < 0)
			return -1;

		/* Return zero-length packet. No data stage. */
		usb_send_data_stage(NULL, 0, NULL, NULL);

		return 0;
	}
#endif

#ifdef CDC_SEND_BREAK_CALLBACK
	if (setup->bRequest == CDC_SEND_BREAK &&
	    setup->REQUEST.bmRequestType == 0x21) {
		int8_t res;

		res = CDC_SEND_BREAK_CALLBACK(interface,
		                              setup->wValue /*duration*/);
		if (res < 0)
			return -1;

		/* Return zero-length packet. No data stage. */
		usb_send_data_stage(NULL, 0, NULL, NULL);

		return 0;
	}
#endif

	return -1;
}
