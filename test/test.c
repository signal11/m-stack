/*********************************
Libusb test for USB Stack

Alan Ott
Signal 11 Software
2013-04-09
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


int main(void)
{
	libusb_device_handle *handle;
	unsigned char buf[64];
	int actual_length;
	int i;
	int res;
	
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
	
	while (res >= 0) {
		res = libusb_bulk_transfer(handle, 0x81, buf, sizeof(buf), &actual_length, 5000);
		if (res < 0) {
			perror("bulk transfer");
			return 1;
		}
		for (i = 0; i < actual_length; i++) {
			printf("%02hhx ", buf[i]);
			if ((i+1) % 8 == 0)
				printf("   ");
			if ((i+1) % 16 == 0)
				printf("\n");
		}
		printf("\n\n");
	}


	return 0;
}



