/*
 * Libusb set/clear feature test for M-Stack
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
 * 2013-04-13
 */

/*
Libusb set/clear feature test for M-Stack

This program will set/clear the endpoint halt feature for an endpoint. Run
with no arguments to set the endpoint halt feature, and run with a single
parameter of "clear" to clear the endpoint halt feature. The endpoint is
hard-coded with a #define (MY_ENDPOINT) lower in the file.

This is just program for exercising the one of the less-commonly encountered
requests and verifying functionality.

What I do:
 ./test          # see that data is received from EP 1 IN
 ./feature       # set endpoint halt on EP 1 IN
 ./test          # verify no data is received (verify STALL packets on EP 1 IN)
 ./feature clear # clear the endpoint halt on EP 1 IN
 ./test          # see that data is received from EP 1 IN
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
	unsigned char buf[64];
	int res;
	int clear = 0;
	
	if (argc == 2) {
		if (!strcmp(argv[1], "clear"))
			clear = 1;
		else {
			fprintf(stderr, "invalid arg\n");
			return 1;
		}
	}
	
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
		perror("claim interface");
		return 1;
	}

	#define MY_ENDPOINT 0x81

	/* Set or clear endpoint halt condition */
	if (clear) {
		res = libusb_clear_halt(handle, MY_ENDPOINT);
	}
	else {
		res = libusb_control_transfer(handle,
			LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_ENDPOINT,
			LIBUSB_REQUEST_SET_FEATURE,
			0, /*wValue: 0=endpoint_halt*/
			MY_ENDPOINT, /*wIndex: Endpoint num*/
			0, 0/*wLength*/,
			1000/*timeout millis*/);
	}

	if (res < 0) {
		fprintf(stderr, "libusb_control_transfer (set feature): %s\n", libusb_error_name(res));
		return 1;
	}

	/* Read the endpoint halt condition */
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_ENDPOINT,
		LIBUSB_REQUEST_GET_STATUS,
		0, /*wValue: */
		MY_ENDPOINT, /*wIndex: Endpoint num*/
		buf, 2/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		fprintf(stderr, "libusb_control_transfer (get status (endpoint)): %s\n", libusb_error_name(res));
		return 1;
	}
	printf("EP Status %02hx\n", *(uint16_t*)buf);

	return 0;
}
