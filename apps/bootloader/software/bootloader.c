/*
 * M-Stack PIC24 Bootloader
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
 *
 * Alan Ott
 * Signal 11 Software
 * 2013-04-29
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if _MSC_VER && _MSC_VER < 1600
	#include "c99.h"
#else
	#include <stdbool.h>
#endif

#ifdef WIN32
	#define snprintf _snprintf
#endif

#include <libusb.h>

#include "hex.h"
#include "bootloader.h"
#include "log.h"
#include "../common/bootloader_protocol.h"

#ifdef _MSC_VER
	#pragma warning (disable:4996)
#endif

#define MIN(X,Y) ((X)<(Y)? (X): (Y))

/* Bootloader Object */
struct bootloader {
	struct hex_data *hd;
	libusb_device_handle *handle;
	struct chip_info chip_info;
	int bytes_per_row;
};

/* Open a libusb device.
 * This function opens the device using libusb, but only if there is a
 * single instance of it attached to the system.  It will fail if multiple
 * instances of the device are connected at once.
 *
 * The return value is a bootloader return code (bootloader.h). 
 * libusb_return is a libusb return code (libusb.h).
 * libusb_return is only valid if the function's return value is negative.
 */
static int open_device(struct bootloader *bl, uint16_t vid, uint16_t pid, int *libusb_return)
{
	libusb_device **devs;
	libusb_device *usb_dev, *dev_to_open = NULL;
	int res = 0;
	int d = 0;
	
	*libusb_return = 0;

	libusb_get_device_list(NULL, &devs);
	while ((usb_dev = devs[d++]) != NULL) {
		struct libusb_device_descriptor desc;
		
		libusb_get_device_descriptor(usb_dev, &desc);
		if (desc.idVendor == vid && desc.idProduct == pid) {
			if (dev_to_open) {
				res = BOOTLOADER_MULTIPLE_CONNECTED;
				goto out;
			}
			dev_to_open = usb_dev;
		}
	}

	if (dev_to_open) {
		res = libusb_open(dev_to_open, &bl->handle);
		if (res < 0) {
			*libusb_return = res;
			res = BOOTLOADER_CANT_OPEN_DEVICE;
			goto out;
		}
	}
	else
		res = BOOTLOADER_CANT_OPEN_DEVICE;

out:
	libusb_free_device_list(devs, 1/*unref devices*/);
	return res;
}


static int clear_flash(libusb_device_handle *handle)
{
	int res;
	
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_OTHER,
		CLEAR_FLASH /* bRequest */,
		0, /* wValue */
		0, /* wIndex */
		NULL, 0/*wLength*/,
		10000/*timeout millis*/);

	if (res < 0) {
		log_libusb("Error clearing flash : %s\n", libusb_error_name(res));
		return res;
	}

	return 0;
}

static int send_data(libusb_device_handle *handle, size_t address, const unsigned char *buf, size_t len)
{
	int res;
	
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_OTHER,
		SEND_DATA /* bRequest */,
		address & 0xffff, /* wValue: Low Address */
		(address & 0xffff0000) >> 16, /* wIndex: High Address */
		(unsigned char*) buf, len/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		log_libusb("Error Sending Data : %s\n", libusb_error_name(res));
		return res;
	}

	return 0;
}

static int get_chip_info(libusb_device_handle *handle, struct chip_info *info)
{
	int res;

	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_OTHER,
		GET_CHIP_INFO /* bRequest */,
		0, /* wValue */
		0, /* wIndex */
		(unsigned char*)info, sizeof(*info)/*wLength*/,
		1000/*timeout millis*/);

	/* TODO: Take care of byte swapping issues in chip_info. */

	if (res < 0) {
		log_libusb("Error request chip info: %s\n", libusb_error_name(res));
		return res;
	}

	return 0;
}

static int request_data(libusb_device_handle *handle, size_t address, unsigned char *buf, size_t len)
{
	int res;
	
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_OTHER,
		REQUEST_DATA /* bRequest */,
		address & 0xffff, /* wValue: Low Address */
		(address & 0xffff0000) >> 16, /* wIndex: High Address */
		buf, len/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		log_libusb("Error requesting data: %s\n", libusb_error_name(res));
		return res;
	}

	return 0;
}

static int send_reset(libusb_device_handle *handle)
{
	int res;
	
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_OTHER,
		SEND_RESET /* bRequest */,
		0, /* wValue */
		0, /* wIndex */
		NULL, 0/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		log_libusb("Error Sending Reset: %s\n", libusb_error_name(res));
		return res;
	}

	return res;
}


static void print_data(const unsigned char *data, size_t len)
{
	size_t i;
	
	for (i = 0; i < len; i++) {
		printf("%02hhx ", data[i]);
		if ((i+1) % 8 == 0)
			printf("  ");
		if ((i+1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
}

int bootloader_verify(struct bootloader *bl)
{
	struct hex_data_region *region;
	int res = 0;

	region = bl->hd->regions;
	while (region) {
		const unsigned char *ptr = region->data;
		const unsigned char *endptr = region->data + region->len;
		size_t address = region->address;
		unsigned char buf[128]; /* This array size is arbitrary */
		int skip_regions;
		int i;

		if (address >= bl->chip_info.config_words_base &&
		    address < bl->chip_info.config_words_top) {
			log("Verify: skipping config words at %lx\n", address);
			goto end_region_verify;
		}

		skip_regions = bl->chip_info.number_of_skip_regions;
		if (skip_regions > MAX_SKIP_REGIONS)
			skip_regions = MAX_SKIP_REGIONS;
		for (i = 0; i < skip_regions; i++) {
			uint32_t skip_base = bl->chip_info.skip_regions[i].base;
			uint32_t skip_top = bl->chip_info.skip_regions[i].top;
			if (address >= skip_base &&
			    address + region->len <  skip_top) {
				log("Verify: skipping region at %lx\n",
					address);
				goto end_region_verify;
			}
		}

		while (ptr < endptr) {
			size_t len_to_request = MIN(sizeof(buf), endptr-ptr);

			res = request_data(bl->handle, address, buf, len_to_request);
			if (res < 0) {
				fprintf(stderr, "Reading data block %lx failed: %s\n", address, libusb_error_name(res));
				res = -1;
				goto failure;
			}
			
			if (memcmp(ptr, buf, len_to_request) != 0) {
				fprintf(stderr, "Verify Failed on block starting at %lx\n", address);

				printf("Read from device: \n");
				print_data(buf, len_to_request);
				
				printf("\nExpected:\n");
				print_data(ptr, len_to_request);
				
				res = -1;
				goto failure;
			}

			ptr += len_to_request;
			address += len_to_request;
		}

end_region_verify:
		region = region->next;
	}

failure:
	return res;
}

int bootloader_program(struct bootloader *bl)
{
	struct hex_data_region *region;
	int res = 0;

	/* Send each memory region to the device */
	region = bl->hd->regions;
	while (region) {
		const unsigned char *ptr = region->data;
		const unsigned char *endptr = region->data + region->len;
		size_t address = region->address;
		int skip_regions;
		int i;
		
		if (address >= bl->chip_info.config_words_base &&
		    address < bl->chip_info.config_words_top) {
			log("Program: skipping config words at %lx\n", address);
			goto end_region;
		}

		skip_regions = bl->chip_info.number_of_skip_regions;
		if (skip_regions > MAX_SKIP_REGIONS)
			skip_regions = MAX_SKIP_REGIONS;
		for (i = 0; i < skip_regions; i++) {
			uint32_t skip_base = bl->chip_info.skip_regions[i].base;
			uint32_t skip_top = bl->chip_info.skip_regions[i].top;
			if (address >= skip_base &&
			    address + region->len <  skip_top) {
				log("Program: skipping region at %lx\n",
					address);
				goto end_region;
			}
		}


		/* If the data isn't aligned to a row, add some extra padding
		 * bytes (0xff) to the beginning of the first packet so that
		 * all the packets align. */
		if (address & (bl->bytes_per_row-1)) {
			unsigned char *buf;
			size_t data_bytes_to_send;
			size_t total_bytes_to_send;
			size_t bytes_to_add;
			
			bytes_to_add = address & (bl->bytes_per_row-1);
			address -= bytes_to_add;
			total_bytes_to_send = MIN(region->len + bytes_to_add,
			                          bl->bytes_per_row);
			data_bytes_to_send = MIN(region->len,
			                         bl->bytes_per_row - bytes_to_add);
			buf = malloc(total_bytes_to_send);
			memset(buf, 0xff, total_bytes_to_send);
			memcpy(buf+bytes_to_add, region->data, data_bytes_to_send);

			log("Padding block at %lx down to %lx\n", region->address, address);
			res = send_data(bl->handle, address, buf, total_bytes_to_send);
			if (res < 0) {
				log_libusb("Sending data block %lx failed: %s\n", region->address, libusb_error_name(res));
				res = -1;
				goto failure;
			}

			ptr += data_bytes_to_send;
			address += total_bytes_to_send;

			free(buf);
		}

		while (ptr < endptr) {
			size_t len_to_send = MIN(bl->bytes_per_row, endptr-ptr);

			res = send_data(bl->handle, address, ptr, len_to_send);
			if (res < 0) {
				log_libusb("Sending data block %lx failed: %s\n", address, libusb_error_name(res));
				res = -1;
				goto failure;
			}

			ptr += len_to_send;
			address += len_to_send;
		}

end_region:
		region = region->next;
	}

failure:
	return res;
}

int bootloader_erase(struct bootloader *bl)
{
	return clear_flash(bl->handle);
}

int bootloader_reset(struct bootloader *bl)
{
	return send_reset(bl->handle);
}

int bootloader_init(struct bootloader **bootl, const char *filename, uint16_t vid, uint16_t pid)
{
	struct bootloader *bl;
	struct hex_data_region *region;
	int libusb_return;
	int res = 0;
	
	*bootl = calloc(1, sizeof(struct bootloader));
	bl = *bootl; /* Convenience pointer */

	/* Load the Hex file */
	if (filename) {
		res = hex_load(filename, &bl->hd);

		if (res < 0) {
			fprintf(stderr, "Unable to load hex file. Error: %d\n", res);
			return BOOTLOADER_CANT_OPEN_FILE;
		}
	}
	else {
		/* No filename, so load up an empty structure. */
		hex_init_empty(&bl->hd);
	}

	log("Hex file regions:\n");
	region = bl->hd->regions;
	while (region) {
		log("  Data Region at %08lx for %4lx bytes (hex)\n", region->address, region->len);
		region = region->next;
	}
	
	/* Init Libusb */
	if (libusb_init(NULL)) {
		res = BOOTLOADER_CANT_OPEN_DEVICE;
		goto free_hex;
	}

	res = open_device(bl, vid, pid, &libusb_return);
	if (res < 0) {
		if (libusb_return < 0) {
			fprintf(stderr, "libusb_open() failed: %s\n", libusb_error_name(libusb_return));
		}
		goto free_hex;
	}

	res = libusb_claim_interface(bl->handle, 0);
	if (res < 0) {
		res = BOOTLOADER_CANT_OPEN_DEVICE;
		goto free_usb;
	}

	res = get_chip_info(bl->handle, &bl->chip_info);
	if (res < 0) {
		fprintf(stderr, "Can't get chip info\n");
		res = BOOTLOADER_CANT_QUERY_DEVICE;
		goto free_usb;
	}

	bl->bytes_per_row = bl->chip_info.bytes_per_instruction *
	                    bl->chip_info.instructions_per_row;

	log("Queried MCU to find:\n");
	log("  bytes per inst: %d\n  inst per row %d\n",
	       bl->chip_info.bytes_per_instruction,
	       bl->chip_info.instructions_per_row);

	return 0;

free_usb:
	libusb_close(bl->handle);
free_hex:
	hex_free(bl->hd);
	return res;
}

void bootloader_free(struct bootloader *bl)
{
	libusb_close(bl->handle);
	hex_free(bl->hd);
}
