/***********************
 PIC 18F4550 - USB
 Alan Ott
 2-24-2008
***********************/


#include <p18f4550.h>

#include <delays.h>
#include <usart.h>
#include <string.h>
//#include <timers.h>

typedef unsigned char uchar;
typedef unsigned short ushort;

#include "usb.h"

#define MIN(x,y) (x<y)?x:y

#define DEBUG_SERIAL
#ifdef DEBUG_SERIAL
	#define SERIAL(x)       \
		do {                    \
			putrsUSART(x "\r\n");  \
		}                       \
		while(0)
#else
	#define SERIAL(x)
#endif

void itoh(uchar val, char out[3])
{
#define CASE(XX) case 0x##XX: *c = 'XX'; break;
#define SWITCH(x) 	switch (x) { \
		CASE(0) CASE(1) CASE(2) CASE(3) \
		CASE(4) CASE(5) CASE(6) CASE(7) \
		CASE(8) CASE(9) CASE(A) CASE(B) \
		CASE(C) CASE(D) CASE(E) CASE(F) \
		};
	
#if 0
	char *c = &out[0];
	SWITCH((val & 0xf0) >> 4);
	c = &out[1];
	SWITCH(val & 0x0f);
	out[2] = 0;
#endif

	static char map[] = { '0', '1', '2', '3', '4', '5','6','7','8','9','A','B','C','D','E','F' };
	out[0] = map[(val & 0xf0) >> 4];
	out[1] = map[val & 0x0f];
	out[2] = 0x0;
	

#undef SWITCH
#undef CASE
}

#ifdef DEBUG_SERIAL
	void SERIAL_VAL(uchar x)
	{
		char s[3];
		itoh(x, s);
		putrsUSART("\t");
		putsUSART(s);
		putrsUSART("\r\n");
	}
#else
	#define SERIAL_VAL(x)
#endif


/*
 Setup so that with an input of 12MHz crystal you get a
 4 MHz input to the phase-locked loop (PLL) generator (PLLDIV=3),
 and set the USB clock source to come from the PLL (USBDIV=2),
 and set the CPU to use the frequency from the crystal (FOSC=HS),
 not the PLL, and not modify it (CPUDIV=0).
 CPU speed = crystal speed. (combination of CPUDIV and FOSC).
*/
#pragma config FOSC = HS  /* High Speed Crystal */
#pragma config PLLDIV = 3 /* 12MHz oscillator input */
#pragma config USBDIV = 2  /* 2 = Use 48MHz PLL Divided by 2 */
#pragma config CPUDIV = OSC1_PLL2 /* Run the CPU at crystal speed */

/* Other configurations */
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config VREGEN = ON /* USB Voltage Regulator */
#pragma config BOR = OFF /* Brown Out Reset */
#pragma config PWRT = ON /* Power-up Timer */
#pragma config WDT = OFF /* Watchdog timer */
#pragma config MCLRE = OFF /* Master-clear Pin */
#pragma config PBADEN = OFF /* Port-B A/D on startup */
#pragma config DEBUG = ON /* RB6 and RB7 used for debug */
#pragma config XINST = OFF /* Extended instruction set */
#pragma config LVP = OFF /* Low voltage programming */


/* Sent in response to a GET_DESCRIPTOR[REPORT] request, this packet
   contains a configuration, interface, class, and endpoint
   data. The packets contained in here will be specific to the
   device. */
struct configuration_packet {
	struct configuration_descriptor  config;
	struct interface_descriptor      interface;
	struct hid_descriptor            hid;
	struct endpoint_descriptor       ep;
#ifdef SECOND_ENDPOINT
	struct endpoint_descriptor       ep1_out;
#endif
};


rom struct device_descriptor this_device_descriptor =
{
	sizeof(struct device_descriptor), // bLength
	DEVICE, // bDescriptorType
	0x0100, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
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
	3, // Serial
	1 // NumConfigurations
};

//#define STOCK_JS
//#define ALAN_JS
//#define ALAN_JS_LED
#define ALAN_RELAY
//#define MOUSE

#ifdef STOCK_JS
rom char hid_report_descriptor[31] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,                    // USAGE (Joystick)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x04,                    //   USAGE (Joystick)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x08,                    //   USAGE_MAXIMUM (Button 8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x10,                    //   REPORT_COUNT (16)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0xc0                           // END_COLLECTION
};
#endif

#ifdef ALAN_JS 
rom char hid_report_descriptor[49] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,                    // USAGE (Joystick)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x04,                    //   USAGE (Joystick)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x00,                    //   USAGE_MINIMUM (Button 0)
    0x29, 0x06,                    //   USAGE_MAXIMUM (Button 6)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x07,                    //   REPORT_COUNT (7)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0x05, 0x05,                    //   USAGE_PAGE (Gaming Controls)
    0x0b, 0xbb, 0x00, 0x02, 0x00,  //   USAGE (Simulation Controls:Throttle)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};
	#define MY_HID_RESPONSE_SIZE 2
#endif

#ifdef ALAN_JS_LED
	// Descriptor with output for LED.
rom char hid_report_descriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,                    // USAGE (Joystick)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x04,                    //   USAGE (Joystick)
#if 0
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x19, 0x01,                    //   USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //   USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //   REPORT_COUNT (3)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0x05, 0x05,                    //   USAGE_PAGE (Gaming Controls)
    0x0b, 0xbb, 0x00, 0x02, 0x00,  //   USAGE (Simulation Controls:Throttle)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
#endif
#if 1
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
	0x09, 0x2a,                    //   USAGE (On-Line)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x19, 0x2a,                    //   USAGE_MINIMUM (On-Line)
    0x29, 0x2a,                    //   USAGE_MAXIMUM (On-Line)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
	0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x07,                    //   REPORT_SIZE (7)
    0x91, 0x01,                    //   OUTPUT (Cnst,Ary,Abs)
#endif
    0xc0                           // END_COLLECTION
	#define MY_HID_RESPONSE_SIZE 2
};
#endif

#ifdef ALAN_RELAY
	// Descriptor with output for LED.
rom char hid_report_descriptor[] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,                    // USAGE (Vendor usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
0x85, 0x01,                    //   REPORT_ID (1)
    0x06, 0x00, 0xff,              //   USAGE_PAGE (Vendor Defined Page 1)
	0x09, 0x02,                    //   USAGE (Vendor Usage 2)
    0x25, 0xff,                    //   LOGICAL_MAXIMUM (255)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x19, 0x02,                    //   USAGE_MINIMUM (vendor 2)
    0x29, 0x09,                    //   USAGE_MAXIMUM (vendor 5)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
//    0x95, 0x01,                    //   REPORT_COUNT (1)
//    0x75, 0x07,                    //   REPORT_SIZE (4)
//    0x91, 0x01,                    //   OUTPUT (Cnst,Ary,Abs)
0x85, 0x02,                    //   REPORT_ID (2)
    0x19, 0x00,                    //   USAGE_MINIMUM (vendor 0)
    0x29, 0x10,                    //   USAGE_MAXIMUM (vendor 16)
    0x95, 0x10,                    //   REPORT_COUNT (16)
    0x75, 0x08,                    //   REPORT_SIZE (8)
	0xb1, 0x02,                    //   FEATURE (Data,Var,Abs)

    0xc0                           // END_COLLECTION
	#define MY_HID_RESPONSE_SIZE 2
};
#endif


#ifdef MOUSE
rom char hid_report_descriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};
#endif




rom struct hid_descriptor this_hid_descriptor = 
{
	// Members of the HID Descriptor
	sizeof(struct hid_descriptor), // bLength
	HID, // bDescriptorType 0x21=HID
	0x0110, // bcdHID, 0x0110=HID1.1
	33,   // bCountryCode 0x00=non-localized, .33=US
	0x01,    // bNumDescriptors
	0x22,    // bDescriptorType 22=Report
	sizeof(hid_report_descriptor),
};

rom struct configuration_packet this_configuration_packet =
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

	// Members from struct interface_descriptor
	sizeof(struct interface_descriptor), // bLength;
	INTERFACE,
	0x0, // InterfaceNumber
	0x0, // AlternateSetting
#ifdef SECOND_ENDPOINT
	0x2, // bNumEndpoints (num besides endpoint 0)
#else
	0x1, // bNumEndpoints (num besides endpoint 0)
#endif
	0x03, // bInterfaceClass 3=HID, 0xFF=VendorDefined
	0x00, // bInterfaceSubclass (0=NoBootInterface for HID)
	0x00, // bInterfaceProtocol
	0x01, // iInterface (index of string describing interface)

	// Members of the HID Descriptor
	sizeof(struct hid_descriptor), // bLength
	HID, // bDescriptorType 0x21=HID
	0x0110, // bcdHID, 0x0110=HID1.1
	33,   // bCountryCode 0x00=non-localized, .33=US
	0x01,    // bNumDescriptors
	0x22,    // bDescriptorType 22=Report
	sizeof(hid_report_descriptor),

	// Members of the Endpoint Descriptor (EP1 IN)
	sizeof(struct endpoint_descriptor),
	ENDPOINT,
	0x01 | 0x80, // endpoint #1 0x80=IN
	0x3, // interrupt
	8, // wMaxPacketSize
	200,   // bInterval in ms.
#ifdef SECOND_ENDPOINT
	// Members of the Endpoint Descriptor (EP1 OUT)
	sizeof(struct endpoint_descriptor),
	ENDPOINT,
	0x01 /*| 0x00*/, // endpoint #1 0x00=IN
	0x3, // interrupt
	8, // wMaxPacketSize
	16,   // bInterval in ms.
#endif
};

/* String index 0, only has one character in it, which is to be set to the
   language ID of the language which the other strings are in. */
rom struct {uchar bLength;uchar bDescriptorType; ushort lang; } str00 = {
	sizeof(str00),
	STRING,
	0x0409 // US English
};

rom struct {uchar bLength;uchar bDescriptorType; ushort chars[23]; } vendor_string = {
	sizeof(vendor_string),
	STRING,
	'S','i','g','n','a','l',' ','1','1',' ','S','o','f','t','w','a','r','e',' ','L','L','C','.'
};

rom struct {uchar bLength;uchar bDescriptorType; ushort chars[11]; } product_string = {
	sizeof(product_string),
	STRING,
	'S','i','g','n','a','l','S','h','a','f','t'
};

rom struct {uchar bLength;uchar bDescriptorType; ushort chars[11]; } interface_string = {
	sizeof(interface_string),
	STRING,
	'I','n','t','e','r','f','a','c','e',' ','1'
};

struct serial_struct{
	uchar bLength;
	uchar bDescriptorType;
	ushort chars[16];
};




/* The buffer descriptors. Per the PIC18F4550 Data sheet, when _not_ using
   ping-pong buffering, these must be laid out sequentially starting at
   address 0x0400 in the following order, ep0_out, ep0_in,ep1_out, ep1_in,
   etc. These must be initialized prior to use. */
#pragma udata buffer_descriptors=0x400
struct buffer_descriptor ep0_out_bd;
struct buffer_descriptor ep0_in_bd;
struct buffer_descriptor ep1_out_bd;
struct buffer_descriptor ep1_in_bd;

/* The actual buffers to and from which the data is transferred from the SIE
   (from the USB bus). These buffers must fully be located between addresses
   0x400 and 0x7FF per the datasheet.*/
#pragma udata usb_buffers=0x500
uchar ep0_out_buf[0x40]; // 500h
uchar ep0_in_buf [0x40]; // 540h
uchar ep1_out_buf[0x40]; // 580h
uchar ep1_in_buf [0x40]; // 5c0h


// Plain old global data
#pragma udata
char g_throttle_pos = 0x00;

void start_serial_number_write(void);
void write_eeprom(uchar addr, uchar val);
int can_write_eeprom = 1;



// Interrupt handlers. Per the C18 documentation. SDCC does a much
// better job with this....
void isr(void);
void isr_low(void);

#pragma code high_vector=0x0008
void interrupt_vec(void)
{
	_asm goto isr _endasm
}

#pragma code low_vector=0x0018
void interrupt_vec_low(void)
{
	_asm goto isr_low _endasm
}

#pragma code
#pragma interrupt isr
void isr (void)
{
	if (UIRbits.URSTIF) {
		// USB Reset
		UIRbits.URSTIF = 0;
	}
	if (PIR2bits.EEIF) {
		// EEPROM Write Complete.
		PIR2bits.EEIF = 0;
		EECON1bits.WREN = 0; // Disable Writes

		can_write_eeprom = 1;
	}
	if (PIR1bits.TXIF) {
		PIR1bits.TXIF = 0;
	}
}

#pragma interrupt isr_low
void isr_low(void)
{
	Nop();
}

// Buffer which holds the Serial number to be set.
uchar serial_number_buffer[16];
uchar serial_number_pos = sizeof(serial_number_buffer);

void start_serial_number_write()
{
	if (serial_number_pos < sizeof(serial_number_buffer)) {
		uchar c = serial_number_buffer[serial_number_pos];
		uchar pos = serial_number_pos;
		serial_number_pos++;

		write_eeprom(pos, c);
		
		// Skip to the end if it's a NULL.
		if (c == 0) {
			serial_number_pos = sizeof(serial_number_buffer);
		}
	}

}

/* write_eeprom() writes a value (val) to an address (addr) in the
    chip's EEPROM memory */
void write_eeprom(uchar addr, uchar val)
{
	can_write_eeprom = 0;

	EEADR = addr;
	EEDATA = val;
	EECON1bits.EEPGD = 0; // Point to DATA memory
	EECON1bits.CFGS = 0; // Access EEPROM
	EECON1bits.WREN = 1; // Enable Writes

	INTCONbits.GIE = 0; // Disable Interrupts
	EECON2 = 0x55;
	EECON2 = 0xAA;
	EECON1bits.WR = 1; // Begin Write
	INTCONbits.GIE = 1; // Re-Enable Interrupts

#if 0
	// Wait for EEPROM Write Done Interrupt.
	while (!PIR2bits.EEIF)
		;
	PIR2bits.EEIF = 0; // Clear the Write Done	
	EECON1bits.WREN = 0; // Disable Writes
#endif

	

	SERIAL("Wrote to EEPROM");
	SERIAL_VAL(addr);
	SERIAL_VAL(val);
}


// Global data pertaining to addressing the device.
uchar addr_pending = 0; // boolean
uchar addr = 0x0;
int got_addr = 0; // whether addr is set DEBUG
char g_configuration = 0;
char g_ep1_halt = 0;


/* InitUSB() is called at powerup time, and when the device gets
   the reset signal from the USB bus (D+ and D- both held low) indicated
   by interrput bit URSTIF. */
void initUSB(void)
{
	uchar i;

	// Initialize the USB
	UCFGbits.UPUEN = 1;  /* pull-up enable */
	UCFGbits.UTRDIS = 0; /* on-chip transceiver Disable */
	UCFGbits.FSEN = 1;   /* Full-speed enable */
	UCFGbits.PPB0 = 0;   /* Ping-pong buffer */
	UCFGbits.PPB1 = 0;   /* Ping-pong buffer */

	UIRbits.TRNIF = 0;   /* Clear 4 times to clear out USTAT FIFO */
	UIRbits.TRNIF = 0;
	UIRbits.TRNIF = 0;
	UIRbits.TRNIF = 0;
	
	UIR = 0;

//	UIEbits.TRNIE = 1;   /* USB Transfer Interrupt Enable */
//	UIEbits.URSTIE = 1;  /* USB Reset Interrupt Enable */
//	UIEbits.SOFIE = 1;   /* USB Start-Of-Frame Interrupt Enable */

	UEP0bits.EPHSHK = 1; /* Endpoint handshaking enable */
	UEP0bits.EPCONDIS = 0; /* 1=Disable control operations */
	UEP0bits.EPOUTEN = 1; /* Endpoint Out Transaction Enable */
	UEP0bits.EPINEN = 1; /* Endpoint In Transaction Enable */
	UEP0bits.EPSTALL = 0; /* Stall */

	UEP1bits.EPHSHK = 1; /* Endpoint handshaking enable */
	UEP1bits.EPCONDIS = 1; /* 1=Disable control operations */
#ifdef SECOND_ENDPOINT
	UEP1bits.EPOUTEN = 1; /* Endpoint Out Transaction Enable */
#else
	UEP1bits.EPOUTEN = 0; /* Endpoint Out Transaction Enable */
#endif
	UEP1bits.EPINEN = 1; /* Endpoint In Transaction Enable */
	UEP1bits.EPSTALL = 0; /* Stall */

	// Clear out UEPn for endpoints 2 through 15.
	for (i = 2; i < 16; i++) {
		near uchar *p = &UEP0;
		p[i] = 0;
	}

	// Reset the Address.
	UADDR = 0x0;
	addr_pending = 0;
	got_addr = 0;
	g_configuration = 0;
	g_ep1_halt = 0;
	

	// Setup endpoint 0 Output buffer descriptor.
	// Input and output are from the HOST perspective.
	ep0_out_bd.BDnADR = ep0_out_buf;
	ep0_out_bd.BDnCNT = sizeof(ep0_out_buf);
	ep0_out_bd.STAT.BDnSTAT = 0;
	ep0_out_bd.STAT.DTSEN = 0;
	ep0_out_bd.STAT.UOWN = 1;

	// Setup endpoint 0 Input buffer descriptor.
	// Input and output are from the HOST perspective.
	ep0_in_bd.BDnADR = ep0_in_buf;
	ep0_in_bd.BDnCNT = sizeof(ep0_in_buf);
	ep0_in_bd.STAT.BDnSTAT = 0;
	ep0_in_bd.STAT.DTSEN = 0;
	ep0_in_bd.STAT.DTS = 0;
	ep0_in_bd.STAT.UOWN = 0;

	// Setup endpoint 1 Output buffer descriptor.
	// Input and output are from the HOST perspective.
	ep1_out_bd.BDnADR = ep1_out_buf;
	ep1_out_bd.BDnCNT = sizeof(ep1_out_buf);
	ep1_out_bd.STAT.BDnSTAT = 0;
	ep1_out_bd.STAT.DTSEN = 0;
	ep1_out_bd.STAT.UOWN = 1;

	// Setup endpoint 1 Input buffer descriptor.
	// Input and output are from the HOST perspective.
	ep1_in_bd.BDnADR = ep1_in_buf;
	ep1_in_bd.BDnCNT = sizeof(ep1_in_buf);
	ep1_in_bd.STAT.BDnSTAT = 0;
	ep1_in_bd.STAT.DTSEN = 0;
	ep1_in_bd.STAT.DTS = 1;
	ep1_in_bd.STAT.UOWN = 0;


	UCONbits.USBEN = 1; /* enable USB module */

	//PIE2bits.USBIE = 1;     /* USB Interrupt enable */
	
	//UIRbits.URSTIF = 0; /* Clear USB Reset on Start */

}


void reset_bd0_out(void)
{
	// Clean up the Buffer Descriptors.
	// Set the length and hand it back to the SIE.
	// The Address stays the same.
	ep0_out_bd.STAT.BDnSTAT = 0;
	ep0_out_bd.BDnCNT = sizeof(ep0_out_buf);
	ep0_out_bd.STAT.UOWN = 1;
}

void stall_ep0(void)
{
	// Stall Endpoint 0. It's important that DTSEN and DTS are zero.
	ep0_in_bd.STAT.BDnSTAT = 0;
	ep0_in_bd.STAT.DTSEN = 0;
	ep0_in_bd.STAT.DTS = 0;
	ep0_in_bd.STAT.BSTALL = 1;
	ep0_in_bd.STAT.UOWN = 1;

}

void brake(void)
{
	Delay1TCY();
	Delay1TCY();
	Delay1TCY();
}

/* checkUSB() is called repeatedly to check for USB interrupts
   and service USB requests */
void checkUSB(void)
{
	if (UIRbits.URSTIF) {
		// A Reset was detected on the wire. Re-init the SIE.
		initUSB();
		UIRbits.URSTIF = 0;
		SERIAL("USB Reset");
	}
	
	if (UIRbits.STALLIF) {
		UIRbits.STALLIF = 0;
	}


	if (UIRbits.TRNIF) {

		struct ustat_bits ustat = *((struct ustat_bits*)&USTAT);
		UIRbits.TRNIF = 0;

		if (ustat.ENDP == 0 && ustat.DIR == 0/*OUT*/) {
			// Packet for us on Endpoint 0.
			if (ep0_out_bd.STAT.PID == PID_SETUP) {
				// SETUP packet.

				far struct setup_packet *setup = (struct setup_packet*) ep0_out_buf;

				// SETUP packet sets PKTDIS Per Doc. Not sure why.
				UCONbits.PKTDIS = 0;

				if (setup->bRequest == GET_DESCRIPTOR) {
					char descriptor = ((setup->wValue >> 8) & 0x00ff);
					uchar descriptor_index = setup->wValue & 0x00ff;
					if (descriptor == DEVICE) {

						reset_bd0_out();

						SERIAL("Get Descriptor for DEVICE");

						// Return Device Descriptor
						memcpypgm2ram(ep0_in_buf, (rom void*)&this_device_descriptor, sizeof(struct device_descriptor));
						ep0_in_bd.STAT.BDnSTAT = 0;
						ep0_in_bd.STAT.DTSEN = 1;
						ep0_in_bd.STAT.DTS = 1;
						ep0_in_bd.BDnCNT = MIN(setup->wLength, sizeof(struct device_descriptor));
						ep0_in_bd.STAT.UOWN = 1;
					}
					else if (descriptor == CONFIGURATION) {
						reset_bd0_out();
						
						// Return Configuration Descriptor. Make sure to only return
						// the number of bytes asked for by the host.
						memcpypgm2ram(ep0_in_buf, (rom void*)&this_configuration_packet, sizeof(struct configuration_packet));
						ep0_in_bd.STAT.BDnSTAT = 0;
						ep0_in_bd.STAT.DTSEN = 1;
						ep0_in_bd.STAT.DTS = 1;
						ep0_in_bd.BDnCNT = MIN(setup->wLength, sizeof(struct configuration_packet));
						ep0_in_bd.STAT.UOWN = 1;
					}
					else if (descriptor == STRING) {
						uchar stall=0;
						uchar len = 0;

						reset_bd0_out();

						if (descriptor_index == 0) {
							memcpypgm2ram(ep0_in_buf, (rom void*)&str00, sizeof(str00));
							len = sizeof(str00);
							putrsUSART("req s0\r\n");
						}
						else if (descriptor_index == 1) {
							memcpypgm2ram(ep0_in_buf, (rom void*)&vendor_string, sizeof(vendor_string));
							len = sizeof(vendor_string);
							putrsUSART("req s1\r\n");
						}
						else if (descriptor_index == 2) {
							memcpypgm2ram(ep0_in_buf, (rom void*)&product_string, sizeof(product_string));
							len = sizeof(product_string);
							putrsUSART("req s2\r\n");
						}
#if 0 ////////////////////////
						else if (descriptor_index == 3) {
							memcpypgm2ram(ep0_in_buf, (rom void*)&interface_string, sizeof(interface_string));
							len = sizeof(interface_string);
							putrsUSART("req s3\r\n");
						}
#else ////////////////////////
						else if (descriptor_index == 3) {
							struct serial_struct s;
							int i;
							s.bLength = sizeof(struct serial_struct);
							s.bDescriptorType = STRING;
							s.chars;
							for (i = 0; i < 16; i++) {
								// Read each part of the serial from EEPROM
								EEADR = i;
								EECON1bits.EEPGD = 0;
								EECON1bits.RD = 1;
								s.chars[i] = EEDATA;
								if (s.chars[i] == 0)
									break;
							};
							s.bLength = s.bLength - 2*((i==16)?0: (16-i));
							memcpy(ep0_in_buf, &s, s.bLength);
							len = s.bLength;
							putrsUSART("req s3\r\n");
						}
#endif ///////////////////////
						else {
							brake();
							// Unsupported string descriptor.
							// Stall the endpoint
							stall_ep0();

							SERIAL("Unsupported string descriptor requested");
							
							stall = 1;
						}
						if (!stall) {
							// Return Descriptor				
							ep0_in_bd.STAT.BDnSTAT = 0;
							ep0_in_bd.STAT.DTSEN = 1;
							ep0_in_bd.STAT.DTS = 1;
							ep0_in_bd.BDnCNT = sizeof(vendor_string);
							ep0_in_bd.STAT.UOWN = 1;
						}
					}
					else if (descriptor == HID) {
						reset_bd0_out();

						SERIAL("Request of HID descriptor.");

						// Return HID Report Descriptor
						memcpypgm2ram(ep0_in_buf, &(this_configuration_packet.hid), sizeof(struct hid_descriptor));
						ep0_in_bd.STAT.BDnSTAT = 0;
						ep0_in_bd.STAT.DTSEN = 1;
						ep0_in_bd.STAT.DTS = 1;
						ep0_in_bd.BDnCNT = MIN(setup->wLength, sizeof(struct hid_descriptor));
						ep0_in_bd.STAT.UOWN = 1;

					}
					else if (descriptor == REPORT) {

						reset_bd0_out();

						SERIAL("Request of HID report descriptor.");

						// Return HID Report Descriptor
						memcpypgm2ram(ep0_in_buf, &hid_report_descriptor, sizeof(hid_report_descriptor));
						ep0_in_bd.STAT.BDnSTAT = 0;
						ep0_in_bd.STAT.DTSEN = 1;
						ep0_in_bd.STAT.DTS = 1;
						ep0_in_bd.BDnCNT = MIN(setup->wLength, sizeof(hid_report_descriptor));
						ep0_in_bd.STAT.UOWN = 1;

						SERIAL_VAL(setup->wLength);
					}
					else {
						// Unknown Descriptor. Stall the endpoint.
						reset_bd0_out();
						stall_ep0();
						SERIAL("Unknown Descriptor");
						SERIAL_VAL(descriptor);

					}
				}
				else if (setup->bRequest == SET_ADDRESS) {

					reset_bd0_out();

					// Mark the ADDR as pending. The address gets set only
					// after the transaction is complete.
					addr_pending = 1;
					addr = setup->wValue;

					// Return a zero-length packet.
					ep0_in_bd.STAT.BDnSTAT = 0;
					ep0_in_bd.STAT.DTSEN = 1;
					ep0_in_bd.STAT.DTS = 1;
					ep0_in_bd.BDnCNT = 0;
					ep0_in_bd.STAT.UOWN = 1;
				}
				else if (setup->bRequest == SET_CONFIGURATION) {
					// Set the configuration. wValue is the configuration.
					// we only have 1, so do nothing, I guess.
					uchar req = setup->wValue & 0x00ff;

					reset_bd0_out();

					if (1) { //g_configuration == 0) {
						if (req == 0) {
							// Go to the Address state (unconfigured)
							g_configuration = 0;
							SERIAL("Unsetting configuration (cfg 0)");
						}
						else {
							// Set the configuration
							g_configuration = req;
							//ep1_in_bd.STAT.DTS = 1;
							SERIAL("Set configuration to");
							SERIAL_VAL(req);
						}
	
						// Return a zero-length packet.
						ep0_in_bd.STAT.BDnSTAT = 0;
						ep0_in_bd.STAT.DTSEN = 1;
						ep0_in_bd.STAT.DTS = 1;
						ep0_in_bd.BDnCNT = 0;
						ep0_in_bd.STAT.UOWN = 1;
					}
					else {
						SERIAL("Set Configuration, but configuration not zero. Stalling.");
						stall_ep0();
					}
				
				}
				else if (setup->bRequest == GET_CONFIGURATION) {
					// Return the current Configuration.
					uchar buf[10];
					reset_bd0_out();

					SERIAL("Get Configuration. Returning:");
					SERIAL_VAL(g_configuration);

					buf[0] = g_configuration;
					memcpy(ep0_in_buf, &buf, 1);
					ep0_in_bd.STAT.BDnSTAT = 0;
					ep0_in_bd.STAT.DTSEN = 1;
					ep0_in_bd.STAT.DTS = 1;
					ep0_in_bd.BDnCNT = 1;
					ep0_in_bd.STAT.UOWN = 1;
				
				}
				else if (setup->bRequest == GET_STATUS) {
					char buf[10];					
					reset_bd0_out();
					
					SERIAL("Get Status (dst, index):");
					SERIAL_VAL(setup->REQUEST.destination);
					SERIAL_VAL(setup->wIndex);

					if (setup->REQUEST.destination == 0 /*0=device*/) {
						// Status for the DEVICE requested
						// Return as a single byte in the return packet.
						buf[0] = 0;
						memcpy(ep0_in_buf, &buf, 1);
						ep0_in_bd.STAT.BDnSTAT = 0;
						ep0_in_bd.STAT.DTSEN = 1;
						ep0_in_bd.STAT.DTS = 1;
						ep0_in_bd.BDnCNT = 1;
						ep0_in_bd.STAT.UOWN = 1;
					}
					else if (setup->REQUEST.destination == 2 /*2=endpoint*/) {
						// Status of endpoint
						
						if (setup->wIndex == 0x81/*81=ep1_in*/) {
							buf[0] = g_ep1_halt;
							buf[1] = 0;//g_ep1_halt;
							memcpy(ep0_in_buf, &buf, 2);
							ep0_in_bd.STAT.BDnSTAT = 0;
							ep0_in_bd.STAT.DTSEN = 1;
							ep0_in_bd.STAT.DTS = 1;
							ep0_in_bd.BDnCNT = 2;
							ep0_in_bd.STAT.UOWN = 1;
						}
						else {
							// Endpoint doesn't exist. STALL.
							stall_ep0();
						}
					}
					else {
						stall_ep0();
						SERIAL("Stalling. Status Requested for destination:");
						SERIAL_VAL(setup->REQUEST.destination);
					}
				
				}
				else if (setup->bRequest == SET_INTERFACE) {
					// Set the interface. wValue is the interface.
					// we only have 1, so Stall.


					
					reset_bd0_out();

					//stall_ep0();
					//ep1_in_bd.STAT.DTS = 1;
#if 1
					// Return a zero-length packet.
					ep0_in_bd.STAT.BDnSTAT = 0;
					ep0_in_bd.STAT.DTSEN = 1;
					ep0_in_bd.STAT.DTS = 1;
					ep0_in_bd.BDnCNT = 0;
					ep0_in_bd.STAT.UOWN = 1;
#endif
				}
				else if (/*setup->REQUEST.type == 0/*0=usb_std_req &&*/
				         setup->bRequest == GET_INTERFACE) {
					char buf[10];

					reset_bd0_out();

					SERIAL("Get Interface");
					SERIAL_VAL(setup->bRequest);
					SERIAL_VAL(setup->REQUEST.destination);
					SERIAL_VAL(setup->REQUEST.type);
					SERIAL_VAL(setup->REQUEST.direction);
					

					// Return the current interface (hard-coded to 1)
					// as a single byte in the return packet.
					buf[0] = 1;
					memcpy(ep0_in_buf, &buf, 1);
					ep0_in_bd.STAT.BDnSTAT = 0;
					ep0_in_bd.STAT.DTSEN = 1;
					ep0_in_bd.STAT.DTS = 1;
					ep0_in_bd.BDnCNT = 1;
					ep0_in_bd.STAT.UOWN = 1;
				}
				else if (setup->bRequest == CLEAR_FEATURE || setup->bRequest == SET_FEATURE) {
					if (setup->REQUEST.destination == 0/*0=device*/) {
						brake();
						SERIAL("Set/Clear feature for device");
					}

					if (setup->REQUEST.destination == 2/*2=endpoint*/) {
						if (setup->wValue == 0/*0=ENDPOINT_HALT*/) {
							if (setup->wIndex == 0x81 /*Endpoint 1 IN*/) {
								if (setup->bRequest == SET_FEATURE) {
									// SET_FEATURE. Halt the Endpoint
									brake();
									SERIAL("Set Feature on Endpoint 1 in.");
									g_ep1_halt = 1;
									//ep1_in_bd.STAT.BDnSTAT = 0;
									//ep1_in_bd.STAT.BSTALL = 1;
									//ep1_in_bd.STAT.UOWN = 1;
								}
								else {
									// CLEAR_FEATURE. Enable (un-halt) Endpoint.
									SERIAL("Clear Feature on Endpoint 1 in");
									g_ep1_halt = 0;
									//ep1_in_bd.STAT.BDnSTAT = 0;
									////ep1_in_bd.STAT.DTS = 1;
									//ep1_in_bd.STAT.UOWN = 0;
								}
							}
							else if (setup->wIndex == 0x80 /*Endpoint 1 OUT*/) {
								if (setup->bRequest == SET_FEATURE) {
									// SET_FEATURE. Halt the Endpoint
									brake();
									SERIAL("Set Feature on endpoint 1 out.");
									ep1_out_bd.STAT.BDnSTAT = 0;
									ep1_out_bd.STAT.BSTALL = 1;
									ep1_out_bd.STAT.UOWN = 1;
								}
								else {
									// CLEAR_FEATURE. Enable (un-halt) Endpoint.
									SERIAL("Clear feature on endpoint 1 out.");
									ep1_out_bd.STAT.BDnSTAT = 0;
									ep1_out_bd.STAT.DTSEN = 1;
									ep1_out_bd.STAT.DTS = 0;									
									ep1_out_bd.STAT.UOWN = 1;
								}

							}
						}
					}

					reset_bd0_out();

					// Return a zero-length packet.
					ep0_in_bd.STAT.BDnSTAT = 0;
					ep0_in_bd.STAT.DTSEN = 1;
					ep0_in_bd.STAT.DTS = 1;
					ep0_in_bd.BDnCNT = 0;
					ep0_in_bd.STAT.UOWN = 1;
					 
				}
				else if (setup->REQUEST.destination == 1/*1=interface*/ &&
				         setup->bRequest == GET_IDLE) {
					stall_ep0();
					SERIAL("Get Idle. Stalling.");
				}
				else if (setup->REQUEST.destination == 1/*1=interface*/ &&
				         setup->REQUEST.type == 1 /*1=class*/ &&
				         setup->bRequest == SET_IDLE) {
					uchar duration = ((setup->wValue >> 8) & 0x00ff);
					uchar report = setup->wValue & 0x00ff;

					SERIAL("Set Idle. (dur, rpt)");
					SERIAL_VAL(duration);
					SERIAL_VAL(report);

					//stall_ep0();
					// Return a zero-length packet.
					ep0_in_bd.STAT.BDnSTAT = 0;
					ep0_in_bd.STAT.DTSEN = 1;
					ep0_in_bd.STAT.DTS = 1;
					ep0_in_bd.BDnCNT = 0;
					ep0_in_bd.STAT.UOWN = 1;

				}
				else {
					// Unsupported Request. Stall the Endpoint.
					stall_ep0();
					SERIAL("unsupported request (req, dest, type, dir) ");
					SERIAL_VAL(setup->bRequest);
					SERIAL_VAL(setup->REQUEST.destination);
					SERIAL_VAL(setup->REQUEST.type);
					SERIAL_VAL(setup->REQUEST.direction);
				}
			}
			else if (ep0_out_bd.STAT.PID == PID_IN) {
				Delay1TCY();
			}
			else if (ep0_out_bd.STAT.PID == PID_OUT) {
				if (ep0_out_bd.BDnCNT == 0) {
					// Empty Packet. Handshake. End of Control Transfer.

					// Clean up the Buffer Descriptors.
					// Set the length and hand it back to the SIE.
					ep0_out_bd.STAT.BDnSTAT = 0;
					ep0_out_bd.STAT.DTS = 1;
					ep0_out_bd.STAT.DTSEN = 1;					
					ep0_out_bd.BDnCNT = sizeof(ep0_out_buf);
					ep0_out_bd.STAT.UOWN = 1;

				}
				else {
					int z;
					Delay1TCY();
					SERIAL("OUT Data packet on 0.");
					for (z = 0; z < ep0_out_bd.BDnCNT; z++)
						SERIAL_VAL(ep0_out_buf[z]);
					
					if (ep0_out_buf[0] == 1) {
						// Report 1 is data.
						if (ep0_out_buf[0])
							PORTAbits.RA1 = 0;
						else
							PORTAbits.RA1 = 1;
					}
					else if (ep0_out_buf[0] == 2) {
						// Report 2 is feature.
						// This report contains ASCII to write the Serial Number.
						int len;
						len = MIN(ep0_out_bd.BDnCNT, sizeof(serial_number_buffer));
						memcpy(serial_number_buffer, ep0_out_buf+1, len);
						serial_number_pos = 0;
						start_serial_number_write();
					}
					reset_bd0_out();
				}
			}
			else {
				// Unsupported PID. Stall the Endpoint.
				brake();
				SERIAL("Unsupported PID. Stall.");
				stall_ep0();
			}
		}
		else if (ustat.ENDP == 0 && ustat.DIR == 1/*1=IN*/) {
			if (addr_pending) {
				UADDR =  addr;
				addr_pending = 0;
				got_addr = 1;
			}
			//else
			//	brake();
		}
		else if (ustat.ENDP == 1) {
			if (ustat.DIR == 1 /*1=IN*/) {
				brake();
				SERIAL("IN Data packet request on 1.");
			}
			else {
				// OUT
				brake();
				SERIAL("OUT Data packet on 1.");
			}
		}
		else {
			// For another endpoint
			brake();
			SERIAL("Request for another endpoint?");
		}
	}
	
	// Check for Start-of-Frame interrupt.
	if (UIRbits.SOFIF) {
		UIRbits.SOFIF = 0;
	}

	// Check for USB Interrupt.
	if (PIR2bits.USBIF) {
		PIR2bits.USBIF = 0;
	}

#if 1
	// If the packet in the the in buffer of ep1 gets sent (UOWN gets
	// set back to zero by the SIE), put another packet in the buffer.
	if (g_configuration > 0 && !g_ep1_halt && ep1_in_bd.STAT.UOWN == 0) {
		uchar pid;

		// Nothing is in the transmit buffer. Send the Data.

		// For whatever Reason, make sure you clear KEN right here.
		// When this packet is done sending, the SIE will enable KEN.
		// IT MUST BE CLEARED!
		ep1_in_buf[0] = 0x01 << 1;
		ep1_in_buf[1] = g_throttle_pos;
		ep1_in_buf[2] = 0x00;
		ep1_in_buf[3] = 0x00;
		pid = !ep1_in_bd.STAT.DTS;
		ep1_in_bd.STAT.BDnSTAT = 0; // clear all bits (looking at you, KEN)
		ep1_in_bd.STAT.DTSEN = 1;
		ep1_in_bd.STAT.DTS = pid;
		ep1_in_bd.BDnCNT = MY_HID_RESPONSE_SIZE;
		ep1_in_bd.STAT.UOWN = 1;

		SERIAL("Sending EP 1. DTS:");
		SERIAL_VAL(pid);
	}
#endif
}

void initAnalog(void)
{
	// Initialize the A/D Converter
	ADCON0bits.CHS3 = 0; // 0000 = Use AN0 pin.
	ADCON0bits.CHS2 = 0; //
	ADCON0bits.CHS1 = 0; //
	ADCON0bits.CHS0 = 0; //
		
	ADCON1bits.VCFG0 = 0;  // 0 = use Vss for ref
	ADCON1bits.VCFG1 = 0;  // 0 = use Vdd for ref
	ADCON1bits.PCFG3 = 1;  //  1110 = AN0 is A/D, rest are GPIO
	ADCON1bits.PCFG2 = 1;  //  (cont, PCFG)
	ADCON1bits.PCFG1 = 1;  //  (cont, PCFG)
	ADCON1bits.PCFG0 = 0;  //  (cont, PCFG)
	
	ADCON2bits.ADFM = 0;  // 0 = Left justified result. (ie: 8 bits in ADRESH)

	ADCON2bits.ACQT2 = 0; 
	ADCON2bits.ACQT1 = 0; // Acquisition time select 000=0TAD
	ADCON2bits.ACQT0 = 0; 
	ADCON2bits.ADCS2 = 1;
	ADCON2bits.ADCS1 = 1; // 1,1,1 = A/D from internal A/D RC oscillator
	ADCON2bits.ADCS0 = 1;

	ADCON0bits.ADON = 1; // Turn on A/D Module.

	PIR1bits.ADIF = 0; // clear A/D interrupt bit.
}

void doAnalog(void)
{
	// Start the conversion.
	ADCON0bits.GO = 1;
	
	// Wait until the conversion is done
	while (ADCON0bits.NOT_DONE)
		;

	// Grab the high byte of the A/D and use it for the throttle.
	g_throttle_pos = ADRESH;

	PIR1bits.ADIF = 0; // clear A/D interrupt bit.
}

void main(void)
{


	/* Global Interrupt Enable */
	INTCONbits.GIE = 1;
	INTCONbits.PEIE = 1; /* peripheral interrupt enable */
	PIE2bits.EEIE = 1; /* EEPROM write interrupt enable */

	/* Setup Port A for GPIO Ouput */
	PORTA = 0;
	LATA = 0;
	//ADCON1 = 0xff;
	//ADCON0 = 0xff;
	//ADCON2 = 0xff;
	TRISA  = 0x01; // Input on AN0, Output on the rest.

#if 1
	OpenUSART(USART_ASYNCH_MODE &
	          USART_EIGHT_BIT &
	          USART_BRGH_HIGH &
	          USART_TX_INT_OFF &
	          USART_RX_INT_OFF, 25); /* 25 = 115200 baud at 12MHz crystal speed. */
	baudUSART(BAUD_16_BIT_RATE);

	// Turn off serial interrupts.
	PIE1bits.TXIE = 0;

	putrsUSART("Hello\r\n");
	putrsUSART("Hello\r\n");
#endif


	/* Timer ??? */
	//OpenTimer0( TIMER_INT_ON & T0_16BIT & T0_SOURCE_INT & T0_PS_1_1);
	//WriteTimer0(65536-50000); 
	INTCONbits.GIE = 1;


	// on SETUP handled, set UCONbits.PKTDIS = 1;

	initAnalog();
	initUSB();

	while (1) {

		doAnalog();
		checkUSB();
		if (can_write_eeprom)
			start_serial_number_write();

#if 0
		PORTAbits.RA1 = 0;
		Delay10KTCYx(100);
		PORTAbits.RA1 = 1;
		Delay10KTCYx(100);
#endif
	}


}
