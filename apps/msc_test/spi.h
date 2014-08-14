/*
 * SPI driver for PIC18, PIC24, and PIC32MX
 *
 * Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *                    Signal 11 Software
 *
 * 2013-02-08
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
 */

#ifndef S11_PIC_SPI_H__
#define S11_PIC_SPI_H__

#include <stdint.h>
#include <stdbool.h>
#include "board.h"

/* SPI Interface for MMC
 *
 * This is an SPI interface intended to be the back-end for the MMC component.
 * It is not a generic SPI implementation. To create such an implementation
 * is beyond the scope of M-Stack. Further, a generic implementation of SPI
 * would require more code space than would be acceptable on some of the
 * smaller supported microcontrollers (namely the 8-bit PICs).
 *
 * Thus, this implementation really only exists to serve the supported boards
 * in their specific configurations. However, modification for a custom board
 * should be easy and straight forward.
 *
 * This SPI interface is configured from board.h, which contains definitions
 * specific to the supported boards.
 */

/* Set the chip select line, 0 for low and 1 for high. This SPI implementation
 * is active-low, and that's how the MMC component will call this function. */
#define spi_set_cs_line(value/*0=low,1=high*/) \
	do { \
		if (value) { \
			SPI_RELEASE_CS(); \
		} \
		else { \
			SPI_ASSERT_CS(); \
		} \
	} while(0);

/* Initialize the SPI interface. */
void spi_init(void);

/* Set the SPI speed. In the common case where the speed cannot be matched
 * exactly, the closest speed below the target speed will be selected. */
void spi_set_speed_hz(uint32_t speed_hz);

/* Perform a bi-directional transfer of data over the SPI interface. The
 * parameters in_buf and out_buf can either or both be NULL if data is not
 * required to be sent in the respective direction. This is common in the
 * MMC card protocol. Also common in the MMC protocol is where out_buf and
 * in_buf are both NULL, which will send 0xff and ignore the input. */
int8_t spi_transfer_block(const uint8_t *out_buf, uint8_t *in_buf, uint16_t len);

#endif /* S11_PIC_SPI_H__ */
