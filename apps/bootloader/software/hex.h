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

#ifndef HEX_H__
#define HEX_H__

struct hex_data_region {
	size_t address;
	unsigned char *data;
	size_t len;
	
	struct hex_data_region *next;
};

struct hex_data {
	/* Linked-list of data regions */
	struct hex_data_region *regions;
};

enum hex_error_code {
	HEX_ERROR_OK = 0,
	HEX_ERROR_CANT_OPEN_FILE = -1,
	HEX_ERROR_FILE_LOAD_ERROR = -2,
	HEX_ERROR_UNSUPPORTED_RECORD = -3,
	HEX_ERROR_DATA_TOO_LARGE = -4,
};

enum hex_error_code hex_load(const char *filename, struct hex_data **data);
void hex_init_empty(struct hex_data **data);
void hex_free(struct hex_data *hd);

#endif // HEX_H__
