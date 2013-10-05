/*
 * Logging Macros
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
 */

#ifndef LOG_H__
#define LOG_H__

/* Log general debugging information */
#ifdef DEBUG
	#define log printf
#else
	#define log(...)
#endif

/* Log libusb error codes */
#ifdef LOG_LIBUSB_ERRORS
	#define log_libusb printf
#else
	#define log_libusb(...)
#endif

/* Log Hex file parsing information */
#ifdef LOG_HEX
	#define log_hex printf
#else
	#define log_hex(...)
#endif


#endif /* LOG_H__ */
