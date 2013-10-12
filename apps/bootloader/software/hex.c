/*
 * M-Stack Intel Hex File Reader
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
 * 2013-04-28
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if _MSC_VER && _MSC_VER < 1600
	#include "c99.h"
#else
	#include <stdint.h>
	#include <stdbool.h>
#endif

#ifdef _MSC_VER
	#pragma warning (disable:4996)
#endif

#ifdef WIN32
	#include <winsock2.h>
#else
	#include <arpa/inet.h>
#endif

#include "hex.h"
#include "log.h"

#define LINE_LENGTH 1024

/* Intel Hex File format record types */
enum {
	REC_DATA = 0,
	REC_EOF = 1,
	REC_EXTENDED_SEGMENT_ADDRESS = 2,
	REC_START_SEGMENT_ADDRESS = 3,
	REC_EXTENDED_LINEAR_ADDRESS = 4,
	REC_START_LINEAR_ADDRESS = 5,
};

/* Character offsets for each piece of data in a record (line). */ 
enum {
	START_INDEX = 0,
	BYTE_COUNT_INDEX = 1,
	ADDRESS_INDEX = 3,
	RECORD_TYPE_INDEX = 7,
	DATA_INDEX = 9,
};


#define MAX(X,Y) ((X)>(Y)? (X): (Y))

static uint8_t read_byte(const char *line, size_t offset)
{
	char chars[3];
	char *endptr;
	uint8_t res;
	
	chars[0] = line[offset];
	chars[1] = line[offset+1];
	chars[2] = '\0';

	res = (uint8_t) strtoul(chars, &endptr, 16);
	
	return res;
}

static uint16_t read_short(const char *line, size_t offset)
{
	char chars[6];
	char *endptr;
	uint16_t res;
	
	memcpy(chars, line + offset, 4);
	chars[5] = '\0';

	res = (uint16_t) strtoul(chars, &endptr, 16);
	
	return res;
}

static bool
create_update_region(struct hex_data *hd, size_t address, uint8_t len) {
	struct hex_data_region *r;
	bool done = false;

	r = hd->regions;
	while (r) {
		if (address == r->address + r->len) {
			/* Address range is exactly at the end of this region.
			   Extend this region to include it. */
			   r->len += len;
			   done = true;
		}
		else if (address + len == r->address) {
			/* Address range is exactly before this region. Extend
			   this region backward to include it */
			   r->address -= len;
			   r->len += len;
			   done = true;
		}
		else if (address <= r->address && address + len > r->address) {
			/* Address range overlaps the beginning of this region.
			   This is an error. */
			return false;
		}
		else if (address >= r->address && address < r->address + r->len) {
			/* Address range overlaps the middle or possibly the
			   the end of this region. This is an error */
			return false;
		}
		
		r = r->next;
	}

	if (!done) {
		/* Need to create a new region */
		r = calloc(1, sizeof(struct hex_data_region));
		r->address = address;
		r->len = len;
		
		if (!hd->regions) {
			/* The new one is the only region */
			hd->regions = r;
		}
		else {
			/* Add it to the end of the list */
			struct hex_data_region *iter = hd->regions;
			while(iter->next) {
				iter = iter->next;
			}
			iter->next = r;
		}
	}
	
	return true;
}

static struct hex_data_region *
find_region(const struct hex_data *hd, size_t address, uint8_t len)
{
	struct hex_data_region *r;

	r = hd->regions;
	while (r) {
		if (address >= r->address &&
		    address + len <= r->address + r->len) {
			return r;
		}
		r = r->next;
	}

	return NULL;
}

enum hex_error_code hex_load(const char *filename, struct hex_data **data_out)
{
	FILE *fp;
	char line[LINE_LENGTH];
	size_t extended_addr = 0;
	struct hex_data *hd;
	enum hex_error_code ret = HEX_ERROR_OK;
	struct hex_data_region *iter;


	fp = fopen(filename, "r");
	if (!fp)
		return HEX_ERROR_CANT_OPEN_FILE;

	/* Set up the return structure */
	hd = malloc(sizeof(struct hex_data));
	hd->regions = NULL;
	
	log_hex("Checking HEX file for data integrity...\n");
	
	/* First pass: check for data integrity and create the
	 * hex_data_region objects which which will contain the data. */
	while (!feof(fp)) {
		char *res;
		int len;
		uint8_t record_type;
		uint8_t byte_count;
		int i;
		uint8_t sum = 0;
		
		res = fgets(line, sizeof(line), fp);
		if (!res)
			break;
		
		/* Eliminate the trailing newline */
		len = strlen(line);
		if (line[len-1] == '\n') {
			line[len-1] = '\0';
			len--;
		}

		/* Eliminate the trailing CR (if there is one) */
		len = strlen(line);
		if (line[len-1] == '\r') {
			line[len-1] = '\0';
			len--;
		}
		
		/* Make sure the record is long enough */
		if (len < 11) {
			ret = HEX_ERROR_FILE_LOAD_ERROR;
			goto out;
		}

		record_type = read_byte(line, RECORD_TYPE_INDEX);
		byte_count = read_byte(line, BYTE_COUNT_INDEX);

		/* Make sure there are the right number of data bytes */
		if (len != byte_count * 2 + 11) {
			ret = HEX_ERROR_FILE_LOAD_ERROR;
			goto out;
		}
		
		/* Read each byte and calculate the checksum */
		for (i = 1; i < len; i += 2) {
			sum += read_byte(line, i);
		}

		/* Verify the checksum */
		if (sum != 0) {
			ret = HEX_ERROR_FILE_LOAD_ERROR;
			goto out;
		}

		switch (record_type) {
		bool res;
		case REC_DATA:
			res = create_update_region(
				hd,
				extended_addr + read_short(line, ADDRESS_INDEX),
				byte_count);
				
			if (!res) {
				ret = HEX_ERROR_FILE_LOAD_ERROR;
				goto out;
			}
			break;
		case REC_EOF:
			break;
		case REC_EXTENDED_SEGMENT_ADDRESS:
			extended_addr = read_short(line, DATA_INDEX) << 4;
			break;
		case REC_EXTENDED_LINEAR_ADDRESS:
			extended_addr = read_short(line, DATA_INDEX) << 16;
			break;
		default:
			fprintf(stderr, "Hex file load: Unsupported Record type: 0x%02hhx\n", record_type);
			ret = HEX_ERROR_UNSUPPORTED_RECORD;
			goto out;
			break;
		}
	}

	log_hex("Integrity check passed.\n");
	log_hex("Parsing data...\n");

	/* Allocate the memory buffer for each hex_data_region. */
	iter = hd->regions;
	while(iter) {
		iter->data = malloc(iter->len);
		memset(iter->data, 0xff, iter->len);
		
		iter = iter->next;
	}

	/* Second pass: Load the data into the hex_data_region buffers. */
	extended_addr = 0;
	rewind(fp);
	
	while (!feof(fp)) {
		char *res;
		uint8_t record_type;
		
		res = fgets(line, sizeof(line), fp);
		if (!res)
			break;
		
		record_type = read_byte(line, RECORD_TYPE_INDEX);
		switch (record_type) {
		case REC_DATA:
		{
			int i;
			size_t offset;
			int byte_count = read_byte(line, BYTE_COUNT_INDEX);
			size_t addr = extended_addr +
					read_short(line, ADDRESS_INDEX);
			struct hex_data_region *region =
					find_region(hd, addr, byte_count);

			if (!region) {
				ret = HEX_ERROR_FILE_LOAD_ERROR;
				goto out;
			}
				
			log_hex("Reading %3d bytes at %06lx\n", byte_count, addr);
			offset = addr - region->address;
			for (i = 0; i < byte_count; i++) {
				region->data[offset + i] =
					read_byte(line, DATA_INDEX + 2 * i);
			}
			break;
		}
		case REC_EOF:
			break;
		case REC_EXTENDED_SEGMENT_ADDRESS:
			extended_addr = read_short(line, DATA_INDEX) << 4;
			log_hex("Setting Extended addr: %lx\n", extended_addr);
			break;
		case REC_EXTENDED_LINEAR_ADDRESS:
			extended_addr = read_short(line, DATA_INDEX) << 16;
			log_hex("Setting Extended addr2: %lx\n", extended_addr);
			break;
		default:
			fprintf(stderr, "Hex file load: Unsupported Record type: 0x%02hhx\n", record_type);
			ret = HEX_ERROR_UNSUPPORTED_RECORD;
			goto out;
			break;
		}
	}

	log_hex("Hex data parsed successfully.\n");

	*data_out = hd;
	fclose(fp);
	return ret;
out:
	fclose(fp);
	hex_free(hd);
	return ret;
}

void hex_init_empty(struct hex_data **data)
{
	struct hex_data *hd;
	hd = malloc(sizeof(struct hex_data));
	hd->regions = NULL;
	*data = hd;
}

void hex_free(struct hex_data *hd)
{
	struct hex_data_region *r;

	r = hd->regions;
	while (r) {
		struct hex_data_region *this_one = r;
		r = r->next;
		free(this_one);
	}

	free(hd);
}
