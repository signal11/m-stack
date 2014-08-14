/*
 * Board-Related Definitions and Pin Assignments
 *
 * Copyright (C) 2014 Alan Ott <alan@signal11.us>
 *                    Signal 11 Software
 *
 * 2014-05-31
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BOARD_H__
#define BOARD_H__

#include <xc.h>

/* SPI Board definitions for Supported demo boards */

#ifdef CHIPKIT_MAX32_BOARD

	/* Definitions for spi.c */
	#define SPI_ASSERT_CS()  LATGCLR = (1<<1)
	#define SPI_RELEASE_CS() LATGSET = (1<<1)
	#define SPI_MMC_CARD_PRESENT()  (!(PORTG & 0x1))

	#define OSCILLATOR_HZ 60000000

	/* SPI Registers. The demo uses SPI2, but this can easily be changed
	 * to use other SPI controllers. */
	#define SPInCONCLR  SPI2CONCLR
	#define SPInCONSET  SPI2CONSET
	#define SPInSTAT    SPI2STAT
	#define SPInSTATCLR SPI2STATCLR
	#define SPInSTATSET SPI2STATSET
	#define SPInBRG     SPI2BRG
	#define SPInBUF     SPI2BUF

#elif PIC24_BREADBOARD
	#define SPI_CS_PIN   PORTBbits.RB9
	#define SPI_CS_TRIS  TRISBbits.TRISB9
	#define CD_TRIS      TRISBbits.TRISB8
	#define CD_PIN       PORTBbits.RB8
	#define CD_CN        CNPU2bits.CN22PUE /* Change-notification address for CD pin */
	#define SET_ANALOG_CONFIG() do { AD1PCFG |= 0xe00 ; } while(0)

	/* SPI Registers. The demo uses SPI1, but this can easily be changed
	 * to use other SPI controllers. */
	#define SPInCON1bits SPI1CON1bits
	#define SPInCON2bits SPI1CON2bits
	#define SPInSTATbits SPI1STATbits
	#define SPInBUF      SPI1BUF
	#define SPInIF       IFS0bits.SPI1IF

	/* Definitions for spi.c */
	#define SPI_ASSERT_CS()  SPI_CS_PIN = 0
	#define SPI_RELEASE_CS() SPI_CS_PIN = 1
	#define SPI_MMC_CARD_PRESENT()  (!CD_PIN)

	#define OSCILLATOR_HZ 32000000

#elif PIC18F_STARTER_KIT

	/* Definitions for spi.c */
	#define SPI_ASSERT_CS()  LATCbits.LATC6 = 0
	#define SPI_RELEASE_CS() LATCbits.LATC6 = 1
	#define SPI_MMC_CARD_PRESENT()  (1) /* PIC18F Starter Kit has no card detect */

	#define OSCILLATOR_HZ 48000000

	/* SPI Registers. This board must use SPI2, as the board is wired
	 * with the MMC socket on remappable pins and SPI2 is connected to
	 * the remappable pins. */
	#define SSPnSTATbits  SSP2STATbits
	#define SSPnCON1bits  SSP2CON1bits
	#define SSPnBUF       SSP2BUF
	#define SSPnIF        PIR3bits.SSP2IF

#else
	#error "Board not supported. Add a section and definitions for your board here."
#endif

/* Board-specific SPI pin setup */
void board_setup_spi_pins(void);

#endif /* BOARD_H__ */
