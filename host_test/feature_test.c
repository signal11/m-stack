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

This program will perform a series of tests involvinv setting and clearing
the endpoint halt feature for an endpoint.  The endpoint is hard-coded with
a #define (MY_ENDPOINT) lower in the file.
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

#define MY_EP_IN  0x81
#define MY_EP_OUT 0x01

#define trace printf

enum expected_failure {SHOULD_SUCCEED = 0, READ_SHOULD_FAIL=1, WRITE_SHOULD_FAIL=2, BOTH_SHOULD_FAIL=3};

static int halt_ep(libusb_device_handle *handle, int ep)
{
	int res;

	trace("halt ep %02x\n", ep);

	/* Set or clear endpoint halt condition */
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_ENDPOINT,
		LIBUSB_REQUEST_SET_FEATURE,
		0, /*wValue: 0=endpoint_halt*/
		ep, /*wIndex: Endpoint num*/
		0, 0/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		fprintf(stderr, "libusb_control_transfer (set feature): %s\n", libusb_error_name(res));
	}

	return res;
}

static int clear_halt(libusb_device_handle *handle, int ep)
{
	trace("clear halt %02x\n", ep);

	int res = libusb_clear_halt(handle, ep);
	if (res < 0) {
		fprintf(stderr, "libusb_clear_halt (set clear): %s\n", libusb_error_name(res));
	}
	return res;
}

static int write_ep(libusb_device_handle *handle, unsigned char *buf, size_t len, int should_fail)
{
	int actual_length;
	int res;

	trace("write ep\n");

	/* Send some data to the device */
	res = libusb_bulk_transfer(handle, MY_EP_OUT, buf, len, &actual_length, 5000);
	if (should_fail) {
		if (res == 0) {
			fprintf(stderr, "bulk transfer (out) should have failed but didn't\n");
		}
		return res;
	}

	if (res < 0) {
		fprintf(stderr, "bulk transfer (out): %s\n", libusb_error_name(res));
	}

	if (len != actual_length) {
		fprintf(stderr, "bulk transfer (out) Incorrect acutal length. len: %lu actual_len: %d\n", len, actual_length);
	}

	return res;
}

static int read_ep(libusb_device_handle *handle, unsigned char *buf, size_t len, int should_fail)
{
	int actual_length;
	int res;

	trace("read ep\n");

	/* Receive data from the device */
	res = libusb_bulk_transfer(handle, MY_EP_IN, buf, len, &actual_length, 5000);
	if (should_fail) {
		if (res == 0) {
			fprintf(stderr, "bulk transfer (in) should have failed but didn't\n");
		}
		return res;
	}

	if (res < 0) {
		fprintf(stderr, "bulk transfer (in): %s\n", libusb_error_name(res));
	}

	return res;
}

static int get_status(libusb_device_handle *handle, int ep, uint16_t *status)
{
	int res;
	unsigned char buf[2];

	trace("get status\n");

	/* Read the endpoint halt condition */
	res = libusb_control_transfer(handle,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_ENDPOINT,
		LIBUSB_REQUEST_GET_STATUS,
		0, /*wValue: */
		ep, /*wIndex: Endpoint num*/
		buf, 2/*wLength*/,
		1000/*timeout millis*/);

	if (res < 0) {
		fprintf(stderr, "libusb_control_transfer (get status (endpoint %x)): %s\n", ep, libusb_error_name(res));
	}

	*status = *(uint16_t*)buf;

	return (res < 0); /* >= 0 is success */
}

static int write_and_read(libusb_device_handle *handle, const unsigned char *buf, size_t len, enum expected_failure expected_failure)
{
	unsigned char work_buf[128];
	int res;

	trace("Write and read\n");

	if (len > sizeof(work_buf)) {
		printf("Too large of buffer\n");
		return -1;
	}

	memcpy(work_buf, buf, len);

	res = write_ep(handle, work_buf, len, expected_failure & WRITE_SHOULD_FAIL);
	if (res)
		return res;

	memset(work_buf, 0xaa, sizeof(work_buf));

	res = read_ep(handle, work_buf, len, expected_failure & READ_SHOULD_FAIL);
	if (res)
		return res;

	if (memcmp(buf, work_buf, len)) {
		printf("Data received is not correct\n");
		return -1;
	}

	return 0;
}


int main(int argc, char **argv)
{
	libusb_device_handle *handle;
	uint16_t status;
	unsigned char buf[63];
	int res;
	int i;

	/* Init Libusb */
	if (libusb_init(NULL))
		return -1;

	handle = libusb_open_device_with_vid_pid(NULL, 0xa0a0, 0x0001);

	if (!handle) {
		printf("libusb_open failed: \n");
		return 1;
	}

	res = libusb_claim_interface(handle, 0);
	if (res < 0) {
		perror("claim interface");
		return 1;
	}

	/* Load the output buffer */
	for (i = 0; i < sizeof(buf); i++) {
		buf[i] = i;
	}

	printf("*********  Read/write endpoint **********\n");

	buf[0] = 0xa1;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res)
		return 1;

	printf("*********  Halt Endpoint (IN) **********\n");

	res = halt_ep(handle, MY_EP_IN);
	if (res)
		return 1;

	res = get_status(handle, MY_EP_IN, &status);
	if (res)
		return 1;
	printf("EP Status %02hx\n", status);

	printf("*********  Read/write Endpoint (check for EPIPE) *********\n");

	buf[0] = 0xa2;
	res = write_and_read(handle, buf, sizeof(buf), READ_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}
	buf[0] = 0xa3;
	res = write_and_read(handle, buf, sizeof(buf), READ_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}
	buf[0] = 0xa4;
	res = write_and_read(handle, buf, sizeof(buf), READ_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}

	printf("*********  Clear Halt (IN) **********\n");

	res = clear_halt(handle, MY_EP_IN);
	if (res)
		return 1;

	printf("*********  Read/write endpoint **********\n");

	buf[0] = 0xa5;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}
	buf[0] = 0xa6;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}
	buf[0] = 0xa7;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}
#if 0
	buf[0] = 0xb7;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}
#endif

	res = get_status(handle, MY_EP_IN, &status);
	if (res)
		return 1;
	printf("EP Status %02hx\n", status);


	printf("*********  Halt Endpoint (OUT) **********\n");

	res = halt_ep(handle, MY_EP_OUT);
	if (res)
		return 1;

	res = get_status(handle, MY_EP_OUT, &status);
	if (res)
		return 1;
	printf("EP Status %02hx\n", status);

	printf("*********  Read/write Endpoint (check for EPIPE) *********\n");

	buf[0] = 0xb1;
	res = write_and_read(handle, buf, sizeof(buf), WRITE_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}
	buf[0] = 0xb2;
	res = write_and_read(handle, buf, sizeof(buf), WRITE_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}
	buf[0] = 0xb3;
	res = write_and_read(handle, buf, sizeof(buf), WRITE_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}
#if 0
	buf[0] = 0xb3;
	res = write_and_read(handle, buf, sizeof(buf), WRITE_SHOULD_FAIL);
	if (res != LIBUSB_ERROR_PIPE) {
		printf("This read should have returned ERROR_PIPE, but returned %s\n", libusb_error_name(res));
		return 1;
	}
#endif
	printf("*********  Clear Halt (OUT) **********\n");

	res = clear_halt(handle, MY_EP_OUT);
	if (res)
		return 1;

	res = get_status(handle, MY_EP_OUT, &status);
	if (res)
		return 1;
	printf("EP Status %02hx\n", status);

	printf("*********  Read/write endpoint **********\n");

	buf[0] = 0xa8;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}
	buf[0] = 0xa9;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}
	buf[0] = 0xaa;
	res = write_and_read(handle, buf, sizeof(buf), SHOULD_SUCCEED);
	if (res) {
		return 1;
	}

	res = get_status(handle, MY_EP_OUT, &status);
	if (res)
		return 1;
	printf("EP Status %02hx\n", status);



	printf("*********  Success  **********\n");

	return 0;
}
