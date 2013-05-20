/*
 * USB Bootloader
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
 * 2013-05-03
 */

#include "usb.h"
#include <xc.h>
#include <string.h>
#include "usb_config.h"
#include "usb_ch9.h"

#ifdef __PIC24FJ64GB002__
_CONFIG1(WDTPS_PS16 & FWPSA_PR32 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx1 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & I2C1SEL_PRI & IOL1WAY_OFF & OSCIOFNC_OFF & FCKSM_CSDCMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP0 & SOSCSEL_IO & WUTSEL_LEG & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)
_CONFIG4(DSWDTPS_DSWDTPS3 & DSWDTOSC_SOSC & RTCOSC_SOSC & DSBOREN_OFF & DSWDTEN_OFF)

#elif __PIC24FJ256DA206__
_CONFIG1(WDTPS_PS32768 & FWPSA_PR128 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx2 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & IOL1WAY_OFF & OSCIOFNC_ON & FCKSM_CSECMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP255 & SOSCSEL_SOSC & WUTSEL_LEG & ALTPMP_ALPMPDIS & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)

#elif _18F46J50
#pragma config PLLDIV = 3 /* 3 = Divide by 3. 12MHz crystal => 4MHz */
#pragma config XINST = OFF
#pragma config WDTEN = OFF
#pragma config CPUDIV = OSC1
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config LPT1OSC = OFF
#pragma config T1DIG = ON
#pragma config OSC = ECPLL
#pragma config DSWDTEN = OFF
#pragma config IOL1WAY = OFF
#pragma config WPDIS = OFF /* This pragma seems backwards */

#else
	#error "Set Configuration bits for your platform"
#endif


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
 * It's also worth noting that on Extended Data Space (EDS) PIC24F parts,
 * the linker will but a 0x01 in the high byte of each of these addresses. 
 * Unfortunately, it's impossible with GCC to take the address of a
 * varialbe, apply a mask (in our cause we'd want to use 0x00ffffff), and
 * use that value as an initializer for a constant.  Becaause of this, the
 * assignment of the uint32_t "constants" below has to be done in main().
 */
const extern __prog__ uint8_t _IVT_MAP_BASE;
const extern __prog__ uint8_t _APP_BASE;
const extern __prog__ uint8_t _APP_LENGTH;
const extern __prog__ uint8_t _FLASH_BLOCK_SIZE;
const extern __prog__ uint8_t _FLASH_TOP;
const extern __prog__ uint8_t _CONFIG_WORDS_BASE;
const extern __prog__ uint8_t _CONFIG_WORDS_TOP;

#define LINKER_VAR(X) (((uint32_t) &_##X) & 0x00ffffff)
/* "Constants" for linker script values. These are assigned in main() using the
 * LINKER_VAR() macro. See the above comment for rationale. */
static uint32_t IVT_MAP_BASE;
static uint32_t APP_BASE;
static uint32_t APP_LENGTH;
static uint32_t FLASH_BLOCK_SIZE;
static uint32_t FLASH_TOP;
static uint32_t CONFIG_WORDS_BASE;
static uint32_t CONFIG_WORDS_TOP;

/* Flash block(s) where the bootloader resides.*/
#define USER_REGION_BASE  IVT_MAP_BASE
#define USER_REGION_TOP   (FLASH_TOP - FLASH_BLOCK_SIZE) /* The last page has the config words. Don't clear that one*/


/* Instruction sizes */
#ifdef __PIC24F__
	#define INSTRUCTIONS_PER_ROW 64
	#define BYTES_PER_INSTRUCTION 4
	#define WORDS_PER_INSTRUCTION 2
#else
	#error "Define instruction sizes for your platform"
#endif

#define BUFFER_LENGTH (INSTRUCTIONS_PER_ROW * WORDS_PER_INSTRUCTION)

/* Protocol commands */
#define CLEAR_FLASH 100
#define SEND_DATA 101
#define GET_CHIP_INFO 102
#define REQUEST_DATA 103
#define SEND_RESET 105

struct chip_info {
	uint32_t user_region_base;
	uint32_t user_region_top;
	uint32_t config_words_base;
	uint32_t config_words_top;

	uint8_t bytes_per_instruction;
	uint8_t instructions_per_row;
	uint8_t pad0;
	uint8_t pad1;
};

/* Data-to-program: buffer and attributes. */
static uint32_t write_address; /* program space word address */
static size_t write_length;    /* number of words, not bytes */
static uint16_t prog_buf[BUFFER_LENGTH];

static struct chip_info chip_info = { };

void clear_flash()
{
	uint32_t prog_addr = USER_REGION_BASE;
	size_t offset;

	/* Clear each flash block. TBLPAG/offset is set to the
	 * base address (lowest address) of each block. */
	while (prog_addr < USER_REGION_TOP) {
		TBLPAG = prog_addr >> 16;
		offset = prog_addr & 0xffff;

		__builtin_tblwtl(offset, 0x00);
		NVMCON = 0x4042;
		asm("DISI #5");
		__builtin_write_NVM();

		while (NVMCONbits.WR == 1)
			;

		prog_addr += FLASH_BLOCK_SIZE;
	}
}

void write_flash_row()
{
	size_t offset;
	uint8_t i;
	uint32_t prog_addr = write_address;

	NVMCON = 0x4001;
	TBLPAG = prog_addr >> 16;
	offset = prog_addr & 0xffff;

	/* Write the data provided */
	for (i = 0; i < write_length; i++) {
		__builtin_tblwtl(offset, prog_buf[i]);
		__builtin_tblwth(offset, prog_buf[++i]);
		offset += 2;
	}

	/* Pad the rest of the row out with 0xff */
	for (; i < BUFFER_LENGTH; i += 2) {
		__builtin_tblwtl(offset, 0xffff);
		__builtin_tblwth(offset, 0xffff);
		offset += 2;
	}

	asm("DISI #5");
	__builtin_write_NVM();

	while (NVMCONbits.WR == 1)
		;
}

/* Read an instruction from flash. word_addr is the word address, not
 * the byte address. */
static void read_flash(uint32_t word_addr, uint16_t *low, uint16_t *high)
{
	TBLPAG = word_addr >> 16 & 0xff;
	*high = __builtin_tblrdh(word_addr & 0xffff);
	*low  = __builtin_tblrdl(word_addr & 0xffff);
}

/* Read data starting at prog_addr into the global prog_buf. prog_addr
 * and len are in words, not bytes. */
static void read_prog_data(uint32_t prog_addr, uint32_t len/*words*/)
{
	int i;
	for (i = 0; i < len; i += 2) {
		read_flash(prog_addr + i,
		           &prog_buf[i]   /*low*/,
		           &prog_buf[i+1] /*high*/);
	}
}

int main(void)
{
	IVT_MAP_BASE = LINKER_VAR(IVT_MAP_BASE);
	APP_BASE = LINKER_VAR(APP_BASE);
	APP_LENGTH = LINKER_VAR(APP_LENGTH);
	FLASH_BLOCK_SIZE = LINKER_VAR(FLASH_BLOCK_SIZE);
	FLASH_TOP = LINKER_VAR(FLASH_TOP);
	CONFIG_WORDS_BASE = LINKER_VAR(CONFIG_WORDS_BASE);
	CONFIG_WORDS_TOP = LINKER_VAR(CONFIG_WORDS_TOP);


#if defined(__PIC24FJ64GB002__) || defined(__PIC24FJ256DA206__)
	unsigned int pll_startup_counter = 600;
	CLKDIVbits.PLLEN = 1;
	while(pll_startup_counter--);
#elif _18F46J50
	unsigned int pll_startup = 600;
	OSCTUNEbits.PLLEN = 1;
	while (pll_startup--)
		;
#endif

#if defined(USB_USE_INTERRUPTS) && defined (_PIC18)
	INTCONbits.PEIE = 1;
	INTCONbits.GIE = 1;
#endif

	if (!(RCONbits.POR || RCONbits.BOR)) {
		/* Jump to application */
		__asm__("goto %0"
		        : /* no outputs */
			: "r" (IVT_MAP_BASE)
			: /* no clobber*/);
	}
	RCONbits.POR = 0;
	RCONbits.BOR = 0;

	usb_init();
	
	while (1) {
		#ifndef USB_USE_INTERRUPTS
		usb_service();
		#endif
	}

	return 0;
}

static void empty_cb(bool transfer_ok, void *context)
{
	/* Nothing to do here. */
}

static void reset_cb(bool transfer_ok, void *context)
{
	asm("reset");
}

static void write_data_cb(bool transfer_ok, void *context)
{
	/* For OUT control transfers, data from the data stage of the request
	 * is in buf[]. */

	if (transfer_ok)
		write_flash_row();
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
			write_address /= 2; /* Convert to word address. */
			write_length = setup->wLength;
			write_length /= 2;  /* Convert to word length. */

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
			chip_info.user_region_base = USER_REGION_BASE * 2;
			chip_info.user_region_top = USER_REGION_TOP * 2;
			chip_info.config_words_base = CONFIG_WORDS_BASE * 2;
			chip_info.config_words_top = CONFIG_WORDS_TOP * 2;

			chip_info.bytes_per_instruction = BYTES_PER_INSTRUCTION;
			chip_info.instructions_per_row = INSTRUCTIONS_PER_ROW;

			usb_send_data_stage((char*)&chip_info, sizeof(struct chip_info), empty_cb/*TODO*/, NULL);
		}

		if (setup->bRequest == REQUEST_DATA) {
			/* Request program data */
			uint32_t read_address;

			read_address = setup->wValue | ((uint32_t) setup->wIndex) << 16;
			read_address /= 2;

			/* Range-check address */
			if (read_address + setup->wLength > FLASH_TOP)
				return -1;

			/* Check for overflow (unlikely on known MCUs) */
			if (read_address + setup->wLength < read_address)
				return -1;

			/* Check length */
			if (setup->wLength > sizeof(prog_buf))
				return -1;

			read_prog_data(read_address, setup->wLength / 2);
			usb_send_data_stage((char*)prog_buf, setup->wLength, empty_cb/*TODO*/, NULL);
		}
	}

	return 0; /* 0 = can handle this request. */
#undef MIN
}

void app_usb_reset_callback(void)
{

}
