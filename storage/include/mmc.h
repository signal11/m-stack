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


#ifndef M_STACK_MMC_H__
#define M_STACK_MMC_H__

/** @file mmc.h
 *  @brief M-Stack MMC/SD Support
 *  @defgroup public_api Public API
 *
 * This component provides an interface to MMC/SD cards, using SPI. A driver
 * for the SPI hardware itself is not provided here. Client software using
 * this component will need to provide a file called mmc_config.h providing
 * #defines binding the MMC class to an externally-provided SPI
 * implementation.
 *
 * This MMC implementation can also make use of an external timer. If
 * MMC_USE_TIMER is defined, mmc_config.h can also provide bindings to an
 * externally-provided timer implementation.
 *
 * References in the code to section numbers are referring to the document
 * titled: "SD Specifications: Part 1, Physical Layer Simplified
 * Specifications"
 *
 * This implementation only supports operation at 3.3v.
 */

/** @addtogroup public_api
 *  @{
 */

#include <stdbool.h>
#include <stdint.h>
#include "mmc_config.h"

/** @brief MMC Block Size
 *
 * The block size to use for reads and writes. This block size is configurable
 * on MMC cards, but 512 is the only size supported by this implementation.
 */
#define MMC_BLOCK_SIZE 512

#ifdef MMC_SPI_TRANSFER
/** @brief MMC SPI Transfer Function
 *
 * MMC_SPI_TRANSFER() is called by the MMC implementation to transfer
 * commands or data to and from the MMC card.
 *
 * This function should block while reading or writing is taking place.
 *
 * @param instance   The MMC instance. This is passed directly from the
 *                   mmc_*() functions.
 */
void MMC_SPI_TRANSFER(uint8_t instance,
                      const uint8_t *out_buf,
                      uint8_t *in_buf,
                      uint16_t len);
#else
	#error "You must define MMC_SPI_TRANSFER"
#endif

#ifdef MMC_SPI_SET_CS
/** @brief Manipulate MMC SPI Chip Select
 *
 * MMC_SPI_SET_CS() is called by the MMC implementation to set the
 * chip-select line. As is standard, 0 means asserted and 1 means not
 * asserted.
 *
 * @param instance   The MMC instance. This is passed directly from the
 *                   mmc_*() functions.
 * @param value      The value to set the CS line to. 0=asserted,
 *                   1=not-asserted
 */
void MMC_SPI_SET_CS(uint8_t instance, uint8_t value);
#else
	#error "You must define MMC_SPI_TRANSFER"
#endif


#ifdef MMC_SPI_SET_SPEED
/** @brief Set Speed of MMC SPI Interface
 *
 * MMC_SPI_SET_SPEED() is called by the MMC implementation when the SPI
 * speed needs to change. This function should set the SPI speed to equal-to
 * or less-than the speed provided.
 *
 * This function should block while reading or writing is taking place.
 *
 * @param instance   The MMC instance. This is passed directly from the
 *                   mmc_*() functions.
 * @param speed_hz   The speed to set the SPI interface to, in Hz
 */
void MMC_SPI_SET_SPEED(uint8_t instance, uint32_t speed_hz);
#else
	#error "You must define MMC_SPI_SET_SPEED"
#endif

#ifdef MMC_USE_TIMER

#ifdef MMC_TIMER_START
/** @brief Start a timer
 *
 * MMC_TIMER_START() is called by the MMC implementation to start a timer
 * used for calculating timeouts.  The MMC implementation will call
 * MMC_TIMER_EXPIRED() periodically to determine whether the specified time
 * has elapsed.
 *
 * This function should not block.
 *
 * @param instance               The MMC instance. This is passed directly
 *                               from the mmc_*() functions.
 * @param timeout_milliseconds   The timeout value in milliseconds.
 *
 */
void MMC_TIMER_START(uint8_t instance, uint16_t timeout_milliseconds);
#else
	#error "You must define MMC_TIMER_START"
#endif

#ifdef MMC_TIMER_EXPIRED
/** @brief Determine if a timer has expired
 *
 * MMC_TIMER_EXPIRED() is called by the MMC implementation to determine
 * whether a timer started with @p MMC_TIMER_START() has expired. A timer is
 * said to have expired if the number of milliseconds passed to @p
 * MMC_TIMER_START()) has elapsed since the call to @p MMC_TIMER_START().
 *
 * This function will be called repeatedly, and thus should have no side
 * effects.
 *
 * This function should not block.
 *
 * @param instance   The MMC instance. This is passed directly from the
 *                   mmc_*() functions.
 * @returns
 *   Return true if the timer has expired or false if it has not.
 */
bool MMC_TIMER_EXPIRED(uint8_t instance);
#else
	#error "You must define MMC_TIMER_EXPIRED"
#endif

#ifdef MMC_TIMER_STOP
/** @brief Stop a Timer
 *
 * MMC_TIMER_STOP() is called by the MMC implementation when it is done
 * using a timer.
 *
 * This function should not block.
 *
 * @param instance   The MMC instance. This is passed directly from the
 *                   mmc_*() functions.
 */
void MMC_TIMER_STOP(uint8_t instance);
#else
	#error "You must define MMC_TIMER_STOP"
#endif

#endif /* MMC_USE_TIMER */

enum MMCState {
	MMC_STATE_IDLE = 0,
	MMC_STATE_READY = 1,
	MMC_STATE_WRITE_MULTIPLE = 2,
};

/** MMC Card Structure
 *
 * This structure represents an instance of an MMC card. The application
 * should create one of these structures for each MMC card socket attached
 * to the system and should fill out the members at the top of the structure.
 * The members at the bottom of the structure should be left alone by the
 * application as they are used by the MMC code internally.
 *
 * A pointer to an array of these structures will be passed to mmc_init() and
 * then a pointer to an individual structure will passed to all of the mmc_*
 * functions.
 */
struct mmc_card {
	/* The maximum speed the physical SD interface on the board can
	 * support in Hz. The SD spec currently specifies 50MHz as the
	 * maximum supported clock speed for the interface, but some boards
	 * may not be able to support the same speed due to routing or other
	 * issues. */
	uint32_t max_speed_hz;

	/* The instance number which will be passed directly to the SPI
	 * functions for operations dealing with this card.  Each card needs
	 * to have a different instance number. How this number is actually
	 * used is up to the SPI layer. */
	uint8_t spi_instance;

	/* The following are used by the MMC system. Do not initialize or
	 * overwrite them. */
	bool card_ccs; /* false: SDSC, true: SDHC or SDXC */
	uint8_t state; /* enum MMCState */
	uint32_t card_size_blocks; /* Card size in 512-byte blocks */
	uint16_t write_position;   /* Position in the current block during a
				    * multi-block write (in bytes). */
	uint16_t checksum;         /* Current checksum value */
};

/** @brief Initialize the MMC System
 *
 * Initialize all instances of the MMC System. Call this function with an
 * array containing an @class mmc_card for each
 * MMC interface (ie, for each MMC card attached to the system). The
 * @p card_data pointer should point to an array of valid
 * @class mmc_card structures which have been filled out properly.
 *
 * The MMC system will store a pointer to the array, so the array should be
 * valid for the lifetime of the application. Global variables work well for
 * this.
 *
 * The index of the @p msc_application_data structures in the array will
 * correspond to the instance number passed into each of the mmc_*()
 * functions.
 *
 * @param card_data   Pointer to an array of card data structures
 * @param count       Number of structures (size of array) in card_data
 * @returns
 *   Return 0 if it can be initialized or -1 if it cannot.
 */
int8_t mmc_init(struct mmc_card *card_data, uint8_t count);

/** @brief Initialize an MMC card
 *
 * Perform initialization on an MMC card and do all the handshaking and
 * querying necessary to transition to the stand-by state (data transfer mode).
 *
 * The SPI system should be fully initialized before this function is called.
 *
 * @param instance    The MMC card to use for the operation
 *
 * @returns
 *   Return 0 if it can be initialized or -1 if it cannot.
 */
int8_t mmc_init_card(struct mmc_card *mmc);

/** @brief Get the number of blocks
 *
 * Get the number of 512-byte blocks on an MMC card. This value is cached
 * from when the card was the initialized. This function does not access
 * the SD card.
 *
 * @param instance   The MMC card to read from
 * @returns
 *   Return the number of blocks on an MMC device
 */
uint32_t mmc_get_num_blocks(struct mmc_card *mmc);

/** @brief Query MMC card status
 *
 * Send a command to the SD card and check the response to verify that
 * it is still connected and working. This function has overhead, so don't
 * call it repeatedly.
 *
 * @param instance   The MMC card to query
 * @returns
 *   Return true if the device is ready, or false if it is not
 */
bool mmc_ready(struct mmc_card *mmc);

/** @brief Return initialization status
 *
 * Return if the card has been initialized. This does not actually query
 * the card, but returns the last known value.
 *
 * @param instance   The MMC card to query
 * @returns
 *   Return true if the device has been initialized, or false if it has not
 */
bool mmc_is_initialized(struct mmc_card *mmc);

/** @brief Unset MMC Card Initialization Status
 *
 * Set the initialization status of the card to false. This may be called
 * if the app detects that the card has been removed.
 *
 * @param instance   The MMC card to set uninitialized
 */
void mmc_set_uninitialized(struct mmc_card *mmc);

/** @brief Read a block of data from the MMC card
 *
 * Read a block of data from the SD card. For the purposes of this library,
 * block size is fixed at 512 bytes for all SD cards, and block_addr is the
 * block address to read from (ie: the multiple of 512 bytes). You must pass
 * in a buffer which is at least 512 bytes long.
 *
 * @param instance   The MMC card to read from
 * @param block_addr The block number to read from
 * @param data       A buffer to place the data in. This buffer must be at
 *                   least MMC_BLOCK_SIZE (512) bytes in length.
 * @returns
 *   Return 0 if the data was read successuflly or -1 otherwise.
 */
int8_t mmc_read_block(struct mmc_card *mmc,
                      uint32_t block_addr,
                      uint8_t *data);

/** @brief Write a block of data to the MMC card
 *
 * Write a block of data to the SD card. For the purposes of this library,
 * block size is fixed at 512 bytes for all SD cards, and block_addr is the
 * block address to write to (ie: the multiple of 512 bytes). You must pass
 * in a buffer which is 512 bytes long.
 *
 * @param instance   The MMC card to write to
 * @param block_addr The block number to write to
 * @param data       The buffer containing the data. This buffer must be
 *                   MMC_BLOCK_SIZE (512) bytes in length.
 * @returns
 *   Return 0 if the data was written successuflly or -1 otherwise.
 */
int8_t mmc_write_block(struct mmc_card *mmc,
                       uint32_t block_addr,
                       uint8_t *data);

/** @brief Begin a multi-block write operation to the MMC card
 *
 * Multi-block writes offer better performance and give a better lifespan to
 * the media. This function only begins the multi-block write, and must be
 * followed-up with calls to @p mmc_multiblock_write_data() (which provide
 * the actual data to be written), and @p mmc_multiblock_write_end(). In the
 * event of an error, mmc_multiblock_write_cancel() will cancel the
 * operation.
 *
 * @param mmc        The MMC card to write to
 * @param block_addr The first block number to write to
 *
 * @returns
 *   Return 0 if the command completed successuflly or -1 otherwise.
 */
int8_t mmc_multiblock_write_start(struct mmc_card *mmc,
                                  uint32_t block_addr);

/** @brief Write data to the MMC card as part of a multi-block write
 *
 * Write data to the MMC card.  This function is expected to be called
 * repeatedly to pass data to the MMC card.  The amount of data can be less
 * than an entire block, but it is important that the data passed in does
 * not cross a block boundary.  The implementation will automatically handle
 * the starting and stopping of blocks on the media when necessary.  @p
 * mmc_multiblock_write_start() must be called prior to this function, and
 * @p mmc_multiblock_write_end() must be called after all the data from all
 * the blocks has been passed in.
 *
 * @param mmc        The MMC card to write to
 * @param data       The buffer containing the data. The data must not
 *                   cross a block boundary (@see MMC_BLOCK_SIZE).
 * @param len        The number of bytes in @p data.
 * @returns
 *   Return 0 if the data was handled successuflly or -1 otherwise.
 */
int8_t mmc_multiblock_write_data(struct mmc_card *mmc,
                                 uint8_t *data, size_t len);

/** @brief End a multi-block write operation
 *
 * Perform the completion of a multi-block write operation.  This function
 * should be called once all the data has been passed in using @p
 * mmc_multiblock_write_data().
 *
 * @param mmc        The MMC card being written
 *
 * @returns
 *   Return 0 if the write operation completed successuflly or -1 otherwise.
 */
int8_t mmc_multiblock_write_end(struct mmc_card *mmc);

/** @brief Cancel a multi-block write operation
 *
 * This function should be called if for some reason a multi-block write
 * operation must be canceled prior to all the data being transferred.
 *
 * @param mmc        The MMC card being written
 *
 * @returns
 *   Return 0 if the write completed successuflly or -1 otherwise.
 */
int8_t mmc_multiblock_write_cancel(struct mmc_card *mmc);


/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* M_STACK_MMC_H__ */
