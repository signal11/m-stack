
#include "usb_config.h"
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

struct configuration_packet {
	struct configuration_descriptor  config;
	struct interface_descriptor      interface;
	struct endpoint_descriptor       ep;
	struct endpoint_descriptor       ep1_out;
};


const ROMPTR struct device_descriptor this_device_descriptor =
{
	sizeof(struct device_descriptor), // bLength
	DEVICE, // bDescriptorType
	0x0200, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
	0x00, // Device class
	0x00, // Device Subclass
	0x00, // Protocol.
	EP_0_LEN, // bMaxPacketSize0
	0xA0A0, // Vendor
	0x0001, // Product
	//0x0c12, 0x0005, // Vendor,Product for zeroplus joystick
	0x0001, // device release (1.0)
	1, // Manufacturer
	2, // Product
	0, // Serial
	NUMBER_OF_CONFIGURATIONS // NumConfigurations
};

const ROMPTR struct configuration_packet this_configuration_packet =
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
const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; ushort lang; } str00 = {
	sizeof(str00),
	STRING,
	0x0409 // US English
};

const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; ushort chars[23]; } vendor_string = {
	sizeof(vendor_string),
	STRING,
	{'S','i','g','n','a','l',' ','1','1',' ','S','o','f','t','w','a','r','e',' ','L','L','C','.'}
};

const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; ushort chars[11]; } product_string = {
	sizeof(product_string),
	STRING,
	{'S','i','g','n','a','l','S','h','a','f','t'}
};

const ROMPTR struct {uint8_t bLength;uint8_t bDescriptorType; ushort chars[11]; } interface_string = {
	sizeof(interface_string),
	STRING,
	{'I','n','t','e','r','f','a','c','e',' ','1'}
};

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
		/* This is where you might have code to do something like read
		   a serial number out of EEPROM and return it. */
		return -1;
	}

	return -1;
}

/* Configuration Descriptor List: The order here is not important */
const struct configuration_descriptor *usb_application_config_descs[] =
{
	(struct configuration_descriptor*) &this_configuration_packet, /* Configuration #1 */
};
STATIC_SIZE_CHECK_EQUAL(USB_ARRAYLEN(USB_CONFIG_DESCRIPTOR_MAP), NUMBER_OF_CONFIGURATIONS);
STATIC_SIZE_CHECK_EQUAL(sizeof(USB_DEVICE_DESCRIPTOR), 18);
