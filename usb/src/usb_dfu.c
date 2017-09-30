/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <xc.h>
#include <string.h>

#include "usb.h"
#include "usb_ch9.h"
#include "usb_config.h"
#include "usb_dfu.h"

/** DFU specific requests */
enum DfuRequest {
	DFU_REQUEST_DETACH			= 0x00,
	DFU_REQUEST_DNLOAD			= 0x01,
	DFU_REQUEST_UPLOAD			= 0x02,
	DFU_REQUEST_GETSTATUS			= 0x03,
	DFU_REQUEST_CLRSTATUS			= 0x04,
	DFU_REQUEST_GETSTATE			= 0x05,
	DFU_REQUEST_ABORT			= 0x06,
};

/* shared */
static uint8_t			 _dfu_state = DFU_STATE_APP_IDLE;
static uint8_t			 _dfu_status = DFU_STATUS_OK;
static uint8_t			 _dfu_buf[DFU_TRANSFER_SIZE];
static void			*_dfu_context = NULL;

/* bootloader */
#ifdef USB_DFU_USE_BOOTLOADER
static uint16_t			 _dfu_block_num = 0;
#endif

/* runtime */
#ifdef USB_DFU_USE_RUNTIME
enum DfuIdleAction {
	DFU_IDLE_ACTION_NOTHING	= 0,
	DFU_IDLE_ACTION_RESET	= 1,
	DFU_IDLE_ACTION_SUCCESS	= 2,
};
static uint8_t		 _dfu_idle_action = DFU_IDLE_ACTION_NOTHING;
#endif

typedef struct {
	uint8_t		bStatus;
	uint8_t		bwPollTimeout[3];
	uint8_t		bState;
	uint8_t		iString;
} DfuPayloadStatus;

void usb_dfu_set_state(uint8_t state)
{
	switch (state) {
	case DFU_STATE_DFU_IDLE:
	case DFU_STATE_DFU_MANIFEST_SYNC:
#ifdef USB_DFU_USE_BOOTLOADER
		_dfu_block_num = 0;
#endif
		break;
	default:
		break;
	}
	_dfu_state = state;
}

uint8_t usb_dfu_get_state (void)
{
	return _dfu_state;
}

void usb_dfu_set_status(uint8_t status)
{
	switch (status) {
	case DFU_STATUS_OK:
		break;
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
		break;
	}
	_dfu_status = status;
}

static int8_t _send_data_stage_cb(bool transfer_ok, void *context)
{
	/* error */
	if (!transfer_ok) {
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
		return -1;
	}

#ifdef USB_DFU_USE_RUNTIME
	/* reboot back into bootloader mode */
	if (_dfu_idle_action == DFU_IDLE_ACTION_RESET)
		RESET();

#ifdef USB_DFU_SUCCESS_FUNC
	/* we've done GetStatus successfully when in appIDLE mode */
	if (_dfu_idle_action == DFU_IDLE_ACTION_SUCCESS)
		USB_DFU_SUCCESS_FUNC(_dfu_context);
#endif

#endif
	return 0;
}

/**
 * usb_dfu_request_helper_GetStatus:
 *
 * Return a packet with the device status.
 **/
static int8_t usb_dfu_request_helper_GetStatus(void)
{
	DfuPayloadStatus *pl = (DfuPayloadStatus *) _dfu_buf;
	memset(pl, 0, sizeof(DfuPayloadStatus));
	pl->bStatus = _dfu_status;
	pl->bState = _dfu_state;
#ifdef USB_DFU_USE_RUNTIME
	if (_dfu_status == DFU_STATE_APP_IDLE)
		_dfu_idle_action = DFU_IDLE_ACTION_SUCCESS;
#endif
	usb_send_data_stage(_dfu_buf, sizeof(DfuPayloadStatus), _send_data_stage_cb, NULL);
	return 0;
}

/**
 * usb_dfu_request_helper_GetState:
 **/
static int8_t usb_dfu_request_helper_GetState(void)
{
	_dfu_buf[0] = _dfu_state;
	usb_send_data_stage(_dfu_buf, 1, _send_data_stage_cb, NULL);
	return 0;
}

#ifdef USB_DFU_USE_RUNTIME
/**
 * usb_dfu_request_state_AppIdle:
 *
 * State 0.
 **/
static int8_t usb_dfu_request_state_AppIdle(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_DETACH:
		/* only reset when USB stack is not busy */
		_dfu_idle_action = DFU_IDLE_ACTION_RESET;
		usb_dfu_set_state(DFU_STATE_APP_DETACH);
		usb_send_data_stage(_dfu_buf, 0, _send_data_stage_cb, NULL);
		return 0;
	case DFU_REQUEST_GETSTATUS:
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		break;
	}
	return -1;
}

/**
 * usb_dfu_request_state_AppDetach:
 *
 * State 1.
 **/
static int8_t usb_dfu_request_state_AppDetach(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_GETSTATUS:
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		/* this is invalid, so just STALL */
		usb_dfu_set_state(DFU_STATE_APP_IDLE);
		return -1;
	}
}
#endif

#ifdef USB_DFU_USE_BOOTLOADER

/**
 * usb_dfu_request_helper_Upload:
 **/
static int8_t usb_dfu_request_helper_Upload(const struct setup_packet *setup)
{
	struct setup_packet *setup2 = (struct setup_packet *) setup;

	/* not valid */
	if (setup->wLength != DFU_TRANSFER_SIZE) {
		usb_dfu_set_status(DFU_STATUS_ERR_TARGET);
		return -1;
	}

	/* are we done */
	if (_dfu_block_num == DFU_FLASH_LENGTH / DFU_TRANSFER_SIZE) {
		usb_dfu_set_state(DFU_STATE_DFU_IDLE);
		setup2->wLength = 0;
		usb_send_data_stage(_dfu_buf, 0, _send_data_stage_cb, NULL);
		return 0;
	}

	/* send next block of ROM */
#ifdef USB_DFU_READ_FUNC
	int8_t rc;
	rc = USB_DFU_READ_FUNC(_dfu_block_num * DFU_TRANSFER_SIZE,
			       _dfu_buf, DFU_TRANSFER_SIZE,
			       _dfu_context);
	if (rc != 0)
		return -1;
#endif
	setup2->wValue = _dfu_block_num++;
	usb_send_data_stage(_dfu_buf, DFU_TRANSFER_SIZE, _send_data_stage_cb, NULL);
	return 0;
}

/**
 * usb_dfu_request_helper_Abort:
 **/
static int8_t usb_dfu_request_helper_Abort(void)
{
	usb_dfu_set_state(DFU_STATE_DFU_IDLE);
	usb_send_data_stage(_dfu_buf, 0, _send_data_stage_cb, NULL);
	return 0;
}
/**
 * _recieve_data_stage_cb:
 **/
static int8_t _recieve_data_stage_cb(bool transfer_ok, void *context)
{
	/* error */
	if (!transfer_ok) {
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
		return -1;
	}

	/* write */
#ifdef USB_DFU_WRITE_FUNC
	int8_t rc;
	rc = USB_DFU_WRITE_FUNC(_dfu_block_num * DFU_TRANSFER_SIZE,
				_dfu_buf, DFU_TRANSFER_SIZE,
				_dfu_context);
	if (rc != 0)
		return 0;
#endif

	/* done */
	_dfu_block_num++;
	return 0;
}

/**
 * usb_dfu_request_helper_Download:
 **/
static int8_t usb_dfu_request_helper_Download(const struct setup_packet *setup)
{
	/* not valid */
	if (setup->wLength != DFU_TRANSFER_SIZE) {
		usb_dfu_set_status(DFU_STATUS_ERR_TARGET);
		return -1;
	}

	/* too big */
	if (_dfu_block_num * DFU_TRANSFER_SIZE > DFU_FLASH_LENGTH) {
		usb_dfu_set_status(DFU_STATUS_ERR_ADDRESS);
		return -1;
	}

	/* wait for the download data chunk */
	usb_start_receive_ep0_data_stage(_dfu_buf, setup->wLength,
					 _recieve_data_stage_cb, NULL);

	/* now in download mode */
	usb_dfu_set_state(DFU_STATE_DFU_DNLOAD_SYNC);
	return 0;
}

/**
 * usb_dfu_request_state_DfuIdle:
 *
 * State 2.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuIdle(const struct setup_packet *setup)
{
	uint8_t i;

	switch (setup->bRequest) {
	case DFU_REQUEST_DNLOAD:
		/* a zero length download is not considered useful */
		if (setup->wLength == 0) {
			usb_dfu_set_status(DFU_STATUS_ERR_FILE);
			return -1;
		}
		return usb_dfu_request_helper_Download(setup);
	case DFU_REQUEST_UPLOAD:
		usb_dfu_set_state(DFU_STATE_DFU_UPLOAD_IDLE);
		return usb_dfu_request_helper_Upload(setup);
	case DFU_REQUEST_ABORT:
		return usb_dfu_request_helper_Abort();
	case DFU_REQUEST_GETSTATUS:
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		/* this is invalid, so just STALL */
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuDnloadSync:
 *
 * State 3.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuDnloadSync(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_GETSTATUS:
		/* we don't have any BUSY state, the EEPROM write is fast */
		usb_dfu_set_state(DFU_STATE_DFU_DNLOAD_IDLE);
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuDnbusy:
 *
 * State 4.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuDnbusy(const struct setup_packet *setup)
{
	//FIXME: if bwPollTimeout elapsed, we can sent GETSTATUS, so set
	// usb_dfu_set_state(DFU_STATE_DFU_DNLOAD_SYNC);
	switch (setup->bRequest) {
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuDnloadIdle:
 *
 * State 5.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuDnloadIdle(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_DNLOAD:
		/* host is saying there is no more data to download */
		if (setup->wLength == 0) {
			usb_dfu_set_state(DFU_STATE_DFU_MANIFEST_SYNC);
			usb_send_data_stage(_dfu_buf, 0, _send_data_stage_cb, NULL);
			return 0;
		}
		return usb_dfu_request_helper_Download(setup);
	case DFU_REQUEST_ABORT:
		return usb_dfu_request_helper_Abort();
	case DFU_REQUEST_GETSTATUS:
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuManifestSync:
 *
 * State 6.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuManifestSync(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_GETSTATUS:
		/* we did this as we went along, so there's no big blit */
		usb_dfu_set_state(DFU_STATE_DFU_IDLE);
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuManifest:
 *
 * State 7. (and 8)
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuManifest(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuUploadIdle:
 *
 * State 9.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuUploadIdle(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_UPLOAD:
		return usb_dfu_request_helper_Upload(setup);
	case DFU_REQUEST_ABORT:
		return usb_dfu_request_helper_Abort();
	case DFU_REQUEST_GETSTATUS:
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}

/**
 * usb_dfu_request_state_DfuError:
 *
 * State 10.
 *
 * This is only valid in DFU bootloader mode
 **/
static int8_t usb_dfu_request_state_DfuError(const struct setup_packet *setup)
{
	switch (setup->bRequest) {
	case DFU_REQUEST_GETSTATUS:
		return usb_dfu_request_helper_GetStatus();
	case DFU_REQUEST_GETSTATE:
		return usb_dfu_request_helper_GetState();
	case DFU_REQUEST_CLRSTATUS:
		usb_dfu_set_state(DFU_STATE_DFU_IDLE);
		usb_dfu_set_status(DFU_STATUS_OK);
		usb_send_data_stage(_dfu_buf, 0, _send_data_stage_cb, NULL);
		return 0;
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}
#endif

#ifdef MULTI_CLASS_DEVICE
static uint8_t *dfu_interfaces;
static uint8_t num_dfu_interfaces;

void dfu_set_interface_list(uint8_t *interfaces, uint8_t num_interfaces)
{
	dfu_interfaces = interfaces;
	num_dfu_interfaces = num_interfaces;
}
#endif

int8_t process_dfu_setup_request(const struct setup_packet *setup)
{
	if (setup->REQUEST.destination != DEST_INTERFACE)
		return -1;
	if (setup->REQUEST.type != REQUEST_TYPE_CLASS)
		return -1;

#ifdef MULTI_CLASS_DEVICE
	/* Check the interface first to make sure the destination is a
	 * DFU interface. Composite devices will need to call
	 * dfu_set_interface_list() first.
	 */
	uint8_t interface = setup->wIndex;
	uint8_t i;
	for (i = 0; i < num_dfu_interfaces; i++) {
		if (interface == dfu_interfaces[i])
			break;
	}

	/* Return if interface is not in the list of HID interfaces. */
	if (i == num_dfu_interfaces)
		return -1;
#endif

	switch (_dfu_state) {
#ifdef USB_DFU_USE_RUNTIME
	case DFU_STATE_APP_IDLE:
		return usb_dfu_request_state_AppIdle(setup);
	case DFU_STATE_APP_DETACH:
		return usb_dfu_request_state_AppDetach(setup);
#endif
#ifdef USB_DFU_USE_BOOTLOADER
	case DFU_STATE_DFU_IDLE:
		return usb_dfu_request_state_DfuIdle(setup);
	case DFU_STATE_DFU_DNLOAD_SYNC:
		return usb_dfu_request_state_DfuDnloadSync(setup);
	case DFU_STATE_DFU_DNBUSY:
		return usb_dfu_request_state_DfuDnbusy(setup);
	case DFU_STATE_DFU_DNLOAD_IDLE:
		return usb_dfu_request_state_DfuDnloadIdle(setup);
	case DFU_STATE_DFU_MANIFEST_SYNC:
		return usb_dfu_request_state_DfuManifestSync(setup);
	case DFU_STATE_DFU_MANIFEST:
		return usb_dfu_request_state_DfuManifest(setup);
	case DFU_STATE_DFU_MANIFEST_WAIT_RESET:
		return usb_dfu_request_state_DfuManifest(setup);
	case DFU_STATE_DFU_UPLOAD_IDLE:
		return usb_dfu_request_state_DfuUploadIdle(setup);
	case DFU_STATE_DFU_ERROR:
		return usb_dfu_request_state_DfuError(setup);
#endif
	default:
		usb_dfu_set_state(DFU_STATE_DFU_ERROR);
	}
	return -1;
}
