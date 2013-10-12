/*
 * M-Stack USB Bootloader
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

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if _MSC_VER && _MSC_VER < 1600
	#include "c99.h"
#else
	#include <inttypes.h>
	#include <stdbool.h>
#endif

#include "hex.h"
#include "bootloader.h"

#ifdef WIN32
	#include <Windows.h>
	#define sleep(X) Sleep(X*1000)
#else
	#include <unistd.h>
#endif

/* Change these for your application */
#define DEFAULT_VID 0xa0a0
#define DEFAULT_PID 0x0002

static bool verbose_output = false;
#define info(...) do { if (verbose_output) printf(__VA_ARGS__); } while(0)

static void print_usage(const char *prog_name)
{
	printf("Usage: %s [OPTION]... FILE\n", prog_name);
	printf("Flash firmware file.\n\n");
	printf("OPTIONS can be one of:\n");
	printf("  -d  --dev=VID:PID     USB VID/PID of the device to program\n");
	printf("  -v, --verify          verify program write\n");
	printf("  -l  --verbose         Verbose (loud) output\n");
	printf("  -r, --reset           reset device when done\n");
	printf("  -h, --help            print help message and exit\n\n");
	printf("Use a single hyphen (-) to read firmware hex file from stdin.\n");
}

static bool parse_vid_pid(const char *str, uint16_t *vid, uint16_t *pid)
{
	const char *colon;
	char *endptr;
	int val;

	/* check the format */
	if (!*str)
		return false;
	colon = strchr(str, ':');
	if (!colon)
		return false;
	if (!colon[1])
		return false;
	if (colon == str)
		return false;

	/* VID */
	val = strtol(str, &endptr, 16);
	if (*endptr != ':')
		return false;
	if (val > 0xffff)
		return false;
	*vid = val;

	/* PID */
	val = strtol(colon+1, &endptr, 16);
	if (*endptr != '\0')
		return false;
	if (val > 0xffff)
		return false;
	*pid = val;
	
	return true;
}

int main(int argc, char **argv)
{
	bool do_program = false;
	bool do_verify = false;
	bool do_reset = false;
	char **itr;
	const char *opt;
	const char *filename = NULL;
	uint16_t vid = 0, pid = 0;
	bool vidpid_valid = false;
	struct bootloader *bl;
	int res;

	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}
	
	/* Parse the command line. */
	itr = argv+1;
	opt = *itr;
	while (opt) {
		if (opt[0] == '-') {
			/* Option parameter */
			if (opt[1] == '-') {
				/* Long option, two dashes. */
				if (!strcmp(opt, "--help")) {
					print_usage(argv[0]);
					return 1;
				}
				else if (!strcmp(opt, "--reset"))
					do_reset = true;
				else if (!strcmp(opt, "--verify"))
					do_verify = true;
				else if (!strcmp(opt, "--verbose"))
					verbose_output = true;
				else if (!strncmp(opt, "--dev", 5)) {
					if (opt[5] != '=') {
						fprintf(stderr, "--dev requires vid/pid pair\n\n");
						return 1;
					}
					vidpid_valid = parse_vid_pid(opt+6, &vid, &pid);
					if (!vidpid_valid) {
						fprintf(stderr, "Invalid VID/PID pair\n\n");
						return 1;
					}
				}
				else {
					fprintf(stderr, "Invalid Parameter %s\n\n", opt);
					return 1;
				}
			}
			else {
				const char *c = opt + 1;
				if (!*c) {
					/* This is a parameter of a single
					   hyphen, which means read from
					   stdin. */
					filename = opt;
				}
				while (*c) {
					/* Short option, only one dash */
					switch (*c) {
					case 'v':
						do_verify = true;
						break;
					case 'r':
						do_reset = true;
						break;
					case 'l':
						verbose_output = true;
						break;
					case 'd':
						itr++;
						opt = *itr;
						if (!opt) {
							fprintf(stderr, "Must specify vid:pid after -d\n\n");
							return 1;
						}
						vidpid_valid = parse_vid_pid(opt, &vid, &pid);
						if (!vidpid_valid) {
							fprintf(stderr, "Invalid VID/PID pair\n\n");
							return 1;
						}
						break;
					default:
						fprintf(stderr, "Invalid parameter '%c'\n\n", *c);
						return 1;
					}
					c++;
				}
			}
		}
		else {
			/* Doesn't start with a dash. Must be the filename */
			if (filename) {
				fprintf(stderr, "Multiple filenames listed. This is not supported\n\n");
				return 1;
			}
			
			filename = opt;
			do_program = true;
		}
		itr++;
		opt = *itr;
	}

	vid = vidpid_valid? vid: DEFAULT_VID;
	pid = vidpid_valid? pid: DEFAULT_PID;
	
	if (!filename && !do_reset) {
	        fprintf(stderr, "No Filename specified. Specify a filename of use \"-\" to read from stdin.\n");
	        return 1;
	}

	/* Command line parsing is done. Do the programming of the device. */

	/* Open the device */
	info("Opening the bootloader device.\n");
	res = bootloader_init(&bl, filename, vid, pid);
	if (res == BOOTLOADER_CANT_OPEN_FILE) {
		fprintf(stderr, "Unable to open file %s\n", filename);
		return 1;
	}
	else if (res == BOOTLOADER_CANT_OPEN_DEVICE) {
		fprintf(stderr, "\nUnable to open device %04hx:%04hx "
			"for programming.\n"
			"Make sure that the device is connected "
			"and that you have proper permissions\nto "
			"open it.\n", vid, pid);
		return 1;
	}
	else if (res == BOOTLOADER_CANT_QUERY_DEVICE) {
		fprintf(stderr, "Unable to query device parameters\n");
		return 1;
	}
	else if (res == BOOTLOADER_MULTIPLE_CONNECTED) {
		fprintf(stderr, "Multiple devices are connected. Remove all but one.\n");
	}
	else if (res < 0) {
		fprintf(stderr, "Unspecified error initializing bootloader %d\n", res);
		return 1;
	}
	
	if (do_program) {
		/* Erase */
		info("Erasing flash.\n");
		res = bootloader_erase(bl);
		if (res < 0) {
			fprintf(stderr, "Erasing of device failed\n");
			return 1;
		}

		/* Program */
		info("Programming.\n");
		res = bootloader_program(bl);
		if (res < 0) {
			fprintf(stderr, "Programming of device failed\n");
			return 1;
		}
	}

	/* Verify */
	if (do_verify) {
		info("Verifying.\n");
		res = bootloader_verify(bl);
		if (res < 0) {
			fprintf(stderr, "Verification of programmed memory failed\n");
			return 1;
		}
	}

	/* Reset */
	if (do_reset) {
		info("Resetting the device.\n");
		res = bootloader_reset(bl);
		if (res < 0) {
			fprintf(stderr, "Device Reset failed\n");
			return 1;
		}
	}

	/* Close */
	bootloader_free(bl);

	return 0;
}
