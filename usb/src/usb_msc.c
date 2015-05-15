/*
 *  M-Stack USB Device Stack Implementation
 *  Copyright (C) 2014 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2014 Signal 11 Software
 *
 *  2014-06-04
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

#include <usb_config.h>

#include <usb_ch9.h>
#include <usb.h>
#include <usb_msc.h>
#include "usb_priv.h"

#include <string.h>

#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_command_block_wrapper), 31);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_command_status_wrapper), 13);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_scsi_inquiry_command), 6);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_scsi_request_sense_command), 6);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_scsi_mode_sense_6_command), 6);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_scsi_start_stop_unit), 6);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_scsi_read_10_command), 10);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct msc_scsi_write_10_command), 10);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct scsi_inquiry_response), 36);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct scsi_capacity_response), 8);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct scsi_mode_sense_response), 4);
STATIC_SIZE_CHECK_EQUAL(sizeof(struct scsi_sense_response), 18);


#if MSC_MAX_LUNS_PER_INTERFACE > 0x10
	#error Too many LUNs per interface. Max is 16 LUNs per interface
#endif

#if MSC_MAX_LUNS_PER_INTERFACE == 0
	#error At least one LUN must be supported.
#endif

static struct msc_application_data *g_application_data;
#ifdef MSC_SUPPORT_MULTIPLE_MSC_INTERFACES
static uint8_t g_application_data_count;
#endif

static inline void swap(uint8_t *v1, uint8_t *v2)
{
	uint8_t tmp;
	tmp = *v1;
	*v1 = *v2;
	*v2 = tmp;
}

static void swap4(uint32_t *val)
{
	union swapper {
		uint32_t val;
		uint8_t v_bytes[4];
	};

	union swapper *sw = (union swapper *) val;
	swap(&sw->v_bytes[0], &sw->v_bytes[3]);
	swap(&sw->v_bytes[1], &sw->v_bytes[2]);
}

static void swap2(uint16_t *val)
{
	union swapper {
		uint16_t val;
		uint8_t v_bytes[2];
	};

	union swapper *sw = (union swapper *) val;
	swap(&sw->v_bytes[0], &sw->v_bytes[1]);
}

static bool direction_is_in(uint8_t flags)
{
	return flags & MSC_DIRECTION_IN_BIT;
}

static bool direction_is_out(uint8_t flags)
{
	return ~flags & MSC_DIRECTION_IN_BIT;
}

#ifdef MSC_SUPPORT_MULTIPLE_MSC_INTERFACES
/* Lookup applicaiton data by interface number. */
static struct msc_application_data *get_app_data(uint8_t interface)
{
	uint8_t i;

	for (i = 0; i < g_application_data_count; i++) {
		struct msc_application_data *d = &g_application_data[i];
		if (d->interface == interface) {
			return d;
		}
	}

	return NULL;
}

/* Lookup application data by endpoint. */
static struct msc_application_data *get_app_data_by_endpoint(
					uint8_t endpoint_num, uint8_t direction)
{
	uint8_t i;

	for (i = 0; i < g_application_data_count; i++) {
		struct msc_application_data *d = &g_application_data[i];
		/* The if-else inside the loop is slower than doing it
		 * outside the loop, but it's less code, and since this
		 * doesn't happen very often (just in the clear_halt case
		 * which only happens on error), this is an acceptable
		 * tradeoff. */
		if (direction && d->in_endpoint == endpoint_num) {
			return d;
		}
		else if (d->out_endpoint == endpoint_num) {
			return d;
		}
	}

	return NULL;
}
#else
/* Lookup applicaiton data by interface number. */
static inline struct msc_application_data *get_app_data(uint8_t interface)
{
	if (interface == g_application_data[0].interface)
		return g_application_data;

	return NULL;
}

/* Lookup application data by endpoint. */
static inline struct msc_application_data *get_app_data_by_endpoint(
					uint8_t endpoint_num, uint8_t direction)
{
	if (direction && g_application_data[0].in_endpoint == endpoint_num) {
		return g_application_data;
	}
	else if (g_application_data[0].out_endpoint == endpoint_num) {
		return g_application_data;
	}

	return NULL;
}
#endif

/* Stall the IN endpoint and set the status which will be returned by the
 * next CSW. */
static void stall_in_and_set_status(struct msc_application_data *msc,
                                    uint32_t residue, uint8_t status)
{
	msc->residue = residue;
	msc->status = status;
	usb_halt_ep_in(msc->in_endpoint);
	msc->state = MSC_CSW;
}

/* Stall the OUT endpoint and set the status which will be returned by the
 * next CSW. */
static void stall_out_and_set_status(struct msc_application_data *msc,
                                     uint32_t residue, uint8_t status)
{
	msc->residue = residue;
	msc->status = status;
	usb_halt_ep_out(msc->out_endpoint);
	msc->state = MSC_CSW;
}

/* Send a Command Status Word (CSW) */
static int8_t send_csw(struct msc_application_data *msc,
                       uint32_t residue, uint8_t status)
{
	struct msc_command_status_wrapper *csw;

	/* Make sure endpoint is free */
	if (usb_in_endpoint_busy(msc->in_endpoint))
		return -1;

	csw = (struct msc_command_status_wrapper *)
			usb_get_in_buffer(msc->in_endpoint);
	csw->dCSWSignature = 0x53425355;
	csw->dCSWTag = msc->current_tag;
	csw->dCSWDataResidue = residue;
	csw->bCSWStatus = status;

	usb_send_in_buffer(msc->in_endpoint, sizeof(*csw));

	/* Reset states and status */
	msc->state = MSC_IDLE;
	msc->status = MSC_STATUS_PASSED;
	msc->residue = 0;

	return 0;
}

static void set_scsi_sense(struct msc_application_data *msc,
                           enum MSCReturnCodes code)
{
	if (code == MSC_ERROR_MEDIUM_NOT_PRESENT) {
		msc->sense_key = SCSI_SENSE_KEY_NOT_READY;
		msc->additional_sense_code = SCSI_ASC_MEDIUM_NOT_PRESENT;
	}
	else if (code == MSC_ERROR_INVALID_LUN) {
		msc->sense_key = SCSI_SENSE_KEY_ILLEGAL_REQUEST;
		msc->additional_sense_code =
			SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED;
	}
	else if (code == MSC_ERROR_INVALID_ADDRESS) {
		msc->sense_key = SCSI_SENSE_KEY_ILLEGAL_REQUEST;
		msc->additional_sense_code =
			SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
	}
	else if (code == MSC_ERROR_WRITE_PROTECTED) {
		msc->sense_key = SCSI_SENSE_KEY_DATA_PROTECT;
		msc->additional_sense_code = SCSI_ASC_WRITE_PROTECTED;
	}
	else if (code == MSC_ERROR_READ) {
		msc->sense_key = SCSI_SENSE_KEY_MEDIUM_ERROR;
		msc->additional_sense_code = SCSI_ASC_UNRECOVERED_READ_ERROR;
	}
	else if (code == MSC_ERROR_WRITE) {
		msc->sense_key = SCSI_SENSE_KEY_MEDIUM_ERROR;
		msc->additional_sense_code =
			SCSI_ASC_PERIPHERAL_DEVICE_WRITE_FAULT;
	}
	else if (code == MSC_ERROR_MEDIUM) {
		msc->sense_key = SCSI_SENSE_KEY_MEDIUM_ERROR;
		msc->additional_sense_code = 0;
	}
}

/* Whether the CBW is valid and meaningful, per the MSC BOT spec */
static bool msc_cbw_valid_and_meaningful(struct msc_application_data *msc,
                                         const uint8_t *data, uint16_t len)
{
	const struct msc_command_block_wrapper *cbw = (const void*) data;

	/* Test whether the CBW isvalid. Conforms to BOT: 6.2.1 */

	if (len != sizeof(struct msc_command_block_wrapper))
		return false;

	if (cbw->dCBWSignature != 0x43425355)
		return false;

	/* Test whether the CBW is meaningful. Conforms to BOT: 6.2.2 */

	if (msc->state != MSC_IDLE)
		return false;

	if (cbw->bCBWCBLength < 1 || cbw->bCBWCBLength > 16)
		return false;

	if (cbw->bmCBWFlags & 0x7F)
		return false;

	if (cbw->bCBWLUN > msc->max_lun ||
	    cbw->bCBWLUN >= MSC_MAX_LUNS_PER_INTERFACE)
		return false;

	return true;
}

static void stall_in_and_phase_error(struct msc_application_data *msc)
{
	stall_in_and_set_status(msc, 0, MSC_STATUS_PHASE_ERROR);
}

static void stall_out_and_phase_error(struct msc_application_data *msc)
{
	stall_out_and_set_status(msc, 0, MSC_STATUS_PHASE_ERROR);
}

static void phase_error(struct msc_application_data *msc)
{
	send_csw(msc, 0, MSC_STATUS_PHASE_ERROR);
}

/** Set the state of the Data-IN endpoint after sending data to the host
 *
 * Set the state of the Data-In endpoint based on the amount of data requested
 * by the host in the CBW packet (cbw_length) and the amount of data actually
 * sent (sent_length).
 *
 * If the cbw_length is more than the sent_length, the endpoint must be
 * stalled (BOT: 6.7.2, Device Requirements, case 5).
 *
 * If the requested length and the number of bytes sent match (the most common
 * case, Case 6), the EP does not have to be stalled and a CSW can be sent.
 *
 * This function should be called after data has been sent to the host for
 * short transports where the device expects to send data (Di) (such as
 * INQUIRY, REQUEST_SENSE, etc.). It is not used for READ, which has a
 * different mechanism for handling status.
 */
static void set_data_in_endpoint_state(struct msc_application_data *msc,
                              uint32_t cbw_length,
                              size_t sent_length)
{
	msc->status = MSC_STATUS_PASSED;

	if (cbw_length > sent_length) {
		/* Case 5 (Hi > Di): Stall the IN EP and set residue */
		msc->residue = cbw_length - sent_length;
		msc->state = MSC_STALL;
	}
	else {
		/* Case 6 (Hi = Di): OK. Host will want the CSW next */
		msc->residue = 0;
		msc->state = MSC_CSW;
	}
}

/* Check the Di cases: 2, 5, 6, 7, and 10 (BOT section 6.7). Case 6 is
 * correct, and 5 is acceptable with residue. 2, 7, and 10 result in a phase
 * error with an endpoint (IN or OUT) potentially stalled.
 *
 * This function returns 0 if case 5 or 6, or -1 if case 2, 7, or 10.
 *
 * The calling function should:
 *   for return value 0:
 *     proceed with returning data to the host, properly handling residue,
 *   for return value -1:
 *     do nothing further in response to the received command.
 */
static int8_t check_di_cases(struct msc_application_data *msc,
                             const struct msc_command_block_wrapper *cbw,
                             uint32_t intended_length)
{
	const uint32_t cbw_length = cbw->dCBWDataTransferLength;
	const bool direc_is_out = direction_is_out(cbw->bmCBWFlags);

	/* Case 2 (Hn < Di): set phase error (no stall) */
	if (cbw_length == 0) {
		phase_error(msc);
		return -1;
	}

	/* Case 7 (Hi < Di): stall IN and phase error */
	if (cbw_length < intended_length) {
		stall_in_and_phase_error(msc);
		return -1;
	}

	/* Case 10 (Ho <> Di): stall OUT and phase error */
	if (direc_is_out) {
		stall_out_and_phase_error(msc);
		return -1;
	}

	/* Case 5 (Hi > Di): OK, with residue.
	 * Case 6 (Hi = Di): OK */
	return 0;
}

/* Check the Dn cases: 1, 4, and 9 (BOT section 6.7). Case 1 is correct, and
 * cases 4 and 9 will result in a stall of an appropriate endpoint.
 *
 * This function returns 0 if case 1 or returns -1 for cases 4 and 9.
 *
 * The calling function should:
 *   for return value 0:
 *     proceed to the CSW state (no data needs to be returned or handled),
 *   for return value -1:
 *     do nothing further in response to the received command.
 *
 */
static int8_t check_dn_cases(struct msc_application_data *msc,
                             const struct msc_command_block_wrapper *cbw)
{
	const uint32_t cbw_length = cbw->dCBWDataTransferLength;
	const bool direc_is_out = direction_is_out(cbw->bmCBWFlags);

	/* Case 9 (Ho > Dn): stall OUT and set status FAILED */
	if (direc_is_out && cbw_length > 0) {
		stall_out_and_set_status(msc, cbw_length, MSC_STATUS_FAILED);
		return -1;
	}

	/* Case 4 (Hi > Dn): Stall IN and set status FAILED. */
	if (cbw_length > 0) {
		stall_in_and_set_status(msc, cbw_length, MSC_STATUS_FAILED);
		return -1;
	}

	/* Case 1 (Hn = Dn): OK */
	return 0;
}

#ifdef MSC_WRITE_SUPPORT
/* Check the Do cases: 3, 8, 11, 12, and 13 (BOT section 6.7). Case 12 is
 * correct. Other cases stall appropriate endpoints.
 *
 * This function returns 0 for case 12 or returns -1 for all other cases.
 *
 * The calling function should:
 *   for return value 0:
 *     proceed with receiving data from the host,
 *   for return value -1:
 *     do nothing further in response to the received command.
 */
static int8_t check_do_cases(struct msc_application_data *msc,
                             const struct msc_command_block_wrapper *cbw,
                             uint32_t intended_length)
{
	const uint32_t cbw_length = cbw->dCBWDataTransferLength;
	const bool direc_is_in = direction_is_in(cbw->bmCBWFlags);

	/* Case 3 (Hn < Do): set phase error (no stall) */
	if (cbw_length == 0) {
		phase_error(msc);
		return -1;
	}

	/* Case 8 (Hi <> Do): stall IN and phase error */
	if (direc_is_in) {
		stall_in_and_phase_error(msc);
		return -1;
	}

	/* Case 13 (Ho < Do): stall OUT and phase error */
	if (cbw_length < intended_length) {
		stall_out_and_phase_error(msc);
		return -1;
	}

	/* Case 11 (Ho > Do): Stall OUT and set status to FAILED
	 *
	 * Note that BOT 6.7.3:device:case_11 says that the device shall
	 * receive the intended data and then stall the bulk-OUT endpoint.
	 * BOT does not define "intended data," but appears to use it
	 * to mean the amount of data that is specified (in our case) in the
	 * SCSI packet. This is the definition used most often in this
	 * implementation of the Mass Storage class, but in this case, for
	 * simplicity, the device will "intend" to receve 0 bytes if the CBW
	 * length and the SCSI length don't match.
	 *
	 * This means instead of setting up to receive intended_length bytes
	 * and then stalling once that amount of data has been received and
	 * then dealing with returning residue, the OUT endpoint will simply
	 * be stalled here.
	 *
	 * This passes the USBCV Case 11 test. */
	if (cbw_length > intended_length) {
		stall_out_and_set_status(msc, cbw_length, MSC_STATUS_FAILED);
		return -1;
	}

	/* Case 12 (Ho = Do): OK */
	return 0;
}
#endif

#ifdef MULTI_CLASS_DEVICE
static uint8_t *msc_interfaces;
static uint8_t num_msc_interfaces;

void msc_set_interface_list(uint8_t *interfaces, uint8_t num_interfaces)
{
	msc_interfaces = interfaces;
	num_msc_interfaces = num_interfaces;
}

static bool interface_is_msc(uint8_t interface)
{
	uint8_t i;
	for (i = 0; i < num_msc_interfaces; i++) {
		if (interface == msc_interfaces[i])
			break;
	}

	/* Return if interface is not in the list of CDC interfaces. */
	if (i == num_msc_interfaces)
		return false;

	return true;
}
#endif

uint8_t msc_init(struct msc_application_data *app_data, uint8_t count)
{
	uint8_t i;

#ifndef MSC_SUPPORT_MULTIPLE_MSC_INTERFACES
	if (count > 1)
		return -1;
#endif

	for (i = 0; i < count; i++) {
		struct msc_application_data *d = &app_data[i];

		/* Validate struct members. */
		if (d->max_lun > 0x0f)
			return -1;
		if (d->max_lun >= MSC_MAX_LUNS_PER_INTERFACE)
			return -1;
		if (d->in_endpoint_size != 64)
			return -1;
		if (d->in_endpoint > 15)
			return -1;
		if (d->out_endpoint > 15)
			return -1;

		/* Initialize the MSC-Class-Controlled members.*/
		d->state = MSC_IDLE;
		d->sense_key = 0;
		d->additional_sense_code = 0;
		d->residue = 0;
		d->status = 0;
		d->requested_bytes = 0;
		d->transferred_bytes = 0;
		d->tx_buf = NULL;
		d->tx_len_remaining = 0;
#ifdef MSC_WRITE_SUPPORT
		d->rx_buf_cur = NULL;
		d->rx_buf_len = 0;
		d->out_ep_missed_transactions = 0;
#endif
		d->operation_complete_callback = NULL;
		memset(d->block_size, 0, sizeof(d->block_size));
	}

	g_application_data = app_data;
#ifdef MSC_SUPPORT_MULTIPLE_MSC_INTERFACES
	g_application_data_count = count;
#endif

	return 0;
}

int8_t process_msc_setup_request(const struct setup_packet *setup)
{
	uint8_t interface = setup->wIndex;
	struct msc_application_data *msc;


#ifdef MULTI_CLASS_DEVICE
	/* Check the interface first to make sure the destination is an
	 * MSC interface. Multi-class devices will need to call
	 * msc_set_interface_list() first.
	 */
	if (!interface_is_msc(interface))
		return -1;
#endif

	/* Get application data for this interface */
	msc = get_app_data(interface);
	if (!msc)
		return -1;

	/* BOT 3.2 says the GET_MAX_LUN request is optional if there's only one
	 * LUN, but stalling this request will cause Windows 7 to hang for 18
	 * seconds when a device is first connected. */
	if (setup->bRequest == MSC_GET_MAX_LUN &&
	    setup->REQUEST.bmRequestType == 0xa1) {

		/* Stall invalid value/length */
		if (setup->wValue != 0 ||
		    setup->wLength != 1)
			return -1;

		usb_send_data_stage((void*)&msc->max_lun,
		                    1, NULL, 0);
		return 0;
	}

	if (setup->bRequest == MSC_BULK_ONLY_MASS_STORAGE_RESET &&
	    setup->REQUEST.bmRequestType == 0x21) {
		struct msc_application_data *d = get_app_data(interface);

		/* Stall invalid value/length */
		if (setup->wValue != 0 ||
		    setup->wLength != 0)
			return -1;

		if (d)
			d->state = MSC_IDLE;
		else
			return -1;

#ifdef MSC_BULK_ONLY_MASS_STORAGE_RESET_CALLBACK
		int8_t res = 0;

		res = MSC_BULK_ONLY_MASS_STORAGE_RESET_CALLBACK(interface);
		if (res < 0)
			return -1;
#endif
		/* Clear the NEEDS_RESET_RECOVERY state */
		msc->state = MSC_IDLE;

		/* Return zero-length packet. No data stage. */
		usb_send_data_stage(NULL, 0, NULL, NULL);

		return 0;
	}
	return -1;
}

#ifdef MSC_WRITE_SUPPORT
/* Copy data to the application's buffer. If the application's buffer
 * is full, return -1.
 *
 * It will be common for this function to be called when the buffer is full,
 * since writing to the medium is slower than USB, and since the host relies
 * on the device to throttle the connection as appropriate.
 *
 * If -1 is returned from this function, it will be up to the caller to make
 * sure that this function is re-called later, when there is buffer space
 * available. */
static inline uint8_t receive_data(struct msc_application_data *msc,
                                       const uint8_t *data, uint16_t len)
{
	/* Make sure this doesn't take us off the end of the
	 * application's buffer. */
	if (msc->rx_buf_cur + len > msc->rx_buf + msc->rx_buf_len)
		return -1;

	/* Ignore this data if it is more than was expected. This indicates
	 * an error on the host side, but it can be handled here by just
	 * ignoring the extra data. The difference will be reflected in the
	 * residue in the CSW. */
	if (msc->transferred_bytes >= msc->requested_bytes)
		return 0;

	/* Copy to the application's buffer. */
	memcpy(msc->rx_buf_cur, data, len);
	msc->rx_buf_cur += len;

	/* If this is the last piece of the data block, notify the
	 * application that it's buffer is now full and that it can start
	 * writing to the medium.  */
	if (msc->rx_buf_cur >= msc->rx_buf + msc->rx_buf_len) {
		msc->operation_complete_callback(msc, true);
	}

	return 0;
}
#endif

/* Send the next transaction containing data from the medium to the host. */
static int8_t send_next_data_transaction(struct msc_application_data *msc)
{
	const uint8_t *cur = msc->tx_buf;
	const uint16_t len = msc->tx_len_remaining;

	if (!usb_is_configured() || usb_in_endpoint_busy(msc->in_endpoint))
		return -1;

	if (len > 0) {
		/* There is data to send; send one packet worth. */
		uint8_t *buf;
		uint16_t to_copy;

		buf = usb_get_in_buffer(msc->in_endpoint);
		to_copy = MIN(len, msc->in_endpoint_size);
		memcpy(buf, cur, to_copy);

		usb_send_in_buffer(msc->in_endpoint, to_copy);

		msc->transferred_bytes += to_copy;
		msc->tx_buf += to_copy;
		msc->tx_len_remaining -= to_copy;
	}
	else {
		/* Transfer of block has completed */
		msc->operation_complete_callback(msc, true);
		msc->operation_complete_callback = NULL;
		msc->tx_buf = NULL;
	}

	return 0;
}

uint8_t msc_start_send_to_host(struct msc_application_data *msc,
                               const uint8_t *data, uint16_t len,
                               msc_completion_callback completion_callback)
{
	int8_t res;

	usb_disable_transaction_interrupt();

	if (msc->state != MSC_DATA_TRANSPORT_IN || len == 0) {
		res = -1;
		goto out;
	}

	msc->tx_buf = data;
	msc->tx_len_remaining = len;
	msc->operation_complete_callback = completion_callback;

	/* Kick off the transmission. */
	res = send_next_data_transaction(msc);

out:
	usb_enable_transaction_interrupt();
	return res;
}

void msc_notify_read_operation_complete(
                                   struct msc_application_data *app_data,
                                   bool passed)
{
	usb_disable_transaction_interrupt();

	if (app_data->state != MSC_DATA_TRANSPORT_IN) {
		goto out;
	}

	uint32_t residue = app_data->requested_bytes_cbw -
	                           app_data->transferred_bytes;

	if (!passed) {
		/* Save off the error codes. These will be read by the host
		 * with a REQUEST_SENSE SCSI command. */
		set_scsi_sense(app_data, MSC_ERROR_READ);
	}

	uint8_t status = passed? MSC_STATUS_PASSED: MSC_STATUS_FAILED;

	if (residue > 0) {
		stall_in_and_set_status(app_data, residue, status);
	}
	else {
		send_csw(app_data, residue, status);
		app_data->state = MSC_IDLE;
	}

out:
	usb_enable_transaction_interrupt();
}

#ifdef MSC_WRITE_SUPPORT
/* Must be called with the transaction interrupt disabled */
static void handle_missed_out_transactions(struct msc_application_data *msc)
{
	/* Handle any pending data.  There's a good chance the USB
	 * peripheral received data while the write was occurring. That data
	 * was held in the endpoint buffer(s) by
	 * msc_out_transaction_complete(). Process that data now. Make sure to
	 * _only_ call msc_out_transaction_complete() the exact number of
	 * times a transaction completed in order to avoid a race condition
	 * window (more below).
	 *
	 * usb_endpoint_has_data() can't be used to determine how many
	 * transactions are pending because since this function runs
	 * with interrupts disabled. Doing so would open up a race condition
	 * window where the endpoint would be read too many times if a
	 * transaction were to complete during this function. For this reason
	 * msc->out_ep_missed_transactions is used to keep track of how many
	 * OUT transactions are pending (with data on the endpoint buffers).
	 *
	 * Make a new variable for count below because
	 * msc->out_ep_missed_transactions is also manipulated by
	 * msc_out_transaction_complete(), and if the application buffer is
	 * short (eg: smaller the length of two transactions), there's a
	 * possibility that one call to msc_out_transaction_complete() could
	 * fail (if the application buffer is full), in which case
	 * msc_out_transaction_complete() would re-increment
	 * msc->out_ep_missed_transactions.  */

	uint8_t i, count;

	count = msc->out_ep_missed_transactions;
	for (i = 0; i < count; i++) {
		msc_out_transaction_complete(msc->out_endpoint);
		msc->out_ep_missed_transactions--;
	}
}

void msc_notify_write_data_handled(struct msc_application_data *msc)
{
	usb_disable_transaction_interrupt();

	if (msc->state != MSC_DATA_TRANSPORT_OUT) {
		goto out;
	}

	/* Ignore if this function has been called too many times. */
	if (msc->transferred_bytes >= msc->requested_bytes)
		goto out;

	msc->transferred_bytes += msc->rx_buf_len;

	if (msc->transferred_bytes < msc->requested_bytes) {
		/* Still more data left to transfer and write. Reset the
		 * buffer pointer to the beginning of the buffer to prepare
		 * to receive more data from the host. */
		msc->rx_buf_cur = msc->rx_buf;
	}

out:
	handle_missed_out_transactions(msc);

	usb_enable_transaction_interrupt();
}

void msc_notify_write_operation_complete(struct msc_application_data *msc,
                                         bool passed,
                                         uint32_t bytes_processed)
{
	uint32_t residue;

	usb_disable_transaction_interrupt();

	if (msc->state != MSC_DATA_TRANSPORT_OUT) {
		goto out;
	}

	residue = msc->requested_bytes_cbw - bytes_processed;

	if (!passed) {
		/* Save off the error codes. These will be read by the host
		 * with a REQUEST_SENSE SCSI command. */
		set_scsi_sense(msc, MSC_ERROR_WRITE);

		stall_out_and_set_status(msc, residue, MSC_STATUS_FAILED);
		msc->out_ep_missed_transactions = 0;
		goto fail;
	}

	if (msc->transferred_bytes < msc->requested_bytes) {
		/* The application is ending the Data Transport before the
		 * host has sent all the data it expects to send. This
		 * becomes case 11 (Ho > Do). Stall the OUT endpoint, but set
		 * the status to PASSED, because the device processed all the
		 * data it intended to process. */
		stall_out_and_set_status(msc, residue, MSC_STATUS_PASSED);
		msc->out_ep_missed_transactions = 0;
		goto fail;
	}
	else {
		/* No more data left to transfer */
		send_csw(msc, residue, MSC_STATUS_PASSED);
		msc->state = MSC_IDLE;
	}

out:
	handle_missed_out_transactions(msc);
fail:
	usb_enable_transaction_interrupt();
}
#endif /* MSC_WRITE_SUPPORT */

static void process_msc_command(struct msc_application_data *msc,
                                const uint8_t *data, uint16_t len)
{
	const struct msc_command_block_wrapper *cbw = (const void *) data;
	const uint8_t command = cbw->CBWCB[0];
	const uint8_t lun = cbw->bCBWLUN;
	const uint32_t cbw_length = cbw->dCBWDataTransferLength;
	int8_t res;

	/* Check the Command Block Wrapper (CBW) */
	if (!msc_cbw_valid_and_meaningful(msc, data,len))
		goto bad_cbw;

	msc->current_tag = cbw->dCBWTag;

	if (command == MSC_SCSI_INQUIRY) {
		uint32_t scsi_request_len;
		struct msc_scsi_inquiry_command *cmd =
			(struct msc_scsi_inquiry_command *) cbw->CBWCB;
		struct scsi_inquiry_response *resp =
			(struct scsi_inquiry_response *)
				usb_get_in_buffer(msc->in_endpoint);

		swap2(&cmd->allocation_length);

		/* The host may request just the first part of the inquiry
		 * response structure. */
		scsi_request_len = MIN(cmd->allocation_length, sizeof(*resp));

		/* INQUIRY: Device indends to send data to the host (Di). */
		res = check_di_cases(msc, cbw, scsi_request_len);
		if (res < 0)
			goto fail;

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		/* Send INQUIRY response */
		memset(resp, 0, sizeof(*resp));
		resp->peripheral = 0x0;
		resp->rmb = (msc->media_is_removable_mask & (1<<lun))? 0x80: 0;
		resp->version = MSC_SCSI_SPC_VERSION_2;
		resp->response_data_format = 0x2;
		resp->additional_length = sizeof(*resp) - 4;
		strncpy(resp->vendor, msc->vendor, sizeof(resp->vendor));
		strncpy(resp->product, msc->product, sizeof(resp->product));
		strncpy(resp->revision, msc->revision, sizeof(resp->revision));

		usb_send_in_buffer(msc->in_endpoint, scsi_request_len);

		set_data_in_endpoint_state(msc, cbw_length, scsi_request_len);
	}
	else if (command == MSC_SCSI_TEST_UNIT_READY) {
		/* TEST_UNIT_READY: Device intends to transfer no data (Dn). */
		res = check_dn_cases(msc, cbw);
		if (res < 0)
			goto fail;

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		res = MSC_UNIT_READY(msc, lun);
		if (res < 0) {
			/* Set error */
			set_scsi_sense(msc, res);
			send_csw(msc, cbw_length, MSC_STATUS_FAILED);
			goto fail;
		}

		send_csw(msc, cbw_length, MSC_STATUS_PASSED);
	}
	else if (command == MSC_SCSI_READ_CAPACITY_10) {
		struct scsi_capacity_response *resp =
			(struct scsi_capacity_response *)
				usb_get_in_buffer(msc->in_endpoint);
		uint32_t block_size, num_blocks;
		bool write_protect;

		/* Read Capacity 10: Device intends to send data
		 *                   to the host (Di) */
		res = check_di_cases(msc, cbw, sizeof(*resp));
		if (res < 0)
			goto fail;

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		res = MSC_GET_STORAGE_INFORMATION(
				msc, lun,
				&block_size, &num_blocks, &write_protect);
		if (res < 0) {
			/* Stall and set error */
			set_scsi_sense(msc, res);
			stall_in_and_set_status(
			                 msc, cbw_length, MSC_STATUS_FAILED);
			goto fail;
		}

		/* Pack and send the response buffer */
		resp->last_block = num_blocks - 1;
		resp->block_length = block_size;
		swap4(&resp->last_block);
		swap4(&resp->block_length);
		usb_send_in_buffer(msc->in_endpoint, sizeof(*resp));

		/* Save off block_size */
		msc->block_size[lun] = block_size;

		set_data_in_endpoint_state(msc, cbw_length, sizeof(*resp));
	}
	else if (command == MSC_SCSI_REQUEST_SENSE) {
		uint32_t scsi_request_len;
		struct msc_scsi_request_sense_command *cmd =
			(struct msc_scsi_request_sense_command *) cbw->CBWCB;
		struct scsi_sense_response *resp =
			(struct scsi_sense_response *)
				usb_get_in_buffer(msc->in_endpoint);

		scsi_request_len = MIN(cmd->allocation_length, sizeof(*resp));

		/* REQUEST_SENSE: Device intends to send data
		 *                to the host (Di) */
		res = check_di_cases(msc, cbw, scsi_request_len);
		if (res < 0)
			goto fail;

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		memset(resp, 0, sizeof(*resp));
		resp->response_code = SCSI_SENSE_CURRENT_ERRORS;
		resp->flags = msc->sense_key;
		resp->additional_sense_length = 0xa;
		resp->additional_sense_code = msc->additional_sense_code;

		usb_send_in_buffer(msc->in_endpoint, scsi_request_len);

		set_data_in_endpoint_state(msc, cbw_length, scsi_request_len);
	}
	else if (command == MSC_SCSI_MODE_SENSE_6) {
		uint32_t block_size, num_blocks;
		int8_t res;
		bool write_protect;

		struct msc_scsi_mode_sense_6_command *cmd =
			(struct msc_scsi_mode_sense_6_command *) cbw->CBWCB;
		struct scsi_mode_sense_response *resp =
			(struct scsi_mode_sense_response *)
				usb_get_in_buffer(msc->in_endpoint);

		/* MODE_SENSE(6): Device intends to send data
		 *                to the host (Di) */
		res = check_di_cases(msc, cbw, sizeof(*resp));
		if (res < 0)
			goto fail;

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		/* Look for page code 0x3f, subpage code 0x0. */
		if (cmd->pc_page_code != 0x3f || cmd->subpage_code != 0) {
			msc->sense_key = SCSI_SENSE_KEY_ILLEGAL_REQUEST;
			msc->additional_sense_code =
			     SCSI_ASC_INVALID_FIELD_IN_COMMAND_PACKET;

			/* Stall and send the status after the stall. */
			stall_in_and_set_status(msc,
			                        cbw_length,
			                        MSC_STATUS_FAILED);
			goto fail;
		}

		res = MSC_GET_STORAGE_INFORMATION(msc, lun, &block_size,
		                                  &num_blocks, &write_protect);
		if (res < 0) {
			/* Stall and set error */
			set_scsi_sense(msc, res);
			stall_in_and_set_status(
			                msc, cbw_length, MSC_STATUS_FAILED);
			goto fail;
		}

#ifndef MSC_WRITE_SUPPORT
		/* Force write-protect on if write is not supported */
		write_protect = true;
#endif
		resp->mode_data_length =
			sizeof(struct scsi_mode_sense_response) - 1;
		resp->medium_type = 0x0; /* 0 = SBC */
		resp->device_specific_parameter = (write_protect)? 0x80: 0;
		resp->block_descriptor_length = 0;

		usb_send_in_buffer(msc->in_endpoint, sizeof(*resp));
		set_data_in_endpoint_state(msc, cbw_length, sizeof(*resp));
	}
	else if (command == MSC_SCSI_START_STOP_UNIT) {
		int8_t res;
		bool start, load_eject;

		struct msc_scsi_start_stop_unit *cmd =
			(struct msc_scsi_start_stop_unit *) cbw->CBWCB;

		/* START STOP UNIT: Device intends to not send or receive
		 *                  any data (Dn). */
		res = check_dn_cases(msc, cbw);
		if (res < 0)
			goto fail;

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		/* Only accept power condition 0x0, START_VALID */
		if ((cmd->command & 0xf0) != 0)
			goto fail;

		start      = ((cmd->command & 0x1) != 0);
		load_eject = ((cmd->command & 0x2) != 0);

		res = MSC_START_STOP_UNIT(msc, lun, start, load_eject);
		if (res < 0) {
			set_scsi_sense(msc, res);
			send_csw(msc, cbw_length, MSC_STATUS_FAILED);
			goto fail;
		}

		send_csw(msc, 0, MSC_STATUS_PASSED);
	}
	else if (command == MSC_SCSI_READ_10) {
		uint32_t scsi_request_len;
		struct msc_scsi_read_10_command *cmd =
			(struct msc_scsi_read_10_command *) cbw->CBWCB;

		swap4(&cmd->logical_block_address);
		swap2(&cmd->transfer_length); /* length in blocks */

		if (usb_in_endpoint_busy(msc->in_endpoint))
			goto fail;

		scsi_request_len = cmd->transfer_length * msc->block_size[lun];

		/* Handle the nonsensical, but possible case of the host
		 * asking to read 0 bytes in the SCSI. That actually makes
		 * this a Dn case rather than a Di case. */
		if (scsi_request_len == 0) {
			res = check_dn_cases(msc, cbw);
			if (res < 0)
				goto fail;

			/* If check_dn_cases() succeeded, then the host is
			 * not expecting any data, so send the CSW*/
			send_csw(msc, 0, MSC_STATUS_PASSED);
			goto fail; /* Not a failure, but handled the same */
		}

		/* READ(10): Device intends to send data to the host (Di) */
		res = check_di_cases(msc, cbw, scsi_request_len);
		if (res < 0)
			goto fail;

		/* Set up the transport state. It's important that this is
		 * done before the call to the MSC_START_READ() callback
		 * below, because the application could concievably start
		 * calling msc_send_to_host() from the callback. */
		msc->requested_bytes = MIN(cbw_length, scsi_request_len);
		msc->requested_bytes_cbw = cbw_length;
		msc->transferred_bytes = 0;
		msc->state = MSC_DATA_TRANSPORT_IN;

		/* Start the Data-Transport. After receiving the call to
		 * MSC_START_READ() the application will repeatedly call
		 * msc_send_to_host() with data read from the medium
		 * and then call msc_data_complete() when finished. */
		res = MSC_START_READ(msc, lun,
			             cmd->logical_block_address,
			             cmd->transfer_length);
		if (res < 0) {
			set_scsi_sense(msc, res);
			stall_in_and_set_status(msc,
			                        cbw_length,
			                        MSC_STATUS_FAILED);

			/* Reset the state */
			msc->requested_bytes = 0;
			msc->requested_bytes_cbw = 0;
			msc->state = MSC_IDLE;
			goto fail;
		}
	}
#ifdef MSC_WRITE_SUPPORT
	else if (command == MSC_SCSI_WRITE_10) {
		uint32_t scsi_request_len;
		int8_t res;
		struct msc_scsi_write_10_command *cmd =
			(struct msc_scsi_write_10_command *) cbw->CBWCB;

		swap4(&cmd->logical_block_address);
		swap2(&cmd->transfer_length); /* length in blocks */

		scsi_request_len = cmd->transfer_length * msc->block_size[lun];

		/* Handle the nonsensical, but possible case of the host
		 * asking to write 0 bytes in the SCSI command. That actually
		 * makes this a Dn case rather than a Do case, and is
		 * required by the USBCV test. */
		if (scsi_request_len == 0) {
			res = check_dn_cases(msc, cbw);
			if (res < 0)
				goto fail;

			/* If check_dn_cases() succeeded, then the host is
			 * not expecting to write any data, so send the CSW */
			send_csw(msc, 0, MSC_STATUS_PASSED);
			goto fail; /* Not a failure, but handled the same */
		}

		/* Write(10): Device intends to receive data
		 *            from the host (Do) */
		res = check_do_cases(msc, cbw, scsi_request_len);
		if (res < 0)
			goto fail;

		/* Start the Data-Transport. The application will give
		 * a buffer to put the data into. */
		res = MSC_START_WRITE(msc,
		               lun,
		               cmd->logical_block_address,
		               cmd->transfer_length,
		               &msc->rx_buf,
		               &msc->rx_buf_len,
		               &msc->operation_complete_callback);

		if (res < 0) {
			set_scsi_sense(msc, res);
			stall_out_and_set_status(msc,
			                         cbw_length,
			                         MSC_STATUS_FAILED);
			goto fail;
		}

		/* Initialize the data transport */
		msc->requested_bytes = scsi_request_len;
		msc->requested_bytes_cbw = cbw_length;
		msc->transferred_bytes = 0;
		msc->rx_buf_cur = msc->rx_buf;
		msc->state = MSC_DATA_TRANSPORT_OUT;
	}
#endif /* MSC_WRITE_SUPPORT */
	else {
		/* Unsupported command. See Axelson, page 69. */
		const bool direc_is_in = direction_is_in(cbw->bmCBWFlags);

		/* Set error codes which will be requested with REQUEST_SENSE
		 * by the host later. */
		msc->sense_key = SCSI_SENSE_KEY_ILLEGAL_REQUEST;
		msc->additional_sense_code =
			SCSI_ASC_INVALID_COMMAND_OPERATION_CODE;

		/* Stall appropriate endpoint and send FAILED for the CSW. */
		if (direc_is_in || cbw_length == 0)
			stall_in_and_set_status(msc,
			                 cbw_length, MSC_STATUS_FAILED);
		else
			stall_out_and_set_status(msc,
			                 cbw_length, MSC_STATUS_FAILED);

		goto fail;
	}

	return;

bad_cbw:
	/* If the CBW is not valid or is not meaningful, then stall both
	 * endpoints until a Reset Recovery (BOT 5.3.4) procedure is
	 * completed by the host (BOT 5.3, Figure 2). */
	usb_halt_ep_in(msc->in_endpoint);
	usb_halt_ep_out(msc->out_endpoint);
	msc->state = MSC_NEEDS_RESET_RECOVERY;
fail:
	return;
}

void msc_clear_halt(uint8_t endpoint, uint8_t direction)
{
	struct msc_application_data *msc;

	msc = get_app_data_by_endpoint(endpoint, direction);
	if (!msc)
		return;

	if (msc->state == MSC_CSW) {
		send_csw(msc, msc->residue, msc->status);
		msc->state = MSC_IDLE;
	}
	else if (msc->state == MSC_NEEDS_RESET_RECOVERY) {
		/* The device needs a Reset Recovery (BOT 5.3.4) but the
		 * host has not performed a Bulk-Only Mass Storage Reset
		 * (BOT 3.1) control transfer yet, so the endpoint must
		 * remain stalled. (BOT 5.3, Figure 2) */
		if (direction)
			usb_halt_ep_in(endpoint);
		else
			usb_halt_ep_out(endpoint);
	}
}

void msc_in_transaction_complete(uint8_t endpoint)
{
	struct msc_application_data *msc;

	msc = get_app_data_by_endpoint(endpoint, 1/*IN*/);
	if (!msc)
		return;

	if (msc->state == MSC_DATA_TRANSPORT_IN) {
		send_next_data_transaction(msc);
	}
	else if (msc->state == MSC_STALL) {
		usb_halt_ep_in(msc->in_endpoint);
		msc->state = MSC_CSW;
	}
	else if (msc->state == MSC_CSW) {
		send_csw(msc, msc->residue, msc->status);
		msc->state = MSC_IDLE;
	}
}

void msc_out_transaction_complete(uint8_t endpoint)
{
	struct msc_application_data *msc;
	const unsigned char *out_buf;
	uint16_t out_buf_len;
#ifdef MSC_WRITE_SUPPORT
	uint8_t res;
#endif

	msc = get_app_data_by_endpoint(endpoint, 0/*OUT*/);
	if (!msc)
		return;

	/* If the OUT endpoint is halted because of a failed write (from
	 * msc_notify_write_operation_complete(), while interrupts are
	 * disabled), it's possible that a transaction completed before the
	 * endpoint was halted, and that interrupt would cause this
	 * function to be called on a halted endpoint.  Since in that case
	 * this is a stray interrupt, there's no need to re-arm the
	 * endpoint.  */
	if (usb_out_endpoint_halted(endpoint))
		return;

	out_buf_len = usb_get_out_buffer(endpoint, &out_buf);

#ifdef MSC_WRITE_SUPPORT
	if (msc->state == MSC_DATA_TRANSPORT_OUT) {
		/* In the DATA_TRANSPORT_OUT state, treat this transaction
		 * as data which is to be written to the medium. This call
		 * may fail if the application's buffer is full, in which
		 * case the endpoint will not be re-armed, and the data
		 * will remain in the endpoint buffer until there is space
		 * in the application's buffer. */
		res = receive_data(msc, out_buf, out_buf_len);
	}
	else if (msc->state == MSC_IDLE) {
		process_msc_command(msc, out_buf, out_buf_len);
		res = 0;
	}
	else {
		/* OUT transaction completed when the device is not in a
		 * state that can handle it. Ignore the data. */
		res = 0;
	}

	/* If the data could not be handled by the application yet (because
	 * the application's buffer is full), don't re-arm the endpoint now.
	 * Keep the received data in the endpoint's buffer until the
	 * application has reset its own buffer (and has signaled such by
	 * calling msc_write_complete()).  At that point, the endpoint's
	 * data will be handled and the endpoint re-armed.  */
	if (res == 0)
		usb_arm_out_endpoint(endpoint);
	else
		msc->out_ep_missed_transactions++;
#else
	/* If read-only, then OUT transactions are always processed
	 * and fully handled, leaving no reason to have missed
	 * transactions, as above. */
	if (msc->state == MSC_IDLE)
		process_msc_command(msc, out_buf, out_buf_len);
	usb_arm_out_endpoint(endpoint);
#endif
}
