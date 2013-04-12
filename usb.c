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
#elif __XC8
#include <xc.h>
#endif

#include <string.h>

#include "usb.h"
#include "usb_hal.h"
#include "usb_config.h"

#define MIN(x,y) ((x<y)?x:y)

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
#pragma udata buffer_descriptors=BD_ADDR
#elif __XC16__
static struct buffer_descriptor_pair __attribute__((aligned(512)))
#elif __XC8
static struct buffer_descriptor_pair
#else
#error compiler not supported
#endif
bds[NUM_ENDPOINT_NUMBERS+1] XC8_BD_ADDR_TAG;

#ifdef __C18
/* The actual buffers to and from which the data is transferred from the SIE
   (from the USB bus). These buffers must fully be located between addresses
   0x400 and 0x7FF per the datasheet.*/
/* This addr is for the PIC18F4550 */
#pragma udata usb_buffers=0x500
#elif __XC16__
	/* Buffers can go anywhere on PIC24 parts which are supported (so far). */
#elif __XC8
	/* Addresses are set by BD_ADDR and BUF_ADDR below. */
#else
	#error compiler not supported
#endif

static struct {
#define EP_BUF(n) \
	uchar ep_##n##_out_buf[EP_##n##_OUT_LEN]; \
	uchar ep_##n##_in_buf[EP_##n##_IN_LEN];

#if NUM_ENDPOINT_NUMBERS >= 0
	EP_BUF(0)
#endif
#if NUM_ENDPOINT_NUMBERS >= 1
	EP_BUF(1)
#endif
#if NUM_ENDPOINT_NUMBERS >= 2
	EP_BUF(2)
#endif
#if NUM_ENDPOINT_NUMBERS >= 3
	EP_BUF(3)
#endif
#if NUM_ENDPOINT_NUMBERS >= 4
	EP_BUF(4)
#endif
#if NUM_ENDPOINT_NUMBERS >= 5
	EP_BUF(5)
#endif
#if NUM_ENDPOINT_NUMBERS >= 6
	EP_BUF(6)
#endif
#if NUM_ENDPOINT_NUMBERS >= 7
	EP_BUF(7)
#endif
#if NUM_ENDPOINT_NUMBERS >= 8
	EP_BUF(8)
#endif
#if NUM_ENDPOINT_NUMBERS >= 9
	EP_BUF(9)
#endif
#if NUM_ENDPOINT_NUMBERS >= 10
	EP_BUF(10)
#endif
#if NUM_ENDPOINT_NUMBERS >= 11
	EP_BUF(11)
#endif
#if NUM_ENDPOINT_NUMBERS >= 12
	EP_BUF(12)
#endif
#if NUM_ENDPOINT_NUMBERS >= 13
	EP_BUF(13)
#endif
#if NUM_ENDPOINT_NUMBERS >= 14
	EP_BUF(14)
#endif
#if NUM_ENDPOINT_NUMBERS >= 15
	EP_BUF(15)
#endif

#undef EP_BUF
} ep_buffers XC8_BUFFER_ADDR_TAG;

struct ep_buf {
	uchar *out;
	uchar *in;
	uint8_t out_len;
	uint8_t in_len;
};

#ifdef __C18
#pragma idata
#endif

#define EP_BUFS(n) { ep_buffers.ep_##n##_out_buf, ep_buffers.ep_##n##_in_buf, EP_##n##_OUT_LEN, EP_##n##_IN_LEN },

static struct ep_buf ep_buf[NUM_ENDPOINT_NUMBERS+1] = {
#if NUM_ENDPOINT_NUMBERS >= 0
	EP_BUFS(0)
#endif
#if NUM_ENDPOINT_NUMBERS >= 1
	EP_BUFS(1)
#endif
#if NUM_ENDPOINT_NUMBERS >= 2
	EP_BUFS(2)
#endif
#if NUM_ENDPOINT_NUMBERS >= 3
	EP_BUFS(3)
#endif
#if NUM_ENDPOINT_NUMBERS >= 4
	EP_BUFS(4)
#endif
#if NUM_ENDPOINT_NUMBERS >= 5
	EP_BUFS(5)
#endif
#if NUM_ENDPOINT_NUMBERS >= 6
	EP_BUFS(6)
#endif
#if NUM_ENDPOINT_NUMBERS >= 7
	EP_BUFS(7)
#endif
#if NUM_ENDPOINT_NUMBERS >= 8
	EP_BUFS(8)
#endif
#if NUM_ENDPOINT_NUMBERS >= 9
	EP_BUFS(9)
#endif
#if NUM_ENDPOINT_NUMBERS >= 10
	EP_BUFS(10)
#endif
#if NUM_ENDPOINT_NUMBERS >= 11
	EP_BUFS(11)
#endif
#if NUM_ENDPOINT_NUMBERS >= 12
	EP_BUFS(12)
#endif
#if NUM_ENDPOINT_NUMBERS >= 13
	EP_BUFS(13)
#endif
#if NUM_ENDPOINT_NUMBERS >= 14
	EP_BUFS(14)
#endif
#if NUM_ENDPOINT_NUMBERS >= 15
	EP_BUFS(15)
#endif

};
#undef EP_BUFS

// Global data pertaining to addressing the device.
static uchar addr_pending = 0; // boolean
static uchar addr = 0x0;
static int got_addr = 0; // whether addr is set DEBUG
static char g_configuration = 0;
static char g_ep1_halt = 0;

extern struct device_descriptor USB_DEVICE_DESCRIPTOR;
extern struct configuration_descriptor *USB_CONFIG_DESCRIPTOR_MAP[];

#define SERIAL(x)
#define SERIAL_VAL(x)

/* usb_init() is called at powerup time, and when the device gets
   the reset signal from the USB bus (D+ and D- both held low) indicated
   by interrput bit URSTIF. */
void usb_init(void)
{
	uchar i;

	/* Initialize the USB. 18.4 of PIC24FJ64GB004 datasheet */
	SET_PING_PONG_MODE(0);   /* 0 = disable ping-pong buffer */
	SFR_USB_INTERRUPT_EN = 0x0;
	SFR_USB_EXTENDED_INTERRUPT_EN = 0x0;
	
	SFR_USB_EN = 1; /* enable USB module */
	//U1OTGCONbits.OTGEN = 1; // TODO Portable

	
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

	CLEAR_ALL_USB_IF();

//	UIEbits.TRNIE = 1;   /* USB Transfer Interrupt Enable */
//	UIEbits.URSTIE = 1;  /* USB Reset Interrupt Enable */
//	UIEbits.SOFIE = 1;   /* USB Start-Of-Frame Interrupt Enable */

#ifdef USB_NEEDS_SET_BD_ADDR_REG
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

	SFR_BD_ADDR_REG = w.hb;
#endif

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
		volatile SFR_EP_MGMT_TYPE *ep = &SFR_EP_MGMT(1) + (i-1);
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
	bds[0].ep_out.BDnADR = (BDNADR_TYPE) ep_buf[0].out;
	bds[0].ep_out.BDnCNT = ep_buf[0].out_len;
	bds[0].ep_out.STAT.DTSEN = 0;
	bds[0].ep_out.STAT.UOWN = 1;

	// Setup endpoint 0 Input buffer descriptor.
	// Input and output are from the HOST perspective.
	bds[0].ep_in.BDnADR = (BDNADR_TYPE) ep_buf[0].in;
	bds[0].ep_in.BDnCNT = ep_buf[0].in_len;
	bds[0].ep_in.STAT.DTSEN = 0;
	bds[0].ep_in.STAT.DTS = 0;
	bds[0].ep_in.STAT.UOWN = 0;

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		// Setup endpoint 1 Output buffer descriptor.
		// Input and output are from the HOST perspective.
		bds[i].ep_out.BDnADR = (BDNADR_TYPE) ep_buf[i].out;
		bds[i].ep_out.BDnCNT = ep_buf[i].out_len;
		bds[i].ep_out.STAT.DTSEN = 0;
		bds[i].ep_out.STAT.UOWN = 1;

		// Setup endpoint 1 Input buffer descriptor.
		// Input and output are from the HOST perspective.
		bds[i].ep_in.BDnADR = (BDNADR_TYPE) ep_buf[i].in;
		bds[i].ep_in.BDnCNT = ep_buf[i].in_len;
		bds[i].ep_in.STAT.DTSEN = 0;
		bds[i].ep_in.STAT.DTS = 1;
		bds[i].ep_in.STAT.UOWN = 0;
	}
	
	#ifdef USB_NEEDS_POWER_ON
	SFR_USB_POWER = 1;
	#endif

#ifdef __XC16__
	U1OTGCONbits.DPPULUP = 1;
#warning Find out if this is needed
#endif
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
	bds[0].ep_out.BDnCNT = ep_buf[0].out_len;
	bds[0].ep_out.STAT.UOWN = 1;
}

void stall_ep0(void)
{
	// Stall Endpoint 0. It's important that DTSEN and DTS are zero.
	bds[0].ep_in.STAT.UOWN = 0;
	bds[0].ep_in.STAT.BDnSTAT = 0;
	bds[0].ep_in.STAT.DTSEN = 0;
	bds[0].ep_in.STAT.DTS = 0;
	bds[0].ep_in.STAT.BSTALL = 1;
	bds[0].ep_in.STAT.UOWN = 1;
	bds[0].ep_in.BDnCNT = ep_buf[0].in_len;

}

void brake(void)
{
	Nop();
	Nop();
	Nop();
}

static char *control_return_ptr;
static int  control_bytes_remaining;
static int control_need_zlp;

static uint8_t start_control_return(void *ptr, size_t len, size_t bytes_asked_for)
{
	uint8_t bytes_to_send = MIN(len, EP_0_IN_LEN);
	bytes_to_send = MIN(bytes_to_send, bytes_asked_for);
	memcpy_from_rom(ep_buf[0].in, ptr, bytes_to_send);
	control_return_ptr = ((char*)ptr) + bytes_to_send;
	control_bytes_remaining = MIN(bytes_asked_for, len) - bytes_to_send;

	return bytes_to_send;
}

/* checkUSB() is called repeatedly to check for USB interrupts
   and service USB requests */
void usb_service(void)
{
	if (SFR_USB_RESET_IF) {
		// A Reset was detected on the wire. Re-init the SIE.
		usb_init();
		CLEAR_USB_RESET_IF();
		SERIAL("USB Reset");
	}
	
	if (SFR_USB_STALL_IF) {
		CLEAR_USB_STALL_IF();
	}


	if (SFR_USB_TOKEN_IF) {

		//struct ustat_bits ustat = *((struct ustat_bits*)&USTAT);

		if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 0/*OUT*/) {
			// Packet for us on Endpoint 0.
			if (bds[0].ep_out.STAT.PID == PID_SETUP) {
				// SETUP packet.

				FAR struct setup_packet *setup = (struct setup_packet*) ep_buf[0].out;

				// SETUP packet sets PKTDIS Per Doc. Not sure why.
				SFR_USB_PKT_DIS = 0;

				if (setup->bRequest == GET_DESCRIPTOR) {
					char descriptor = ((setup->wValue >> 8) & 0x00ff);
					uchar descriptor_index = setup->wValue & 0x00ff;
					uint8_t bytes_to_send;

					if (descriptor == DEVICE) {
						SERIAL("Get Descriptor for DEVICE");

						// Return Device Descriptor
						bds[0].ep_in.STAT.UOWN = 0;
						bytes_to_send =  start_control_return(&USB_DEVICE_DESCRIPTOR, USB_DEVICE_DESCRIPTOR.bLength, setup->wLength);
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = bytes_to_send;
						bds[0].ep_in.STAT.UOWN = 1;
					}
					else if (descriptor == CONFIGURATION) {
						struct configuration_descriptor *desc;
						if (descriptor_index >= NUMBER_OF_CONFIGURATIONS)
							stall_ep0();
						else {
							desc = USB_CONFIG_DESCRIPTOR_MAP[descriptor_index];

							// Return Configuration Descriptor. Make sure to only return
							// the number of bytes asked for by the host.
							bds[0].ep_in.STAT.UOWN = 0;
							//CHECK check length
							bytes_to_send = start_control_return(desc, desc->wTotalLength, setup->wLength);
							bds[0].ep_in.STAT.BDnSTAT = 0;
							bds[0].ep_in.STAT.DTSEN = 1;
							bds[0].ep_in.STAT.DTS = 1;
							bds[0].ep_in.BDnCNT = bytes_to_send;
							bds[0].ep_in.STAT.UOWN = 1;
						}
					}
					else if (descriptor == STRING) {
						void *desc;
						int16_t len;

						bds[0].ep_in.STAT.UOWN = 0;

						len = USB_STRING_DESCRIPTOR_FUNC(descriptor_index, &desc);
						if (len < 0) {
							stall_ep0();
							SERIAL("Unsupported string descriptor requested");
						}
						else {
							bytes_to_send = start_control_return(desc, len, setup->wLength);

							// Return Descriptor
							bds[0].ep_in.STAT.BDnSTAT = 0;
							bds[0].ep_in.STAT.DTSEN = 1;
							bds[0].ep_in.STAT.DTS = 1;
							bds[0].ep_in.BDnCNT = bytes_to_send;
							bds[0].ep_in.STAT.UOWN = 1;
						}
					}
#if 0
					else if (descriptor == HID) {

						SERIAL("Request of HID descriptor.");

						// Return HID Report Descriptor
						bds[0].ep_in.STAT.UOWN = 0;
						memcpy_from_rom(ep_buf[0].in, &(this_configuration_packet.hid), sizeof(struct hid_descriptor));
						bds[0].ep_in.STAT.BDnSTAT = 0;
						bds[0].ep_in.STAT.DTSEN = 1;
						bds[0].ep_in.STAT.DTS = 1;
						bds[0].ep_in.BDnCNT = MIN(setup->wLength, sizeof(struct hid_descriptor));
						bds[0].ep_in.STAT.UOWN = 1;

					}
					else if (descriptor == REPORT) {

						SERIAL("Request of HID report descriptor.");

						// Return HID Report Descriptor
						bds[0].ep_in.STAT.UOWN = 0;
						memcpy_from_rom(ep_buf[0].in, &hid_report_descriptor, sizeof(hid_report_descriptor));
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
						stall_ep0();
						SERIAL("Unknown Descriptor");
						SERIAL_VAL(descriptor);

					}
				}
				else if (setup->bRequest == SET_ADDRESS) {
					// Mark the ADDR as pending. The address gets set only
					// after the transaction is complete.
					addr_pending = 1;
					addr = setup->wValue;

					// Return a zero-length packet.
					bds[0].ep_in.STAT.UOWN = 0;
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
						bds[0].ep_in.STAT.UOWN = 0;
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

					SERIAL("Get Configuration. Returning:");
					SERIAL_VAL(g_configuration);

					bds[0].ep_in.STAT.UOWN = 0;
					buf[0] = g_configuration;
					memcpy(ep_buf[0].in, &buf, 1);
					bds[0].ep_in.STAT.BDnSTAT = 0;
					bds[0].ep_in.STAT.DTSEN = 1;
					bds[0].ep_in.STAT.DTS = 1;
					bds[0].ep_in.BDnCNT = 1;
					bds[0].ep_in.STAT.UOWN = 1;
				
				}
				else if (setup->bRequest == GET_STATUS) {
					char buf[10];					
					
					SERIAL("Get Status (dst, index):");
					SERIAL_VAL(setup->REQUEST.destination);
					SERIAL_VAL(setup->wIndex);

					if (setup->REQUEST.destination == 0 /*0=device*/) {
						// Status for the DEVICE requested
						// Return as a single byte in the return packet.
						buf[0] = 0;
						bds[0].ep_in.STAT.UOWN = 0;
						memcpy(ep_buf[0].in, &buf, 1);
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
							bds[0].ep_in.STAT.UOWN = 0;
							memcpy(ep_buf[0].in, &buf, 2);
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

					//stall_ep0();
					//bds[1].ep_in.STAT.DTS = 1;
#if 1
					// Return a zero-length packet.
					bds[0].ep_in.STAT.UOWN = 0;
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

					SERIAL("Get Interface");
					SERIAL_VAL(setup->bRequest);
					SERIAL_VAL(setup->REQUEST.destination);
					SERIAL_VAL(setup->REQUEST.type);
					SERIAL_VAL(setup->REQUEST.direction);
					

					// Return the current interface (hard-coded to 1)
					// as a single byte in the return packet.
					buf[0] = 1;
					bds[0].ep_in.STAT.UOWN = 0;
					memcpy(ep_buf[0].in, &buf, 1);
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

					// Return a zero-length packet.
					bds[0].ep_in.STAT.UOWN = 0;
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
					bds[0].ep_in.STAT.UOWN = 0;
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
				/* Nonsense condition:
				   (PID IN on SFR_USB_STATUS_DIR == OUT) */
			}
			else if (bds[0].ep_out.STAT.PID == PID_OUT) {
				if (bds[0].ep_out.BDnCNT == 0) {
					// Empty Packet. Handshake. End of Control Transfer.

					// Clean up the Buffer Descriptors.
					// Set the length and hand it back to the SIE.
					bds[0].ep_out.STAT.BDnSTAT = 0;
					bds[0].ep_out.STAT.DTS = 1;
					bds[0].ep_out.STAT.DTSEN = 1;					
					bds[0].ep_out.BDnCNT = ep_buf[0].out_len;
					bds[0].ep_out.STAT.UOWN = 1;

				}
				else {
					int z;
					Nop();
					SERIAL("OUT Data packet on 0.");
					for (z = 0; z < bds[0].ep_out.BDnCNT; z++)
						SERIAL_VAL(ep_buf[0].out[z]);
					
					if (ep_buf[0].out[0] == 1) {
						// Report 1 is data.
						if (ep_buf[0].out)
							PORTAbits.RA1 = 0;
						else
							PORTAbits.RA1 = 1;
					}
					else if (ep_buf[0].out[0] == 2) {
						// Report 2 is feature.
						// This report contains ASCII to write the Serial Number.
					}
				}
			}
			else {
				// Unsupported PID. Stall the Endpoint.
				brake();
				SERIAL("Unsupported PID. Stall.");
				stall_ep0();
			}

			reset_bd0_out();
		}
		else if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 1/*1=IN*/) {
			if (addr_pending) {
				SFR_USB_ADDR =  addr;
				addr_pending = 0;
				got_addr = 1;
			}

			if (control_bytes_remaining) {
				/* There's already a multi-transaction transfer in process. */
				uint8_t bytes_to_send = MIN(control_bytes_remaining, EP_0_IN_LEN);

				memcpy_from_rom(ep_buf[0].in, control_return_ptr, bytes_to_send);
				control_bytes_remaining -= bytes_to_send;
				control_return_ptr += bytes_to_send;

				/* If we hit the end with a full-length packet, set up
				   to send a zero-length packet at the next IN token. */
				if (control_bytes_remaining == 0 && bytes_to_send == EP_0_IN_LEN)
					control_need_zlp = 1;

				usb_send_in_buffer(0, bytes_to_send);
			}
			else if (control_need_zlp) {
				usb_send_in_buffer(0, 0);
				control_need_zlp = 0;
			}
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

		CLEAR_USB_TOKEN_IF();
	}
	
	// Check for Start-of-Frame interrupt.
	if (SFR_USB_SOF_IF) {
		CLEAR_USB_SOF_IF();
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
		ep_buf[1].in[0] = 0x01 << 1;
		ep_buf[1].in[1] = g_throttle_pos;
		ep_buf[1].in[2] = 0x00;
		ep_buf[1].in[3] = 0x00;
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

uchar *usb_get_in_buffer(uint8_t endpoint)
{
	return ep_buf[endpoint].in;
}

void usb_send_in_buffer(uint8_t endpoint, size_t len)
{
	if (g_configuration > 0 || endpoint == 0) {
		uchar pid;
		bds[endpoint].ep_in.STAT.UOWN = 0;
		pid = !bds[endpoint].ep_in.STAT.DTS;
		bds[endpoint].ep_in.STAT.BDnSTAT = 0; // clear all bits (looking at you, KEN)
		bds[endpoint].ep_in.STAT.DTSEN = 1;
		bds[endpoint].ep_in.STAT.DTS = pid;
		bds[endpoint].ep_in.BDnCNT = len;
		bds[endpoint].ep_in.STAT.UOWN = 1;
	}
}

bool usb_in_endpoint_busy(uint8_t endpoint)
{
	return bds[endpoint].ep_in.STAT.UOWN;
}

uchar *usb_get_out_buffer(uint8_t endpoint)
{
	return ep_buf[endpoint].out;
}

bool usb_out_endpoint_busy(uint8_t endpoint)
{
	return bds[endpoint].ep_out.STAT.UOWN;
}
