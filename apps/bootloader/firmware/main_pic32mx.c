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
 * 2016-03-31
 */

#include "usb.h"
#include <xc.h>
#include <sys/kmem.h>
#include <string.h>
#include "usb_config.h"
#include "usb_ch9.h"
#include "hardware.h"
#include "../common/bootloader_protocol.h"

/* Variables from linker script.
 *
 * The way to pass values from the linker script to the program is to create
 * variables in the linker script and extern them in the program.  The only
 * catch is that assignment operations in the linker script only set
 * variable _addresses_, and not variable values.  Thus to get the data in
 * the program, you need to take the _address_ of the variables created by
 * the linker script (and ignore their actual values).  This may seem hacky,
 * but the GNU LD manual cites it as the recommended way to do it.  See
 * section 3.5.5 "Source Code Reference."
 *
 * It's also worth noting that addresses in the Linker Script are virtual,
 * and what we want in most cases in a bootloader are physical addresses.
 */
const extern uint8_t _APP_BASE;
const extern uint8_t _APP_LENGTH;
const extern uint8_t _FLASH_BLOCK_SIZE;
const extern uint8_t _CONFIG_WORDS_BASE;
const extern uint8_t _CONFIG_WORDS_TOP;

#define LINKER_VAR(X) (((uint32_t) &_##X) & 0x1fffffff) /* convert to physical */
/* "Constants" for linker script values. These are assigned in main() using the
 * LINKER_VAR() macro. See the above comment for rationale. */
static uint32_t APP_BASE;
static uint32_t APP_LENGTH;
static uint32_t FLASH_BLOCK_SIZE;
static uint32_t CONFIG_WORDS_BASE;
static uint32_t CONFIG_WORDS_TOP;

/* Flash block(s) where the bootloader resides.*/
#define USER_REGION_BASE  APP_BASE
#define USER_REGION_TOP   (APP_BASE + APP_LENGTH)


/* Instruction sizes */
#ifdef __PIC32MX__
	#define INSTRUCTIONS_PER_ROW 128
	#define BYTES_PER_INSTRUCTION 4
	#define WORDS_PER_INSTRUCTION 1
#else
	#error "Define instruction sizes for your platform"
#endif

#define BUFFER_LENGTH (INSTRUCTIONS_PER_ROW * BYTES_PER_INSTRUCTION)

/* Data-to-program: buffer and attributes. */
static uint32_t write_address; /* Physical flash address */
static size_t write_length;    /* Bytes */
static uint8_t prog_buf[BUFFER_LENGTH];

static struct chip_info chip_info = { };

/* Perform the non-volatile memory command
   Return 0 for success, -1 on error */
static __attribute__((nomips16)) int8_t nvm_command(uint32_t command)
{
	uint32_t irq_state;

	/* Disable interrupts and store the state. */
	asm volatile("di %0" : "=r" (irq_state));

	/* Set WREN and the command */
	NVMCON = 0x4000 | command;

	/* Unlock and perform the NVM write. The following is effectively:
	     NVMKEY = 0xaa996655;
	     NVMKEY = 0x556699aa;
	     NVMCONSET = 0x8000;
	   but such that the setting of the registers happens in single
	   instructions, as seems to be required by the Family Reference
	   Manual yet ignored in the sample code in the same manual.
	 */
	__asm__ volatile(
	        "LI $t0, 0xaa996655\n"
	        "LI $t1, 0x556699aa\n"
	        "LI $t2, 0x8000\n"  /* 8000 = Set WR */
	        "LA $t3, NVMKEY\n"
		"LA $t4, NVMCONSET\n"
	        "SW $t0, ($t3)\n"
	        "SW $t1, ($t3)\n"
	        "SW $t2, ($t4)\n"
	        : /* no outputs */
		: /* no inputs */
		: /* clobber */ "t0", "t1", "t2", "t3", "t4");

	/* Wait for WR to become set */
	while (NVMCON & 0x8000)
		;

	/* Clear WREN */
	NVMCONCLR = 0x00004000;

	/* Restore interrupt state */
	if (irq_state & 0x1)
		asm volatile("ei");
	else
		asm volatile("di");

	/* Return error WRERR | LVDERR */
	return (NVMCON & 0x3000)? -1: 0;
}

static int8_t clear_flash()
{

	uint32_t prog_addr = USER_REGION_BASE;
	int8_t res;

	while (prog_addr < USER_REGION_TOP) {
		NVMADDR = prog_addr;
		res = nvm_command(0x04);
		if (res < 0)
			return res;

		prog_addr += FLASH_BLOCK_SIZE;
	}

	return 0;
}

/* This function makes use of globals:
      write_address
      write_length
      prog_buf
 */
static int8_t write_flash_row()
{
	/* Make sure a short buffer is padded with 0xff. */
	if (write_length < BUFFER_LENGTH)
		memset(prog_buf + write_length, 0xff,
		       BUFFER_LENGTH - write_length);

	NVMADDR = write_address; /* physical flash address */
	NVMSRCADDR =  KVA_TO_PA(prog_buf);

	return nvm_command(0x03);
}

/* Read data starting at prog_addr into the global prog_buf. prog_addr
 * is a physical address. */
static void read_prog_data(uint32_t prog_addr, uint32_t len/*bytes*/)
{
	uint32_t *virt_addr_uncached = (uint32_t*) PA_TO_KVA1(prog_addr);

	if (len > sizeof(prog_buf))
		len = sizeof(prog_buf);

	memcpy(prog_buf, virt_addr_uncached, len);
}

int main(void)
{
	/* Set the flash parameters from the linker file */
	APP_BASE = LINKER_VAR(APP_BASE);
	APP_LENGTH = LINKER_VAR(APP_LENGTH);
	FLASH_BLOCK_SIZE = LINKER_VAR(FLASH_BLOCK_SIZE);
	CONFIG_WORDS_BASE = LINKER_VAR(CONFIG_WORDS_BASE);
	CONFIG_WORDS_TOP = LINKER_VAR(CONFIG_WORDS_TOP);

	hardware_init();

	/* If there was a software reset, jump to the application. In real
	 * life, this is where you'd put your logic for whether you are
	 * to enter the bootloader or the application */
	if (RCONbits.SWR) {
		/* Jump to application */
		asm volatile ("JR %0\n"
		              "NOP\n"
		              : /* no outputs */
			      : "r" (PA_TO_KVA1(APP_BASE))
			      : /* no clobber*/);
	}

	RCONbits.SWR = 0;

	usb_init();

	while (1) {
		#ifndef USB_USE_INTERRUPTS
		usb_service();
		#endif
	}

	return 0;
}

static int8_t empty_cb(bool transfer_ok, void *context)
{
	/* Nothing to do here. */
	return 0;
}

static int8_t reset_cb(bool transfer_ok, void *context)
{
	uint32_t x;
	uint16_t i = 65535;

	/* Delay before resetting*/
	while(i--)
		;

	/* The following reset procedure is from the Family Reference Manual,
	 * Chapter 7, "Resets," under "Software Resets." */

	/* Unlock sequence */
	SYSKEY = 0x00000000;
	SYSKEY = 0xaa996655;
	SYSKEY = 0x556699aa;

	/* Set the reset bit and then read it to trigger the reset. */
	RSWRSTSET = 1;
	x = RSWRST;

	/* Required NOP instructions */
	asm("nop\n nop\n nop\n nop\n");

	return 0;
}

static int8_t write_data_cb(bool transfer_ok, void *context)
{
	/* For OUT control transfers, data from the data stage of the request
	 * is in prog_buf[]. */
	int8_t res = -1;

	if (!transfer_ok)
		return -1;

	res = write_flash_row();

	return res;
}

int8_t app_unknown_setup_request_callback(const struct setup_packet *setup)
{
#define MIN(X,Y) ((X)<(Y)?(X):(Y))

	/* This handler handles request 254/dest=other/type=vendor only.*/
	if (setup->REQUEST.destination == DEST_OTHER_ELEMENT &&
	    setup->REQUEST.type == REQUEST_TYPE_VENDOR &&
	    setup->REQUEST.direction == 0/*OUT*/) {

		if (setup->bRequest == CLEAR_FLASH) {
			/* Clear flash Request */
			clear_flash();

			/* There will be NO data stage. This sends back the
			 * STATUS stage packet. */
			usb_send_data_stage(NULL, 0, empty_cb, NULL);
		}
		else if (setup->bRequest == SEND_DATA) {
			/* Write Data Request */
			if (setup->wLength > sizeof(prog_buf))
				return -1;

			write_address = setup->wValue | ((uint32_t) setup->wIndex) << 16;
			write_length = setup->wLength;

			/* Make sure it is within writable range (ie: don't
			 * overwrite the bootloader or config words). */
			if (write_address < USER_REGION_BASE)
				return -1;
			if (write_address + write_length > USER_REGION_TOP)
				return -1;

			/* Check for overflow (unlikely on known MCUs) */
			if (write_address + write_length < write_address)
				return -1;

			/* Check length */
			if (setup->wLength > sizeof(prog_buf))
				return -1;

			memset(prog_buf, 0xff, sizeof(prog_buf));
			usb_start_receive_ep0_data_stage((char*)prog_buf, setup->wLength, &write_data_cb, NULL);
		}
		else if (setup->bRequest == SEND_RESET) {
			/* Reset to Application Request*/

			/* There will be NO data stage. This sends back the
			 * STATUS stage packet. */
			usb_send_data_stage(NULL, 0, reset_cb, NULL);
		}
	}

	if (setup->REQUEST.destination == DEST_OTHER_ELEMENT &&
	    setup->REQUEST.type == REQUEST_TYPE_VENDOR &&
	    setup->REQUEST.direction == 1/*IN*/) {

		if (setup->bRequest == GET_CHIP_INFO) {
			/* Request Device Info Struct */
			chip_info.user_region_base = USER_REGION_BASE;
			chip_info.user_region_top = USER_REGION_TOP;
			chip_info.config_words_base = CONFIG_WORDS_BASE;
			chip_info.config_words_top = CONFIG_WORDS_TOP;

			chip_info.bytes_per_instruction = BYTES_PER_INSTRUCTION;
			chip_info.instructions_per_row = INSTRUCTIONS_PER_ROW;
			chip_info.number_of_skip_regions = 1;

			/* Skip the debug executive which it's impossible
			 * to remove using the linker script. This has to be
			 * aligned (at least the base) to a flash row. */
			chip_info.skip_regions[0].base = 0x1fc00400;
			chip_info.skip_regions[0].top = 0x1fc01480;

			usb_send_data_stage((char*)&chip_info,
				MIN(sizeof(struct chip_info), setup->wLength),
				empty_cb/*TODO*/, NULL);
		}

		if (setup->bRequest == REQUEST_DATA) {
			/* Request program data */
			uint32_t read_address;

			read_address = setup->wValue | ((uint32_t) setup->wIndex) << 16;

			/* Range-check address */
			if (write_address < USER_REGION_BASE)
				return -1;
			if (read_address + setup->wLength > USER_REGION_TOP)
				return -1;

			/* Check for overflow (unlikely on known MCUs) */
			if (read_address + setup->wLength < read_address)
				return -1;

			/* Check length */
			if (setup->wLength > sizeof(prog_buf))
				return -1;

			read_prog_data(read_address, setup->wLength);
			usb_send_data_stage((char*)prog_buf, setup->wLength, empty_cb/*TODO*/, NULL);
		}
	}

	return 0; /* 0 = can handle this request. */
#undef MIN
}

void app_usb_reset_callback(void)
{

}
