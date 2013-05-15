/*********************************
 PIC24 Bootloader 
 
 Alan Ott
 Signal 11 Software
 2013-04-29
*********************************/

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <errno.h>

/* Unix */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <pthread.h>
#include <wchar.h>

/* GNU / LibUSB */
#include "libusb.h"

#include "hex.h"

/* Protocol commands */
#define CLEAR_FLASH 100
#define SEND_DATA 101
#define GET_CHIP_INFO 102
#define REQUEST_DATA 103
#define SEND_RESET 105

#define MIN(X,Y) ((X)<(Y)? (X): (Y))

/* Shared with the firmware */
struct chip_info {
	uint32_t user_region_base;
	uint32_t user_region_top;
	uint32_t config_words_base;
	uint32_t config_words_top;

	uint8_t bytes_per_instruction;
	uint8_t instructions_per_row;
	uint8_t pad0;
	uint8_t pad1;
};

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
		fprintf(stderr, "Error clearing flash : %s\n", libusb_error_name(res));
		return -1;
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
		fprintf(stderr, "Error Sending Data : %s\n", libusb_error_name(res));
		return -1;
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
		fprintf(stderr, "Error request chip info: %s\n", libusb_error_name(res));
		return -1;
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
		fprintf(stderr, "Error requesting data: %s\n", libusb_error_name(res));
		return -1;
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
		fprintf(stderr, "Error Sending Reset: %s\n", libusb_error_name(res));
		return -1;
	}

	return 0;
}


static void print_data(const unsigned char *data, size_t len)
{
	int i;
	
	for (i = 0; i < len; i++) {
		printf("%02hhx ", data[i]);
		if ((i+1) % 8 == 0)
			printf("  ");
		if ((i+1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
}


int main(int argc, char **argv)
{
	struct hex_data *hd;
	struct hex_data_region *region;
	int res;
	libusb_device_handle *handle;
	unsigned char buf[1024];
	int i;
	struct chip_info chip_info;
	int bytes_per_row;

	if (argc < 2) {
		fprintf(stderr, "%s [hex_file]\n", argv[0]);
		return 1;
	}

	/* Load the Hex file */
	
	res = hex_load(argv[1], &hd);
	
	if (res < 0) {
		printf("Unable to load hex file: %d\n", res);
		return 1;
	}

	region = hd->regions;
	while (region) {
		printf("Data Region at %08lx for %4lx\n", region->address, region->len);
		region = region->next;
	}
	
	/* Init Libusb */
	if (libusb_init(NULL))
		return -1;

	handle = libusb_open_device_with_vid_pid(NULL, 0xa0a0, 0x0002);
	if (!handle) {
		perror("libusb_open failed: ");
		return 1;
	}

	res = libusb_claim_interface(handle, 0);
	if (res < 0) {
		return 1;
	}

	for (i = 0; i < sizeof(buf); i++) {
		buf[i] = i;
	}

	res = get_chip_info(handle, &chip_info);
	if (res < 0) {
		fprintf(stderr, "Can't get chip info\n");
		return 1;
	}
	
	bytes_per_row = chip_info.bytes_per_instruction *
	                chip_info.instructions_per_row;
	
	printf("bytes per inst: %d\n inst per row %d\n",
	       chip_info.bytes_per_instruction,
	       chip_info.instructions_per_row);

	printf("Clearing flash\n");
	clear_flash(handle);
	printf("Done\n");

	/* Send each memory region to the device */
	region = hd->regions;
	while (region) {
		const unsigned char *ptr = region->data;
		const unsigned char *endptr = region->data + region->len;
		size_t address = region->address;
		
		if (address >= chip_info.config_words_base &&
		    address < chip_info.config_words_top) {
			printf("Skipping Config words at %lx\n", address);
			goto end_region;
		}
			

		/* If the data isn't aligned to a row, add some extra padding
		 * bytes (0xff) to the beginning of the first packet so that
		 * all the packets align. */
		if (address & (bytes_per_row-1)) {
			unsigned char *buf;
			size_t data_bytes_to_send;
			size_t total_bytes_to_send;
			size_t bytes_to_add;
			
			bytes_to_add = address & (bytes_per_row-1);
			address -= bytes_to_add;
			total_bytes_to_send = MIN(region->len + bytes_to_add,
			                          bytes_per_row);
			data_bytes_to_send = MIN(region->len,
			                         bytes_per_row - bytes_to_add);
			buf = malloc(total_bytes_to_send);
			memset(buf, 0xff, total_bytes_to_send);
			memcpy(buf+bytes_to_add, region->data, data_bytes_to_send);

			printf("Padding block at %lx down to %lx\n", region->address, address);
			res = send_data(handle, address, buf, total_bytes_to_send);
			if (res < 0) {
				fprintf(stderr, "Sending data block: %s\n", libusb_error_name(res));
				break;
			}

			ptr += data_bytes_to_send;
			address += total_bytes_to_send;

			free(buf);
		}

		
		while (ptr < endptr) {
			size_t len_to_send = MIN(bytes_per_row, endptr-ptr);

			res = send_data(handle, address, ptr, len_to_send);
			if (res < 0) {
				fprintf(stderr, "Sending data block: %s\n", libusb_error_name(res));
				break;
			}

			ptr += len_to_send;
			address += len_to_send;
		}

end_region:
		region = region->next;
	}

	printf("Beginning Verify\n");

	/* Verify */
	region = hd->regions;
	while (region) {
		const unsigned char *ptr = region->data;
		const unsigned char *endptr = region->data + region->len;
		size_t address = region->address;
		unsigned char buf[128];

		if (address >= chip_info.config_words_base &&
		    address < chip_info.config_words_top) {
			printf("Skipping Config words at %lx\n", address);
			goto end_region_verify;
		}
		
		while (ptr < endptr) {
			size_t len_to_request = MIN(sizeof(buf), endptr-ptr);

			res = request_data(handle, address, buf, len_to_request);
			if (res < 0) {
				fprintf(stderr, "Reading data block: %s\n", libusb_error_name(res));
				return 1;
			}
			
			if (memcmp(ptr, buf, len_to_request) != 0) {
				fprintf(stderr, "Verify Failed on block starting at %lx\n", address);

				printf("Read from device: \n");
				print_data(ptr, len_to_request);
				
				printf("\nExpected:\n");
				print_data(buf, len_to_request);
				
				return 1;
			}

			ptr += len_to_request;
			address += len_to_request;
		}

end_region_verify:
		region = region->next;
	}
	
	/* Reset */
	send_reset(handle);

	return 0;
};
