/*
 * USB Mass Storage Class Demo
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
 * 2014-06-04
 */

#include "usb.h"
#include <xc.h>
#include <string.h>
#include "usb_config.h"
#include "usb_ch9.h"
#include "usb_msc.h"

#include "mmc.h"

#include "spi.h"
#include "timer.h"

#ifdef __PIC32MX__
	#include <plib.h>
#endif

#ifdef __PIC24FJ64GB002__
_CONFIG1(WDTPS_PS16 & FWPSA_PR32 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx1 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & I2C1SEL_PRI & IOL1WAY_OFF & OSCIOFNC_OFF & FCKSM_CSDCMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP0 & SOSCSEL_IO & WUTSEL_LEG & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)
_CONFIG4(DSWDTPS_DSWDTPS3 & DSWDTOSC_SOSC & RTCOSC_SOSC & DSBOREN_OFF & DSWDTEN_OFF)

#elif __PIC24FJ256DA206__
_CONFIG1(WDTPS_PS32768 & FWPSA_PR128 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx2 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & IOL1WAY_OFF & OSCIOFNC_ON & FCKSM_CSECMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP255 & SOSCSEL_SOSC & WUTSEL_LEG & ALTPMP_ALPMPDIS & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)

#elif _18F46J50
#pragma config PLLDIV = 3 /* 3 = Divide by 3. 12MHz crystal => 4MHz */
#pragma config XINST = OFF
#pragma config WDTEN = OFF
#pragma config CPUDIV = OSC1
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config LPT1OSC = OFF
#pragma config T1DIG = ON
#pragma config OSC = ECPLL
#pragma config DSWDTEN = OFF
#pragma config IOL1WAY = OFF
#pragma config WPDIS = OFF /* This pragma seems backwards */

#elif _16F1459
#pragma config FOSC = INTOSC
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config MCLRE = ON
#pragma config CP = OFF
#pragma config BOREN = ON
#pragma config CLKOUTEN = OFF
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config WRT = OFF
#pragma config CPUDIV = NOCLKDIV
#pragma config USBLSCLK = 48MHz
#pragma config PLLMULT = 3x
#pragma config PLLEN = ENABLED
#pragma config STVREN = ON
#pragma config BORV = LO
#pragma config LPBOR = ON
#pragma config LVP = OFF

#elif __32MX460F512L__
#pragma config DEBUG = OFF, ICESEL = ICS_PGx2, PWP = OFF, BWP = OFF, CP = OFF
#pragma config FNOSC = PRIPLL, FSOSCEN = OFF, IESO = OFF, POSCMOD = HS, \
	       OSCIOFNC = OFF, FPBDIV = DIV_1, FCKSM = CSDCMD, WDTPS = PS1, \
	       FWDTEN = OFF
#pragma config FPLLIDIV = DIV_2, FPLLMUL = MUL_15, UPLLIDIV = DIV_2, \
	       UPLLEN = ON, FPLLODIV = DIV_1

#elif __32MX795F512L__
// DEVCFG3
// USERID = No Setting
#pragma config FSRSSEL = PRIORITY_7     // SRS Select (SRS Priority 7)
#pragma config FMIIEN = ON              // Ethernet RMII/MII Enable (MII Enabled)
#pragma config FETHIO = ON              // Ethernet I/O Pin Select (Default Ethernet I/O)
#pragma config FCANIO = ON              // CAN I/O Pin Select (Default CAN I/O)
#pragma config FUSBIDIO = ON            // USB USID Selection (Controlled by the USB Module)
#pragma config FVBUSONIO = ON           // USB VBUS ON Selection (Controlled by USB Module)

// DEVCFG2
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider (2x Divider)
#pragma config FPLLMUL = MUL_15         // PLL Multiplier (15x Multiplier)
#pragma config UPLLIDIV = DIV_2         // USB PLL Input Divider (2x Divider)
#pragma config UPLLEN = ON              // USB PLL Enable (Enabled)
#pragma config FPLLODIV = DIV_1         // System PLL Output Clock Divider (PLL Divide by 1)

// DEVCFG1
#pragma config FNOSC = PRIPLL           // Oscillator Selection Bits (Primary Osc (XT,HS,EC), PLL)
#pragma config FSOSCEN = OFF            // Secondary Oscillator Enable (Disabled)
#pragma config IESO = OFF               // Internal/External Switch Over (Disabled)
#pragma config POSCMOD = HS             // Primary Oscillator Configuration (HS osc mode)
#pragma config OSCIOFNC = OFF           // CLKO Output Signal Active on the OSCO Pin (Disabled)
#pragma config FPBDIV = DIV_1           // Peripheral Clock Divisor (Pb_Clk is Sys_Clk/1)
#pragma config FCKSM = CSDCMD           // Clock Switching and Monitor Selection (Clock Switch Disable, FSCM Disabled)
#pragma config WDTPS = PS1048576        // Watchdog Timer Postscaler (1:1048576)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable (WDT Disabled (SWDTEN Bit Controls))

// DEVCFG0
#pragma config DEBUG = OFF           // Background Debugger Enable (Debugger is disabled)
#pragma config ICESEL = ICS_PGx2        // ICE/ICD Comm Channel Select (ICE EMUC2/EMUD2 pins shared with PGC2/PGD2)
#pragma config PWP = OFF                // Program Flash Write Protect (Disable)
#pragma config BWP = OFF                // Boot Flash Write Protect bit (Protection Disabled)
#pragma config CP = OFF                 // Code Protect (Protection Disabled)

#else
	#error "Config flags for your device not defined"

#endif

#ifdef __XC8
#error "Due to some issues with the XC8 compiler, it is not currently supported for this project."
#error "When errors in the compiler are fixed, PIC18 will be supported."
#endif

#ifdef MULTI_CLASS_DEVICE
static uint8_t msc_interfaces[] = { 0 };
#endif

/* Data for each instance of the USB Mass Storage class. This needs to be made
 * global (or have a lifetime of the entire time the MSC interface is active).
 * This object will be used as a handle to the MSC instance. If this is a
 * multi-MSC-interface composite device, make this an array. */
static struct msc_application_data msc_data;

/* Data for each instance of the MMC interface. This needs to be made
 * global (or have a lifetime of the entire time the MMC interface is active).
 * If there are more than one MMC interface, make this an array (as mmc_init()
 * can take an array and length). */
static struct mmc_card mmc;

/* Data to manage the reading of data from the SD card as requested by the
 * host. One of these structures is necessary for each MSC interface, so make
 * an array of these if this is a multi-MSC-interface composite device. */
struct msc_rw_data {
	bool read_operation_needed;
	bool write_operation_needed;
	uint8_t lun;
	uint32_t lba_address;
	uint16_t num_blocks;
	bool stopped;
	uint32_t bytes_handled;
};
struct msc_rw_data msc_rw_data;

/* This flag is set when a USB protocol reset is initiated by the host,
 * requiring the MSC class to be reset */
static bool msc_reset_required;

static uint8_t mmc_read_buf[MMC_BLOCK_SIZE];

/* Transmission complete callback. This is called when an entire block has
 * been transfered to the host. */
static void tx_complete_callback(struct msc_application_data *app_data,
                                         bool transfer_ok)
{
	/* Transmission to the host has completed. Set up to read and send the
	 * next block */
	msc_rw_data.lba_address++;
	msc_rw_data.num_blocks--;
	msc_rw_data.read_operation_needed = true;
}

/* Read a block from the MMC card and initiate transferring to the host. The
 * call to mmc_read_block() will block, so don't call this from interrupt
 * context. This should only be called when d->read_operation_needed is
 * true. */
static int8_t do_read(struct msc_application_data *msc,
                      struct msc_rw_data *d)
{
	int8_t res = 0;

	if (d->num_blocks > 0) {
		/* There are more blocks to read and send, so read and
		 * send the next one. */
		msc_rw_data.read_operation_needed = false;

		res = mmc_read_block(&mmc, d->lba_address, mmc_read_buf);
		if (res < 0)
			goto fail;

		res = msc_start_send_to_host(msc,
		                            mmc_read_buf, MMC_BLOCK_SIZE,
		                            &tx_complete_callback);
		if (res < 0)
			goto fail;
	}
	else {
		/* No more blocks to read. */
		msc_rw_data.read_operation_needed = false;
		msc_notify_read_operation_complete(msc, true);
	}

	return 0;

fail:
	msc_notify_read_operation_complete(msc, false);
	return -1;
}

/* Receive complete callback. This is called when an entire block has
 * been received from the host which needs to be written to the media. */
static void rx_complete_callback(struct msc_application_data *app_data,
                                         bool transfer_ok)
{
	if (!transfer_ok)
		return;

	msc_rw_data.write_operation_needed = true;
}

#ifdef MSC_WRITE_SUPPORT
static int8_t do_write(struct msc_application_data *msc,
                       struct msc_rw_data *d)
{
	int8_t res = 0;

	/* Perform the blocking write */
	res = mmc_write_block(&mmc, msc_rw_data.lba_address, mmc_read_buf);
	msc_rw_data.write_operation_needed = false;

	/* Increment the LBA address for the next write. Since the USB
	 * host will concatenate the data for all the blocks, the LBA
	 * address has to be kept track of in the application. */
	msc_rw_data.lba_address++;
	msc_rw_data.num_blocks--;

	/* Notify the MSC stack that the write has completed */
	msc_notify_write_data_handled(msc);

	/* Fail the operation early if the write failed */
	if (res < 0) {
		msc_notify_write_operation_complete(
		                       msc, false, msc_rw_data.bytes_handled);
		goto fail;
	}

	msc_rw_data.bytes_handled += MMC_BLOCK_SIZE;

	/* Report successful transport if this was the last block */
	if (msc_rw_data.num_blocks == 0)
		msc_notify_write_operation_complete(
		                       msc, true, msc_rw_data.bytes_handled);

fail:
	return res;
}
#endif

int main(void)
{
#if defined(__PIC24FJ64GB002__) || defined(__PIC24FJ256DA206__)
	unsigned int pll_startup_counter = 600;
	CLKDIVbits.PLLEN = 1;
	while(pll_startup_counter--);
#elif _18F46J50
	unsigned int pll_startup = 600;
	OSCTUNEbits.PLLEN = 1;
	while (pll_startup--)
		;
#elif _16F1459
	OSCCONbits.IRCF = 0b1111; /* 0b1111 = 16MHz HFINTOSC postscalar */

	/* Enable Active clock-tuning from the USB */
	ACTCONbits.ACTSRC = 1; /* 1=USB */
	ACTCONbits.ACTEN = 1;
#elif __32MX460F512L__
	SYSTEMConfigPerformance(80000000);
#elif __32MX795F512L__
	SYSTEMConfigPerformance(60000000);
#endif


/* Configure interrupts, per architecture */
#ifdef USB_USE_INTERRUPTS
	#if defined (_PIC18) || defined(_PIC14E)
		INTCONbits.PEIE = 1;
		INTCONbits.GIE = 1;
	#elif __PIC32MX__
		INTCONbits.MVEC = 1; /* Multi-vector interrupts */
		IPC11bits.USBIP = 4; /* Interrupt priority, must set to != 0. */
		__asm volatile("ei");
	#endif
#endif

#ifdef PIC18F_STARTER_KIT
	/* Turn on the APP_VDD power bus on the PIC18F starter kit. This turns
	 * on the MMC card socket, among other things (OLED, BMA150, etc). The
	 * documentation talks about inrush current, and the MLA provides code
	 * to gradually ramp up the voltage on APP_VDD to mitigate this, but
	 * in my testing, the MLA's ramp-up code doesn't do anything
	 * significantly different to the current consumption graph than just
	 * turning the voltage on, so that's what we do here.
	 *
	 * The whole board seems to draw < 100mA with the oLED and the BMA150
	 * untouched but the MMC used. */
	TRISCbits.TRISC0 = 0; /* Output */
	LATCbits.LATC0 = 0;
#endif

	/* Initialize the MMC card interface */
	spi_init();

	mmc.max_speed_hz = 50000000;
	mmc.spi_instance = 0;

	int8_t res = mmc_init(&mmc, 1);
	if (res < 0) {
		return 1;
	}

	res = mmc_init_card(&mmc);
	if (res < 0) {
		/* Failure is ok here. There could be no card present, but
		 * one could be inserted into the drive later. */
	}

#ifdef MULTI_CLASS_DEVICE
	msc_set_interface_list(msc_interfaces, sizeof(msc_interfaces));
#endif
	/* Initialize the MSC data for each interface. Make sure these
	 * match the values in the device descriptor. */
	msc_data.interface = APP_MSC_INTERFACE;
	msc_data.max_lun = 0;
	msc_data.in_endpoint = APP_MSC_IN_ENDPOINT;
	msc_data.out_endpoint = APP_MSC_OUT_ENDPOINT;
	msc_data.in_endpoint_size = EP_1_IN_LEN;
	msc_data.media_is_removable_mask = (1 << 0); /* One bit per LUN */
	msc_data.vendor = "Signal11"; /* Get a vendor ID from http://www.t10.org/lists/2vid.htm */
	msc_data.product = "TEST";
	msc_data.revision = "0001";

	res = msc_init(&msc_data, 1);
	if (res < 0)
		return 1;

	usb_init();

	unsigned char *buf = usb_get_in_buffer(1);
	memset(buf, 0xa0, EP_1_IN_LEN);

	while (1) {
		if (usb_is_configured()) {

			/* Handle reading from the MMC card if necessary.
			 * Since reading from the MMC is a blocking operation,
			 * is handled here in the main thread. The read
			 * is initiated (from interrupt context) in
			 * app_msc_start_read(). */

			if (msc_reset_required) {
				/* Reset the MSC. */
				msc_init(&msc_data, 1);
				msc_reset_required = false;
			}

			if (msc_rw_data.read_operation_needed) {
				do_read(&msc_data, &msc_rw_data);
                        }
#ifdef MSC_WRITE_SUPPORT
			if (msc_rw_data.write_operation_needed) {
				do_write(&msc_data, &msc_rw_data);
			}
#endif
                }

		#ifndef USB_USE_INTERRUPTS
		usb_service();
		#endif
	}

	return 0;
}

/* Callbacks. These function names are set in usb_config.h. */
void app_set_configuration_callback(uint8_t configuration)
{

}
uint16_t app_get_device_status_callback()
{
	return 0x0000;
}

void app_endpoint_halt_callback(uint8_t endpoint, bool halted)
{
	if (!halted) {
		/* Clear halt. Stalling is a standard part of the MSC
		 * class protocol. */
		msc_clear_halt(endpoint & 0x7f, (endpoint & 0x80) != 0);
	}
}

int8_t app_set_interface_callback(uint8_t interface, uint8_t alt_setting)
{
	return 0;
}

int8_t app_get_interface_callback(uint8_t interface)
{
	return 0;
}

void app_out_transaction_callback(uint8_t endpoint)
{
	if (endpoint == APP_MSC_OUT_ENDPOINT)
		msc_out_transaction_complete(endpoint);
}

void app_in_transaction_complete_callback(uint8_t endpoint)
{
	if (endpoint == APP_MSC_IN_ENDPOINT)
		msc_in_transaction_complete(endpoint);
}

int8_t app_unknown_setup_request_callback(const struct setup_packet *setup)
{
	/* To use the Mass Storage device class, have a handler for unknown
	 * setup requests and call process_msc_setup_request() (as shown here),
	 * which will check if the setup request is MSC-related, and will
	 * call the MSC application callbacks defined in usb_msc.h. For
	 * composite devices containing other device classes, make sure
	 * MULTI_CLASS_DEVICE is defined in usb_config.h and call all
	 * appropriate device class setup request functions here.
	 */

	return process_msc_setup_request(setup);
}

int16_t app_unknown_get_descriptor_callback(const struct setup_packet *pkt, const void **descriptor)
{
	return -1;
}

void app_start_of_frame_callback(void)
{

}

void app_usb_reset_callback(void)
{
	/* Reset the MSC class */
	msc_reset_required = true;
}

/* MSC class callbacks */

int8_t app_msc_reset(uint8_t interface)
{
	/* RESET control transfer. In our case it's the same
	 * as a USB reset. */
	app_usb_reset_callback();

	return 0;
}

int8_t app_get_storage_info(const struct msc_application_data *app_data,
                            uint8_t lun,
                            uint32_t *block_size,
                            uint32_t *num_blocks,
                            bool *write_protect)
{
	if (lun > 0)
		return MSC_ERROR_INVALID_LUN;

	if (!SPI_MMC_CARD_PRESENT())
		return MSC_ERROR_MEDIUM_NOT_PRESENT;

	*block_size = MMC_BLOCK_SIZE;
	*num_blocks = mmc_get_num_blocks(&mmc);
#ifdef MSC_WRITE_SUPPORT
	/* Write protection switch status can be read from MMC card sockets
	 * which support it, usually as an I/O line. It cannot be read from
	 * the MMC card's SPI interface. */
	*write_protect = false;
#else
	/* With a read-only MSC implementation, write-protect is always on. */
	*write_protect = true;
#endif

	return MSC_SUCCESS;
}

int8_t app_get_unit_ready(const struct msc_application_data *app_data,
                        uint8_t lun)
{
	/* Detect and return whether the MMC card is present and ready. In
	 * the case of this example, detecting whether the card is simply
	 * present (from the hard switch) is good enough. If there is no hard
	 * switch on the MMC card socket, then calling mmc_ready() may be a
	 * good substitute.
	 *
	 * Of course having this function return true doesn't mean that a
	 * subsequent read or write operation will succeed, since the card
	 * could be pulled between the call to this function and that one.
	 */

	/* Check that the LUN is within range. In most cases there will only
	 * be one LUN. */
	if (lun > 0)
		return MSC_ERROR_INVALID_LUN;

	if (msc_rw_data.stopped && SPI_MMC_CARD_PRESENT())
		return MSC_ERROR_MEDIUM_NOT_PRESENT;
	else if (msc_rw_data.stopped) {
		/* Unit is stopped but there is now no media. Clear
		 * the stopped flag so that when new media is inserted, it
		 * will be read. */
		msc_rw_data.stopped = false;
	}

	if (!SPI_MMC_CARD_PRESENT()) {
		/* The card is not present, so notify the MMC system that
		 * the card is no longer initialized. */
		mmc_set_uninitialized(&mmc);
		return MSC_ERROR_MEDIUM_NOT_PRESENT;
	}
	else if (!mmc_is_initialized(&mmc)) {
		/* Card is present, but has not been initialized. */
		int8_t res = mmc_init_card(&mmc);
		if (res < 0)
			return MSC_ERROR_MEDIUM;
	}
	return MSC_SUCCESS;
}

int8_t app_start_stop_unit(const struct msc_application_data *app_data,
			   uint8_t lun, bool start, bool load_eject)
{
	/* Check that the LUN is within range. In most cases there will only
	 * be one LUN. */
	if (lun > 0)
		return MSC_ERROR_INVALID_LUN;

	if (!start) {
		/* Stop the unit */
		mmc_set_uninitialized(&mmc);
		msc_rw_data.stopped = true;
	}
	else {
		/* Start the unit */
		if (SPI_MMC_CARD_PRESENT() && !mmc_is_initialized(&mmc)) {
			int8_t res = mmc_init_card(&mmc);
			if (res < 0)
				return MSC_ERROR_MEDIUM;
		}
		msc_rw_data.stopped = false;
	}

	/* load_eject is not handled here because the software can't
	 * physically eject an MMC card. */

	return MSC_SUCCESS;
}

/* This is called when a READ command is received from the host. All it does
 * is set flags which will initiate a read operation from the storage medium
 * which will happen in the main thread of execution, since this function
 * is called from interrupt context.
 *
 * This function is called from interrupt context and must not block.
 */
int8_t app_msc_start_read(struct msc_application_data *app_data, uint8_t lun,
                  uint32_t lba_address, uint16_t num_blocks)
{
	/* If a reset is in progress, don't allow any reads to start. */
	if (msc_reset_required)
		return MSC_ERROR_MEDIUM_NOT_PRESENT;

	if (lun > 0)
		return MSC_ERROR_INVALID_LUN;

	if (lba_address + num_blocks > mmc_get_num_blocks(&mmc))
		return MSC_ERROR_INVALID_ADDRESS;

	msc_rw_data.lun = lun;
	msc_rw_data.lba_address = lba_address;
	msc_rw_data.num_blocks = num_blocks;

	msc_rw_data.read_operation_needed = true;

	return MSC_SUCCESS;
}

int8_t app_msc_start_write(
		struct msc_application_data *app_data,
		uint8_t lun, uint32_t lba_address, uint16_t num_blocks,
		uint8_t **buffer, size_t *buffer_len,
		msc_completion_callback *callback)
{
	/* If a reset is in progress, don't allow any writes to start. */
	if (msc_reset_required)
		return MSC_ERROR_MEDIUM_NOT_PRESENT;

	if (lun > 0)
		return MSC_ERROR_INVALID_LUN;

	if (lba_address + num_blocks > mmc_get_num_blocks(&mmc))
		return MSC_ERROR_INVALID_ADDRESS;

	msc_rw_data.lba_address = lba_address;
	msc_rw_data.num_blocks = num_blocks;
	msc_rw_data.bytes_handled = 0;
	*buffer = mmc_read_buf;
	*buffer_len = MMC_BLOCK_SIZE;
	*callback = rx_complete_callback;

	return MSC_SUCCESS;
}

/* MMC implementation callbacks. These just glue the MMC implementation
 * to the SPI implementation. */

void app_spi_transfer(uint8_t instance,
                      const uint8_t *out_buf,
                      uint8_t *in_buf,
                      uint16_t len)
{
	/* Ignore instance since we only have one MMC card connected. */
	spi_transfer_block(out_buf, in_buf, len);
}

void app_spi_set_cs(uint8_t instance, uint8_t value)
{
	/* Ignore instance since we only have one MMC card connected. */
	spi_set_cs_line(value);
}

void app_spi_set_speed(uint8_t instance, uint32_t speed_hz)
{
	/* Ignore instance since we only have one MMC card connected. */
	spi_set_speed_hz(speed_hz);
}

void app_timer_start(uint8_t instance, uint16_t milliseconds)
{
	/* Ignore instance since we only have one MMC card connected. */
	timer_start(milliseconds);
}

bool app_timer_expired(uint8_t instance)
{
	/* Ignore instance since we only have one MMC card connected. */
	return timer_expired();
}

void app_timer_stop(uint8_t instance)
{
	/* Ignore instance since we only have one MMC card connected. */
	timer_stop();
}

#ifdef _PIC14E
void interrupt isr()
{
	usb_service();
}
#elif _PIC18

#ifdef __XC8
void interrupt high_priority isr()
{
	usb_service();
}
#elif _PICC18
#error need to make ISR
#endif

#endif
