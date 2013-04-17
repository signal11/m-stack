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

#include "usb_config.h"
#include "usb_hal.h"

#ifdef __XC16__
#pragma pack(push, 1)
#elif __XC8
#else
#error "Compiler not supported"
#endif

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

/* Buffer Descriptor BDnSTAT flags. On Some MCUs, apparently, when handing
 * a buffer descriptor to the SIE, there's a race condition that can happen
 * if you don't set the BDnSTAT byte as a single operation. This was observed
 * on the PIC18F46J50 when sending 8-byte IN-transactions while doing control
 * transfers. */
#define BDNSTAT_UOWN   0x80
#define BDNSTAT_DTS    0x40
#define BDNSTAT_DTSEN  0x08
#define BDNSTAT_BSTALL 0x04

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
		struct {
			uint8_t BDnCNT_byte; //BDnCNT is a macro on PIC24
			uint8_t BDnSTAT;
		};
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

/* Required functions from application-provided usb_descriptors.c */
extern int16_t USB_STRING_DESCRIPTOR_FUNC(uint8_t string_number, const void **ptr);

/* Optional user-defined functions from usb_config.c */

#ifdef SET_CONFIGURATION_CALLBACK
/* SET_CONFIGURATION_CALLBACK() is called whenever a SET_CONFIGURATION request
 * is received from the host. The configuration parameter is the new
 * configuration the host requests. If configuration is zero, then the device
 * is to enter the ADDRESS state. If it is non-zero then the device is to enter
 * the CONFIGURED state.
 *
 * There's no way to fail. The host commands a configuration be set, and it
 * shall be done.
 */
void SET_CONFIGURATION_CALLBACK(uint8_t configuration);
#endif

#ifdef GET_DEVICE_STATUS_CALLBACK
/* GET_DEVICE_STATUS_CALLBACK() is called when a GET_STATUS request is
 * received from the host for the device (not the interface or the endpoint).
 * The callback is to return the status of the device as a 16-bit
 * unsigned integer per section 9.4.5 of the USB 2.0 specification.
 *   Bit 0 (LSB) - 0=bus_powered, 1=self_powered
 *   Bit 1       - 0=no_remote_wakeup, 1=remote_wakeup
 *   Bits 2-15   - reserved, set to zero.
 */
uint16_t GET_DEVICE_STATUS_CALLBACK();
#endif

#ifdef ENDPOINT_HALT_CALLBACK
/* ENDPOINT_HALT_CALLBACK() is called when a SET_FEATURE or CLEAR_FEATURE is
 * received from the host changing the endpoint halt value. This is a
 * notification only. There is no way to reject this request.
 * Parameters:
 *   endpoint - the endpoint identifier of the affected endpoint
 *              (direction and number, e.g.: 0x81 means EP 1 IN).
 *   halted   - 1=endpoint_halted (set), 0=endpoint_not_halted (clear)
 */
void ENDPOINT_HALT_CALLBACK(uint8_t endpoint, uint8_t halted);
#endif

#ifdef SET_INTERFACE_CALLBACK
/* SET_INTERFACE_CALLBACK() is called when a SET_INTERFACE request is
 * received from the host. SET_INTERFACE is used to set the alternate setting
 * for the specified interface. The parameters interface and alt_setting come
 * directly from the device request (from the host). The callback should
 * return 0 if the new alternate setting can be set or -1 if it cannot.
 * This callback is completely unnecessary if you only have one alternate
 * setting (alternate setting zero) for each interface.
 * Parameters:
 *   interface   - the interface on which to set the alternate setting
 *   alt_setting - the alternate setting
 * Return:
 *   Return 0 for success and -1 for error (will send a STALL to the host)
 */
int8_t SET_INTERFACE_CALLBACK(uint8_t interface, uint8_t alt_setting);
#endif

#ifdef GET_INTERFACE_CALLBACK
/* GET_INTERFACE_CALLBACK() is called when a GET_INTERFACE request is received
 * from the host. GET_INTERFACE is a request for the current alternate setting
 * selected for a given interface. The application should return the
 * interface's current alternate setting from this callback function.
 * If this callback is not present, zero will be returned as the current
 * alternate setting for all interfaces.
 * Parameters:
 *   interface - the interface queried for current altertate setting
 * Return:
 *   Return the current alternate setting for the interface requested or -1
 *   if the interface does not exist.
 */
int8_t GET_INTERFACE_CALLBACK(uint8_t interface);
#endif

#ifdef UNKNOWN_SETUP_REQUEST_CALLBACK
/* UNKNOWN_SETUP_REQUEST_CALLBACK() is called when a SETUP packet is
 * received with a request (bmRequestType,bRequest) which is unknown to the
 * the USB stack. This could be because it is a vendor-defined
 * request or because it is some other request which is not supported, for
 * example if you were implementing a device class in your application. There
 * are four ways to handle this:
 * 0. For unknown requests, return -1. This will send a STALL to the host.
 * 1. For requests which have no data stage, the callback should call
 *    usb_send_data_stage() with a length of zero to send a zero-length packet
 *    back to the host.
 * 2. For requests which expect an IN data stage, the callback should call
 *    usb_send_data_stage() with the data to be sent, and a callback which will
 *    get called when the data stage is complete. The callback is required, and
 *    the data buffer passed to usb_send_data_stage() must remain valid until
 *    the callback is called.
 * 3. For requests which will come with an OUT data stage, the callback
 *    should call usb_start_receive_ep0_data_stage() and provide a buffer,
 *    and a callback which will get called when the data stage has completed.
 *    The callback is required, and the data in the buffer passed to
 *    usb_start_receive_ep0_data_stage() is not valid until the callback is
 *    called.
 * Parameters:
 *   pkt - The SETUP packet
 * Return
 *   Return 0 if the SETUP can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
int8_t UNKNOWN_SETUP_REQUEST_CALLBACK(const struct setup_packet *pkt);
#endif

#ifdef UNKNOWN_GET_DESCRIPTOR_CALLBACK
/* UNKNOWN_GET_DESCRIPTOR_CALLBACK() is called when a GET_DESCRIPTOR
 * request is received from the host for a descriptor which is unrecognized
 * by the USB stack. This could be because it is a vendor-defined
 * descriptor or because it is some other descriptor which is not supported,
 * for example if you were implementing a device class in your application. The
 * callback function should set the descriptor pointer and return the number
 * of bytes in the descriptor. If the descriptor is not supported, the callback
 * should return -1, which will cause a STALL to be sent to the host.
 * Parameters:
 *   pkt - The SETUP packet with the request in it.
 *   descriptor - a pointer to a pointer which should be set to the descriptor
 *                data.
 * Return
 *   Return the length of the descriptor pointed to by *descriptor, or -1
 *   if the descriptor does not exist.
 */
int16_t UNKNOWN_GET_DESCRIPTOR_CALLBACK(const struct setup_packet *pkt, const void **descriptor);
#endif

//TODO Find a better place for this stuff
#define USB_ARRAYLEN(X) (sizeof(X)/sizeof(*X))
#define STATIC_SIZE_CHECK_EQUAL(X,Y) typedef char USB_CONCAT(STATIC_SIZE_CHECK_LINE_,__LINE__) [(X==Y)?1:-1]
#define USB_CONCAT(X,Y)  USB_CONCAT_HIDDEN(X,Y)
#define USB_CONCAT_HIDDEN(X,Y) X ## Y

STATIC_SIZE_CHECK_EQUAL(sizeof(struct endpoint_descriptor), 7);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct hid_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct interface_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct configuration_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct device_descriptor), 18);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct setup_packet), 8);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct buffer_descriptor), 4);

extern const struct device_descriptor USB_DEVICE_DESCRIPTOR;
extern const struct configuration_descriptor *USB_CONFIG_DESCRIPTOR_MAP[];

void usb_init(void);
void usb_isr(void);
void usb_service(void);

uchar *usb_get_in_buffer(uint8_t endpoint);
void usb_send_in_buffer(uint8_t endpoint, size_t len);
bool usb_in_endpoint_busy(uint8_t endpoint);
bool usb_in_endpoint_halted(uint8_t endpoint);

bool usb_out_endpoint_busy(uint8_t endpoint);
bool usb_out_endpoint_halted(uint8_t endpoint);
uchar *usb_get_out_buffer(uint8_t endpoint);

typedef void (*usb_ep0_data_stage_callback)(bool transfer_ok, void *context);

void usb_start_receive_ep0_data_stage(char *buffer, size_t len,
	usb_ep0_data_stage_callback callback, void *context);

void usb_send_data_stage(char *buffer, size_t len,
	usb_ep0_data_stage_callback callback, void *context);


#ifdef __XC16__
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif

#endif /* USB_H_ */

