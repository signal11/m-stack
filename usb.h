/*****************************
 USB Header File
 Alan Ott
 3-12-2008
*****************************/

#ifndef USB_H_
#define USB_H_

typedef unsigned char uchar;
typedef unsigned short ushort;
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "usb_hal.h"

// USB PIDs
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

// These are requests sent in the SETUP packet (bRequest field)
enum ControlRequest {
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

// Requests from the SETUP packet when wIndex is INTERFACE
// and the interface is of class HID.
enum HIDRequest {
	GET_REPORT = 0x1,
	GET_IDLE = 0x2,
	GET_PROTOCOL = 0x3,
	SET_REPORT = 0x9,
	SET_IDLE = 0xA,
	SET_PROTOCOL = 0xB,
};

enum DescriptorTypes {
	DEVICE = 0x1,
	CONFIGURATION = 0x2,
	STRING = 0x3,
	INTERFACE = 0x4,
	ENDPOINT = 0x5,
	DEVICE_QUALIFIER = 0x6,
	OTHER_SPEED_CONFIGURATION = 0x7,
	INTERFACE_POWER = 0x8,
	OTG = 0x9,
	DEBUG = 0xA,
	INTERFACE_ASSOCIATION = 0xB,

	HID = 0x21,
	REPORT = 0x22, // The HID REPORT descriptor
};

enum EndpointAttributes {
	EP_CONTROL = 0x0,
	EP_ISOCHRONOUS = 0x1,
	EP_BULK = 0x2,
	EP_INTERRUPT = 0x3,

	/* More bits here for ISO endpoints only. */
};

#ifdef _PIC18
// This represents the Buffer Descriptor as laid out in the
// PIC18F4550 Datasheet. It contains data about an endpoint
// buffer, either in or out. Buffer descriptors must be laid
// out at mem address 0x0400 on the PIC18F4550. Details on
// the fields can be found in the Buffer Descriptor section
// in the datasheet.
struct buffer_descriptor {
	union {
		struct {
			// For OUT (received) packets.
			uchar BC8 : 1;
			uchar BC9 : 1;
			uchar PID : 4; /* See enum PID */
			uchar reserved: 1;
			uchar UOWN : 1;
		};
		struct {
			// For IN (transmitted) packets.
			uchar /*BC8*/ : 1;
			uchar /*BC9*/ : 1;
			uchar BSTALL : 1;
			uchar DTSEN : 1;
			uchar INCDIS : 1;
			uchar KEN : 1;
			uchar DTS : 1;
			uchar /*UOWN*/ : 1;
		};
		uchar BDnSTAT;
	} STAT;
	uchar BDnCNT;
	BDNADR_TYPE BDnADR; // uchar BDnADRL; uchar BDnADRH;
};
#elif defined __XC16__
/* Represents BDnSTAT in the datasheet */
struct buffer_descriptor {
	union {
		struct {
			// For OUT (received) packets. (USB Mode)
			uint16_t BC : 10;
			uint16_t PID : 4; /* See enum PID */
			uint16_t DTS: 1;
			uint16_t UOWN : 1;
		};
		struct {
			// For IN (transmitted) packets. (CPU Mode)
			uint16_t /*BC*/ : 10;
			uint16_t BSTALL : 1;
			uint16_t DTSEN : 1;
			uint16_t reserved : 2;
			uint16_t DTS : 1;
			uint16_t /*UOWN*/ : 1;
		};
		uint16_t BDnSTAT;
	}STAT;
	BDNADR_TYPE BDnADR; // uchar BDnADRL; uchar BDnADRH;
};
#endif

// Layout of the USTAT. This is different from the one in the
// Microchip header file because the ENDP here is a 4-bit field
// as opposed to 4 individual bits as in the Microchip header.
struct ustat_bits {
	uchar : 1;
	uchar PPBI : 1;
	uchar DIR : 1;
	uchar ENDP : 4;
	uchar : 1;
};

/* USB Packets */
/* The SETUP packet, as defined by the USB spec. The setup packet
   is the contents of the packet sent on endpoint 0 with the
   PID_SETUP endpoint. */
struct setup_packet {
	union {
		struct {
			uchar destination : 5; /* 0=device, 1=interface, 2=endpoint, 3=other_element*/
			uchar type : 2; /* 0=usb_standard_req, 1=usb_class, 2=vendor_specific*/
			uchar direction : 1; /* 0=out, 1=in */
		};
		uchar bmRequestType;
	} REQUEST;
	uchar bRequest;  /* see enum ControlRequest */
	ushort wValue;
	ushort wIndex;
	ushort wLength;
};

struct device_descriptor {
	uchar bLength;
	uchar bDescriptorType; // DEVICE
	ushort bcdUSB; // 0x0200 USB 2.0
	uchar bDeviceClass;
	uchar bDeviceSubclass;
	uchar bDeviceProtocol;
	uchar bMaxPacketSize0; // Max packet size for ep 0
	ushort idVendor;
	ushort idProduct;
	ushort bcdDevice;
	uchar  iManufacturer; // index of string descriptor
	uchar  iProduct;      // index of string descriptor
	uchar  iSerialNumber; // index of string descriptor
	uchar  bNumConfigurations;
};

struct configuration_descriptor {
	uchar bLength;
	uchar bDescriptorType; // 0x02 CONFIGURATION
	ushort wTotalLength;
	uchar bNumInterfaces;
	uchar bConfigurationValue;
	uchar iConfiguration; // index of string descriptor
	uchar bmAttributes;
	uchar bMaxPower; // one-half the max power required.
};

struct interface_descriptor {
	uchar bLength;
	uchar bDescriptorType;
	uchar bInterfaceNumber;
	uchar bAlternateSetting;
	uchar bNumEndpoints;
	uchar bInterfaceClass;
	uchar bInterfaceSubclass;
	uchar bInterfaceProtocol;
	uchar iInterface;
};

struct hid_descriptor {
	uchar bLength;
	uchar bDescriptorType;
	ushort bcdHID;
	uchar bCountryCode;
	uchar bNumDescriptors;
	uchar bDescriptorType2;
	ushort wDescriptorLength;
	//bDescriptorType
	//wDescriptorLength
};

struct endpoint_descriptor {
	// ...
	uchar bLength;
	uchar bDescriptorType; // ENDPOINT
	uchar bEndpointAddress;
	uchar bmAttributes;
	ushort wMaxPacketSize;
	uchar bInterval;
};



struct string_descriptor {
	uchar bLength;
	uchar bDescriptorType; // STRING;
	ushort chars[];
};


void usb_init(void);
void usb_isr(void);
void usb_service(void);

uchar *usb_get_in_buffer(uint8_t endpoint);
void usb_send_in_buffer(uint8_t endpoint, size_t len);
bool usb_in_endpoint_busy(uint8_t endpoint);

bool usb_out_endpoint_busy(uint8_t endpoint);
uchar *usb_get_out_buffer(uint8_t endpoint);

#endif /* USB_H_ */

