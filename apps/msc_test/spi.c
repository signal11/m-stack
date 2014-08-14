/*
 * SPI driver for PIC16F1459 and PIC24FJ32GB002
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

#include "board.h"
#include "spi.h"

#if defined(__XC8) || defined (__XC16__) || defined (__XC32__)
	#include <xc.h>
#else
	#error "Compiler not supported"
#endif

#define MIN(x,y) (((x)<(y))?(x):(y))

/* The device uses mode 0,0. SDI/SDO leads SCK, and SCK is idle LOW.
   CKP = 0, CKE = 1 */

void spi_init(void)
{
#ifdef _PIC18
	board_setup_spi_pins();

	SSPnSTATbits.CKE = 1; /* SPI Mode (edge) */

	SSPnCON1bits.CKP = 0; /* SPI Mode (polarity) */
	spi_set_speed_hz(40000);
	SSPnCON1bits.WCOL = 0; /* Clear write-collision */

	SSPnCON1bits.SSPEN = 1; /* Enable SSP Module */

#elif __XC16__
	board_setup_spi_pins();

	SPInCON1bits.CKE = 1; /* Data is valid before clock edge on SCK */
	SPInCON1bits.SSEN = 0; /* Slave Select (chip select) is manual (GPIO) */
	SPInCON1bits.MSTEN = 1; /* 1= SPIn is master mode. */

	SPInCON2bits.FRMEN = 0; /* Framed support is disabled (do it manually)*/
	SPInCON2bits.SPIBEN = 0; /* Enhanced Buffer Disabled */

	SPInSTATbits.SPIROV = 0; /* Clear overflow indication */
	SPInSTATbits.SPIEN = 1; /* Enable SPI operation */
	spi_set_speed_hz(40000);

#elif __PIC32MX__
	board_setup_spi_pins();

	SPInCONCLR = _SPI1CON_ON_MASK;
	SPInCONCLR = _SPI1CON_ENHBUF_MASK;
	spi_set_speed_hz(40000);
	SPInSTATCLR = _SPI1STAT_SPIROV_MASK;
	SPInCONSET = _SPI1CON_CKE_MASK |_SPI1CON_MSTEN_MASK | _SPI1CON_SMP_MASK;
	SPInCONSET = _SPI1CON_ON_MASK;
#else
	#error "Processor family not supported"
#endif
}

void spi_set_speed_hz(uint32_t speed_hz)
{
#ifdef _PIC18
	/* On the PIC18, there are 4 speeds at which the SPI module
	 * can operate, Fosc/4, Fosc/16, Fosc/64, and using TMR2. */
	T2CONbits.TMR2ON = 0;
	if (speed_hz >= OSCILLATOR_HZ / 4)
		SSPnCON1bits.SSPM = 0b0000; /* SPI Master, Clock = Fosc/4 */
	else if (speed_hz >= OSCILLATOR_HZ / 16)
		SSPnCON1bits.SSPM = 0b0001; /* SPI Master, Clock = Fosc/16 */
	else if (speed_hz >= OSCILLATOR_HZ / 64)
		SSPnCON1bits.SSPM = 0b0010; /* SPI Master, Clock = Fosc/64 */
	else {
		/* Use Timer2 for the SPI timing.
		 *
		 * When using Timer2, the SPI clock will run at the frequency
		 * of TMR2/2. With a 1:16 pre-scalar, the frequency of TMR2
		 * is determined as:
		 *
		 *      TMR2 = Fosc / 4 / 16 / PR2
		 *
		 * The SPI peripheral will run at (in Hz):
		 *
		 *      SPI_SPEED = TMR2 / 2
		 *
		 * Combining and solving for PR2, we get:
		 *      PR2 = Fosc / (SPI_SPEED * 128)
		 *
		 * Take the ceiling value of the above equation. That way if the
		 * division doesn't come out even, we end up below the target
		 * frequency rather than above it.
		 *
		 * Note that on an 8-bit processor with no hardware divider,
		 * doing this 32-bit math is going to be really slow.
		 * Fortunately it isn't done very often.
		 */

		T2CONbits.T2CKPS = 0b10; /* Timer2 Prescale to 1:16 */
		PR2 = OSCILLATOR_HZ / (speed_hz * 128);

		/* Take the ceiling */
		if (OSCILLATOR_HZ % (speed_hz * 128))
			PR2++;
		T2CONbits.TMR2ON = 1;
	}

#elif __PIC24F__
	const uint8_t pri_scalars[] = { 1, 4, 16, 64 };
	uint8_t p, s;

	/* Find the scalar combination that best matches, looping over the
	 * secondary scalars first (since there are more of them and they
	 * are closer together). Break at the first one which is under the
	 * target frequency.
	 *
	 * Valid secondary scalar values go from 1 to 8, while primary scalar
	 * values are shown in pri_scalars[]. */
	for (p = 0; p < sizeof(pri_scalars); p++) {
		for (s = 1; s < 8; s++) {
			uint32_t fsck = ((uint32_t)OSCILLATOR_HZ / 2) /
				            (pri_scalars[p] * s);
			if (fsck < speed_hz)
				goto loops_break;
		}
	}

loops_break:

	/* For some reason (as stated in the family reference manual), it
	 * is invalid to have both the prescalar and the postscalar be set
	 * to 1:1. */
	if (p == 0 && s == 1)
		s = 2;

	/* Pass the bitwise-negation of the scalars to SPInCON1. The SPI
	 * module must be disabled to change the scalars. */
	SPInSTATbits.SPIEN = 0; /* Disable SPI */
	SPInCON1bits.SPRE = ~(s - 1) & 0x7; /* Values 1-8 are encoded as 0-7 */
	SPInCON1bits.PPRE = ~p & 0x3;
	SPInSTATbits.SPIEN = 1; /* Enable SPI */

#elif __PIC32MX__
	/* On the PIC32, as configured by spi_init() above, the SPI clock
	 * frequency is calculated as:
	 *    SPIBRG = [Fpb  / (Fsck * 2)] - 1
	 * where:
	 *    Fpb  = The peripheral bus frequency,
	 *           in this case the main clock.
	 *    Fsck = The desired SPI clock frequency.
	 *
	 * Take the ceiling value of the above equation. That way if the
	 * division doesn't come out even, we end up below the target
	 * frequency rather than above it.
	 */

	uint32_t brg_val = OSCILLATOR_HZ / (speed_hz * 2) - 1;

	/* Take the ceiling */
	if (OSCILLATOR_HZ % (speed_hz * 2))
		brg_val++;

	SPInBRG = brg_val;

#else
	#error "Processor family not supported"
#endif
}


static inline uint8_t spi_transfer(uint8_t out)
{
#ifdef _PIC18
	SSPnIF = 0;

	SSPnBUF = out;
	Nop();
	Nop();
	while (!SSPnIF && !SSPnSTATbits.BF)
		;
	return SSPnBUF;
#elif __XC16__
	SPInBUF = out;
	Nop();
	Nop();
	while (!SPInIF)
		;
	SPInIF = 0;
	return SPInBUF;
#elif __PIC32MX__
	uint32_t read_byte;
	while (SPInSTAT & _SPI1STAT_SPIRBF_MASK)
		read_byte = SPInBUF;
	SPInBUF = out;
	Nop();
	Nop();
	while (SPInSTAT & _SPI1STAT_SPIBUSY_MASK)
		;

	/* Wait until the RX Buffer has data. This happens a few instructions
	 * after the SPIBUSY mask gets cleared.  */
	while (!(SPInSTAT & _SPI1STAT_SPIRBF_MASK))
		;
	read_byte = SPInBUF;

	return read_byte & 0xff;
#else
	#error "Processor family not supported"
#endif
}

int8_t spi_transfer_block(const uint8_t *out_buf, uint8_t *in_buf, uint16_t len)
{
	uint16_t i;

	if (in_buf && out_buf) {
		for (i = 0; i < len; i++) {
			in_buf[i] = spi_transfer(out_buf[i]);
		}
	}
	else if (!in_buf && out_buf) {
		for (i = 0; i < len; i++) {
			spi_transfer(out_buf[i]);
		}
	}
	else if (in_buf && !out_buf) {
		for (i = 0; i < len; i++) {
			in_buf[i] = spi_transfer(0xff);
		}
	}
	else if (!in_buf && !out_buf) {
		/* No in or out buffer; just move the clock line. */
		for (i = 0; i < len; i++) {
			spi_transfer(0xff);
		}
	}

	return 0;
}
