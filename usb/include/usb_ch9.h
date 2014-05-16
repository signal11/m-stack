/*
 *  M-Stack USB Chapter 9 Structures
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2013-04-26
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

#ifndef USB_CH9_H__
#define USB_CH9_H__

/** @file usb.h
 *  @brief USB Chapter 9 Enumerations and Structures
 *  @defgroup public_api Public API
 */

/** @addtogroup public_api
 *  @{
 */

#include <stdint.h>

#if defined(__XC16__) || defined(__XC32__)
#pragma pack(push, 1)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/** @defgroup ch9_items USB Chapter 9 Enumerations and Descriptors
 *  @brief Packet structs from Chapter 9 of the USB spec which deals with
 *  device enumeration.
 *
 *  For more information about these structures, see Chapter 9 of the USB
 *  specification, available from http://www.usb.org .
 *  @addtogroup ch9_items
 *  @{
 */

/** USB PIDs */
enum PID {
	PID_OUT = 0x01,
	PID_IN  = 0x09,
	PID_SOF = 0x05,
	PID_SETUP = 0x0D,
	PID_DATA0 = 0x03,
	PID_DATA1 = 0x0B,
	PID_DATA2 = 0x07,
	PID_MDATA = 0x0F,
	PID_ACK   = 0x02,
	PID_NAK   = 0x0A,
	PID_STALL = 0x0E,
	PID_NYET  = 0x06,
	PID_PRE   = 0x0C,
	PID_ERR   = 0x0C,
	PID_SPLIT = 0x08,
	PID_PING  = 0x04,
	PID_RESERVED = 0x00,
};

/** Destination type
 *
 * This is present in the SETUP packet's bmRequestType field as Direction.
 */
enum DestinationType {
	DEST_DEVICE = 0,
	DEST_INTERFACE = 1,
	DEST_ENDPOINT = 2,
	DEST_OTHER_ELEMENT = 3,
};

/** Request type
 *
 * These are present in the SETUP packet's bmRequestType field as Type.
 */
enum RequestType {
	REQUEST_TYPE_STANDARD = 0,
	REQUEST_TYPE_CLASS    = 1,
	REQUEST_TYPE_VENDOR   = 2,
	REQUEST_TYPE_RESERVED = 3,
};

/** Control Request
 *
 * These are requests sent in the SETUP packet's bRequest field.
 */
enum StandardControlRequest {
	GET_STATUS = 0x0,
	CLEAR_FEATURE = 0x1,
	SET_FEATURE = 0x3,
	SET_ADDRESS = 0x5,
	GET_DESCRIPTOR = 0x6,
	SET_DESCRIPTOR = 0x7,
	GET_CONFIGURATION = 0x8,
	SET_CONFIGURATION = 0x9,
	GET_INTERFACE = 0xA,
	SET_INTERFACE = 0xB,
	SYNCH_FRAME = 0xC,
};

/** Standard Descriptor Types */
enum DescriptorTypes {
	DESC_DEVICE = 0x1,
	DESC_CONFIGURATION = 0x2,
	DESC_STRING = 0x3,
	DESC_INTERFACE = 0x4,
	DESC_ENDPOINT = 0x5,
	DESC_DEVICE_QUALIFIER = 0x6,
	DESC_OTHER_SPEED_CONFIGURATION = 0x7,
	DESC_INTERFACE_POWER = 0x8,
	DESC_OTG = 0x9,
	DESC_DEBUG = 0xA,
	DESC_INTERFACE_ASSOCIATION = 0xB,
};

/** Device Classes
 *
 * Some Device class constants which don't correspond to actual
 * classes.
 *
 * Device class codes which correspond to actual device classes are
 * defined in that device class's header file (for M-Stack supported
 * device classes).
 */
enum DeviceClassCodes {
	DEVICE_CLASS_DEFINED_AT_INTERFACE_LEVEL = 0x0,
	DEVICE_CLASS_MISC = 0xef,
	DEVICE_CLASS_APPLICATION_SPECIFIC = 0xfe,
	DEVICE_CLASS_VENDOR_SPECIFIC = 0xff,
};

/** Endpoint Attributes */
enum EndpointAttributes {
	EP_CONTROL = 0x0,
	EP_ISOCHRONOUS = 0x1,
	EP_BULK = 0x2,
	EP_INTERRUPT = 0x3,

	/* More bits here for ISO endpoints only. */
};

/** The SETUP packet, as defined by the USB specification.
 *
 * The contents of the packet sent from the host during the SETUP stage of
 * every control transfer
 */
struct setup_packet {
	union {
		struct {
			uint8_t destination : 5; /**< @see enum DestinationType */
			uint8_t type : 2;        /**< @see enum RequestType */
			uint8_t direction : 1;   /**< 0=out, 1=in */
		};
		uint8_t bmRequestType;
	} REQUEST;
	uint8_t bRequest;  /**< Dependent on @p type. @see enum StandardControlRequest */
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};

/** Device Descriptor */
struct device_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType; /**< set to DESC_DEVICE */
	uint16_t bcdUSB; /**< Set to 0x0200 for USB 2.0 */
	uint8_t bDeviceClass;
	uint8_t bDeviceSubclass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0; /**< Max packet size for ep 0. Must be 8, 16, 32, or 64. */
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer; /**< index of manufacturer string descriptor */
	uint8_t  iProduct;      /**< index of product string descriptor */
	uint8_t  iSerialNumber; /**< index of serial number string descriptor */
	uint8_t  bNumConfigurations;
};

/** Configuration Descriptor */
struct configuration_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType; /**< Set to DESC_CONFIGURATION */
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration; /**< index of string descriptor */
	uint8_t bmAttributes;
	uint8_t bMaxPower; /**< one-half the max power required by this device. */
};

/** Interface Descriptor */
struct interface_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType; /**< Set to DESC_INTERFACE */
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubclass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
};

/** Endpoint Descriptor */
struct endpoint_descriptor {
	// ...
	uint8_t bLength;
	uint8_t bDescriptorType; /**< Set to DESC_ENDPOINT */
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
};

/** String Descriptor */
struct string_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType; /**< Set to DESC_STRING */
	uint16_t chars[];
};

/** Interface Association Descriptor
 *
 * See the Interface Association Descriptors Engineering Change Note (ECN)
 * available from www.usb.org .
 */
struct interface_association_descriptor {
	uint8_t bLength;         /**< Set to 8 bytes */
	uint8_t bDescriptorType; /**< Set to DESC_INTERFACE_ASSOCIATION = 0xB */
	uint8_t bFirstInterface;
	uint8_t bInterfaceCount;
	uint8_t bFunctionClass;
	uint8_t bFunctionSubClass;
	uint8_t bFunctionProtocol;
	uint8_t iFunction; /**< String Descriptor Index */
};

/* Doxygen end-of-group for ch9_items */
/** @}*/


/** @cond INTERNAL */

/* So far this is the best place for these macros because they are used in
 * usb.c and also in the application's usb_descriptors.c.  Most applications
 * will not need to include this file outside their usb_descriptors.c, so
 * there isn't much namespace pollution.
 */
#define USB_ARRAYLEN(X) (sizeof(X)/sizeof(*X))
#define STATIC_SIZE_CHECK_EQUAL(X,Y) typedef char USB_CONCAT(STATIC_SIZE_CHECK_LINE_,__LINE__) [(X==Y)?1:-1]
#define USB_CONCAT(X,Y)  USB_CONCAT_HIDDEN(X,Y)
#define USB_CONCAT_HIDDEN(X,Y) X ## Y

/** @endcond */


#if defined(__XC16__) || defined(__XC32__)
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* USB_CH9_H__ */
