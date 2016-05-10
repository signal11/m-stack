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
 * 2016-04-27
 */

#ifndef BL_PROTOCOL_H__
#define BL_PROTOCOL_H__

#include <stdint.h>

/* This file contains the protocol used to communicate between the firmware
 * and software components of the bootloader. It is shared by the PIC24 and
 * PIC32 versions of the bootloader */

/* Protocol commands */
#define CLEAR_FLASH 100
#define SEND_DATA 101
#define GET_CHIP_INFO 102
#define REQUEST_DATA 103
#define SEND_RESET 105

#define MAX_SKIP_REGIONS 10

struct skip_region {
	uint32_t base;
	uint32_t top;
};

struct chip_info {
	uint32_t user_region_base;
	uint32_t user_region_top;
	uint32_t config_words_base;
	uint32_t config_words_top;

	uint8_t bytes_per_instruction;
	uint8_t instructions_per_row;
	uint8_t number_of_skip_regions;
	uint8_t pad1;

	uint32_t reserved;
	uint32_t reserved2;

	struct skip_region skip_regions[10];
};

#endif /* BL_PROTOCOL_H__ */
