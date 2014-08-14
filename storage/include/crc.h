/*
 *  M-Stack CRC Implementations
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2014-06-01
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


#ifndef M_STACK_CRC_H__
#define M_STACK_CRC_H__

/** @file crc.h
 *  @brief M-Stack CRC Implementations
 *  @defgroup public_api Public API
 *
 * CRC7 and CRC16 implementations.
 *
 */

/** @addtogroup public_api
 *  @{
 */

/** @brief Update a CRC7 checksum with additional input data
 *
 * Update a CRC7 checksum with an additional input byte. For example, to
 * checksum an array, call this function for each member of the array.
 *
 * @param csum   The original checksum
 * @param input  The additional value with which to update the checksum.
 *
 * @returns
 *   Return the CRC7 checksum of @p csum updated by @p input.
 */
uint8_t add_crc7(uint8_t csum, uint8_t input);

/** @brief Update a CRC16 checksum with additional input data
 *
 * Update a CRC16 checksum with an additional input byte. For example, to
 * checksum an array, call this function for each member of the array.
 *
 * @param csum   The original checksum
 * @param input  The additional value with which to update the checksum.
 *
 * @returns
 *   Return the CRC16 checksum of @p csum updated by @p input.
 */
uint16_t add_crc16(uint16_t csum, uint8_t input);

/** @brief Update a CRC16 checksum with an array of data
 *
 * Update a CRC16 checksum with an additional array of input data.
 *
 * @param csum   The original checksum
 * @param data   The additional array with which to update the checksum.
 * @param len    The length of the @p data array.
 *
 * @returns
 *   Return the CRC16 checksum of @p csum updated by @p data.
 */
uint16_t add_crc16_array(uint16_t csum, uint8_t *data, uint16_t len);

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* M_STACK_CRC_H__ */
