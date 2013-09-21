/*
 * USB HID Mouse Configuration
 *
 * This file may be used by anyone for any purpose and may be used as a
 * starting point making your own application using M-Stack.
 *
 * It is worth noting that M-Stack itself is not under the same license
 * as this file.
 *
 * M-Stack is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  For details, see sections 7, 8, and 9
 * of the Apache License, version 2.0 which apply to this file.  If you have
 * purchased a commercial license for this software from Signal 11 Software,
 * your commerical license superceeds the information in this header.
 *
 * Alan Ott
 * Signal 11 Software
 * 2013-04-08
 */

#ifndef USB_CONFIG_H__
#define USB_CONFIG_H__

/* Number of endpoint numbers besides endpoint zero. It's worth noting that
   and endpoint NUMBER does not completely describe an endpoint, but the
   along with the DIRECTION does (eg: EP 1 IN).  The #define below turns on
   BOTH IN and OUT endpoints for endpoint numbers (besides zero) up to the
   value specified.  For example, setting NUM_ENDPOINT_NUMBERS to 2 will
   activate endpoints EP 1 IN, EP 1 OUT, EP 2 IN, EP 2 OUT.  */
#define NUM_ENDPOINT_NUMBERS 1

/* Only 8, 16, 32 and 64 are supported for endpoint zero length. */
#define EP_0_LEN 8

#define EP_1_OUT_LEN 8
#define EP_1_IN_LEN 8

#define NUMBER_OF_CONFIGURATIONS 1

/* Ping-pong buffering mode. Valid values are:
	PPB_NONE         - Do not ping-pong any endpoints
	PPB_EPO_OUT_ONLY - Ping-pong only endpoint 0 OUT
	PPB_ALL          - Ping-pong all endpoints
	PPB_EPN_ONLY     - Ping-pong all endpoints except 0
*/
#ifdef __PIC32MX__
	/* PIC32MX only supports PPB_ALL */
	#define PPB_MODE PPB_ALL
#else
	#define PPB_MODE PPB_NONE
#endif

/* Comment the following line to use polling USB operation. When using polling,
   You are responsible for calling usb_service() periodically from your
   application. */
#define USB_USE_INTERRUPTS

/* Uncomment if you have a composite device which has multiple different types
 * of device classes. For example a device which has HID+CDC or
 * HID+VendorDefined, but not a device which has multiple of the same class
 * (such as HID+HID). Device class implementations have additional requirements
 * for multi-class devices. See the documentation for each device class for
 * details. */
//#define MULTI_CLASS_DEVICE


/* Objects from usb_descriptors.c */
#define USB_DEVICE_DESCRIPTOR this_device_descriptor
#define USB_CONFIG_DESCRIPTOR_MAP usb_application_config_descs
#define USB_STRING_DESCRIPTOR_FUNC usb_application_get_string

/* Optional callbacks from usb.c. Leave them commented if you don't want to
   use them. For the prototypes and documentation for each one, see usb.h. */

#define SET_CONFIGURATION_CALLBACK app_set_configuration_callback
#define GET_DEVICE_STATUS_CALLBACK app_get_device_status_callback
#define ENDPOINT_HALT_CALLBACK     app_endpoint_halt_callback
#define SET_INTERFACE_CALLBACK     app_set_interface_callback
#define GET_INTERFACE_CALLBACK     app_get_interface_callback
#define OUT_TRANSACTION_CALLBACK   app_out_transaction_callback
#define IN_TRANSACTION_COMPLETE_CALLBACK   app_in_transaction_complete_callback
#define UNKNOWN_SETUP_REQUEST_CALLBACK app_unknown_setup_request_callback
#define UNKNOWN_GET_DESCRIPTOR_CALLBACK app_unknown_get_descriptor_callback
#define START_OF_FRAME_CALLBACK    app_start_of_frame_callback
#define USB_RESET_CALLBACK         app_usb_reset_callback

/* HID Configuration functions. See usb_hid.h for documentation. */
#define USB_HID_DESCRIPTOR_FUNC usb_application_get_hid_descriptor
#define USB_HID_REPORT_DESCRIPTOR_FUNC usb_application_get_hid_report_descriptor
//#define USB_HID_PHYSICAL_DESCRIPTOR_FUNC usb_application_get_hid_physical_descriptor

/* HID Callbacks. See usb_hid.h for documentation. */
#define HID_GET_REPORT_CALLBACK app_get_report_callback
#define HID_SET_REPORT_CALLBACK app_set_report_callback
#define HID_GET_IDLE_CALLBACK app_get_idle_callback
#define HID_SET_IDLE_CALLBACK app_set_idle_callback
#define HID_GET_PROTOCOL_CALLBACK app_get_protocol_callback
#define HID_SET_PROTOCOL_CALLBACK app_set_protocol_callback

#endif /* USB_CONFIG_H__ */
