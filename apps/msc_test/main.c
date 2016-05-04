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
#include "hardware.h"


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
	bool cancel_multiblock_write;
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

/* The buffer used for both transmitting and receiving data. Reads will use
 * the entire buffer, and writes will only use WRITE_BUF_SIZE bytes (which
 * is defined below). Thus MMC_BLOCK_SIZE must be >= WRITE_BUF_SIZE. */
static uint8_t mmc_read_buf[MMC_BLOCK_SIZE];

/* Define MULTI_BLOCK_WRITE to use the multi-block write API of the MMC
 * layer. In practice there is no reason to _not_ use multi-block writes. */
#define MULTI_BLOCK_WRITE

/* Setting the write buffer to something smaller than one block will allow the
 * USB peripheral to be working at the same time as the main CPU thread is
 * writing to the device. Setting it to the endpoint size will trigger a write
 * operation to the MMC card each time a data transaction is received from
 * the host (allowing the MSC stack to begin receiving the next packet during
 * the write).
 *
 * In this implementation, since the code in this main.c file uses
 * mmc_read_buf for both reading and writing, this buffer size must be smaller
 * than or equal to MMC_BLOCK_SIZE. It also must be at least as large as the
 * OUT endpoint size. It must also be divisible by the OUT endpoint size. */
#ifdef MULTI_BLOCK_WRITE
	#define WRITE_BUF_SIZE 64
#else
	#define WRITE_BUF_SIZE MMC_BLOCK_SIZE
#endif

#define CONCAT3(x, y, z) x ## y ## z
#define CONCAT(x, y, z) CONCAT3(x, y, z)

/* Make sure the write buffer is an appropriate size */
#if WRITE_BUF_SIZE > MMC_BLOCK_SIZE
	#error "WRITE_BUF_SIZE must be <= MMC_BLOCK_SIZE"
#elif WRITE_BUF_SIZE < CONCAT(EP_, APP_MSC_OUT_ENDPOINT, _OUT_LEN)
	#error WRITE_BUF_SIZE must be >= the OUT endpoint size
#elif WRITE_BUF_SIZE % CONCAT(EP_, APP_MSC_OUT_ENDPOINT, _OUT_LEN) != 0
	#error WRITE_BUF_SIZE must be divisible by the OUT endpoint size
#endif
#if !defined(MULTI_BLOCK_WRITE) && WRITE_BUF_SIZE != MMC_BLOCK_SIZE
	#error WRITE_BUF_SIZE must be set to MMC_BLOCK_SIZE if MULTI_BLOCK_WRITE is not set.
#endif

#undef CONCAT3
#undef CONCAT

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

#ifdef MULTI_BLOCK_WRITE
	if (msc_rw_data.cancel_multiblock_write) {
		/* The transport has been canceled from the USB side either by
		 * a USB reset or an MSC Reset Recovery. It's safe to call
		 * msc_multiblock_write_cancel() even if there is not a write
		 * in progress. */
		mmc_multiblock_write_cancel(&mmc);
		msc_rw_data.write_operation_needed = false;
		msc_rw_data.num_blocks = 0;
		msc_rw_data.bytes_handled = 0;
		msc_rw_data.cancel_multiblock_write = false;

		return 0;
	}

	if (msc_rw_data.bytes_handled == 0) {
		/* Write is requested, but hasn't started yet.
		 * Start the write operation. */
		res = mmc_multiblock_write_start(&mmc, msc_rw_data.lba_address);
		if (res < 0)
			goto fail;
	}

	/* Give the data to the MMC card */
	res = mmc_multiblock_write_data(&mmc, mmc_read_buf, WRITE_BUF_SIZE);
	if (res < 0)
		goto fail;

	/* Set the write_operation_needed flag false before the call to
	 * msc_notify_write_data_handled() because
	 * msc_notify_write_data_handled() _might_ call the
	 * rx_complete_callback() which would set write_operation_needed
	 * back to true if another transaction is already waiting (which is
	 * a likely case). */
	msc_rw_data.write_operation_needed = false;

	/* Tell the MSC stack that the data was processed */
	msc_notify_write_data_handled(msc);

	msc_rw_data.bytes_handled += WRITE_BUF_SIZE;

	if (msc_rw_data.bytes_handled ==
	    (uint32_t) msc_rw_data.num_blocks * MMC_BLOCK_SIZE) {
		/* All the expected data has been received. Finish
		 * the write operation. */
		res = mmc_multiblock_write_end(&mmc);
		if (res < 0)
			goto fail;

		/* Tell the MSC stack that the write operation has completed */
		msc_notify_write_operation_complete(msc,
		                            true, msc_rw_data.bytes_handled);
	}

	return res;
fail:
	/* Report failure to the MSC layer. Ideally we could report the
	 * number of bytes that were successfully written (in the case that
	 * they were, but that isn't supported by the MMC layer at the
	 * current time. Reporting zero bytes written is safe, as the host
	 * will then try to write all the blocks again. */
	msc_notify_write_operation_complete(msc, false, 0/*TODO*/);
	msc_rw_data.write_operation_needed = false;

	return res;
#else
	/* Perform the blocking, single-block write */
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
#endif
}
#endif

int main(void)
{
	hardware_init();

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

			/* Handle reading/writing the MMC card if necessary.
			 * Since reading/writing is a blocking operation,
			 * it is handled here in the main thread. The read
			 * is initiated (from interrupt context) in
			 * app_msc_start_read(). The write is initiated
			 * (from interrupt context) in app_msc_start_write().*/

			if (msc_reset_required) {
				/* Make sure to cancel any multi-block
				 * write operations in progress. */
				msc_rw_data.write_operation_needed = true;
				msc_rw_data.cancel_multiblock_write = true;

				/* do_write() will now reset the MMC card to a
				 * known state */
				do_write(&msc_data, &msc_rw_data);

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
	*buffer_len = WRITE_BUF_SIZE;
	*callback = rx_complete_callback;

	return MSC_SUCCESS;
}

/* MMC implementation callbacks. These just glue the MMC implementation
 * to the SPI implementation and the timer implementation. */

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
