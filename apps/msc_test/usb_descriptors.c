/*
 * USB Descriptors file
 *
 * This file may be used by anyone for any purpose and may be used as a
 * starting point making your own application using M-Stack.
 *
 * It is worth noting that M-Stack itself is not under the same license as
 * this file.
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
 */

#include "usb_config.h"
#include "usb.h"
#include "usb_ch9.h"
#include "usb_msc.h"

#ifdef __C18
#define ROMPTR rom
#else
#define ROMPTR
#endif

/* VID/PID Descriptor Definitions
 *
 * Utilize user-defined VID/PID values if provided in usb_config.h
 * otherwise, defaults are provided by the stack.  The defaults are
 * only intended for testing purposes, and a unique VID/PID pair
 * should be provided for an end-product release, otherwise driver
 * issues may occur due to VID/PID conflicts.
 */
#ifdef MY_VID
#define DESC_VID MY_VID
#else
#define DESC_VID 0xA0A0
#warning "No user-defined VID, using the default instead.  MY_VID should be set in usb_config.h"
#endif

#ifdef MY_PID
#define DESC_PID MY_PID
#else
#define DESC_PID 0x0005
#warning "No user-defined PID, using the default instead.  MY_PID should be set in usb_config.h"
#endif

/* Configuration Packet
 *
 * This packet contains a configuration descriptor, one or more interface
 * descriptors, class descriptors(optional), and endpoint descriptors for a
 * single configuration of the device.  This struct is specific to the
 * device, so the application will need to add any interfaces, classes and
 * endpoints it intends to use.  It is sent to the host in response to a
 * GET_DESCRIPTOR[CONFIGURATION] request.
 *
 * While Most devices will only have one configuration, a device can have as
 * many configurations as it needs.  To have more than one, simply make as
 * many of these structs as are required, one for each configuration.
 *
 * An instance of each configuration packet must be put in the
 * usb_application_config_descs[] array below (which is #defined in
 * usb_config.h) so that the USB stack can find it.
 *
 * See Chapter 9 of the USB specification from usb.org for details.
 *
 * It's worth noting that adding endpoints here does not automatically
 * enable them in the USB stack.  To use an endpoint, it must be declared
 * here and also in usb_config.h.
 *
 * The configuration packet below is for the demo application.  Yours will
 * of course vary.
 */
struct configuration_1_packet {
	struct configuration_descriptor  config;
	struct interface_descriptor      interface;
	struct endpoint_descriptor       ep;
	struct endpoint_descriptor       ep1_out;
};


/* Device Descriptor
 *
 * Each device has a single device descriptor describing the device.  The
 * format is described in Chapter 9 of the USB specification from usb.org.
 * USB_DEVICE_DESCRIPTOR needs to be defined to the name of this object in
 * usb_config.h.  For more information, see USB_DEVICE_DESCRIPTOR in usb.h.
 */
const ROMPTR struct device_descriptor this_device_descriptor =
{
	sizeof(struct device_descriptor), // bLength
	DESC_DEVICE, // bDescriptorType
	0x0200, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
	0x00, // Device class
	0x00, // Device Subclass
	0x00, // Protocol.
	EP_0_LEN, // bMaxPacketSize0
	DESC_VID, // Vendor ID: Defined in usb_config.h
	DESC_PID, // Product ID: Defined in usb_config.h
	0x0001, // device release (1.0)
	1, // Manufacturer
	2, // Product
	3, // Serial
	NUMBER_OF_CONFIGURATIONS // NumConfigurations
};

/* Configuration Packet Instance
 *
 * This is an instance of the configuration_packet struct containing all the
 * data describing a single configuration of this device.  It is wise to use
 * as much C here as possible, such as sizeof() operators, and #defines from
 * usb_config.h.  When stuff is wrong here, it can be difficult to track
 * down exactly why, so it's good to get the compiler to do as much of it
 * for you as it can.
 */
static const ROMPTR struct configuration_1_packet configuration_1 =
{
	{
	// Members from struct configuration_descriptor
	sizeof(struct configuration_descriptor),
	DESC_CONFIGURATION,
	sizeof(configuration_1), //wTotalLength (length of the whole packet)
	1, // bNumInterfaces
	1, // bConfigurationValue
	2, // iConfiguration (index of string descriptor)
	0b10000000,
	100/2,   // 100/2 indicates 100mA
	},

	{
	// Members from struct interface_descriptor
	sizeof(struct interface_descriptor), // bLength;
	DESC_INTERFACE,
	APP_MSC_INTERFACE, // InterfaceNumber
	0x0, // AlternateSetting
	0x2, // bNumEndpoints (num besides endpoint 0)
	MSC_DEVICE_CLASS, // bInterfaceClass
	MSC_SCSI_TRANSPARENT_COMMAND_SET_SUBCLASS, // bInterfaceSubclass
	MSC_PROTOCOL_CODE_BBB, // bInterfaceProtocol
	0x04, // iInterface (index of string describing interface)
	},

	{
	// Members of the Endpoint Descriptor (EP1 IN)
	sizeof(struct endpoint_descriptor),
	DESC_ENDPOINT,
	APP_MSC_IN_ENDPOINT | 0x80, // endpoint #1 0x80=IN
	EP_BULK, // bmAttributes
	EP_1_IN_LEN, // wMaxPacketSize
	1,   // bInterval in ms.
	},

	{
	// Members of the Endpoint Descriptor (EP1 OUT)
	sizeof(struct endpoint_descriptor),
	DESC_ENDPOINT,
	APP_MSC_OUT_ENDPOINT /*| 0x00*/, // endpoint #1 0x00=OUT
	EP_BULK, // bmAttributes
	EP_1_OUT_LEN, // wMaxPacketSize
	1,   // bInterval in ms.
	},
};

/* String Descriptors
 *
 * String descriptors are optional. If strings are used, string #0 is
 * required, and must contain the language ID of the other strings.  See
 * Chapter 9 of the USB specification from usb.org for more info.
 *
 * Strings are UTF-16 Unicode, and are not NULL-terminated, hence the
 * unusual syntax.
 */

/* String index 0, only has one character in it, which is to be set to the
   language ID of the language which the other strings are in. */
static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t lang; } str00 = {
	sizeof(str00),
	DESC_STRING,
	0x0409 // US English
};

static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t chars[23]; } vendor_string = {
	sizeof(vendor_string),
	DESC_STRING,
	{'S','i','g','n','a','l',' ','1','1',' ','S','o','f','t','w','a','r','e',' ','L','L','C','.'}
};

static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t chars[21]; } product_string = {
	sizeof(product_string),
	DESC_STRING,
	{'U','S','B',' ','S','t','a','c','k',' ','T','e','s','t',' ','D','e','v','i','c','e'}
};

static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t chars[22]; } interface_string = {
	sizeof(interface_string),
	DESC_STRING,
	{'M','a','s','s',' ','S','t','o','r','a','g','e',' ',
	 'I','n','t','e','r','f','a','c','e'}
};

/* See comment at usb_application_get_string() below for information about
 * serial numbers and the Mass Storage Class. */
static const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; uint16_t chars[87]; } fake_serial_num = {
	sizeof(fake_serial_num),
	DESC_STRING,
	{'F','A','K','E','0','S','E','R','I','A','L','0',
	 'N','U','M','B','E','R','0','M','A','S','S','0',
	 'S','T','O','R','A','G','E','0','D','E','V','I','C','E','S','0',
	 'M','U','S','T','0','H','A','V','E','0','A','0','R','E','A','L','0',
	 'A','N','D','0','U','N','I','Q','U','E','0',
	 'S','E','R','I','A','L','0','P','E','R','0','T','H','E','0',
	 'S','P','E','C',}
};

/* Get String function
 *
 * This function is called by the USB stack to get a pointer to a string
 * descriptor.  If using strings, USB_STRING_DESCRIPTOR_FUNC must be defined
 * to the name of this function in usb_config.h.  See
 * USB_STRING_DESCRIPTOR_FUNC in usb.h for information about this function.
 * This is a function, and not simply a list or map, because it is useful,
 * and advisable, to have a serial number string which may be read from
 * EEPROM or somewhere that's not part of static program memory.
 */
int16_t usb_application_get_string(uint8_t string_number, const void **ptr)
{
	if (string_number == 0) {
		*ptr = &str00;
		return sizeof(str00);
	}
	else if (string_number == 1) {
		*ptr = &vendor_string;
		return sizeof(vendor_string);
	}
	else if (string_number == 2) {
		*ptr = &product_string;
		return sizeof(product_string);
	}
	else if (string_number == 3) {
		/* This is where you need to have code to do something like
		 * read a serial number out of EEPROM and return it. Mass
		 * Storage Devices _must_ have a real and unique serial
		 * number according to the specification. The serial must
		 * contain only 0-9 and A-F (capital letters), and the last
		 * 12 characters (at least), must be unique per VID/PID. See
		 * section 4.1.1 and 4.1.2 of the document "USB Mass Storage
		 * Class - Bulk Only Transport" from usb.org. */
		*ptr = &fake_serial_num;
		return sizeof(fake_serial_num);
	}
	else if (string_number == 4) {
		*ptr = &interface_string;
		return sizeof(interface_string);
	}

	return -1;
}

/* Configuration Descriptor List
 *
 * This is the list of pointters to the device's configuration descriptors.
 * The USB stack will read this array looking for descriptors which are
 * requsted from the host.  USB_CONFIG_DESCRIPTOR_MAP must be defined to the
 * name of this array in usb_config.h.  See USB_CONFIG_DESCRIPTOR_MAP in
 * usb.h for information about this array.  The order of the descriptors is
 * not important, as the USB stack reads bConfigurationValue for each
 * descriptor to know its index.  Make sure NUMBER_OF_CONFIGURATIONS in
 * usb_config.h matches the number of descriptors in this array.
 */
const struct configuration_descriptor *usb_application_config_descs[] =
{
	(struct configuration_descriptor*) &configuration_1,
};
STATIC_SIZE_CHECK_EQUAL(USB_ARRAYLEN(USB_CONFIG_DESCRIPTOR_MAP), NUMBER_OF_CONFIGURATIONS);
STATIC_SIZE_CHECK_EQUAL(sizeof(USB_DEVICE_DESCRIPTOR), 18);
