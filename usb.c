/********************************
Signal 11 PIC USB Stack

Initial Rev for PIC18 2008-02-24
PIC24 port, 2013-04-08

Alan Ott
Signal 11 Software
********************************/

#ifdef __XC16__
#include <libpic30.h>
#include <xc.h>
#elif __C18
#include <p18f4550.h>
#include <delays.h>
#include <usart.h>
#endif

#include <string.h>

#include "usb.h"
#include "usb_hal.h"
#include "usb_config.h"
#include "usb_descriptors.c" // TODO HACK!

#define MIN(x,y) (x<y)?x:y

struct serial_struct{
	uchar bLength;
	uchar bDescriptorType;
	ushort chars[16];
};

struct buffer_descriptor_pair {
	struct buffer_descriptor ep_out;
	struct buffer_descriptor ep_in;
};

#ifdef __C18
/* The buffer descriptors. Per the PIC18F4550 Data sheet, when _not_ using
   ping-pong buffering, these must be laid out sequentially starting at
   address 0x0400 in the following order, ep0_out, ep0_in,ep1_out, ep1_in,
   etc. These must be initialized prior to use. */
#pragma udata buffer_descriptors=0x400
#elif __XC16__
static struct buffer_descriptor_pair __attribute__((aligned(512)))
	bds[NUM_ENDPOINT_NUMBERS+1];
#else
#error compiler not supported
#endif

#ifdef __C18
/* The actual buffers to and from which the data is transferred from the SIE
   (from the USB bus). These buffers must fully be located between addresses
   0x400 and 0x7FF per the datasheet.*/
/* This addr is for the PIC18F4550 */
#pragma udata usb_buffers=0x500
#elif __XC16__
//TODO: Figure out how to handle the buffers.
uchar ep0_out_buf[0x40]; // 500h
uchar ep0_in_buf [0x40]; // 540h
uchar ep1_out_buf[0x40]; // 580h
uchar ep1_in_buf [0x40]; // 5c0h
#else
#error compiler not supported
#endif

// Global data pertaining to addressing the device.
static uchar addr_pending = 0; // boolean
static uchar addr = 0x0;
static int got_addr = 0; // whether addr is set DEBUG
static char g_configuration = 0;
static char g_ep1_halt = 0;


#define SERIAL(x)
#define SERIAL_VAL(x)

/* usb_init() is called at powerup time, and when the device gets
   the reset signal from the USB bus (D+ and D- both held low) indicated
   by interrput bit URSTIF. */
void usb_init(void)
{
	uchar i;

	/* Initialize the USB. 18.4 of PIC24FJ64GB004 datasheet */
	SFR_PING_PONG_MODE = 0;   /* 0 = disable ping-pong buffer */
	SFR_USB_INTERRUPT_EN = 0x0;
	SFR_USB_EXTENDED_INTERRUPT_EN = 0x0;
	
	SFR_USB_EN = 1; /* enable USB module */
	U1OTGCONbits.OTGEN = 1; // TODO Portable

	
#ifdef NEEDS_PULL
	SFR_PULL_EN = 1;  /* pull-up enable */
#endif
	SFR_ON_CHIP_XCVR_DIS = 0; /* on-chip transceiver Disable */
#ifdef HAS_LOW_SPEED
	SFR_FULL_SPEED_EN = 1;   /* Full-speed enable */
#endif

	SFR_TOKEN_COMPLETE = 0;   /* Clear 4 times to clear out USTAT FIFO */
	SFR_TOKEN_COMPLETE = 0;
	SFR_TOKEN_COMPLETE = 0;
	SFR_TOKEN_COMPLETE = 0;
	
	SFR_USB_INTERRUPT_FLAGS = 0xff; // TODO: Portable?
	U1EIR = 0xff;

//	UIEbits.TRNIE = 1;   /* USB Transfer Interrupt Enable */
//	UIEbits.URSTIE = 1;  /* USB Reset Interrupt Enable */
//	UIEbits.SOFIE = 1;   /* USB Start-Of-Frame Interrupt Enable */

	// TODO reg
	union WORD {
		struct {
			uint8_t lb;
			uint8_t hb;
		};
		uint16_t w;
		void *ptr;
	};
	union WORD w;
	w.ptr = bds;
	
	U1BDTP1 = w.hb;

	/* These are the UEP/U1EP endpoint management registers. */
	
	/* Clear them all out. This is important because a bootloader
	   could have set them to non-zero */
	memset((void*)&SFR_EP_MGMT(0), 0x0, sizeof(SFR_EP_MGMT(0)) * 16);
	
	/* Set up Endpoint zero */
	SFR_EP_MGMT(0).SFR_EP_MGMT_HANDSHAKE = 1; /* Endpoint handshaking enable */
	SFR_EP_MGMT(0).SFR_EP_MGMT_CON_DIS = 0; /* 1=Disable control operations */
	SFR_EP_MGMT(0).SFR_EP_MGMT_OUT_EN = 1; /* Endpoint Out Transaction Enable */
	SFR_EP_MGMT(0).SFR_EP_MGMT_IN_EN = 1; /* Endpoint In Transaction Enable */
	SFR_EP_MGMT(0).SFR_EP_MGMT_STALL = 0; /* Stall */

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		SFR_EP_MGMT_TYPE *ep = &SFR_EP_MGMT(1) + (i-1);
		ep->SFR_EP_MGMT_HANDSHAKE = 1; /* Endpoint handshaking enable */
		ep->SFR_EP_MGMT_CON_DIS = 1; /* 1=Disable control operations */
		ep->SFR_EP_MGMT_OUT_EN = 1; /* Endpoint Out Transaction Enable */
		ep->SFR_EP_MGMT_IN_EN = 1; /* Endpoint In Transaction Enable */
		ep->SFR_EP_MGMT_STALL = 0; /* Stall */
	}

	// Reset the Address.
	SFR_USB_ADDR = 0x0;
	addr_pending = 0;
	got_addr = 0;
	g_configuration = 0;
	g_ep1_halt = 0;
	

	memset(bds, 0x0, sizeof(bds));

	// Setup endpoint 0 Output buffer descriptor.
	// Input and output are from the HOST perspective.
	bds[0].ep_out.BDnADR = ep0_out_buf;
	bds[0].ep_out.BDnCNT = sizeof(ep0_out_buf);
	bds[0].ep_out.STAT.DTSEN = 0;
	bds[0].ep_out.STAT.UOWN = 1;

	// Setup endpoint 0 Input buffer descriptor.
	// Input and output are from the HOST perspective.
	bds[0].ep_in.BDnADR = ep0_in_buf;
	bds[0].ep_in.BDnCNT = sizeof(ep0_in_buf);
	bds[0].ep_in.STAT.DTSEN = 0;
	bds[0].ep_in.STAT.DTS = 0;
	bds[0].ep_in.STAT.UOWN = 0;

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		// Setup endpoint 1 Output buffer descriptor.
		// Input and output are from the HOST perspective.
		bds[i].ep_out.BDnADR = ep1_out_buf;
		bds[i].ep_out.BDnCNT = sizeof(ep1_out_buf);
		bds[i].ep_out.STAT.DTSEN = 0;
		bds[i].ep_out.STAT.UOWN = 1;

		// Setup endpoint 1 Input buffer descriptor.
		// Input and output are from the HOST perspective.
		bds[i].ep_in.BDnADR = ep1_in_buf;
		bds[i].ep_in.BDnCNT = sizeof(ep1_in_buf);
		bds[i].ep_in.STAT.DTSEN = 0;
		bds[i].ep_in.STAT.DTS = 1;
		bds[i].ep_in.STAT.UOWN = 0;
	}
	
	#ifdef USB_NEEDS_POWER_ON
	SFR_USB_POWER = 1;
	#endif

	//TODO reg
	U1OTGCONbits.DPPULUP = 1;

	/* TODO: Interrupts */
	//PIE2bits.USBIE = 1;     /* USB Interrupt enable */
	
	//UIRbits.URSTIF = 0; /* Clear USB Reset on Start */
}

void reset_bd0_out(void)
{
	// Clean up the Buffer Descriptors.
	// Set the length and hand it back to the SIE.
	// The Address stays the same.
	bds[0].ep_out.STAT.BDnSTAT = 0;
	bds[0].ep_out.BDnCNT = sizeof(ep0_out_buf);
	bds[0].ep_out.STAT.UOWN = 1;
}

void stall_ep0(void)
{
	// Stall Endpoint 0. It's important that DTSEN and DTS are zero.
	bds[0].ep_in.STAT.BDnSTAT = 0;
	bds[0].ep_in.STAT.DTSEN = 0;
	bds[0].ep_in.STAT.DTS = 0;
	bds[0].ep_in.STAT.BSTALL = 1;
	bds[0].ep_in.STAT.UOWN = 1;
	bds[0].ep_in.BDnCNT = sizeof(ep0_in_buf);

}

void brake(void)
{
	Nop();
	Nop();
	Nop();
}

/* checkUSB() is called repeatedly to check for USB interrupts
   and service USB requests */
void usb_service(void)
{
	if (SFR_USB_RESET_IF) {
		// A Reset was detected on the wire. Re-init the SIE.
		usb_init();
		U1IR = 0x1;//SFR_USB_RESET_IF = 0; TODO PORTABLE
		SERIAL("USB Reset");
	}
	
	if (SFR_USB_STALL_IF) {
		U1IR = 0x80; // TODO PORTABLE
		//SFR_USB_STALL_IF = 0;
	}


	if (SFR_USB_TOKEN_IF) {

		//struct ustat_bits ustat = *((struct ustat_bits*)&USTAT);

		if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 0/*OUT*/) {
			// Packet for us on Endpoint 0.
			if (bds[0].ep_out.STAT.PID == PID_SETUP) {
				// SETUP packet.

				FAR struct setup_packet *setup = (struct setup_packet*) ep0_out_buf;

				// SETUP packet sets PKTDIS Per Doc. Not sure why.
				SFR_USB_PKT_DIS = 0;

				if (setup->bRequest == GET_DESCRIPTOR) {
					char descriptor = ((setup->wValue >> 8) & 0x00ff);
					uchar descriptor_index = setup->wValue & 0x00ff;
					if (descriptor == DEVICE) {

						reset_bd0_out();

						SERIAL("Get Descriptor for DEVICE");

						// Return Device Descriptor
						memcpy_from_rom(ep0_in_buf, &this_device_descriptor, sizeof(struct device_descriptor));
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = MIN(setup->wLength, sizeof(struct device_descriptor));
						bds[0].ep_in.STAT.UOWN = 1;
						static int cnt;
						cnt++;
					}
					else if (descriptor == CONFIGURATION) {
						reset_bd0_out();
						
						// Return Configuration Descriptor. Make sure to only return
						// the number of bytes asked for by the host.
						memcpy_from_rom(ep0_in_buf, &this_configuration_packet, sizeof(struct configuration_packet));
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = MIN(setup->wLength, sizeof(struct configuration_packet));
						bds[0].ep_in.STAT.UOWN = 1;
					}
					else if (descriptor == STRING) {
						uchar stall=0;
						uchar len = 0;

						reset_bd0_out();

						if (descriptor_index == 0) {
							memcpy_from_rom(ep0_in_buf, &str00, sizeof(str00));
							len = sizeof(str00);
						}
						else if (descriptor_index == 1) {
							memcpy_from_rom(ep0_in_buf, &vendor_string, sizeof(vendor_string));
							len = sizeof(vendor_string);
						}
						else if (descriptor_index == 2) {
							memcpy_from_rom(ep0_in_buf, &product_string, sizeof(product_string));
							len = sizeof(product_string);
						}
#if 0 ////////////////////////
						else if (descriptor_index == 3) {
							memcpy_from_rom(ep0_in_buf, &interface_string, sizeof(interface_string));
							len = sizeof(interface_string);
						}
//#else ////////////////////////
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
							bds[0].ep_in.STAT.BDnSTAT = 0;
							bds[0].ep_in.STAT.DTSEN = 1;
							bds[0].ep_in.STAT.DTS = 1;
							bds[0].ep_in.BDnCNT = sizeof(vendor_string);
							bds[0].ep_in.STAT.UOWN = 1;
						}
					}
#if 0
					else if (descriptor == HID) {
						reset_bd0_out();

						SERIAL("Request of HID descriptor.");

						// Return HID Report Descriptor
						memcpy_from_rom(ep0_in_buf, &(this_configuration_packet.hid), sizeof(struct hid_descriptor));
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = MIN(setup->wLength, sizeof(struct hid_descriptor));
						bds[0].ep_in.STAT.UOWN = 1;

					}
					else if (descriptor == REPORT) {

						reset_bd0_out();

						SERIAL("Request of HID report descriptor.");

						// Return HID Report Descriptor
						memcpy_from_rom(ep0_in_buf, &hid_report_descriptor, sizeof(hid_report_descriptor));
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = MIN(setup->wLength, sizeof(hid_report_descriptor));
						bds[0].ep_in.STAT.UOWN = 1;

						SERIAL_VAL(setup->wLength);
					}
#endif
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
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 0;
					bds[0].ep_in.STAT.UOWN = 1;
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
							//bds[1].ep_in.STAT.DTS = 1;
							SERIAL("Set configuration to");
							SERIAL_VAL(req);
						}
	
						// Return a zero-length packet.
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = 0;
						bds[0].ep_in.STAT.UOWN = 1;
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
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 1;
					bds[0].ep_in.STAT.UOWN = 1;
				
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
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = 1;
						bds[0].ep_in.STAT.UOWN = 1;
					}
					else if (setup->REQUEST.destination == 2 /*2=endpoint*/) {
						// Status of endpoint
						
						if (setup->wIndex == 0x81/*81=ep1_in*/) {
							buf[0] = g_ep1_halt;
							buf[1] = 0;//g_ep1_halt;
							memcpy(ep0_in_buf, &buf, 2);
							bds[0].ep_in.STAT.BDnSTAT = 0;
							bds[0].ep_in.STAT.DTSEN = 1;
							bds[0].ep_in.STAT.DTS = 1;
							bds[0].ep_in.BDnCNT = 2;
							bds[0].ep_in.STAT.UOWN = 1;
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
					//bds[1].ep_in.STAT.DTS = 1;
#if 1
					// Return a zero-length packet.
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 0;
					bds[0].ep_in.STAT.UOWN = 1;
#endif
				}
				else if (/*setup->REQUEST.type == 0 //0=usb_std_req &&*/
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
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 1;
					bds[0].ep_in.STAT.UOWN = 1;
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
									//bds[1].ep_in.STAT.BDnSTAT = 0;
									//bds[1].ep_in.STAT.BSTALL = 1;
									//bds[1].ep_in.STAT.UOWN = 1;
								}
								else {
									// CLEAR_FEATURE. Enable (un-halt) Endpoint.
									SERIAL("Clear Feature on Endpoint 1 in");
									g_ep1_halt = 0;
									//bds[1].ep_in.STAT.BDnSTAT = 0;
									////bds[1].ep_in.STAT.DTS = 1;
									//bds[1].ep_in.STAT.UOWN = 0;
								}
							}
							else if (setup->wIndex == 0x80 /*Endpoint 1 OUT*/) {
								if (setup->bRequest == SET_FEATURE) {
									// SET_FEATURE. Halt the Endpoint
									brake();
									SERIAL("Set Feature on endpoint 1 out.");
									bds[1].ep_out.STAT.BDnSTAT = 0;
									bds[1].ep_out.STAT.BSTALL = 1;
									bds[1].ep_out.STAT.UOWN = 1;
								}
								else {
									// CLEAR_FEATURE. Enable (un-halt) Endpoint.
									SERIAL("Clear feature on endpoint 1 out.");
									bds[1].ep_out.STAT.BDnSTAT = 0;
									bds[1].ep_out.STAT.DTSEN = 1;
									bds[1].ep_out.STAT.DTS = 0;									
									bds[1].ep_out.STAT.UOWN = 1;
								}

							}
						}
					}

					reset_bd0_out();

					// Return a zero-length packet.
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 0;
					bds[0].ep_in.STAT.UOWN = 1;
					 
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
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 0;
					bds[0].ep_in.STAT.UOWN = 1;

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
			else if (bds[0].ep_out.STAT.PID == PID_IN) {
				Nop();
			}
			else if (bds[0].ep_out.STAT.PID == PID_OUT) {
				if (bds[0].ep_out.BDnCNT == 0) {
					// Empty Packet. Handshake. End of Control Transfer.

					// Clean up the Buffer Descriptors.
					// Set the length and hand it back to the SIE.
					bds[0].ep_out.STAT.BDnSTAT = 0;
					bds[0].ep_out.STAT.DTS = 1;
					bds[0].ep_out.STAT.DTSEN = 1;					
					bds[0].ep_out.BDnCNT = sizeof(ep0_out_buf);
					bds[0].ep_out.STAT.UOWN = 1;

				}
				else {
					int z;
					Nop();
					SERIAL("OUT Data packet on 0.");
					for (z = 0; z < bds[0].ep_out.BDnCNT; z++)
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
		else if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 1/*1=IN*/) {
			if (addr_pending) {
				SFR_USB_ADDR =  addr;
				addr_pending = 0;
				got_addr = 1;
			}
			//else
			//	brake();
		}
		else if (SFR_USB_STATUS_EP == 1) {
			if (SFR_USB_STATUS_DIR == 1 /*1=IN*/) {
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

		U1IR = 0x08; //SFR_USB_TOKEN_IF = 0; TODO PORTABLE
	}
	
	// Check for Start-of-Frame interrupt.
	if (SFR_USB_SOF_IF) {
		U1IR = 0x4; //TODO PORTABLE
		//SFR_USB_SOF_IF = 0;
	}

	// Check for USB Interrupt.
	if (SFR_USB_IF) {
		SFR_USB_IF = 0;
	}

#if 0
	// If the packet in the the in buffer of ep1 gets sent (UOWN gets
	// set back to zero by the SIE), put another packet in the buffer.
	if (g_configuration > 0 && !g_ep1_halt && bds[1].ep_in.STAT.UOWN == 0) {
		uchar pid;

		// Nothing is in the transmit buffer. Send the Data.

		// For whatever Reason, make sure you clear KEN right here.
		// When this packet is done sending, the SIE will enable KEN.
		// IT MUST BE CLEARED!
		ep1_in_buf[0] = 0x01 << 1;
		ep1_in_buf[1] = g_throttle_pos;
		ep1_in_buf[2] = 0x00;
		ep1_in_buf[3] = 0x00;
		pid = !bds[1].ep_in.STAT.DTS;
		bds[1].ep_in.STAT.BDnSTAT = 0; // clear all bits (looking at you, KEN)
		bds[1].ep_in.STAT.DTSEN = 1;
		bds[1].ep_in.STAT.DTS = pid;
		bds[1].ep_in.BDnCNT = MY_HID_RESPONSE_SIZE;
		bds[1].ep_in.STAT.UOWN = 1;

		SERIAL("Sending EP 1. DTS:");
		SERIAL_VAL(pid);
	}
#endif
}

void usb_isr (void)
{
	usb_service();
}

