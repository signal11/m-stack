/***********************************
 Intel Hex File Reader
 
 Alan Ott
 Signal 11 Software
 2013-04-28
**********************************/

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
void hex_free(struct hex_data *hd);

#endif // HEX_H__
