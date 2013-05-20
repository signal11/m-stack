/*
 * M-Stack Bootloader
 *
 * This file may be used under the terms of the Simplified BSD License
 * (2-clause), which can be found in LICENSE-bsd.txt in the parent
 * directory.
 *
 * It is worth noting that M-Stack itself is not under the same license as
 * this file.  See the top-level README.txt for more information.
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
