/*
 *  M-Stack USB Device Stack Implementation
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  Initial version for PIC18, 2008-02-24
 *  PIC24 port, 2013-04-08
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

#ifdef __XC32__
#include <xc.h>
#include <sys/kmem.h>
#elif __XC16__
#include <libpic30.h>
#include <xc.h>
#elif __C18
#include <p18f4550.h>
#include <delays.h>
#elif __XC8
#include <xc.h>
#else
#error "Compiler not supported"
#endif

#include <string.h>

#include "usb_config.h"
#include "usb.h"
#include "usb_hal.h"
#include "usb_ch9.h"
#include "usb_microsoft.h"
#include "usb_winusb.h"

#if _PIC14E && __XC8
	/* This is necessary to avoid a warning about ep0_data_stage_callback
	 * never being assigned to anything other than NULL. Since this is a
	 * library, it's possible (and likely) that the application will not
	 * make use of data stage callbacks, or control transfers at all. This
	 * is only an issue on PIC16F parts (so far) on XC8.
	 */
	#pragma warning disable 1088
#endif

#define MIN(x,y) (((x)<(y))?(x):(y))

/* Even though they're the same, It's convenient below (for the buffer
 * macros) to have separate #defines for IN and OUT EP 0 lengths which
 * match the format of the other endpoint length #defines. */
#define EP_0_OUT_LEN EP_0_LEN
#define EP_0_IN_LEN  EP_0_LEN

#ifndef PPB_MODE
	#error "PPB_MODE not defined. Define it to one of the four PPB_* macros in usb_hal.h"
#endif

#ifdef USB_FULL_PING_PONG_ONLY
	#if PPB_MODE != PPB_ALL
		#error "This hardware only supports PPB_ALL"
	#endif
#endif


#if PPB_MODE == PPB_EPO_OUT_ONLY
	#define PPB_EP0_OUT
	#undef  PPB_EP0_IN
	#undef  PPB_EPn
#elif PPB_MODE == PPB_EPN_ONLY
	#undef  PPB_EP0_OUT
	#undef  PPB_EP0_IN
	#define PPB_EPn
#elif PPB_MODE == PPB_NONE
	#undef PPB_EP0_OUT
	#undef PPB_EP0_IN
	#undef PPB_EPn
#elif PPB_MODE == PPB_ALL
	#define PPB_EP0_OUT
	#define PPB_EP0_IN
	#define PPB_EPn
#else
	#error "Must select a valid PPB_MODE"
#endif

STATIC_SIZE_CHECK_EQUAL(sizeof(struct endpoint_descriptor), 7);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct interface_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct configuration_descriptor), 9);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct device_descriptor), 18);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct interface_association_descriptor), 8);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct setup_packet), 8);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct microsoft_os_descriptor), 18);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct microsoft_extended_compat_header), 16);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct microsoft_extended_compat_function), 24);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct microsoft_extended_properties_header), 10);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct microsoft_extended_property_section_header), 8);
#ifdef __XC32__
STATIC_SIZE_CHECK_EQUAL(sizeof(struct buffer_descriptor), 8);
#else
STATIC_SIZE_CHECK_EQUAL(sizeof(struct buffer_descriptor), 4);
#endif

#ifdef __C18
/* The buffer descriptors. Per the PIC18F4550 Data sheet, when _not_ using
   ping-pong buffering, these must be laid out sequentially starting at
   address 0x0400 in the following order, ep0_out, ep0_in,ep1_out, ep1_in,
   etc. These must be initialized prior to use. */
#pragma udata buffer_descriptors=BD_ADDR
#endif


/* Calculate the number of Buffer Descriptor pairs, which depends on the
   ping-pong modes active and the number of endpoints used. */
#if defined(PPB_EP0_IN) && defined(PPB_EP0_OUT)
	#define NUM_BD_0 4
#elif !defined(PPB_EP0_IN) && defined(PPB_EP0_OUT)
	#define NUM_BD_0 3
#elif !defined(PPB_EP0_IN) && !defined(PPB_EP0_OUT)
	#define NUM_BD_0 2
#else
	#error "Nonsense condition detected"
#endif

#ifdef PPB_EPn
	#define NUM_BD (4 * (NUM_ENDPOINT_NUMBERS) + NUM_BD_0)
#else
	#define NUM_BD (2 * (NUM_ENDPOINT_NUMBERS) + NUM_BD_0)
#endif

/* Macros to access a specific buffer descriptor, which depends on the
   ping-pong modes active. Since EP 0 can have a different ping-pong mode
   than the other endpoints, EP0's buffer descriptor should be accessed through
   BDS0OUT() and BDS0IN() only. It is not valid to call BDSnOUT(0,oe), for
   example*/
#if PPB_MODE == PPB_EPO_OUT_ONLY
	#define BDS0OUT(oe) bds[0 + oe]
	#define BDS0IN(oe) bds[2]
	#define BDSnOUT(EP,oe) bds[(EP) * 2 + 1]
	#define BDSnIN(EP,oe) bds[(EP) * 2 + 2]
#elif PPB_MODE == PPB_EPN_ONLY
	#define BDS0OUT(oe) bds[0]
	#define BDS0IN(oe) bds[1]
	#define BDSnOUT(EP,oe) bds[(EP) * 4 - 2 + (oe)]
	#define BDSnIN(EP,oe) bds[(EP) * 4 + (oe)]
#elif PPB_MODE == PPB_NONE
	#define BDS0OUT(oe) bds[0]
	#define BDS0IN(oe) bds[1]
	#define BDSnOUT(EP,oe) bds[(EP) * 2]
	#define BDSnIN(EP,oe) bds[(EP) * 2 + 1]
#elif PPB_MODE == PPB_ALL
	#define BDS0OUT(oe) bds[0 + oe]
	#define BDS0IN(oe) bds[2 + oe]
	#define BDSnOUT(EP,oe) bds[(EP) * 4 + (oe)]
	#define BDSnIN(EP,oe) bds[(EP) * 4 + 2 + (oe)]
#else
#error "Must select a valid PPB_MODE"
#endif

#if defined(AUTOMATIC_WINUSB_SUPPORT) && !defined(MICROSOFT_OS_DESC_VENDOR_CODE)
#error "Must define a MICROSOFT_OS_DESC_VENDOR_CODE for Automatic WinUSB"
#endif

#ifdef AUTOMATIC_WINUSB_SUPPORT
	/* Make sure the Microsoft descriptor functions aren't defined */
	#ifdef MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC
		#error "Must not define MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC when using Automatic WinUSB"
	#endif
	#ifdef MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC
		#error "Must not define MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC when using Automatic WinUSB"
	#endif

	/* Define the Microsoft descriptor functions to the handlers
	 * implemented in usb_winusb.c */
	#define MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC m_stack_winusb_get_microsoft_compat
	#define MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC m_stack_winusb_get_microsoft_property
#endif

static struct buffer_descriptor bds[NUM_BD] BD_ATTR_TAG;

#ifdef __C18
/* The actual buffers to and from which the data is transferred from the SIE
   (from the USB bus). These buffers must fully be located between addresses
   0x400 and 0x7FF per the datasheet.*/
/* This addr is for the PIC18F4550 */
#pragma udata usb_buffers=0x500
#elif defined(__XC16__) || defined(__XC32__)
	/* Buffers can go anywhere on PIC24/PIC32 parts which are supported
	   (so far). */
#elif __XC8
	/* Addresses are set by BD_ADDR and BUF_ADDR below. */
#else
	#error compiler not supported
#endif

static struct {
/* Set up the EP_BUF() macro for EP0 */
#if defined(PPB_EP0_IN) && defined(PPB_EP0_OUT)
	#define EP_BUF(n) \
		unsigned char ep_##n##_out_buf[2][EP_##n##_OUT_LEN]; \
		unsigned char ep_##n##_in_buf[2][EP_##n##_IN_LEN];
#elif !defined(PPB_EP0_IN) && defined(PPB_EP0_OUT)
	#define EP_BUF(n) \
		unsigned char ep_##n##_out_buf[2][EP_##n##_OUT_LEN]; \
		unsigned char ep_##n##_in_buf[1][EP_##n##_IN_LEN];
#elif !defined(PPB_EP0_IN) && !defined(PPB_EP0_OUT)
	#define EP_BUF(n) \
		unsigned char ep_##n##_out_buf[1][EP_##n##_OUT_LEN]; \
		unsigned char ep_##n##_in_buf[1][EP_##n##_IN_LEN];
#else
	#error "Nonsense condition detected"
#endif

#if NUM_ENDPOINT_NUMBERS >= 0
	EP_BUF(0)
#endif

/* Re-setup the EP_BUF() macro for the rest of the endpoints */
#undef EP_BUF
#ifdef PPB_EPn
	#define EP_BUF(n) \
		unsigned char ep_##n##_out_buf[2][EP_##n##_OUT_LEN]; \
		unsigned char ep_##n##_in_buf[2][EP_##n##_IN_LEN];
#else
	#define EP_BUF(n) \
		unsigned char ep_##n##_out_buf[1][EP_##n##_OUT_LEN]; \
		unsigned char ep_##n##_in_buf[1][EP_##n##_IN_LEN];
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
	unsigned char * const out; /* buffers for the even buffer descriptor */
	unsigned char * const in;  /* ie: ppbi = 0 */
#ifdef PPB_EPn
	unsigned char * const out1; /* buffers for the odd buffer descriptor */
	unsigned char * const in1;  /* ie: ppbi = 1 */
#endif
	const uint8_t out_len;
	const uint8_t in_len;

#define EP_OUT_HALT_FLAG 0x1
#define EP_IN_HALT_FLAG 0x2
#define EP_RX_DTS 0x4  /* The DTS of the _next_ packet */
#define EP_TX_DTS 0x8
#define EP_RX_PPBI 0x10 /* Represents the next buffer which will be need to be
                           reset and given back to the SIE. */
#define EP_TX_PPBI 0x20 /* Represents the _next_ buffer to write into. */
	uint8_t flags;
};

struct ep0_buf {
	unsigned char * const out; /* buffers for the even buffer descriptor */
	unsigned char * const in;  /* ie: ppbi = 0 */
#ifdef PPB_EP0_OUT
	unsigned char * const out1; /* buffer for the odd buffer descriptor */
#endif
#ifdef PPB_EP0_IN
	unsigned char * const in1;  /* buffer for the odd buffer descriptor */
#endif

	/* Use the EP_* flags from ep_buf for flags */
	uint8_t flags;
};

#ifdef __C18
#pragma idata
#endif

#if defined(PPB_EP0_IN) && defined(PPB_EP0_OUT)
	#define EP_BUFS0() { ep_buffers.ep_0_out_buf[0], \
	                     ep_buffers.ep_0_in_buf[0], \
	                     ep_buffers.ep_0_out_buf[1], \
	                     ep_buffers.ep_0_in_buf[1] }

#elif !defined(PPB_EP0_IN) && defined(PPB_EP0_OUT)
	#define EP_BUFS0() { ep_buffers.ep_0_out_buf[0], \
	                     ep_buffers.ep_0_in_buf[0], \
	                     ep_buffers.ep_0_out_buf[1] }

#elif !defined(PPB_EP0_IN) && !defined(PPB_EP0_OUT)
	#define EP_BUFS0() { ep_buffers.ep_0_out_buf[0], \
	                     ep_buffers.ep_0_in_buf[0] }

#else
	#error "Nonsense condition detected"
#endif


#ifdef PPB_EPn
	#define EP_BUFS(n) { ep_buffers.ep_##n##_out_buf[0], \
	                     ep_buffers.ep_##n##_in_buf[0], \
	                     ep_buffers.ep_##n##_out_buf[1], \
	                     ep_buffers.ep_##n##_in_buf[1], \
	                     EP_##n##_OUT_LEN, \
	                     EP_##n##_IN_LEN },
#else
	#define EP_BUFS(n) { ep_buffers.ep_##n##_out_buf[0], \
	                     ep_buffers.ep_##n##_in_buf[0], \
	                     EP_##n##_OUT_LEN, \
	                     EP_##n##_IN_LEN },
#endif

static struct ep0_buf ep0_buf = EP_BUFS0();

static struct ep_buf ep_buf[NUM_ENDPOINT_NUMBERS+1] = {
#if NUM_ENDPOINT_NUMBERS >= 0
	{ NULL, NULL },
	/* TODO wasted space here */
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
#undef EP_BUFS0

/* Global data */
static bool addr_pending;
static uint8_t addr;
static uint8_t g_configuration;
static bool control_need_zlp;
static bool returning_short;

/* Data associated with multi-packet control transfers */
static usb_ep0_data_stage_callback ep0_data_stage_callback;
static char   *ep0_data_stage_in_buffer; /* XC8 v1.12 fails if this is const on PIC16 */
static char   *ep0_data_stage_out_buffer;
static size_t  ep0_data_stage_buf_remaining;
static void   *ep0_data_stage_context;
static uint8_t ep0_data_stage_direc; /*1=IN, 0=OUT, Same as USB spec.*/

#ifdef _PIC14E
/* Convert a pointer, which can be a normal banked pointer or a linear
 * pointer, to a linear pointer.
 *
 * The USB buffer descriptors need linear addresses. The XC8 compiler will
 * generate banked (not linear) addresses for the arrays in ep_buffers if
 * ep_buffers can fit within a single bank. This is good for code size, but
 * the buffer descriptors cannot take banked addresses, so they must be
 * generated from the banked addresses.
 *
 * See section 3.6.2 of the PIC16F1459 datasheet for details.
 */
static uint16_t pic16_linear_addr(void *ptr)
{
	uint8_t high, low;
	uint16_t addr = (uint16_t) ptr;

	/* Addresses over 0x2000 are already linear addresses. */
	if (addr >= 0x2000)
		return addr;

	high = (addr & 0xff00) >> 8;
	low  = addr & 0x00ff;

	return 0x2000 +
	       (low & 0x7f) - 0x20 +
	       ((high << 1) + (low & 0x80)? 1: 0) * 0x50;
}
#endif

static void reset_ep0_data_stage()
{
	ep0_data_stage_in_buffer = NULL;
	ep0_data_stage_out_buffer = NULL;
	ep0_data_stage_buf_remaining = 0;
	ep0_data_stage_callback = NULL;

	/* There's no need to reset the following because no decisions are
	   made based on them:
	     ep0_data_stage_context,
	     ep0_data_stage_direc
	 */
}

#define SERIAL(x)
#define SERIAL_VAL(x)

/* usb_init() is called at powerup time, and when the device gets
   the reset signal from the USB bus (D+ and D- both held low) indicated
   by interrput bit URSTIF. */
void usb_init(void)
{
	uint8_t i;

	/* Initialize the USB. 18.4 of PIC24FJ64GB004 datasheet */
#ifndef USB_FULL_PING_PONG_ONLY
	SET_PING_PONG_MODE(PPB_MODE);
#endif
#if PPB_MODE != PPB_NONE
	SFR_USB_PING_PONG_RESET = 1;
	SFR_USB_PING_PONG_RESET = 0;
#endif
	SFR_USB_INTERRUPT_EN = 0x0;
	SFR_USB_EXTENDED_INTERRUPT_EN = 0x0;
	
	SFR_USB_EN = 1; /* enable USB module */

#ifdef USE_OTG
	SFR_OTGEN = 1;
#endif

	
#ifdef NEEDS_PULL
	SFR_PULL_EN = 1;  /* pull-up enable */
#endif

#ifdef HAS_ON_CHIP_XCVR_DIS
	SFR_ON_CHIP_XCVR_DIS = 0; /* on-chip transceiver Disable */
#endif

#ifdef HAS_LOW_SPEED
	SFR_FULL_SPEED_EN = 1;   /* Full-speed enable */
#endif

	CLEAR_USB_TOKEN_IF();   /* Clear 4 times to clear out USTAT FIFO */
	CLEAR_USB_TOKEN_IF();
	CLEAR_USB_TOKEN_IF();
	CLEAR_USB_TOKEN_IF();

	CLEAR_ALL_USB_IF();

#ifdef USB_USE_INTERRUPTS
	SFR_TRANSFER_IE = 1; /* USB Transfer Interrupt Enable */
	SFR_STALL_IE = 1;    /* USB Stall Interrupt Enable */
	SFR_RESET_IE = 1;    /* USB Reset Interrupt Enable */
#ifdef START_OF_FRAME_CALLBACK
	SFR_SOF_IE = 1;      /* USB Start-Of-Frame Interrupt Enable */
#endif
#endif

#ifdef USB_NEEDS_SET_BD_ADDR_REG
#ifdef __XC16__
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

#elif __XC32__
	union WORD {
		struct {
			uint8_t lb;
			uint8_t hb;
			uint8_t ub;
			uint8_t eb;
		};
		uint32_t w;
		void *ptr;
	};
	union WORD w;
	w.w = KVA_TO_PA(bds);

	SFR_BD_ADDR_REG1 = w.hb & 0xFE;
	SFR_BD_ADDR_REG2 = w.ub;
	SFR_BD_ADDR_REG3 = w.eb;
#endif
#endif

	/* These are the UEP/U1EP endpoint management registers. */
	
	/* Clear them all out. This is important because a bootloader
	   could have set them to non-zero */
	memset(SFR_EP_MGMT(0), 0x0, sizeof(*SFR_EP_MGMT(0)) * 16);
	
	/* Set up Endpoint zero */
	SFR_EP_MGMT(0)->SFR_EP_MGMT_HANDSHAKE = 1; /* Endpoint handshaking enable */
	SFR_EP_MGMT(0)->SFR_EP_MGMT_CON_DIS = 0; /* 1=Disable control operations */
	SFR_EP_MGMT(0)->SFR_EP_MGMT_OUT_EN = 1; /* Endpoint Out Transaction Enable */
	SFR_EP_MGMT(0)->SFR_EP_MGMT_IN_EN = 1; /* Endpoint In Transaction Enable */
	SFR_EP_MGMT(0)->SFR_EP_MGMT_STALL = 0; /* Stall */

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		volatile SFR_EP_MGMT_TYPE *ep = SFR_EP_MGMT(i);
		ep->SFR_EP_MGMT_HANDSHAKE = 1; /* Endpoint handshaking enable */
		ep->SFR_EP_MGMT_CON_DIS = 1; /* 1=Disable control operations */
		ep->SFR_EP_MGMT_OUT_EN = 1; /* Endpoint Out Transaction Enable */
		ep->SFR_EP_MGMT_IN_EN = 1; /* Endpoint In Transaction Enable */
		ep->SFR_EP_MGMT_STALL = 0; /* Stall */
	}

	/* Reset the Address. */
	SFR_USB_ADDR = 0x0;
	addr_pending = 0;
	g_configuration = 0;
	ep0_buf.flags = 0;
	for (i = 0; i <= NUM_ENDPOINT_NUMBERS; i++) {
#ifdef PPB_EPn
		ep_buf[i].flags = 0;
#else
		ep_buf[i].flags = EP_RX_DTS;
#endif
	}

	memset(bds, 0x0, sizeof(bds));

	/* Setup endpoint 0 Output buffer descriptor.
	   Input and output are from the HOST perspective. */
	BDS0OUT(0).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep0_buf.out);
	SET_BDN(BDS0OUT(0), BDNSTAT_UOWN, EP_0_LEN);

#ifdef PPB_EP0_OUT
	BDS0OUT(1).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep0_buf.out1);
	SET_BDN(BDS0OUT(1), BDNSTAT_UOWN, EP_0_LEN);
#endif

	/* Setup endpoint 0 Input buffer descriptor.
	   Input and output are from the HOST perspective. */
	BDS0IN(0).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep0_buf.in);
	SET_BDN(BDS0IN(0), 0, EP_0_LEN);
#ifdef PPB_EP0_IN
	BDS0IN(1).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep0_buf.in1);
	SET_BDN(BDS0IN(1), 0, EP_0_LEN);
#endif

	for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
		/* Setup endpoint 1 Output buffer descriptor.
		   Input and output are from the HOST perspective. */
		BDSnOUT(i,0).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep_buf[i].out);
		SET_BDN(BDSnOUT(i,0), BDNSTAT_UOWN|BDNSTAT_DTSEN, ep_buf[i].out_len);
#ifdef PPB_EPn
		/* Initialize EVEN buffers when in ping-pong mode. */
		BDSnOUT(i,1).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep_buf[i].out1);
		SET_BDN(BDSnOUT(i,1), BDNSTAT_UOWN|BDNSTAT_DTSEN|BDNSTAT_DTS, ep_buf[i].out_len);
#endif
		/* Setup endpoint 1 Input buffer descriptor.
		   Input and output are from the HOST perspective. */
		BDSnIN(i,0).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep_buf[i].in);
		SET_BDN(BDSnIN(i,0), 0, ep_buf[i].in_len);
#ifdef PPB_EPn
		/* Initialize EVEN buffers when in ping-pong mode. */
		BDSnIN(i,1).BDnADR = (BDNADR_TYPE) PHYS_ADDR(ep_buf[i].in1);
		SET_BDN(BDSnIN(i,1), 0, ep_buf[i].in_len);
#endif
	}
	
	#ifdef USB_NEEDS_POWER_ON
	SFR_USB_POWER = 1;
	#endif

#ifdef USE_OTG
	SFR_DPPULUP = 1;
#endif

	reset_ep0_data_stage();

#ifdef USB_USE_INTERRUPTS
	SFR_USB_IE = 1;     /* USB Interrupt enable */
#endif
	
	//UIRbits.URSTIF = 0; /* Clear USB Reset on Start */
}

static void reset_bd0_out(void)
{
	/* Clean up the Buffer Descriptors.
	 * Set the length and hand it back to the SIE.
	 * The Address stays the same. */
#ifdef PPB_EP0_OUT
	SET_BDN(BDS0OUT(SFR_USB_STATUS_PPBI), BDNSTAT_UOWN, EP_0_LEN);
#else
	SET_BDN(BDS0OUT(0), BDNSTAT_UOWN, EP_0_LEN);
#endif
}

static void stall_ep0(void)
{
	/* Stall Endpoint 0. It's important that DTSEN and DTS are zero. */
#ifdef PPB_EP0_IN
	uint8_t ppbi = (ep0_buf.flags & EP_TX_PPBI)? 1: 0;
	SET_BDN(BDS0IN(ppbi), BDNSTAT_UOWN|BDNSTAT_BSTALL, EP_0_LEN);
	/* The PPBI does not advance for STALL. */
#else
	SET_BDN(BDS0IN(0), BDNSTAT_UOWN|BDNSTAT_BSTALL, EP_0_LEN);
#endif
}

#ifdef NEEDS_CLEAR_STALL
static void clear_ep0_stall(void)
{
	/* Clear Endpoint 0 Stall and UOWN. This is supposed to be done by
	 * the hardware, but it isn't on PIC16 and PIC18. */
#ifdef PPB_EP0_IN
	uint8_t ppbi = (ep0_buf.flags & EP_TX_PPBI)? 1: 0;
	SET_BDN(BDS0IN(ppbi), 0, EP_0_LEN);
	/* The PPBI does not advance for STALL. */
#else
	SET_BDN(BDS0IN(0), 0, EP_0_LEN);
#endif
}
#endif

static void stall_ep_in(uint8_t ep)
{
	/* Stall Endpoint. It's important that DTSEN and DTS are zero.
	 * Although the datasheet doesn't stay it, the only safe way to do this
	 * is to set BSTALL on BOTH buffers when in ping-pong mode. */
	SET_BDN(BDSnIN(ep, 0), BDNSTAT_UOWN|BDNSTAT_BSTALL, ep_buf[ep].in_len);
#ifdef PPB_EPn
	SET_BDN(BDSnIN(ep, 1), BDNSTAT_UOWN|BDNSTAT_BSTALL, ep_buf[ep].in_len);
#endif
}

static void stall_ep_out(uint8_t ep)
{
	/* Stall Endpoint. It's important that DTSEN and DTS are zero.
	 * Although the datasheet doesn't stay it, the only safe way to do this
	 * is to set BSTALL on BOTH buffers when in ping-pong mode. */
	SET_BDN(BDSnOUT(ep, 0), BDNSTAT_UOWN|BDNSTAT_BSTALL , 0);
#ifdef PPB_EPn
	SET_BDN(BDSnOUT(ep, 1), BDNSTAT_UOWN|BDNSTAT_BSTALL , 0);
#endif
}

static void send_zero_length_packet_ep0()
{
#ifdef PPB_EP0_IN
	uint8_t ppbi = (ep0_buf.flags & EP_TX_PPBI)? 1: 0;
	BDS0IN(ppbi).STAT.BDnSTAT = 0;
	SET_BDN(BDS0IN(ppbi), BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 0);
	ep0_buf.flags ^= EP_TX_PPBI;
#else
	BDS0IN(0).STAT.BDnSTAT = 0;
	SET_BDN(BDS0IN(0), BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, 0);
#endif
}

static void usb_send_in_buffer_0(size_t len)
{
	if (!usb_in_endpoint_halted(0)) {
#ifdef PPB_EP0_IN
		struct buffer_descriptor *bd;
		uint8_t ppbi = (ep0_buf.flags & EP_TX_PPBI)? 1: 0;
		uint8_t pid = (ep0_buf.flags & EP_TX_DTS)? 1 : 0;
		bd = &BDS0IN(ppbi);
		bd->STAT.BDnSTAT = 0;

		if (pid)
			SET_BDN(*bd,
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, len);
		else
			SET_BDN(*bd,
				BDNSTAT_UOWN|BDNSTAT_DTSEN, len);

		ep0_buf.flags ^= EP_TX_PPBI;
		ep0_buf.flags ^= EP_TX_DTS;
#else
		uint8_t pid;
		pid = (ep0_buf.flags & EP_TX_DTS)? 1 : 0;
		BDS0IN(0).STAT.BDnSTAT = 0;

		if (pid)
			SET_BDN(BDS0IN(0),
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, len);
		else
			SET_BDN(BDS0IN(0),
				BDNSTAT_UOWN|BDNSTAT_DTSEN, len);

		ep0_buf.flags ^= EP_TX_DTS;
#endif
	}
}

/* Copy Data to Endpoint 0's IN Buffer
 *
 * Copy len bytes from ptr into endpoint 0's current IN
 * buffer, taking into account the ping-pong state.
 */
#ifdef PPB_EP0_IN
static void copy_to_ep0_in_buf(const void *ptr, size_t len)
{
	uint8_t ppbi = (ep0_buf.flags & EP_TX_PPBI)? 1: 0;
	if (ppbi)
		memcpy_from_rom(ep0_buf.in1, ptr, len);
	else
		memcpy_from_rom(ep0_buf.in, ptr, len);
}
#else
	#define copy_to_ep0_in_buf(PTR, LEN) memcpy_from_rom(ep0_buf.in, PTR, LEN);
#endif

/* Start Control Return
 *
 * Start the data stage of an IN control transfer. This is primarily used
 * for sending descriptors and other chapter 9 data back to the host, but it
 * is also called from usb_send_data_stage() for handling control transfers
 * handled by the application.
 *
 * This function sets up the global state variables necessary to do a
 * multi-transaction IN data stage and sends the first transaction.
 *
 * Params:
 *   ptr             - a pointer to the data to send
 *   len             - the size of the data which can be sent (ie: the size
 *                     of the entire descriptor)
 *   bytes_asked_for - the number of bytes asked for by the host in
 *                     the SETUP phase
 */
static void start_control_return(const void *ptr, size_t len, size_t bytes_asked_for)
{
	uint8_t bytes_to_send = MIN(len, EP_0_IN_LEN);
	bytes_to_send = MIN(bytes_to_send, bytes_asked_for);
	returning_short = len < bytes_asked_for;
	copy_to_ep0_in_buf(ptr, bytes_to_send);
	ep0_data_stage_in_buffer = ((char*)ptr) + bytes_to_send;
	ep0_data_stage_buf_remaining = MIN(bytes_asked_for, len) - bytes_to_send;

	/* Send back the first transaction */
	ep0_buf.flags |= EP_TX_DTS;
	usb_send_in_buffer_0(bytes_to_send);
}

static inline int8_t handle_standard_control_request()
{
	FAR struct setup_packet *setup;
	int8_t res = 0;

#ifdef PPB_EP0_OUT
	if (SFR_USB_STATUS_PPBI)
		setup = (struct setup_packet*) ep0_buf.out1;
	else
		setup = (struct setup_packet*) ep0_buf.out;
#else
	setup = (struct setup_packet*) ep0_buf.out;
#endif

	if (setup->bRequest == GET_DESCRIPTOR &&
	    setup->REQUEST.bmRequestType == 0x80 /* Section 9.4, Table 9-3 */) {
		char descriptor = ((setup->wValue >> 8) & 0x00ff);
		uint8_t descriptor_index = setup->wValue & 0x00ff;

		if (descriptor == DESC_DEVICE) {
			SERIAL("Get Descriptor for DEVICE");

			/* Return Device Descriptor */
			start_control_return(&USB_DEVICE_DESCRIPTOR, USB_DEVICE_DESCRIPTOR.bLength, setup->wLength);
		}
		else if (descriptor == DESC_CONFIGURATION) {
			const struct configuration_descriptor *desc;
			if (descriptor_index >= NUMBER_OF_CONFIGURATIONS)
				stall_ep0();
			else {
				desc = USB_CONFIG_DESCRIPTOR_MAP[descriptor_index];
				start_control_return(desc, desc->wTotalLength, setup->wLength);
			}
		}
		else if (descriptor == DESC_STRING) {
#ifdef MICROSOFT_OS_DESC_VENDOR_CODE
			if (descriptor_index == 0xee) {
				/* Microsoft descriptor Requested */
				#ifdef __XC8
				/* static is better in all cases on XC8. On
				 * XC16/32, non-static uses less RAM. */
				static
				#endif
				struct microsoft_os_descriptor os_descriptor =
				{
					0x12,                          /* bLength */
					0x3,                           /* bDescriptorType */
					{'M','S','F','T','1','0','0'}, /* qwSignature */
					MICROSOFT_OS_DESC_VENDOR_CODE, /* bMS_VendorCode */
					0x0,                           /* bPad */
				};

				start_control_return(&os_descriptor, sizeof(os_descriptor), setup->wLength);
			}
			else
#endif
			{
#ifdef USB_STRING_DESCRIPTOR_FUNC
				const void *desc;
				int16_t len;
				{
					len = USB_STRING_DESCRIPTOR_FUNC(descriptor_index, &desc);
					if (len < 0) {
						stall_ep0();
						SERIAL("Unsupported string descriptor requested");
					}
					else
						start_control_return(desc, len, setup->wLength);
				}
#else
				/* Strings are not supported on this device. */
				stall_ep0();
#endif
			}
		}
		else {
#ifdef UNKNOWN_GET_DESCRIPTOR_CALLBACK
			int16_t len;
			const void *desc;
			len = UNKNOWN_GET_DESCRIPTOR_CALLBACK(setup, &desc);
			if (len < 0) {
				stall_ep0();
				SERIAL("Unsupported descriptor requested");
			}
			else
				start_control_return(desc, len, setup->wLength);
#else
			/* Unknown Descriptor. Stall the endpoint. */
			stall_ep0();
			SERIAL("Unknown Descriptor");
			SERIAL_VAL(descriptor);
#endif
		}
	}
	else if (setup->bRequest == SET_ADDRESS) {
		/* Mark the ADDR as pending. The address gets set only
		   after the transaction is complete. */
		addr_pending = 1;
		addr = setup->wValue;

		send_zero_length_packet_ep0();
	}
	else if (setup->bRequest == SET_CONFIGURATION) {
		/* Set the configuration. wValue is the configuration.
		 * A value of 0 means to un-set the configuration and
		 * go back to the ADDRESS state. */
		uint8_t req = setup->wValue & 0x00ff;
#ifdef SET_CONFIGURATION_CALLBACK
		SET_CONFIGURATION_CALLBACK(req);
#endif
		send_zero_length_packet_ep0();
		g_configuration = req;

		SERIAL("Set configuration to");
		SERIAL_VAL(req);
	}
	else if (setup->bRequest == GET_CONFIGURATION) {
		/* Return the current Configuration. */
		SERIAL("Get Configuration. Returning:");
		SERIAL_VAL(g_configuration);

		start_control_return(&g_configuration, 1, setup->wLength);
	}
	else if (setup->bRequest == GET_STATUS) {

		SERIAL("Get Status (dst, index):");
		SERIAL_VAL(setup->REQUEST.destination);
		SERIAL_VAL(setup->wIndex);

		if (setup->REQUEST.destination == 0 /*0=device*/) {
			/* Status for the DEVICE requested
			   Return as a single byte in the return packet. */
			uint16_t ret;
#ifdef GET_DEVICE_STATUS_CALLBACK
			ret = GET_DEVICE_STATUS_CALLBACK();
#else
			ret = 0x0000;
#endif
			start_control_return(&ret, 2, setup->wLength);
		}
		else if (setup->REQUEST.destination == 2 /*2=endpoint*/) {
			/* Status of endpoint */
			uint8_t ep_num = setup->wIndex & 0x0f;
			if (ep_num <= NUM_ENDPOINT_NUMBERS) {
				uint8_t flags = ep_buf[ep_num].flags;
				uint8_t ret[2];
				ret[0] = ((setup->wIndex & 0x80) ?
					flags & EP_IN_HALT_FLAG :
					flags & EP_OUT_HALT_FLAG) != 0;
				ret[1] = 0;
				start_control_return(ret, 2, setup->wLength);
			}
			else {
				/* Endpoint doesn't exist. STALL. */
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
		/* Set the alternate setting for an interface.
		 * wIndex is the interface.
		 * wValue is the alternate setting. */
#ifdef SET_INTERFACE_CALLBACK
		int8_t res;
		res = SET_INTERFACE_CALLBACK(setup->wIndex, setup->wValue);
		if (res < 0) {
			stall_ep0();
		}
		else
			send_zero_length_packet_ep0();
#else
		/* If there's no callback, then assume that
		 * we only have one alternate setting per
		 * interface. */
		send_zero_length_packet_ep0();
#endif
	}
	else if (setup->bRequest == GET_INTERFACE) {
		int8_t ret;
		SERIAL("Get Interface");
		SERIAL_VAL(setup->bRequest);
		SERIAL_VAL(setup->REQUEST.destination);
		SERIAL_VAL(setup->REQUEST.type);
		SERIAL_VAL(setup->REQUEST.direction);
#ifdef GET_INTERFACE_CALLBACK
		ret = GET_INTERFACE_CALLBACK(setup->wIndex);
		if (ret < 0)
			stall_ep0();
		else {
			/* Return the current alternate setting
			   as a single byte in the return packet. */
			start_control_return(&ret, 1, setup->wLength);
		}
#else
		/* If there's no callback, then assume that
		 * we only have one alternate setting per
		 * interface and return zero as that
		 * alternate setting. */
		ret = 0;
		start_control_return(&ret, 1, setup->wLength);
#endif
	}
	else if (setup->bRequest == CLEAR_FEATURE || setup->bRequest == SET_FEATURE) {
		uint8_t stall = 1;
		if (setup->REQUEST.destination == 0/*0=device*/) {
			SERIAL("Set/Clear feature for device");
			/* TODO Remote Wakeup flag */
		}

		if (setup->REQUEST.destination == 2/*2=endpoint*/) {
			if (setup->wValue == 0/*0=ENDPOINT_HALT*/) {
				uint8_t ep_num = setup->wIndex & 0x0f;
				uint8_t ep_dir = setup->wIndex & 0x80;
				if (ep_num <= NUM_ENDPOINT_NUMBERS) {
					if (setup->bRequest == SET_FEATURE) {
						/* Set Endpoint Halt Feature.
						   Stall the affected endpoint. */
						if (ep_dir) {
							ep_buf[ep_num].flags |= EP_IN_HALT_FLAG;
							stall_ep_in(ep_num);
						}
						else {
							ep_buf[ep_num].flags |= EP_OUT_HALT_FLAG;
							stall_ep_out(ep_num);
						}
					}
					else {
						/* Clear Endpoint Halt Feature.
						   Clear the STALL on the affected endpoint. */
						if (ep_dir) {
#ifdef PPB_EPn
							SET_BDN(BDSnIN(ep_num, 0), 0, ep_buf[ep_num].in_len);
							SET_BDN(BDSnIN(ep_num, 1), 0, ep_buf[ep_num].in_len);
#else
							SET_BDN(BDSnIN(ep_num, 0), 0, ep_buf[ep_num].in_len);
#endif
							/* Clear DTS. Next packet to be sent will be DATA0. */
							ep_buf[ep_num].flags &= ~EP_TX_DTS;

							ep_buf[ep_num].flags &= ~(EP_IN_HALT_FLAG);
						}
						else {
#ifdef PPB_EPn
							uint8_t ppbi = (ep_buf[ep_num].flags & EP_RX_PPBI)? 1 : 0;
							/* Put the current buffer at DTS 0, and the next (opposite) buffer at DTS 1 */
							SET_BDN(BDSnOUT(ep_num, ppbi), BDNSTAT_UOWN|BDNSTAT_DTSEN, ep_buf[ep_num].out_len);
							SET_BDN(BDSnOUT(ep_num, !ppbi), BDNSTAT_UOWN|BDNSTAT_DTSEN|BDNSTAT_DTS, ep_buf[ep_num].out_len);

							/* Clear DTS */
							ep_buf[ep_num].flags &= ~EP_RX_DTS;
#else
							SET_BDN(BDSnOUT(ep_num, 0), BDNSTAT_UOWN|BDNSTAT_DTSEN, ep_buf[ep_num].out_len);

							/* Set DTS */
							ep_buf[ep_num].flags |= EP_RX_DTS;
#endif
							ep_buf[ep_num].flags &= ~(EP_OUT_HALT_FLAG);
						}
					}
#ifdef ENDPOINT_HALT_CALLBACK
					ENDPOINT_HALT_CALLBACK(setup->wIndex, (setup->bRequest == SET_FEATURE));
#endif
					stall = 0;
				}
			}
		}

		if (!stall) {
			send_zero_length_packet_ep0();
		}
		else
			stall_ep0();
	}
	else {
		res = -1;

		SERIAL("unsupported request (req, dest, type, dir) ");
		SERIAL_VAL(setup->bRequest);
		SERIAL_VAL(setup->REQUEST.destination);
		SERIAL_VAL(setup->REQUEST.type);
		SERIAL_VAL(setup->REQUEST.direction);
	}

	return res;
}

static inline void handle_ep0_setup()
{
	FAR struct setup_packet *setup;
#ifdef PPB_EP0_OUT
	if (SFR_USB_STATUS_PPBI)
		setup = (struct setup_packet*) ep0_buf.out1;
	else
		setup = (struct setup_packet*) ep0_buf.out;
#else
	setup = (struct setup_packet*) ep0_buf.out;
#endif

	ep0_data_stage_direc = setup->REQUEST.direction;
	int8_t res;

#ifdef NEEDS_CLEAR_STALL
	/* The datasheets say the MCU will clear BSTALL and UOWN when
	 * a SETUP packet is received. This does not seem to happen on
	 * PIC16 or PIC18, so clear the stall explicitly. */
	clear_ep0_stall();
#endif
	if (ep0_data_stage_buf_remaining) {
		/* A SETUP transaction has been received while waiting
		 * for a DATA stage to complete; something is broken.
		 * If this was an application-controlled transfer (and
		 * there's a callback), notify the application of this. */
		if (ep0_data_stage_callback)
			ep0_data_stage_callback(0/*fail*/, ep0_data_stage_context);

		reset_ep0_data_stage();
	}

	if (setup->REQUEST.type == REQUEST_TYPE_STANDARD) {
		res = handle_standard_control_request();
		if (res < 0)
			goto handle_unknown;
	}
#ifdef MICROSOFT_OS_DESC_VENDOR_CODE
	else if (setup->bRequest == MICROSOFT_OS_DESC_VENDOR_CODE) {
		const void *desc;
		int16_t len = -1;

		if (setup->REQUEST.bmRequestType == 0xC0 &&
		    setup->wIndex == 0x0004) {
			len = MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC(
				setup->wValue,
				&desc);
		}
		else if (setup->REQUEST.bmRequestType == 0xC1 &&
		         setup->wIndex == 0x0005) {
			len = MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC(
				setup->wValue,
				&desc);
		}

		if (len < 0)
			stall_ep0();
		else
			start_control_return(desc, len, setup->wLength);
	}
#endif
	else
		goto handle_unknown;

	goto out;

handle_unknown:

#ifdef UNKNOWN_SETUP_REQUEST_CALLBACK
	res = UNKNOWN_SETUP_REQUEST_CALLBACK(setup);
	if (res < 0)
		stall_ep0();
	else {
		/* If the application has handled this request, it
		 * will have already set up whatever needs to be set
		 * up for the data stage. */
	}
#else
	/* Unsupported Request. Stall the Endpoint. */
	stall_ep0();
#endif

out:
	/* SETUP packet sets PKTDIS which disables
	 * future SETUP packet reception. Turn it off
	 * afer we've processed the current SETUP
	 * packet to avoid a race condition. */
	SFR_USB_PKT_DIS = 0;
}

static inline void handle_ep0_out()
{
#ifdef PPB_EP0_OUT
	uint8_t pkt_len = BDN_LENGTH(BDS0OUT(SFR_USB_STATUS_PPBI));
#else
	uint8_t pkt_len = BDN_LENGTH(BDS0OUT(0));
#endif
	if (ep0_data_stage_direc == 1/*1=IN*/) {
		/* An empty OUT packet on an IN control transfer
		 * means the STATUS stage of the control
		 * transfer has completed (possibly early). */

		/* Notify the application (if applicable) */
		if (ep0_data_stage_callback)
			ep0_data_stage_callback(1/*true*/, ep0_data_stage_context);
		reset_ep0_data_stage();
	}
	else {
		/* A packet received as part of the data stage of
		 * a control transfer. Pack what data we received
		 * into the application's buffer (if it has
		 * provided one). When all the data has been
		 * received, call the application-provided callback.
		 */

		if (ep0_data_stage_out_buffer) {
			uint8_t bytes_to_copy = MIN(pkt_len, ep0_data_stage_buf_remaining);
#ifdef PPB_EP0_OUT
			if (SFR_USB_STATUS_PPBI)
				memcpy(ep0_data_stage_out_buffer, ep0_buf.out1, bytes_to_copy);
			else
				memcpy(ep0_data_stage_out_buffer, ep0_buf.out, bytes_to_copy);
#else
			memcpy(ep0_data_stage_out_buffer, ep0_buf.out, bytes_to_copy);
#endif
			ep0_data_stage_out_buffer += bytes_to_copy;
			ep0_data_stage_buf_remaining -= bytes_to_copy;

			/* It's possible that bytes_to_copy is less than pkt_len
			 * here because the application provided too small a buffer. */

			if (pkt_len < EP_0_OUT_LEN || ep0_data_stage_buf_remaining == 0) {
				/* Short packet or we've received the expected length.
				 * All data has been transferred (or all the data
				 * has been received which can be received). */

				if (bytes_to_copy < pkt_len) {
					/* The buffer provided by the application was too short */
					stall_ep0();
					if (ep0_data_stage_callback)
						ep0_data_stage_callback(0/*false*/, ep0_data_stage_context);
					reset_ep0_data_stage();
				}
				else {
					/* The data stage has completed. Set up the status stage. */
					send_zero_length_packet_ep0();
				}
			}
		}
	}
}

static inline void handle_ep0_in()
{
	if (addr_pending) {
		SFR_USB_ADDR =  addr;
		addr_pending = 0;
	}

	if (ep0_data_stage_buf_remaining) {
		/* There's already a multi-transaction transfer in process. */
		uint8_t bytes_to_send = MIN(ep0_data_stage_buf_remaining, EP_0_IN_LEN);

		copy_to_ep0_in_buf(ep0_data_stage_in_buffer, bytes_to_send);
		ep0_data_stage_buf_remaining -= bytes_to_send;
		ep0_data_stage_in_buffer += bytes_to_send;

		/* If we hit the end with a full-length packet, set up
		   to send a zero-length packet at the next IN token, but only
		   if we are returning less data than was requested. */
		if (ep0_data_stage_buf_remaining == 0 &&
		    bytes_to_send == EP_0_IN_LEN &&
		    returning_short)
			control_need_zlp = 1;

		usb_send_in_buffer_0(bytes_to_send);
	}
	else if (control_need_zlp) {
		usb_send_in_buffer_0(0);
		control_need_zlp = 0;
		reset_ep0_data_stage();
	}
	else {
		if (ep0_data_stage_direc == 0/*OUT*/) {
			/* An IN on the control endpoint with no data pending
			 * and during an OUT transfer means the STATUS stage
			 * of the control transfer has completed. Notify the
			 * application, if applicable. */
			if (ep0_data_stage_callback)
				ep0_data_stage_callback(1/*true*/, ep0_data_stage_context);
			reset_ep0_data_stage();
		}
	}
}

/* checkUSB() is called repeatedly to check for USB interrupts
   and service USB requests */
void usb_service(void)
{
	if (SFR_USB_RESET_IF) {
		/* A Reset was detected on the wire. Re-init the SIE. */
#ifdef USB_RESET_CALLBACK
		USB_RESET_CALLBACK();
#endif
		usb_init();
		CLEAR_USB_RESET_IF();
		SERIAL("USB Reset");
	}
	
	if (SFR_USB_STALL_IF) {
		/* On PIC24/32, EPSTALL bits must be cleared, or else the
		 * stalled endpoint's opposite direction (eg: EP1 IN => EP1
		 * OUT) will also stall (incorrectly). There is no way to
		 * determine which endpoint generated this interrupt, so all
		 * the endpoints' EPSTALL bits must be checked and cleared. */
		int i;
		for (i = 1; i <= NUM_ENDPOINT_NUMBERS; i++) {
			volatile SFR_EP_MGMT_TYPE *ep = SFR_EP_MGMT(i);
			ep->SFR_EP_MGMT_STALL = 0;
		}

		CLEAR_USB_STALL_IF();
	}


	if (SFR_USB_TOKEN_IF) {

		//struct ustat_bits ustat = *((struct ustat_bits*)&USTAT);

		if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 0/*OUT*/) {
			/* An OUT or SETUP transaction has completed on
			 * Endpoint 0.  Handle the data that was received.
			 */
#ifdef PPB_EP0_OUT
			uint8_t pid = BDS0OUT(SFR_USB_STATUS_PPBI).STAT.PID;
#else
			uint8_t pid = BDS0OUT(0).STAT.PID;
#endif
			if (pid == PID_SETUP) {
				handle_ep0_setup();
			}
			else if (pid == PID_IN) {
				/* Nonsense condition:
				   (PID IN on SFR_USB_STATUS_DIR == OUT) */
			}
			else if (pid == PID_OUT) {
				handle_ep0_out();
			}
			else {
				/* Unsupported PID. Stall the Endpoint. */
				SERIAL("Unsupported PID. Stall.");
				stall_ep0();
			}

			reset_bd0_out();
		}
		else if (SFR_USB_STATUS_EP == 0 && SFR_USB_STATUS_DIR == 1/*1=IN*/) {
			/* An IN transaction has completed. The endpoint
			 * needs to be re-loaded with the next transaction's
			 * data if there is any.
			 */
			handle_ep0_in();
		}
		else if (SFR_USB_STATUS_EP > 0 && SFR_USB_STATUS_EP <= NUM_ENDPOINT_NUMBERS) {
			if (SFR_USB_STATUS_DIR == 1 /*1=IN*/) {
				/* An IN transaction has completed. */
				SERIAL("IN transaction completed on non-EP0.");
				if (ep_buf[SFR_USB_STATUS_EP].flags & EP_IN_HALT_FLAG)
					stall_ep_in(SFR_USB_STATUS_EP);
				else {
#ifdef IN_TRANSACTION_COMPLETE_CALLBACK
					IN_TRANSACTION_COMPLETE_CALLBACK(SFR_USB_STATUS_EP);
#endif
				}
			}
			else {
				/* An OUT transaction has completed. */
				SERIAL("OUT transaction received on non-EP0");
				if (ep_buf[SFR_USB_STATUS_EP].flags & EP_OUT_HALT_FLAG)
					stall_ep_out(SFR_USB_STATUS_EP);
				else {
#ifdef OUT_TRANSACTION_CALLBACK
					OUT_TRANSACTION_CALLBACK(SFR_USB_STATUS_EP);
#endif
				}
			}
		}
		else {
			/* Transaction completed on an endpoint not used.
			 * This should never happen. */
			SERIAL("Transaction completed for unknown endpoint");
		}

		CLEAR_USB_TOKEN_IF();
	}
	
	/* Check for Start-of-Frame interrupt. */
	if (SFR_USB_SOF_IF) {
#ifdef START_OF_FRAME_CALLBACK
		START_OF_FRAME_CALLBACK();
#endif
		CLEAR_USB_SOF_IF();
	}

	/* Check for USB Interrupt. */
	if (SFR_USB_IF) {
		SFR_USB_IF = 0;
	}
}

uint8_t usb_get_configuration(void)
{
	return g_configuration;
}

unsigned char *usb_get_in_buffer(uint8_t endpoint)
{
#ifdef PPB_EPn
	if (ep_buf[endpoint].flags & EP_TX_PPBI /*odd*/)
		return ep_buf[endpoint].in1;
	else
		return ep_buf[endpoint].in;
#else
	return ep_buf[endpoint].in;
#endif
}

void usb_send_in_buffer(uint8_t endpoint, size_t len)
{
#ifdef DEBUG
	if (endpoint == 0)
		error();
#endif
	if (g_configuration > 0 && !usb_in_endpoint_halted(endpoint)) {
		uint8_t pid;
		struct buffer_descriptor *bd;
#ifdef PPB_EPn
		uint8_t ppbi = (ep_buf[endpoint].flags & EP_TX_PPBI)? 1 : 0;

		bd = &BDSnIN(endpoint,ppbi);
		pid = (ep_buf[endpoint].flags & EP_TX_DTS)? 1 : 0;
		bd->STAT.BDnSTAT = 0;

		if (pid)
			SET_BDN(BDSnIN(endpoint,ppbi),
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, len);
		else
			SET_BDN(BDSnIN(endpoint,ppbi),
				BDNSTAT_UOWN|BDNSTAT_DTSEN, len);

		ep_buf[endpoint].flags ^= EP_TX_PPBI;
		ep_buf[endpoint].flags ^= EP_TX_DTS;
#else
		bd = &BDSnIN(endpoint,0);
		pid = (ep_buf[endpoint].flags & EP_TX_DTS)? 1 : 0;
		bd->STAT.BDnSTAT = 0;

		if (pid)
			SET_BDN(*bd,
				BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN, len);
		else
			SET_BDN(*bd,
				BDNSTAT_UOWN|BDNSTAT_DTSEN, len);

		ep_buf[endpoint].flags ^= EP_TX_DTS;
#endif
	}
}

bool usb_in_endpoint_busy(uint8_t endpoint)
{
#ifdef PPB_EPn
	uint8_t ppbi = (ep_buf[endpoint].flags & EP_TX_PPBI)? 1: 0;
	return BDSnIN(endpoint, ppbi).STAT.UOWN;
#else
	return BDSnIN(endpoint,0).STAT.UOWN;
#endif
}

bool usb_in_endpoint_halted(uint8_t endpoint)
{
	return ep_buf[endpoint].flags & EP_IN_HALT_FLAG;
}

uint8_t usb_get_out_buffer(uint8_t endpoint, const unsigned char **buf)
{
#ifdef PPB_EPn
	uint8_t ppbi = (ep_buf[endpoint].flags & EP_RX_PPBI)? 1: 0;

	if (ppbi /*odd*/)
		*buf = ep_buf[endpoint].out1;
	else
		*buf = ep_buf[endpoint].out;

	return BDN_LENGTH(BDSnOUT(endpoint, ppbi));
#else
	*buf = ep_buf[endpoint].out;
	return BDN_LENGTH(BDSnOUT(endpoint, 0));
#endif
}

bool usb_out_endpoint_has_data(uint8_t endpoint)
{
#ifdef PPB_EPn
	uint8_t ppbi = (ep_buf[endpoint].flags & EP_RX_PPBI)? 1: 0;
	return !BDSnOUT(endpoint,ppbi).STAT.UOWN;
#else
	return !BDSnOUT(endpoint,0).STAT.UOWN;
#endif
}

void usb_arm_out_endpoint(uint8_t endpoint)
{
#ifdef PPB_EPn
	uint8_t ppbi = (ep_buf[endpoint].flags & EP_RX_PPBI)? 1: 0;
	uint8_t pid = (ep_buf[endpoint].flags & EP_RX_DTS)? 1: 0;

	if (pid)
		SET_BDN(BDSnOUT(endpoint,ppbi),
			BDNSTAT_UOWN|BDNSTAT_DTSEN|BDNSTAT_DTS,
			ep_buf[endpoint].out_len);
	else
		SET_BDN(BDSnOUT(endpoint,ppbi),
			BDNSTAT_UOWN|BDNSTAT_DTSEN,
			ep_buf[endpoint].out_len);

	/* Alternate the PPBI */
	ep_buf[endpoint].flags ^= EP_RX_PPBI;
	ep_buf[endpoint].flags ^= EP_RX_DTS;

#else
	uint8_t pid = (ep_buf[endpoint].flags & EP_RX_DTS)? 1: 0;
	if (pid)
		SET_BDN(BDSnOUT(endpoint,0),
			BDNSTAT_UOWN|BDNSTAT_DTS|BDNSTAT_DTSEN,
			ep_buf[endpoint].out_len);
	else
		SET_BDN(BDSnOUT(endpoint,0),
			BDNSTAT_UOWN|BDNSTAT_DTSEN,
			ep_buf[endpoint].out_len);

	ep_buf[endpoint].flags ^= EP_RX_DTS;
#endif

}

bool usb_out_endpoint_halted(uint8_t endpoint)
{
	return ep_buf[endpoint].flags & EP_OUT_HALT_FLAG;
}

void usb_start_receive_ep0_data_stage(char *buffer, size_t len,
                                      usb_ep0_data_stage_callback callback, void *context)
{
	reset_ep0_data_stage();

	ep0_data_stage_callback = callback;
	ep0_data_stage_out_buffer = buffer;
	ep0_data_stage_buf_remaining = len;
	ep0_data_stage_context = context;
}

void usb_send_data_stage(char *buffer, size_t len,
	usb_ep0_data_stage_callback callback, void *context)
{
	/* Start sending the first block. Subsequent blocks will be sent
	   when IN tokens are received on endpoint zero. */
	ep0_data_stage_callback = callback;
	ep0_data_stage_context = context;
	start_control_return(buffer, len, len);
}



#ifdef USB_USE_INTERRUPTS
#ifdef __XC16__

void _ISR __attribute((auto_psv)) _USB1Interrupt()
{
	usb_service();
}

#elif __XC32__

/* No parameter for interrupt() means to use IPL=RIPL and to detect whether
   to use shadow registers or not. This is the safest option, but if a user
   wanted maximum performance, they could use IPL7SRS and set the USBIP to 7.
   IPL 7 is the only time the shadow register set can be used on PIC32MX. */
void __attribute__((vector(_USB_1_VECTOR), interrupt(), nomips16)) _USB1Interrupt()
{
	usb_service();
}

#elif __C18
#elif __XC8
	/* On these systems, interupt handlers are shared. An interrupt
	 * handler from the application must call usb_service(). */
#else
#error Compiler not supported yet
#endif
#endif