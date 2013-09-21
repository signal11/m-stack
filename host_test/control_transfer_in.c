/*
 * Libusb Control Transfer Test for M-Stack
 *
 * This file may be used by anyone for any purpose and may be used as a
 * starting point making your own application using M-Stack.
 *
 * It is worth noting that M-Stack itself is not under the same license as
 * this file.  See the top-level README.txt for more information.
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
 * 2013-04-17
 */

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


int main(int argc, char **argv)
{
	libusb_device_handle *handle;
	unsigned char buf[1024];
	int length;
	int i;
	int res;

	if (argc < 2) {
		fprintf(stderr, "%s: [bytes to request]\n", argv[0]);
		return 1;
	}

	length = atoi(argv[1]);
	if (length > sizeof(buf))
		length = sizeof(buf);

	printf("Asking for %d bytes\n", length);


	/* Init Libusb */
	if (libusb_init(NULL))
		return -1;

	handle = libusb_open_device_with_vid_pid(NULL, 0xa0a0, 0x0001);
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

	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_RECIPIENT_OTHER,
		245,
		0, /*wValue: 0=endpoint_halt*/
		0, /*wIndex: Endpoint num*/
		buf, length/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		fprintf(stderr, "control transfer (in): %s\n", libusb_error_name(res));
		return 1;
	}

	for (i = 0; i < length; i++) {
		printf("%02hhx ", buf[i]);
		if ((i + 1) % 8 == 0)
			printf("  ");
		if ((i + 1) % 16 == 0)
			printf("\n");
	}
	puts("\n");

	return 0;
}
