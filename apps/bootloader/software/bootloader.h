/*
 * M-Stack Bootloader
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
 * 2013-05-15
 */

#ifndef BOOTLOADER_H__
#define BOOTLOADER_H__

#ifdef _MSC_VER
    #if _MSC_VER < 1600
        typedef unsigned __int8 uint8_t;
        typedef unsigned __int16 uint16_t;
        typedef unsigned __int32 uint32_t;
    #else
        #include <stdint.h>
    #endif
#else
	#include <stdint.h>
#endif


/* The Bootloader API functions return 0 on success and a negative error
 * code on error. */

#define BOOTLOADER_ERROR -1
#define BOOTLOADER_CANT_OPEN_FILE -2   /* Returned from *_init() */
#define BOOTLOADER_CANT_OPEN_DEVICE -3 /* Returned from *_init() */
#define BOOTLOADER_CANT_QUERY_DEVICE -4 /* Returned from *_init() */
#define BOOTLOADER_MULTIPLE_CONNECTED -5 /* Returned from *_init() */

struct bootloader; /* opaque struct */

int  bootloader_init(struct bootloader **bootl,
                     const char *filename,
                     uint16_t vid,
                     uint16_t pid);
int  bootloader_erase(struct bootloader *bl);
int  bootloader_program(struct bootloader *bl);
int  bootloader_verify(struct bootloader *bl);
int  bootloader_reset(struct bootloader *bl);
void bootloader_free(struct bootloader *bl);

#endif /* BOOTLOADER_H__ */
