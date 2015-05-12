/*
 *  M-Stack USB Device Stack - MMC/SD card implementation
 *  Copyright (C) 2014 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2014 Signal 11 Software
 *
 *  2014-05-31
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

#if defined(__XC32__) || defined(__XC16__) || defined(__XC8)
#include <xc.h>
#else
#error "Compiler not supported"
#endif

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "crc.h"
#include "mmc.h"

/* Response Lengths: 7.3.2 */
#define RESP_R1_LEN 1
#define RESP_R2_LEN 2
#define RESP_R3_LEN 5
#define RESP_R7_LEN 5

/* Response Flags from R1 Response */
#define RESP_IN_IDLE_STATE 0x1
#define RESP_ERASE_RESET 0x2
#define RESP_ILLEGAL_COMMAND 0x4
#define RESP_COM_CRC_ERROR 0x8
#define RESP_ERASE_SEQUENCE_ERROR 0x10
#define RESP_ADDRESS_ERROR 0x20
#define RESP_PARAMETER_ERROR 0x40

#define CMD_LEN 6
#define CHECK_PATTERN 0xa0 /* For CMD8. The sent pattern is echoed back. */

#define NUM_READ_RETRIES 32768
#define NUM_WRITE_RETRIES 65535
#define NUM_ACMD41_RETRIES 32768

#define MMC_COMMAND_TIMEOUT  150 /* milliseconds (made up, not in spec) */
#define MMC_READ_TIMEOUT     150 /* milliseconds (4.6.2.1) */
#define MMC_WRITE_TIMEOUT    500 /* milliseconds (4.6.2.2) */

#ifndef MMC_USE_TIMER
	#undef  MMC_TIMER_START
	#undef  MMC_TIMER_EXPIRED
	#undef  MMC_TIMER_STOP

	#define MMC_TIMER_START(spi, timeout) do { } while(0)
	#define MMC_TIMER_EXPIRED(spi) false
	#define MMC_TIMER_STOP(spi)  do { } while(0)
#endif

#define MIN(x,y) (((x)<(y))?(x):(y))

/* Debugging Defines
 *
 * Define either (or both) of the following for debugging:
 *   MMC_DEBUG       - Read a few sectors for inspection during mmc_init()
 *   MMC_DEBUG_WRITE - Destructively write and read a sector during mmc_init().
 *
 * The design for MMC_DEBUG is that breakpoints can be set after each read
 * in mmc_init(), and the data can be inspected from the static array named
 * "read" defined below.
 *
 * MMC_DEBUG_WRITE will automatically write to a sector, read it back, and
 * verify the contents.
 */

#ifdef MMC_DEBUG
static uint8_t read[1024];
#endif

static void reset_state(struct mmc_card *cd)
{
	cd->card_ccs = false;
	cd->state = MMC_STATE_IDLE;
	cd->card_size_blocks = 0;
	cd->write_position = 0;
	cd->checksum = 0;
}

int8_t mmc_init(struct mmc_card *card_data, uint8_t count)
{
	uint8_t i;

	for (i = 0; i < count; i++) {
		reset_state(&card_data[i]);
	}

	return 0;
}

/* Calculate the max speed in Hz based on the TRANS_SPEED field of the
 * CSD register, section 5.3.2. Return 0 on failure. */
static uint32_t calculate_speed(uint8_t tran_speed)
{
	/* Time values from table 5-6 (section 5.3.2). Time vals are
	 * multiplied by 10 and transfer rates are divided by 10 from their
	 * values in the table; this way there's no floating point. */
	uint8_t time_vals[] = {  0, 10, 12, 13, 15, 20, 25, 30,
	                        35, 40, 45, 50, 55, 60, 70, 80 };
	uint32_t transfer_rates[] = { 10000, 100000, 1000000, 10000000 };

	uint8_t time_val_code = tran_speed >> 3 & 0xf;
	if (time_val_code > 0xf || time_val_code == 0)
		return 0;

	uint32_t transfer_unit = tran_speed & 0x7;
	if (transfer_unit > 3)
		return 0;

	return time_vals[time_val_code] * transfer_rates[transfer_unit];
}

/* Skip specific-valued bytes from the SPI interface. The MMC card will
 * return 0xff while it is busy processing a command or reading, and will
 * return 0x0 while it is busy writing to the flash memory.
 *
 * This function will read from the SPI up to retries times and for up to
 * timeout_milliseconds or until a character other than the skip_character
 * is read. It will then place that non-skip_character into read_char.
 *
 * If a timeout occurs or if the SPI is read more than retries times
 * without finding a non-skip_character, then -1 will be returned.
 * Otherwise 0 is returned and read_char is set to the first
 * non-skip_character which was read.
 *  */
static int8_t skip_bytes_timeout(uint8_t spi_instance,
                                 uint8_t skip_character,
                                 uint8_t *read_char,
                                 uint16_t timeout_milliseconds,
                                 uint16_t retries)
{
	uint8_t c, res;
	uint16_t i;

	MMC_TIMER_START(spi_instance, timeout_milliseconds);

	/* Wait for Response */
	for (i = 0;
	     i < retries && !MMC_TIMER_EXPIRED(spi_instance);
	     i++) {
		MMC_SPI_TRANSFER(spi_instance, NULL, &c, 1);
		if (c != skip_character) {
			*read_char = c;
			res = 0;
			goto out;
		}
	}

	res = -1;
out:
	MMC_TIMER_STOP(spi_instance);
	return res;
}

/* Send an MMC command. The __ version of this function does not set/clear
 * the chip select (CS) or send the extra clocks at the end. See
 * send_mmc_command() for a more general purpose version of this function. */
static int8_t __send_mmc_command(uint8_t spi_instance, uint8_t *buf,
                                 uint8_t cmd_len, uint8_t resp_len)
{
	int8_t res;
	uint8_t crc7 = 0;
	uint16_t i;

	/* Add the CRC into byte 5. */
	for (i = 0; i < 5; i++)
		crc7 = add_crc7(crc7, buf[i]);
	buf[5] = (crc7 << 1) | 0x1;

	MMC_SPI_TRANSFER(spi_instance, buf, NULL, cmd_len);

	/* Skip 0xff characters which come before the response */
	res = skip_bytes_timeout(spi_instance, 0xff, &buf[0],
	                         MMC_COMMAND_TIMEOUT, NUM_READ_RETRIES);

	if (res < 0)
		goto out;

	if (buf[0] & 0x80) {
		/* Upper bit is set. Protocol error. */
		res = -1;
		goto out;
	}

	/* Upper bit is clear; this is a response byte. Read the rest. */
	if (resp_len > 1)
		MMC_SPI_TRANSFER(spi_instance, NULL, buf+1, resp_len-1);

out:
	return res;
}

static int8_t send_mmc_command(uint8_t spi_instance, uint8_t *buf,
                               uint8_t cmd_len, uint8_t resp_len)
{
	int8_t res;

	MMC_SPI_SET_CS(spi_instance, 0);
	res = __send_mmc_command(spi_instance, buf, cmd_len, resp_len);
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	return res;
}

static void blank_clock(uint8_t spi_instance)
{
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 12);
}

/* This is used for reading a block of data from a READ_SINGLE_BLOCK or from
 * a SEND_CSD.*/
static int8_t __read_data_block(struct mmc_card *mmc,
                                uint8_t *data,
                                uint16_t len)
{
	uint8_t spi_instance = mmc->spi_instance;
	uint16_t ck; /* Checksum */
	int8_t res = 0;
	uint8_t token;
	uint8_t csum_bytes[2];

	/* Skip 0xff bytes waiting for the read token (7.3.3). */
	res = skip_bytes_timeout(spi_instance, 0xff, &token,
	                         MMC_READ_TIMEOUT, NUM_READ_RETRIES);
	if (res < 0)
		goto card_error;

	if (~token & 0xf0) {
		/* Data error Token (7.3.3.3) */
		goto read_error;
	}

	/* 0xfe is the data start token (7.3.3.2). */
	if (token != 0xfe) {
		goto card_error;
	}

	MMC_SPI_TRANSFER(spi_instance, NULL, data, len);     /* Read the data */
	MMC_SPI_TRANSFER(spi_instance, NULL, csum_bytes, 2); /* Read the CRC */

	/* Calculate the checksum over the data. */
	ck = 0;
	ck = add_crc16_array(ck, data, len);
	ck = add_crc16_array(ck, csum_bytes, sizeof(csum_bytes));

	/* Verify the checksum */
	if (ck != 0)
		goto read_error;

	return 0;

card_error:
	/* card_error is a protocol error with the communication.
	 * In this case, the card will need to be reset. */
	mmc->state = MMC_STATE_IDLE;
read_error:
	/* read_error means a read operation failed, but the
	 * protocol is still intact. */
	return -1;
}

uint32_t mmc_get_num_blocks(struct mmc_card *mmc)
{
	if (mmc->state == MMC_STATE_IDLE)
		return 0;

	return mmc->card_size_blocks;
}

bool mmc_ready(struct mmc_card *mmc)
{
	uint8_t buf[6];

	if (mmc->state == MMC_STATE_IDLE)
		return false;
	if (mmc->state == MMC_STATE_WRITE_MULTIPLE)
		return true;

	/* Issue SPI CMD13: SEND_STATUS */
	buf[0] = 0x40 | 55;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	send_mmc_command(mmc->spi_instance, buf, CMD_LEN, RESP_R1_LEN);

	if (buf[0] != 0x0) {
		mmc->state = MMC_STATE_IDLE;
		return false;
	}

	return true;
}

bool mmc_is_initialized(struct mmc_card *mmc)
{
	return mmc->state != MMC_STATE_IDLE;
}

void mmc_set_uninitialized(struct mmc_card *mmc)
{
	mmc->state = MMC_STATE_IDLE;
}

int8_t mmc_read_block(struct mmc_card *mmc,
                      uint32_t block_addr,
                      uint8_t *data)
{
	uint8_t buf[6];
	uint8_t spi_instance = mmc->spi_instance;
	int8_t res = 0;

	/* Range check the starting addr against the card size. */
	if (block_addr >= mmc->card_size_blocks)
		return -1;

	/* For SDSC cards, the address specified is the byte address. For
	 * SDHC and SDXC cards, the address specified is the block address */
	if (!mmc->card_ccs)
		block_addr *= 512;

	/* Send CMD17: READ_SINGLE_BLOCK */
	buf[0] = 0x40 | 17;
	buf[1] = (block_addr & 0xff000000) >> 24;
	buf[2] = (block_addr & 0x00ff0000) >> 16;
	buf[3] = (block_addr & 0x0000ff00) >> 8;
	buf[4] = block_addr & 0x000000ff;

	MMC_SPI_SET_CS(spi_instance, 0);
	res = __send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R1_LEN);
	if (res < 0) {
		mmc->state = MMC_STATE_IDLE;
		goto err;
	}

	if (buf[0] != 0x0) {
		res = -1;
		mmc->state = MMC_STATE_IDLE;
		goto err;
	}

	res = __read_data_block(mmc, data, 512);

err:
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	return res;
}

/* Perform the sequence shown in section 7.2.4, figure 7-6. */
int8_t mmc_write_block(struct mmc_card *mmc,
                       uint32_t block_addr,
                       uint8_t *data)
{
	uint8_t buf[6];
	uint8_t spi_instance = mmc->spi_instance;
	uint16_t ck;
	int8_t res = 0;

	/* Range check the starting addr against the card size. */
	if (block_addr >= mmc->card_size_blocks)
		return -1;

	/* For SDSC cards, the address specified is the byte address. For
	 * SDHC and SDXC cards, the address specified is the block address */
	if (!mmc->card_ccs)
		block_addr *= 512;

	/* Send CMD24: WRITE_SINGLE_BLOCK */
	buf[0] = 0x40 | 24;
	buf[1] = (block_addr & 0xff000000) >> 24;
	buf[2] = (block_addr & 0x00ff0000) >> 16;
	buf[3] = (block_addr & 0x0000ff00) >> 8;
	buf[4] = block_addr & 0x000000ff;

	MMC_SPI_SET_CS(spi_instance, 0);
	res = __send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R1_LEN);
	if (res < 0)
		goto card_error;

	if (buf[0] != 0x0) {
		res = -1;
		goto card_error;
	}

	buf[0] = 0xfe; /* Start Block Token (7.3.3.2) */
	MMC_SPI_TRANSFER(spi_instance, buf, NULL, 1);
	MMC_SPI_TRANSFER(spi_instance, data, NULL, MMC_BLOCK_SIZE);

	/* Calculate the checksum over the data and send it. */
	ck = 0;
	ck = add_crc16_array(ck, data, MMC_BLOCK_SIZE);
	buf[0] = ck & 0xff;
	buf[1] = (ck >> 8) & 0xff;
	MMC_SPI_TRANSFER(spi_instance, buf, NULL, 2);

	/* Skip 0xff bytes before the response */
	res = skip_bytes_timeout(
	                     spi_instance, 0xff, &buf[0],
			     MMC_COMMAND_TIMEOUT, NUM_READ_RETRIES);

	if (res < 0)
		goto card_error;

	/* Read the data response (7.3.3.1) */
	if ((buf[0] & 0x1f) != 0x05) {
		res = -1;
		goto out;
	}

	/* Skip the busy bytes (0x00) which the MMC card sends while writing */
	res = skip_bytes_timeout(spi_instance, 0x0, &buf[0],
	                         MMC_WRITE_TIMEOUT, NUM_WRITE_RETRIES);

	if (res < 0)
		goto card_error;

out:
	/* End the write operation, successful or not. */
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	/* Give up if the data response indicated failure */
	if (res < 0)
		return -1;

	/* Issue SPI CMD13: SEND_STATUS to make sure the
	 * write completed successfully */
	buf[0] = 0x40 | 13;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	res = send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R2_LEN);
	if (res < 0)
		goto card_error_no_cs;

	/* A non-zero repsonse (R2, 16-bit) indicates write failure */
	if (buf[0] != 0 || buf[1] != 0)
		return -1;

	return 0;

card_error:
	/* End the write operation, successful or not. */
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

card_error_no_cs:
	mmc->state = MMC_STATE_IDLE;

	return -1;
}

int8_t mmc_multiblock_write_start(struct mmc_card *mmc, uint32_t block_addr)
{
	uint8_t buf[6];
	uint8_t spi_instance = mmc->spi_instance;
	int8_t res = 0;

	/* Range check the starting addr against the card size. */
	if (block_addr >= mmc->card_size_blocks)
		return -1;

	/* For SDSC cards, the address specified is the byte address. For
	 * SDHC and SDXC cards, the address specified is the block address */
	if (!mmc->card_ccs)
		block_addr *= 512;

	/* Send CMD25: WRITE_MULTIPLE_BLOCK */
	buf[0] = 0x40 | 25;
	buf[1] = (block_addr & 0xff000000) >> 24;
	buf[2] = (block_addr & 0x00ff0000) >> 16;
	buf[3] = (block_addr & 0x0000ff00) >> 8;
	buf[4] = block_addr & 0x000000ff;

	MMC_SPI_SET_CS(spi_instance, 0);
	res = __send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R1_LEN);
	if (res < 0) {
		mmc->state = MMC_STATE_IDLE;
		goto err;
	}

	if (buf[0] != 0x0) {
		res = -1;
		goto err;
	}

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	mmc->write_position = 0;
	mmc->checksum = 0;

	mmc->state = MMC_STATE_WRITE_MULTIPLE;

	return 0;

err:
	/* An error occurred. End the write operation */
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	return res;
}

int8_t mmc_multiblock_write_data(struct mmc_card *mmc,
                                 uint8_t *data, size_t len)
{
	uint8_t spi_instance = mmc->spi_instance;
	uint8_t buf[2];
	int8_t res;

	/* Make sure there is a multi-block write underway */
	if (mmc->state != MMC_STATE_WRITE_MULTIPLE)
		return -1;

	/* Make sure the data won't cross a block boundary. */
	if (mmc->write_position + len > MMC_BLOCK_SIZE) {
		goto write_failed;
	}

	if (mmc->write_position == 0) {
		/* A new block is starting. Send start token. */
		buf[0] = 0xfc; /* Multi-Block Start Block Token (7.3.3.2) */
		MMC_SPI_TRANSFER(spi_instance, buf, NULL, 1);

		/* Clear the checksum for this new block*/
		mmc->checksum = 0;
	}

	/* Send the data */
	MMC_SPI_TRANSFER(spi_instance, data, NULL, len);

	/* Update the checksum. */
	mmc->checksum = add_crc16_array(mmc->checksum, data, len);
	mmc->write_position += len;

	if (mmc->write_position >= MMC_BLOCK_SIZE) {
		/* An entire block has been sent. Send the checksum
		 * and end this block. */
		buf[0] = mmc->checksum & 0xff;
		buf[1] = mmc->checksum >> 8 & 0xff;
		MMC_SPI_TRANSFER(spi_instance, buf, NULL, 2);

		/* Skip 0xff bytes before the response */
		res = skip_bytes_timeout(
				     spi_instance, 0xff, &buf[0],
				     MMC_COMMAND_TIMEOUT, NUM_READ_RETRIES);

		if (res < 0)
			goto card_error;

		/* Read the Data Response (CRC Status) Token (7.3.3.1) */
		if ((buf[0] & 0x1f) != 0x05)
			goto write_failed;

		/* Give it 8 extra clocks per section 4.4. */
		MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

		/* Skip the busy bytes (0x00) which the MMC card
		 * sends while writing */
		res = skip_bytes_timeout(spi_instance, 0x0, &buf[0],
					 MMC_WRITE_TIMEOUT, NUM_WRITE_RETRIES);
		if (res < 0)
			goto card_error;

		mmc->write_position = 0;
	}

	return 0;

card_error:
	mmc->state = MMC_STATE_IDLE;

write_failed:
	/* End the write operation, successful or not. */
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	mmc->state = MMC_STATE_READY;

	return -1;
}

int8_t mmc_multiblock_write_end(struct mmc_card *mmc)
{
	uint8_t spi_instance = mmc->spi_instance;
	uint8_t buf[6];
	uint8_t c, res;

	/* Finishing write. Send stop token. */
	c = 0xfd; /* Multi-Block Stop Transmission Token (7.3.3.2) */
	MMC_SPI_TRANSFER(spi_instance, &c, NULL, 1);

	/* Skip 0xff bytes after the token */
	res = skip_bytes_timeout(
			     spi_instance, 0xff, &c,
			     MMC_COMMAND_TIMEOUT, NUM_WRITE_RETRIES);
	if (res < 0)
		goto err;

	/* Skip the busy bytes (0x00) which the MMC card
	 * sends while finishing the multi-block write */
	res = skip_bytes_timeout(spi_instance, 0x0, &c,
				 MMC_WRITE_TIMEOUT, NUM_WRITE_RETRIES);

	/* After the 0x0 bytes, the card should send 0xff bytes, but there
	 * might be one byte of half 0's and half 1's (eg: 0x1f, 0x0f, or
	 * 0x07) depending on the timing. */
	if (c != 0xff) {
		MMC_SPI_TRANSFER(spi_instance, NULL, &c, 1);
		if (c != 0xff) {
			res = -1;
			goto card_error;
		}
	}

err:
	/* End the write operation, successful or not. */
	MMC_SPI_SET_CS(spi_instance, 1);

	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	if (res < 0)
		goto card_error;

	/* Issue SPI CMD13: SEND_STATUS to make sure the
	 * write completed successfully */
	buf[0] = 0x40 | 13;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	res = send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R2_LEN);
	if (res < 0)
		goto card_error;

	if (buf[0] != 0 || buf[1] != 0) {
		/* Write failed, for some reason*/
		res = -1;
	}

	mmc->state = MMC_STATE_READY;

	return res;

card_error:
	/* End the write operation, successful or not. */
	MMC_SPI_SET_CS(spi_instance, 1);

	mmc->state = MMC_STATE_IDLE;

	return res;
}

int8_t mmc_multiblock_write_cancel(struct mmc_card *mmc)
{
	uint16_t bytes_to_write = MMC_BLOCK_SIZE - mmc->write_position;
	uint8_t c = 0xff;
	uint8_t res;

	/* Make sure there is a multi-block write underway */
	if (mmc->state != MMC_STATE_WRITE_MULTIPLE)
		return -1;

	/* Clock out the rest of this block and end the write. Calling
	 * *_write_data() once per byte is not very efficient, but without a
	 * buffer to pass in, there isn't much other way which doesn't
	 * significantly increase stack usage. Since this is an extremely
	 * infrequent case, small code size should win out anyway. */
	while (bytes_to_write > 0) {
		res = mmc_multiblock_write_data(mmc, &c, 1);
		if (res < 0)
			return -1;
		bytes_to_write--;
	}

	res = mmc_multiblock_write_end(mmc);

	return res;
}

int8_t mmc_init_card(struct mmc_card *mmc)
{
	uint8_t buf[16]; /* Used for commands and register response. */
	uint32_t max_speed_hz;
	uint8_t spi_instance = mmc->spi_instance;
	bool cmd8_passed;
	uint16_t count;
	bool timer_started = false;
	int8_t res;

	reset_state(mmc);

	MMC_SPI_SET_SPEED(spi_instance, 40000);

	/* Send some blank clocks to initialize the card. 6.4.1.1 */
	blank_clock(spi_instance);

	/* Send CMD0 with CS low (active) to put the card in SD mode. */
	buf[0] = 0x40 | 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R1_LEN);

	if (buf[0] != RESP_IN_IDLE_STATE)
		return -1;

	blank_clock(spi_instance);

	/* Send CMD8: SEND_IF_COND with a proper voltage range. */
	buf[0] = 0x40 | 8;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 1;
	buf[4] = CHECK_PATTERN;
	send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R7_LEN);

	if (buf[0] & RESP_ILLEGAL_COMMAND) {
		/* CMD8 is not supported. This is an SD 1.x or MMC card,
		 * and not SDHC or SDXC. */
		cmd8_passed = false;
	}
	else {
		if (buf[0] != 0x1)
			return -1; /* Some Comm Error (See R1 Format) */
		if (buf[4] != CHECK_PATTERN)
			return -1; /* Check pattern failed */
		if ((buf[3] & 0xf) != 0x1)
			return -1; /* Wrong voltage return */

		cmd8_passed = true;
	}

	if (cmd8_passed) {
		/* Issue SPI CMD58: READ_OCR */
		buf[0] = 0x40 | 58;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0;
		buf[4] = 0;
		send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R3_LEN);

		if (buf[0] != RESP_IN_IDLE_STATE)
			return -1;

		/* Make sure that 3.3v is supported */
		if (!(buf[2] & 0x30))
			return -1; /* Voltage not supported */

		/* Initialize the card with ACMD41. ACMD requires CMD55 to be
		 * sent prior to it every time, since ACMD41 is app-specific.
		 * The first time ACMD41 is sent, set the HCS bit, to tell the
		 * card that we can handle SDHC/SDXC cards. Then send ACMD41
		 * until the IDLE bit in the respose is zero, indicating that
		 * the card is out of IDLE state. */
		count = 0;
		do {
			/* Issue SPI CMD55: APP_CMD to tell it that an
			 * app-specific command is coming up next. */
			buf[0] = 0x40 | 55;
			buf[1] = 0;
			buf[2] = 0;
			buf[3] = 0;
			buf[4] = 0;
			send_mmc_command(spi_instance, buf,
			                 CMD_LEN, RESP_R1_LEN);

			if (buf[0] != RESP_IN_IDLE_STATE) {
				if (timer_started)
					MMC_TIMER_STOP(spi_instance);
				return -1;
			}

			/* Issue SPI ACMD41: SD_SEND_OP_COND */
			buf[0] = 0x40 | 41;
			buf[1] = (count == 0)? 0x40: 0; /* Set HCS the first time (since we can handle SDHC, SDXC) */
			buf[2] = 0;
			buf[3] = 0;
			buf[4] = 0;
			send_mmc_command(spi_instance, buf,
			                 CMD_LEN, RESP_R1_LEN);

			if (count == 0) {
				/* 4.2.3 says the card has 1 second "from the
				 * first ACMD41" to become ready, which is
				 * assumed to mean 1 second after the first
				 * ACMD41 completes, so start the timer. */
				MMC_TIMER_START(spi_instance, 1000);
				timer_started = true;
			}

		} while (buf[0] == RESP_IN_IDLE_STATE &&
		         count++ != NUM_ACMD41_RETRIES &&
		         !MMC_TIMER_EXPIRED(spi_instance));

		MMC_TIMER_STOP(spi_instance);

		if (buf[0] != 0x0)
			return -1; /* An error was reported */

		/* Issue SPI CMD58: READ_OCR. This time read the CCS bit */
		buf[0] = 0x40 | 58;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0;
		buf[4] = 0;
		send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R3_LEN);

		if (buf[0] != 0x0)
			return -1;
		if (buf[1] & 0x40)
			mmc->card_ccs = true; /* Card is SDHC or SDXC */
	}

	/* Send CMD9: SEND_CSD to get the CSD Register. The CSD Register
	 * contents are sent back as a 16-byte data block response
	 * (+ 2 bytes CRC). */
	buf[0] = 0x40 | 9;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;

	MMC_SPI_SET_CS(spi_instance, 0);
	__send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R1_LEN);

	if (buf[0] != 0x0) {
		MMC_SPI_SET_CS(spi_instance, 1);
		return -1;
	}

	res = __read_data_block(mmc, buf, 16);
	MMC_SPI_SET_CS(spi_instance, 1);
	/* Give it 8 extra clocks per section 4.4. */
	MMC_SPI_TRANSFER(spi_instance, NULL, NULL, 1);

	if (res < 0)
		return -1;

	if (mmc->card_ccs) {
		/* Card is SDHC/SDXC. 5.3.3 */
		if ((buf[0] & 0xc0) != 0x40) {
			/* Card is SDHC/SDXC but the CSD register is
			 * not version 2.0. 5.3.3. */
			return -1;
		}

		/* Calculate the size of the card in 512-byte blocks. */
		mmc->card_size_blocks = buf[9];
		mmc->card_size_blocks += buf[8] << 8;
		mmc->card_size_blocks += ((uint32_t)(buf[7] & 0x3f)) << 16;
		mmc->card_size_blocks += 1; /* Per the spec, 5.3.3 */
		mmc->card_size_blocks *= 1024;
	}
	else if (!mmc->card_ccs) {
		/* Card is SDSC. 5.3.2 */
		if ((buf[0] & 0xc0) != 0x00) {
			/* Card is SDSC but the CSD reg is not version 1.0.
			 * 5.3.2. */
			return -1;
		}

		/* Calculate the size of the card. For SDSC cards, a few
		 * registers have to be read and combined to get this
		 * value. For the algorithm, see section 5.3.2, and start at
		 * the section on the C_SIZE field. The real fun here is that
		 * the fields cross byte-boundaries in strange ways. */
		uint16_t c_size;
		c_size = (buf[8] & 0xc0) >> 6 & 3;
		c_size += buf[7] << 2;
		c_size += (buf[6] & 0x3) << 10;

		uint8_t c_size_mult;
		c_size_mult = (buf[9] & 0x3) << 1;
		c_size_mult += buf[10] >> 7 & 1;

		uint16_t mult = 1 << (c_size_mult + 2);

		/* Block Length exponent, base 2 (ie: the block length is
		 * 2 raised to this value). This represents the max
		 * supported block size, not to be confused with the block
		 * size with which we communicate with the card (set below
		 * with SET_BLOCKLEN). */
		uint8_t read_bl_len_exp = buf[5] & 0xf;

		/* This algorithm comes from the C_SIZE section of 5.3.2. */
		uint32_t blocknr = (uint32_t)(c_size + 1) * mult;
		uint16_t block_len = 1 << read_bl_len_exp;
		uint32_t capacity_in_bytes = blocknr * block_len;

		/* Since we only support 512-byte blocks, divide capacity by
		 * 512 to get the number of blocks for our use. */
		mmc->card_size_blocks = capacity_in_bytes / 512;
	}

	/* Get the speed from buf[3]. 5.3.2, 5.3.3. The transfer speed is the
	 * same in the version 1.0 and 2.0 CSD structures for default and high-
	 * speed modes. Make sure it's under the board's maximum speed,
	 * as specified in the mmc structure. */
	max_speed_hz = MIN(calculate_speed(buf[3]),
	                   mmc->max_speed_hz);
	if (max_speed_hz == 0)
		return -1;

	if (!mmc->card_ccs) {
		/* CMD16: SET_BLOCKLEN. Set Block length of SDSC cards to
		 * 512 to match SDHC/SDXC cards */
		buf[0] = 0x40 | 16;
		buf[1] = 0;
		buf[2] = 0;
		buf[3] = 0x2;
		buf[4] = 0;
		send_mmc_command(spi_instance, buf, CMD_LEN, RESP_R3_LEN);

		if (buf[0] != 0x0)
			return -1;
	}

	MMC_SPI_SET_SPEED(spi_instance, max_speed_hz);

	mmc->state = MMC_STATE_READY;

#ifdef MMC_DEBUG
	/* Read some blocks and test out the mechanism. */
	res = mmc_read_block(mmc, 0, read);
	if (res < 0)
		goto fail;
	res = mmc_read_block(mmc, 1, read);
	if (res < 0)
		goto fail;
	res = mmc_read_block(mmc, 8192, read);
	if (res < 0)
		goto fail;
	res = mmc_read_block(mmc, 8193, read);
	if (res < 0)
		goto fail;
	res = mmc_read_block(mmc, 0x00400c00/512, read);
	if (res < 0)
		goto fail;
	res = mmc_read_block(mmc, 0x00400e00/512, read);
	if (res < 0)
		goto fail;

#ifdef MMC_DEBUG_WRITE
	/* Write to the last block on the card. This is destructive to the
	 * data on the MMC card! Don't do it on a production card. */
	#warning "Destructive DEBUG_WRITE enabled. Make sure this is what you want!"

	/* Write data to the card */
	uint16_t i;
	for (i = 0; i < MMC_BLOCK_SIZE; i++)
		read[i] = (uint8_t) i;

	res = mmc_write_block(mmc, mmc->card_size_blocks-1, read);
	if (res < 0)
		goto fail;

	memset(read, 0, MMC_BLOCK_SIZE);

	/* Read the same block and make sure it's what was written */
	res = mmc_read_block(mmc, mmc->card_size_blocks-1, read);
	if (res < 0)
		goto fail;

	for (i = 0; i < MMC_BLOCK_SIZE; i++)
		if (read[i] != (uint8_t) i)
			break;

	if (i != MMC_BLOCK_SIZE)
		goto fail;
#endif

	return 0;
fail:
	return -1;
#else
	return 0;
#endif
}
