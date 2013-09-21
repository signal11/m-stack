/*
 *  M-Stack USB Chapter 9 Structures
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2013-08-13
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

#ifndef USB_HID_H__
#define USB_HID_H__

/** @file usb_hid.h
 *  @brief USB HID Class Enumerations and Structures
 *  @defgroup public_api Public API
 */

/** @addtogroup public_api
 *  @{
 */

#include <stdint.h>
#include "usb_config.h"

#if defined(__XC16__) || defined(__XC32__)
#pragma pack(push, 1)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/** @defgroup hid_items USB HID Class Enumerations and Descriptors
 *  @brief Packet structs from the Device Class Definition for Human
 *  Interface Devices (commonly the USB HID Specification), version 1.11,
 *  chapter 6.
 *
 *  For more information about these structures, see the above referenced
 *  document, available from http://www.usb.org .
 *  @addtogroup hid_items
 *  @{
 */

#define HID_INTERFACE_CLASS 0x03

/** HID Class Descriptor Tyes */
enum HIDDescriptorTypes {
	DESC_HID = 0x21,
	DESC_REPORT = 0x22,
	DESC_PHYSICAL = 0x23,
};

/** HID Class Requests */
enum HIDRequests {
	HID_GET_REPORT = 0x1,
	HID_GET_IDLE = 0x2,
	HID_GET_PROTOCOL = 0x3,
	HID_SET_REPORT = 0x9,
	HID_SET_IDLE = 0xa,
	HID_SET_PROTOCOL = 0xb,
};

/** HID Report Types */
enum HIDReportTypes {
	HID_INPUT = 0x1,
	HID_OUTPUT = 0x2,
	HID_FEATURE = 0x3,
};

/** HID Protocols */
enum HIDProtocols {
	HID_PROTO_BOOT = 0,
	HID_PROTO_REPORT = 1,
};

struct hid_descriptor {
	uint8_t bLength; /**< Size of this struct plus any optional descriptors */
	uint8_t bDescriptorType; /**< Set to DESC_HID */
	uint16_t bcdHID; /**< HID Version in BCD. (0x0100 is 1.00) */
	uint8_t bCountryCode;
	uint8_t bNumDescriptors; /**< Number of class descriptors (always at least 1) */
	uint8_t bDescriptorType2; /**< Set to DESC_REPORT */
	uint16_t wDescriptorLength; /**< Set to size of report descriptor */
};

struct hid_optional_descriptor {
	uint8_t bDescriptorType;
	uint16_t wDescriptorLength;
};

/** HID Descriptor Function
 *
 * The USB Stack will call this function to retrieve the HID descriptor for
 * each HID interface when requested by the host. This function is mandatory
 * for HID devices.
 *
 * @param interface  The interface for which the descriptor is requested
 * @param ptr        A Pointer to a pointer which should be set to the
 *                   requested HID descriptor by this function.
 *
 * @returns
 *   Return the length of the HID descriptor in bytes or -1 if the descriptor
 *   does not exist.
 */
extern int16_t USB_HID_DESCRIPTOR_FUNC(uint8_t interface, const void **ptr);

/** HID Report Descriptor Function
 *
 * The USB Stack will call this function to retrieve the HID report
 * descriptor for each HID interface when requested by the host.  This
 * function is mandatory for HID devices.
 *
 * @param interface  The interface for which the descriptor is requested
 * @param ptr        A Pointer to a pointer which should be set to the
 *                   requested HID report descriptor by this function.
 *
 * @returns
 *   Return the length of the HID report descriptor in bytes or -1 if the
 *   descriptor does not exist.
 */
extern int16_t USB_HID_REPORT_DESCRIPTOR_FUNC(uint8_t interface, const void **ptr);

#ifdef USB_HID_PHYSICAL_DESCRIPTOR_FUNC
/** HID Physical Descriptor Function
 *
 * The USB Stack will call this function to retrieve the physical
 * descriptor for each HID interface when requested by the host.  This
 * function, and physical descriptors, are optional.
 *
 * @param interface  The interface for which the descriptor is requested
 * @param index      The physical descriptor set to return. Index zero
 *                   requests a special descriptor describing the number of
 *                   descriptor sets and their sizes. See the HID
 *                   specification, version 1.11, section 7.1.1.
 * @param ptr        A Pointer to a pointer which should be set to the
 *                   requested physical descriptor by this function
 *
 * @returns
 *   Return the length of the physical descriptor in bytes or -1 if the
 *   descriptor does not exist.
 */
extern int16_t USB_HID_PHYSICAL_DESCRIPTOR_FUNC(uint8_t interface, uint8_t index, const void **ptr);
#endif

#ifdef HID_GET_REPORT_CALLBACK
/** HID Get_Report request callback
 *
 * The USB Stack will call this function when a Get_Report request has been
 * received from the host.  This function should set the @p report pointer
 * to a buffer containing the report data and return the length of the
 * report.  Once the transfer has completed, @p callback will be called with
 * the @p context pointer provided.  The buffer pointed to by @p report
 * should be considered to be owned by the USB stack until the @p callback
 * is called and should not be modified by the application until that time.
 *
 *
 * @param interface      The interface for which the report is requested
 * @param report_type    The type of report, either @p HID_INPUT,
 *                       @p HID_OUTPUT, or @p HID_FEATURE.
 * @param report_id      The report index requested. This will be zero if the
 *                       device does not use numbered reports.
 * @param report         A pointer to a pointer which should be set to the
 *                       report data.
 * @param callback       A callback function to call when the transfer
 *                       completes.  This parameter is mandatory.  Once the
 *                       callback is called, the transfer is over, and the
 *                       buffer can be considered to be owned by the
 *                       application again.
 * @param context        A pointer to be passed to the callback.  The USB stack
 *                       does not dereference this pointer.
 *
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int16_t HID_GET_REPORT_CALLBACK(uint8_t interface, uint8_t report_type,
                                       uint8_t report_id, const void **report,
                                       usb_ep0_data_stage_callback *callback,
                                       void **context);
#endif


#ifdef HID_SET_REPORT_CALLBACK
/** HID Set_Report request callback
 *
 * The USB Stack will call this function when a Set_Report request has been
 * received from the host. There are two ways to handle this:
 *
 * 0. For unknown requests, return -1. This will send a STALL to the host. 
 * 1. For known requests the callback should call
 *    @p usb_start_receive_ep0_data_stage() with a buffer to be filled with
 *    the report data and a callback which will get called when the data
 *    stage is complete.  The callback is required, and the data buffer
 *    passed to @p usb_start_receive_ep0_data_stage() must remain valid
 *    until the callback is called.
 *
 * It is worth noting that only one control transfer can be active at any
 * given time.  Once HID_SET_REPORT_CALLBACK() has been called, it will not
 * be called again until the next transfer, meaning that if the
 * application-provided HID_SET_REPORT_CALLBACK() function performs option 1
 * above, the callback function passed to @p
 * usb_start_receive_ep0_data_stage() will be called before any other setup
 * transfer can happen again.  Thus, it is safe to use the same buffer
 * for all control transfers if desired.
 *
 *
 * @param interface      The interface for which the report is provided
 * @param report_type    The type of report, either @p HID_INPUT,
 *                       @p HID_OUTPUT, or @p HID_FEATURE.
 * @param report_id      The report index requested. This will be zero if the
 *                       device does not use numbered reports.
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int8_t HID_SET_REPORT_CALLBACK(uint8_t interface, uint8_t report_type,
                                      uint8_t report_id);
#endif

#ifdef HID_GET_IDLE_CALLBACK
/** HID Get_Idle request callback
 *
 * The USB Stack will call this function when a Get_Idle request has been
 * received from the host. The application should return the current idle
 * rate.
 *
 * @param interface      The interface for which the report is provided
 * @param report_id      The report index requested.
 *
 * @returns
 *   Return the current idle rate.
 */
extern uint8_t HID_GET_IDLE_CALLBACK(uint8_t interface, uint8_t report_id);
#endif

#ifdef HID_SET_IDLE_CALLBACK
/** HID Set_Idle request callback
 *
 * The USB Stack will call this function when a Set_Idle request has been
 * received from the host. The application should use the provided value
 * as the idle rate.
 *
 * @param interface      The interface for which the report is provided
 * @param report_id      The report index requested. Zero means all reports.
 * @param idle_rate      The idle rate to set, in multiples of 4 milliseconds.
 *
 * @returns
 *   Return 0 on success and -1 on failure.
 */
extern int8_t HID_SET_IDLE_CALLBACK(uint8_t interface, uint8_t report_id,
                                    uint8_t idle_rate);
#endif

#ifdef HID_GET_PROTOCOL_CALLBACK
/** HID Get_Protocol request callback
 *
 * The USB Stack will call this function when a Get_Protocol request has
 * been received from the host.  The application should return the current
 * protocol.
 *
 * @param interface      The interface for which the report is provided
 *
 * @returns
 *   Return the current protocol (@p HID_PROTO_BOOT, @p HID_PROTO_REPORT) or
 *   -1 on failure.
 */
extern int8_t HID_GET_PROTOCOL_CALLBACK(uint8_t interface);
#endif

#ifdef HID_SET_PROTOCOL_CALLBACK
/** HID Set_Protocol request callback
 *
 * The USB Stack will call this function when a Set_Protocol request has
 * been received from the host, and will provide the protocol to set as
 * either @p HID_PROTO_BOOT, or @p HID_PROTO_REPORT.
 *
 * @param interface      The interface for which the report is provided
 * @param protocol       The protocol to set. Either @p HID_PROTO_BOOT, or
 *                       @p HID_PROTO_REPORT.
 *
 * @returns
 *   Return 0 on success or -1 on failure.
 */
extern int8_t HID_SET_PROTOCOL_CALLBACK(uint8_t interface, uint8_t protocol);
#endif

#ifdef MULTI_CLASS_DEVICE
/** Set the list of HID interfaces on this device
 *
 * Provide a list to the HID class implementation of the interfaces on this
 * device which should be treated as HID devices.  This is only necessary
 * for multi-class composite devices to make sure that requests are not
 * confused between interfaces.  It should be called before usb_init().
 *
 * @param interfaces      An array of interfaces which are HID class.
 * @param num_interfaces  The size of the @p interfaces array.
 */
void hid_set_interface_list(uint8_t *interfaces, uint8_t num_interfaces);
#endif

/** Process HID Setup Request
 *
 * Process a setup request which has been unhandled as if it is potentially
 * a HID setup request. This function will then call appropriate callbacks
 * into the appliction if the setup packet is one recognized by the HID
 * specification.
 *
 * @param setup          A setup packet to handle
 *
 * @returns
 *   Returns 0 if the setup packet could be processed or -1 if it could not.
 */
uint8_t process_hid_setup_request(const struct setup_packet *setup);


/* Doxygen end-of-group for hid_items */
/** @}*/


#if defined(__XC16__) || defined(__XC32__)
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* USB_HID_H__ */
