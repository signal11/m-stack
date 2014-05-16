/*
 *  M-Stack USB CDC Device Class Structures
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2013-09-27
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

#ifndef USB_CDC_H__
#define USB_CDC_H__

/** @file usb_cdc.h
 *  @brief USB CDC Class Enumerations and Structures
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

/** @defgroup cdc_items USB CDC Class Enumerations and Descriptors
 *  @brief Packet structs, constants, and callback functions implementing
 *  the "Universal Serial Bus Class Definitions for Communication Devices"
 *  (commonly the USB CDC Specification), version 1.1.
 *
 *  For more information, see the above referenced document, available from
 *  http://www.usb.org .
 *  @addtogroup cdc_items
 *  @{
 */

/* CDC Specification 1.1 document sections are listed in the comments */
#define CDC_DEVICE_CLASS 0x02 /* 4.1 */
#define CDC_COMMUNICATION_INTERFACE_CLASS 0x02 /* 4.2 */
#define CDC_COMMUNICATION_INTERFACE_CLASS_ACM_SUBCLASS 0x02 /* 4.3 */
/* Many of the subclass codes (section 4.3) are omitted here. Get in
 * contact with Signal 11 if you need something specific. */

#define CDC_DATA_INTERFACE_CLASS 0x0a /* 4.5 */
#define CDC_DATA_INTERFACE_CLASS_PROTOCOL_NONE 0x0 /* 4.7 */
#define CDC_DATA_INTERFACE_CLASS_PROTOCOL_VENDOR 0xff /* 4.7 */
/* Many of the protocol codes (section 4.7) are omitted here. Get in
 * contact with Signal 11 if you need something specific. */

/** CDC Descriptor types: 5.2.3 */
enum CDCDescriptorTypes {
	DESC_CS_INTERFACE = 0x24,
	DESC_CS_ENDPOINT  = 0x25,
};

/* Descriptor subtypes: 5.2.3 */
enum CDCFunctionalDescriptorSubtypes {
	CDC_FUNCTIONAL_DESCRIPTOR_SUBTYPE_HEADER = 0x0,
	CDC_FUNCTIONAL_DESCRIPTOR_SUBTYPE_ACM = 0x2,
	CDC_FUNCTIONAL_DESCRIPTOR_SUBTYPE_UNION = 0x6,
};
/* Many of the descriptor subtypes (section 5.2.3, table 25) are omitted
 * here.  Get in contact with Signal 11 if you need something specific. */

/** Abstract Control Management (ACM) capabilities
 *
 * See section 5.2.3.3 of the CDC Specification, version 1.1.
 */
enum CDCACMCapabilities {
	CDC_ACM_CAPABILITY_COMM_FEATURES = 0x1,
	CDC_ACM_CAPABILITY_LINE_CODINGS = 0x2,
	CDC_ACM_CAPABILITY_SEND_BREAK = 0x4,
	CDC_ACM_CAPABILITY_NETWORK_CONNECTION = 0x8,
};

/** CDC ACM Class Requests
 *
 * These are the class requests needed for ACM (see section 6.2, table 45).
 * Others are omitted.  Get in contact with Signal 11 if you need something
 * specific.
 */
enum CDCRequests {
	CDC_SEND_ENCAPSULATED_COMMAND = 0x0,
	CDC_GET_ENCAPSULATED_RESPONSE = 0x1,
	CDC_SET_COMM_FEATURE = 0x2,
	CDC_GET_COMM_FEATURE = 0x3,
	CDC_CLEAR_COMM_FEATURE = 0x4,
	CDC_SET_LINE_CODING = 0x20,
	CDC_GET_LINE_CODING = 0x21,
	CDC_SET_CONTROL_LINE_STATE = 0x22,
	CDC_SEND_BREAK = 0x23,
};

/** CDC Communication Feature Selector Codes
 *
 * See section 6.2.4, Table 47 of the CDC Specification, version 1.1.
 */
enum CDCCommFeatureSelector {
	CDC_FEATURE_ABSTRACT_STATE = 0x1,
	CDC_FEATURE_COUNTRY_SETTING = 0x2,
};

/** CDC Character Format
 *
 * These values are used in the bCharFormat field of the GET_LINE_CODING and
 * SET_LINE_CODING requests.  See section 6.2.13 (table 50) of the CDC
 * Specification, version 1.1.
 */
enum CDCCharFormat {
	CDC_CHAR_FORMAT_1_STOP_BIT = 0,
	CDC_CHAR_FORMAT_1_POINT_5_STOP_BITS = 1,
	CDC_CHAR_FORMAT_2_STOP_BITS = 2,
};

/** CDC Parity Type
 *
 * These values are used in the bParityType field of the GET_LINE_CODING and
 * SET_LINE_CODING requests.  See section 6.2.13 (table 50) of the CDC
 * Specification, version 1.1.
 */
enum CDCParityType {
	CDC_PARITY_NONE  = 0,
	CDC_PARITY_ODD   = 1,
	CDC_PARITY_EVEN  = 2,
	CDC_PARITY_MARK  = 3,
	CDC_PARITY_SPACE = 4,
};

/** CDC Class-Specific Notification Codes
 *
 * See section 6.3 (table 68) of the CDC Specification, version 1.1.
 */
enum CDCNotifications {
	CDC_NETWORK_CONNECTION = 0x0,
	CDC_RESPONSE_AVAILABLE = 0x1,
	CDC_SERIAL_STATE = 0x20,
};
/* Many of the CDC Notifications are omitted here.  Get in contact with
 * Signal 11 if you need something specific.  */

/** CDC Functional Descriptor Header.
 *
 * See section 5.2.3.1 of the CDC Specification, version 1.1.
 */
struct cdc_functional_descriptor_header {
	uint8_t bFunctionLength; /**< Size of this functional  descriptor (5) */
	uint8_t bDescriptorType; /**< Use DESC_CS_INTERFACE */
	uint8_t bDescriptorSubtype; /**< CDC_DESCRIPTOR_SUBTYPE_HEADER */
	uint16_t bcdCDC; /**< CDC version in BCD format. Use 0x0101 (1.1). */
};

/** CDC Abstract Control Management Functional Descriptor
 *
 * See Section 5.2.3.3 of the CDC Specification, version 1.1.
 */
struct cdc_acm_functional_descriptor {
	uint8_t bFunctionLength; /**< Size of this functional descriptor (4) */
	uint8_t bDescriptorType; /**< Use DESC_CS_INTERFACE */
	uint8_t bDescriptorSubtype; /**< CDC_DESCRIPTOR_SUBTYPE_ACM */
	uint8_t bmCapabilities; /**< See CDC_ACM_CAPABILITY* definitions */
};

/** CDC Union Functional Registor
 *
 * See Section 5.2.3.8 of the CDC Specification, version 1.1.
 */
struct cdc_union_functional_descriptor {
	uint8_t bFunctionLength; /**< Size of this functional descriptor */
	uint8_t bDescriptorType; /**< Use DESC_CS_INTERFACE */
	uint8_t bDescriptorSubtype; /**< CDC_DESCRIPTOR_SUBTYPE_ACM */
	uint8_t bMasterInterface;
	uint8_t bSlaveInterface0;
	/* More bSlaveInterfaces cound go here, but you'll have to pack them
	 * yourself into the configuration descriptor, and make sure
	 * bFunctionLength covers them all. */
};

/* CDC Notification Header
 *
 * CDC Notifications all share this same header. It's very similar to a
 * @p setup_packet .
 */
struct cdc_notification_header {
	union {
		struct {
			uint8_t destination : 5; /**< @see enum DestinationType */
			uint8_t type : 2;        /**< @see enum RequestType */
			uint8_t direction : 1;   /**< 0=out, 1=in */
		};
		uint8_t bmRequestType;
	} REQUEST;
	uint8_t bNotification;  /**< @see enum CDCNotifications */
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};


/* CDC Serial State Notification
 *
 * See Section 6.3.5 of the CDC Specification, version 1.1.
 */
struct cdc_serial_state_notification {
	struct cdc_notification_header header;
	union {
		struct {
			uint16_t bRxCarrier : 1; /**< Indicates DCD */
			uint16_t bTxCarrier : 1; /**< Indicates DSR */
			uint16_t bBreak : 1;
			uint16_t bRingSignal : 1;
			uint16_t bFraming : 1;
			uint16_t bParity : 1;
			uint16_t bOverrun : 1;
			uint16_t : 1;
			uint16_t : 8; /* XC8 can't handle a 9-bit bitfield */
		} bits;
		uint16_t serial_state;
	} data;
};

/* Many functional descriptors are omitted here. Get in contact with
 * Signal 11 if you need something specific. */

/* Message Structures */

/** CDC Line Coding Structure
 *
 * See Section 6.2.13 of the CDC Specification, version 1.1.
 */
struct cdc_line_coding {
	uint32_t dwDTERate; /**< Data Terminal Rate (bits per second) */
	uint8_t bCharFormat; /**< Stop bits: @see CDCCharFormat */
	uint8_t bParityType; /**< Parity Type: @see CDCParityType */
	uint8_t bDataBits; /**< Data Bits: 5, 6, 7, 8 or 16 */
};


/** Process CDC Setup Request
 *
 * Process a setup request which has been unhandled as if it is potentially
 * a CDC setup request. This function will then call appropriate callbacks
 * into the appliction if the setup packet is one recognized by the CDC
 * specification.
 *
 * @param setup          A setup packet to handle
 *
 * @returns
 *   Returns 0 if the setup packet could be processed or -1 if it could not.
 */
uint8_t process_cdc_setup_request(const struct setup_packet *setup);

/** CDC SEND_ENCAPSULATED_COMMAND callback
 *
 * The USB Stack will call this function when a GET_ENCAPSULATED_COMMAND
 * request has been received from the host.  There are two ways to handle
 * this:
 *
 * 1. If the request can't be handled, return -1. This will send a STALL
 *    to the host.
 * 2. If the request can be handled, call @p
 *    usb_start_receive_ep0_data_stage() with a buffer to be filled with the
 *    command data and a callback which will get called when the data stage
 *    is complete.  The callback is required, and the command data buffer
 *    passed to @p usb_start_receive_ep0_data_stage() must remain valid
 *    until the callback is called.
 *
 * It is worth noting that only one control transfer can be active at any
 * given time.  Once HID_SET_REPORT_CALLBACK() has been called, it will not
 * be called again until the next transfer, meaning that if the
 * application-provided HID_SET_REPORT_CALLBACK() function performs option 1
 * above, the callback function passed to @p
 * usb_start_receive_ep0_data_stage() will be called before any other setup
 * transfer can happen again.  Thus, it is safe to use the same buffer for
 * all control transfers if desired.
 *
 *
 * @param interface      The interface for which the command is intended
 * @param length         The length of the command which will be present
 *                       in the data stage.
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int8_t CDC_SEND_ENCAPSULATED_COMMAND_CALLBACK(uint8_t interface,
                                                     uint16_t length);

#ifdef CDC_GET_ENCAPSULATED_RESPONSE_CALLBACK
/** CDC GET_ENCAPSULATED_RESPONSE callback
 *
 * The USB Stack will call this function when a GET_ENCAPSULATED_RESPONSE
 * request has been received from the host.  This function should set the @p
 * response pointer to a buffer containing the response data and return the
 * length of the response.  Once the transfer has completed, @p callback will
 * be called with the @p context pointer provided.  The buffer pointed to by
 * @p response should be considered to be owned by the USB stack until the @p
 * callback is called and should not be modified by the application until
 * that time.
 *
 *
 * @param interface      The interface for which the report is requested
 * @param length         The length of the response requested by the host
 * @param response       A pointer to a pointer which should be set to the
 *                       response data.
 * @param callback       A callback function to call when the transfer
 *                       completes.  This parameter is mandatory.  Once the
 *                       callback is called, the transfer is over, and the
 *                       buffer can be considered to be owned by the
 *                       application again.
 * @param context        A pointer to be passed to the callback.  The USB
 *                       stack does not dereference this pointer.
 *
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int16_t CDC_GET_ENCAPSULATED_RESPONSE_CALLBACK(uint8_t interface,
                               uint16_t length, const void **response,
                               usb_ep0_data_stage_callback *callback,
                               void **context);
#endif

#ifdef CDC_SET_COMM_FEATURE_CALLBACK
/** CDC SET_COMM_FEATURE callback
 *
 * The USB Stack will call this function when a SET_COMM_FEATURE request has
 * been received from the host.  The device should set the idle setting
 * and/or data multiplexed state if requested.  There is no way to refuse
 * this message.  If this message is not supported by the device, simply do
 * not define CDC_SET_COMM_FEATURE_CALLBACK.
 *
 * @param interface      The interface for which the command is intended
 * @param idle_setting   Whether to set the idle setting. True = clear
 *                       the idle setting.
 * @param data_multiplexed_state  Whether to set the data multiplexed
 *                                state. True = clear the multiplexed state.
 */
extern void CDC_SET_COMM_FEATURE_CALLBACK(uint8_t interface,
                                            bool idle_setting,
                                            bool data_multiplexed_state);
#endif

#ifdef CDC_CLEAR_COMM_FEATURE_CALLBACK
/** CDC CLEAR_COMM_FEATURE callback
 *
 * The USB Stack will call this function when a CLEAR_COMM_FEATURE request
 * has been received from the host.  The device should clear the idle
 * setting and/or data multiplexed state if requested.  There is no way to
 * refuse this message.  If this message is not supported by the device,
 * simply do not define CDC_CLEAR_COMM_FEATURE_CALLBACK.
 *
 * @param interface      The interface for which the command is intended
 * @param idle_setting   Whether to clear the idle setting. True = clear
 *                       the idle setting.
 * @param data_multiplexed_state  Whether to clear the data multiplexed
 *                                state. True = clear the multiplexed state.
 */
extern void CDC_CLEAR_COMM_FEATURE_CALLBACK(uint8_t interface,
                                            bool idle_setting,
                                            bool data_multiplexed_state);
#endif

#ifdef CDC_GET_COMM_FEATURE_CALLBACK
/** CDC GET_COMM_FEATURE callback
 *
 * The USB Stack will call this function when a GET_COMM_FEATURE request has
 * been received from the host.  This function should set the @p
 * idle_setting and @p multiplexed_state pointers to their current values.
 *
 * @param interface      The interface for which the report is requested
 * @param idle_setting   Set to the current value of the idle setting.
 * @param multiplexed_state   Set to the current value of the multiplexed
 *                            state.
 *
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int8_t CDC_GET_COMM_FEATURE_CALLBACK(
                               uint8_t interface,
                               bool *idle_setting,
                               bool *data_multiplexed_state);
#endif

#ifdef CDC_SET_LINE_CODING_CALLBACK
/** CDC SET_LINE_CODING callback
 *
 * The USB Stack will call this function when a SET_LINE_CODING
 * request has been received from the host. The device should then set
 * the line coding to the specified values. There is no way to refuse
 * this message. If this message is not supported by the device, simply
 * do not define CDC_SET_LINE_CODING_CALLBACK.
 *
 * @param interface      The interface for which the command is intended
 * @param coding         The new line coding set by the host
 *
 */
extern void CDC_SET_LINE_CODING_CALLBACK(uint8_t interface,
                                         const struct cdc_line_coding *coding);
#endif

#ifdef CDC_GET_LINE_CODING_CALLBACK
/** CDC GET_LINE_CODING callback
 *
 * The USB Stack will call this function when a GET_LINE_CODING request has
 * been received from the host.  This function should set the @p values in
 * the structure pointed to by @p coding to the current line coding values.
 *
 * @param interface      The interface for which the report is requested
 * @param coding         Pointer to a structure which must be filled with
 *                       the current line coding values.
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
extern int8_t CDC_GET_LINE_CODING_CALLBACK(uint8_t interface,
                                           struct cdc_line_coding *coding);
#endif

#ifdef CDC_SET_CONTROL_LINE_STATE_CALLBACK
/** CDC SET_CONTROL_LINE_STATE callback
 *
 * The USB Stack will call this function when a SET_CONTROL_LINE_STATE
 * request has been received from the host.  The device should then set the
 * control lines as requested.
 *
 * @param interface      The interface for which the command is intended
 * @param dtr            The state of the DTR line. True = activated.
 * @param rts            The state of the RTS line. True = activated.
 *
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int8_t CDC_SET_CONTROL_LINE_STATE_CALLBACK(uint8_t interface,
                                                  bool dtr, bool dts);
#endif

#ifdef CDC_SEND_BREAK_CALLBACK
/** CDC SEND_BREAK callback
 *
 * The USB Stack will call this function when a SEND_BREAK request has been
 * received from the host.  The device should then assert a break condition
 * for the specified number of milliseconds.
 *
 * @param interface      The interface for which the command is intended
 * @param duration       The duration of the break in milliseconds
 *
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int8_t CDC_SEND_BREAK_CALLBACK(uint8_t interface, uint16_t duration);
#endif


/* Doxygen end-of-group for cdc_items */
/** @}*/


#if defined(__XC16__) || defined(__XC32__)
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* USB_CDC_H__ */
