
#include "usb.h"

#ifdef __C18
#define ROMPTR rom
#else
#define ROMPTR
#endif

/* Sent in response to a GET_DESCRIPTOR[REPORT] request, this packet
   contains a configuration, interface, class, and endpoint
   data. The packets contained in here will be specific to the
   device. */
#ifdef __XC16__
#pragma pack(push, 1)
#elif __XC8
#else
#error "Compiler not supported"
#endif
struct configuration_packet {
	struct configuration_descriptor  config;
	struct interface_descriptor      interface;
	struct endpoint_descriptor       ep;
	struct endpoint_descriptor       ep1_out;
};


ROMPTR struct device_descriptor this_device_descriptor =
{
	sizeof(struct device_descriptor), // bLength
	DEVICE, // bDescriptorType
	0x0200, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
	0x00, // Device class
	0x00, // Device Subclass
	0x00, // Protocol.
	0x40, // bMaxPacketSize0
	0xA0A0, // Vendor
	0x0001, // Product
	//0x0c12, 0x0005, // Vendor,Product for zeroplus joystick
	0x0001, // device release (1.0)
	1, // Manufacturer
	2, // Product
	0, // Serial
	1 // NumConfigurations
};

ROMPTR struct configuration_packet this_configuration_packet =
{
	{
	// Members from struct configuration_descriptor
	sizeof(struct configuration_descriptor),
	CONFIGURATION,
	sizeof(struct configuration_packet), //wTotalLength (length of the whole packet)
	1, // bNumInterfaces
	1, // bConfigurationValue
	2, // iConfiguration (index of string descriptor)
	0b10000000,
	100/2,   // 100/2 indicates 100mA
	},

	{
	// Members from struct interface_descriptor
	sizeof(struct interface_descriptor), // bLength;
	INTERFACE,
	0x0, // InterfaceNumber
	0x0, // AlternateSetting
	0x2, // bNumEndpoints (num besides endpoint 0)
	0xff, // bInterfaceClass 3=HID, 0xFF=VendorDefined
	0x00, // bInterfaceSubclass (0=NoBootInterface for HID)
	0x00, // bInterfaceProtocol
	0x01, // iInterface (index of string describing interface)
	},

	{
	// Members of the Endpoint Descriptor (EP1 IN)
	sizeof(struct endpoint_descriptor),
	ENDPOINT,
	0x01 | 0x80, // endpoint #1 0x80=IN
	EP_BULK, // bmAttributes
	64, // wMaxPacketSize
	1,   // bInterval in ms.
	},

	{
	// Members of the Endpoint Descriptor (EP1 OUT)
	sizeof(struct endpoint_descriptor),
	ENDPOINT,
	0x01 /*| 0x00*/, // endpoint #1 0x00=IN
	EP_BULK, // bmAttributes
	64, // wMaxPacketSize
	1,   // bInterval in ms.
	},
};

/* String index 0, only has one character in it, which is to be set to the
   language ID of the language which the other strings are in. */
ROMPTR struct {uchar bLength;uchar bDescriptorType; ushort lang; } str00 = {
	sizeof(str00),
	STRING,
	0x0409 // US English
};

ROMPTR struct {uchar bLength;uchar bDescriptorType; ushort chars[23]; } vendor_string = {
	sizeof(vendor_string),
	STRING,
	{'S','i','g','n','a','l',' ','1','1',' ','S','o','f','t','w','a','r','e',' ','L','L','C','.'}
};

ROMPTR struct {uchar bLength;uchar bDescriptorType; ushort chars[11]; } product_string = {
	sizeof(product_string),
	STRING,
	{'S','i','g','n','a','l','S','h','a','f','t'}
};

ROMPTR struct {uchar bLength;uchar bDescriptorType; ushort chars[11]; } interface_string = {
	sizeof(interface_string),
	STRING,
	{'I','n','t','e','r','f','a','c','e',' ','1'}
};

#ifdef __XC16__
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif
