/*
 * Sample USB Configuration
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

/* Uncomment these lines only if you have purchased a valid
 * VID/PID pair.  Do not attempt to use a VID/PID pair that
 * you do not own or that has not been sub-licensed to you.
 * A VID/PID pair should only be used for a single device,
 * and should not be re-used between multiple projects.  */
//#define MY_VID 0xA0A0
//#define MY_PID 0x0005

/* Number of endpoint numbers besides endpoint zero. It's worth noting that
   and endpoint NUMBER does not completely describe an endpoint, but the
   along with the DIRECTION does (eg: EP 1 IN).  The #define below turns on
   BOTH IN and OUT endpoints for endpoint numbers (besides zero) up to the
   value specified.  For example, setting NUM_ENDPOINT_NUMBERS to 2 will
   activate endpoints EP 1 IN, EP 1 OUT, EP 2 IN, EP 2 OUT.  */
#define NUM_ENDPOINT_NUMBERS 1

/* Only 8, 16, 32 and 64 are supported for endpoint zero length. */
#define EP_0_LEN 8

#define EP_1_OUT_LEN 64
#define EP_1_IN_LEN 64

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

/* Configuration from the MSC Class (usb_msc.h) */
#define MSC_MAX_LUNS_PER_INTERFACE 1
//#define MSC_SUPPORT_MULTIPLE_MSC_INTERFACES
#define MSC_WRITE_SUPPORT

/* Callbacks from the MSC class (usb_msc.h) */
#define MSC_GET_MAX_LUN_CALLBACK app_get_max_lun
#define MSC_BULK_ONLY_MASS_STORAGE_RESET_CALLBACK app_msc_reset
#define MSC_GET_STORAGE_INFORMATION app_get_storage_info
#define MSC_UNIT_READY app_get_unit_ready
#define MSC_START_STOP_UNIT app_start_stop_unit
#define MSC_START_READ app_msc_start_read
#define MSC_START_WRITE app_msc_start_write

/* Application callbacks, not used by the MSC class or USB stack, but used
 * for the application. */
#define APP_MSC_INTERFACE 0
#define APP_MSC_IN_ENDPOINT 1
#define APP_MSC_OUT_ENDPOINT 1

#endif /* USB_CONFIG_H__ */
