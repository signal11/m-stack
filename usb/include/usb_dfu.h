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

#ifndef __USB_DFU_H
#define __USB_DFU_H

/** @file usb_dfu.h
 *  @brief USB DFU Class Enumerations and Structures
 *  @defgroup public_api Public API
 */

/** @addtogroup public_api
 *  @{
 */

#include <stdint.h>

/** @defgroup dfu_items USB DFU Class Enumerations and Descriptors
 *  @brief Packet structs from the Device Class Definition for Device Firmware
 *  Update (commonly the USB DFU Specification), version 1.1.
 *
 *  For more information about these structures, see the above referenced
 *  document, available from www.usb.org/developers/docs/devclass_docs/DFU_1.1.pdf .
 *  @addtogroup dfu_items
 *  @{
 */

// FIXME: move to m-stack?
#define DFU_INTERFACE_CLASS			0xfe
#define DFU_INTERFACE_SUBCLASS			0x01
#define DESC_DFU_FUNCTIONAL_DESCRIPTOR		0x21

/** DFU Interface Protocol */
enum DfuInterfaceProtocol {
	DFU_INTERFACE_PROTOCOL_RUNTIME		= 0x01, /** usual application firmware */
	DFU_INTERFACE_PROTOCOL_DFU		= 0x02, /** special mode for bootloader */
};;

/** DFU Interface Attributes */
enum DfuInterfaceAttributes {
	DFU_ATTRIBUTE_CAN_DOWNLOAD		= 0x01, /** can transfer host -> device */
	DFU_ATTRIBUTE_CAN_UPLOAD		= 0x02, /** can transfer device -> host */
	DFU_ATTRIBUTE_MANIFESTATON_TOLERANT	= 0x04, /** can do GetStatus in dfuMANIIFEST */
	DFU_ATTRIBUTE_WILL_DETACH		= 0x08, /** will self-detach on appDETACH */
};

// FIXME: move to m-stack?
/** DFU Functional Descriptor */
struct dfu_functional_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bmAttributes;
	uint16_t wDetachTimeOut;
	uint16_t wTransferSize;
	uint16_t bcdDFUVersion;
};

/** DFU Class Status */
enum DfuStatus {
	DFU_STATUS_OK				= 0x00,
	DFU_STATUS_ERR_TARGET			= 0x01,
	DFU_STATUS_ERR_FILE			= 0x02,
	DFU_STATUS_ERR_WRITE			= 0x03,
	DFU_STATUS_ERR_ERASE			= 0x04,
	DFU_STATUS_ERR_CHECK_ERASED		= 0x05,
	DFU_STATUS_ERR_PROG			= 0x06,
	DFU_STATUS_ERR_VERIFY			= 0x07,
	DFU_STATUS_ERR_ADDRESS			= 0x08,
	DFU_STATUS_ERR_NOTDONE			= 0x09,
	DFU_STATUS_ERR_FIRMWARE			= 0x0a,
	DFU_STATUS_ERR_VENDOR			= 0x0b,
	DFU_STATUS_ERR_USBR			= 0x0c,
	DFU_STATUS_ERR_POR			= 0x0d,
	DFU_STATUS_ERR_UNKNOWN			= 0x0e,
	DFU_STATUS_ERR_STALLDPKT		= 0x0f,
};

/** DFU Class State */
enum DfuState {
	DFU_STATE_APP_IDLE			= 0x00,
	DFU_STATE_APP_DETACH			= 0x01,
	DFU_STATE_DFU_IDLE			= 0x02,
	DFU_STATE_DFU_DNLOAD_SYNC		= 0x03,
	DFU_STATE_DFU_DNBUSY			= 0x04,
	DFU_STATE_DFU_DNLOAD_IDLE		= 0x05,
	DFU_STATE_DFU_MANIFEST_SYNC		= 0x06,
	DFU_STATE_DFU_MANIFEST			= 0x07,
	DFU_STATE_DFU_MANIFEST_WAIT_RESET	= 0x08,
	DFU_STATE_DFU_UPLOAD_IDLE		= 0x09,
	DFU_STATE_DFU_ERROR			= 0x0a,
};

int8_t	 process_dfu_setup_request	(const struct setup_packet	*setup);
uint8_t	 usb_dfu_get_state		(void);
void	 usb_dfu_set_state		(uint8_t			 state);
void	 usb_dfu_set_status		(uint8_t			 status);

#ifdef USB_DFU_USE_BOOTLOADER

#ifdef USB_DFU_WRITE_FUNC
/** DFU Write Function
 *
 * The DFU Stack will call this function when a block of data needs to be
 * written to a flash memory location.
 *
 * Is is the responsibility of the application to erase blocks of memory when
 * required.
 *
 * @param addr           The memory address to write
 * @param data           The memory block to copy from
 * @param len            The length of @data
 * @param context        A pointer to be passed to the callback.  The USB stack
 *                       does not dereference this pointer.
 *
 * @returns
 *   Return 0 if the write completed or -1 if it cannot, or if the alternate
 *   setting was invalid.
 */
extern int8_t USB_DFU_WRITE_FUNC(uint16_t addr, uint8_t *data,
				 uint16_t len, void *context);
#endif

#ifdef USB_DFU_READ_FUNC
/** DFU Write Function
 *
 * The DFU Stack will call this function when a block of data needs to be
 * read from a flash memory location.
 *
 * @param addr           The memory address to read
 * @param data           The memory block to copy to
 * @param len            The length of @data
 * @param context        A pointer to be passed to the callback.  The USB stack
 *                       does not dereference this pointer.
 *
 * @returns
 *   Return 0 if the read completed or -1 if it cannot, or if the alternate
 *   setting was invalid.
 */
extern int8_t USB_DFU_READ_FUNC(uint16_t addr, uint8_t *data,
				uint16_t len, void *context);
#endif

#endif

#ifdef USB_DFU_USE_RUNTIME

#ifdef USB_DFU_SUCCESS_FUNC
/** DFU Successful appIDLE GetStatus Function
 *
 * The DFU Stack will call this function when a GetStatus request has been
 * successfully processed in appIDLE mode. This could be used to set a
 * "firmware started successfully" bit in the flash memory so that a bootloader
 * automatically starts the firmware each time.
 *
 * @param context        A pointer to be passed to the callback.  The USB stack
 *                       does not dereference this pointer.
 *
 */
extern void USB_DFU_SUCCESS_FUNC(void *context);
#endif

#endif

#ifdef MULTI_CLASS_DEVICE
/** Set the list of DFU interfaces on this device
 *
 * Provide a list to the DFU class implementation of the interfaces on this
 * device which should be treated as DFU devices.  This is only necessary
 * for multi-class composite devices to make sure that requests are not
 * confused between interfaces.  It should be called before usb_init().
 *
 * @param interfaces      An array of interfaces which are DFU class.
 * @param num_interfaces  The size of the @p interfaces array.
 */
void dfu_set_interface_list(uint8_t *interfaces, uint8_t num_interfaces);
#endif

/* Doxygen end-of-group for dfu_items */
/** @}*/

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* __USB_DFU_H */
