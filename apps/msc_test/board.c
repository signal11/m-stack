/*
 * Board-Related Definitions and Pin Assignments
 *
 * Copyright (C) 2014 Alan Ott <alan@signal11.us>
 *                    Signal 11 Software
 *
 * 2014-08-08
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

#include "board.h"

void board_setup_spi_pins(void)
{
#ifdef CHIPKIT_MAX32_BOARD
	/* Initialization. Make sure to turn off Analog if need be here. */
	TRISGCLR = (1 << 6);
	TRISGCLR = (1 << 8);
	TRISGCLR = (1 << 1);
	TRISGSET = 0x1;

#elif PIC24_BREADBOARD
	/* Set up peripheral pin select (PPS) */
	/* unlock  registers */
	__asm(" \n \
	push    w1;\n \
	push    w2;\n \
	push    w3;\n \
	mov     #OSCCON, w1;\n \
	mov     #0x46, w2;\n \
	mov     #0x57, w3;\n \
	mov.b   w2, [w1];\n \
	mov.b   w3, [w1];\n \
	bclr    OSCCON, #6;\n");

	#define SCK_PIN _RP15R /* OUT */
	#define SDI_PIN 14     /* IN  */
	#define SDO_PIN _RP13R /* OUT */
	#define INT_PIN 8      /* IN  */

	/* Outputs (Table 10-3 in the datasheet (Selectable Output Sources)) */
	SCK_PIN = 8; /* 8 = SCK1OUT (table 10-3) */
	SDO_PIN = 7; /* 7 = SDO1    (table 10-3) */

	/* Inputs (Table 10-2 in the datasheet (Selectable Input Sources)) */
	_SDI1R = SDI_PIN;

	/* lock registers */
	__asm("\n \
	mov   #OSCCON, w1;\n \
	mov   #0x46, w2;\n \
	mov   #0x57, w3;\n \
	mov.b w2, [w1];\n \
	mov.b w3, [w1];\n \
	bset  OSCCON, #6;\n \
	pop   w3;\n \
	pop   w2;\n \
	pop   w1;\n");

	SET_ANALOG_CONFIG();

	SPI_CS_PIN = 1;
	SPI_CS_TRIS = 0;  /* 0 = OUTPUT */
	CD_TRIS = 1;      /* 1 = INPUT  */

	/* Enable the pull-up on the card-detect pin. Pull-ups are indexed
	 * by their change-notification number. */
	CD_CN = 1;

#elif PIC18F_STARTER_KIT
	/* Set appropriate pins to be digital instead of analog. */
	ANCON1bits.PCFG9 = 1;  /* RB3 and AN9 share a pin */
	ANCON1bits.PCFG11 = 1; /* RC2 and AN11 share a pin */

	/* Set up the tri-state registers */
	TRISDbits.TRISD6 = 1; /* RD6 (SDI) is input */
	TRISBbits.TRISB3 = 0; /* RB3 (SDO) is output */
	TRISCbits.TRISC2 = 0; /* RC2 (SCK) is output */
	TRISCbits.TRISC6 = 0; /* RC6 (CS)  is output */

	/* Set the peripheral PIN Select to put the SPI peripheral on the
	 * appropriate pins. */
	INTCONbits.GIE = 0;
	EECON2 = 0x55;
	EECON2 = 0xAA;
	PPSCONbits.IOLOCK = 0;

	/* Set the Inputs. PIC18F46J50 Datasheet sec 10.7.3.1, Table 10-13 */
	RPINR21 = 23; /* Put SDI on RD6 which is RP23 */

	/* Set the Outputs. PIC18F46J50 Datasheet sec 10.7.3.1, Table 10-14 */
	RPOR6 = 9;   /* Put SDO on RB3 which is RP6 */
	RPOR13 = 10; /* Put SCK on RC2 which is RP13 */
	/* CS is on RC6 (RP17), but since it's handled manually there's
	 * no need to do anything here. */

	EECON2 = 0x55;
	EECON2 = 0xAA;
	PPSCONbits.IOLOCK = 1;
	INTCONbits.GIE = 1;
#else
	#error "Board not supported. Add a section in board_setup_spi_pins() for your board here."
#endif
}
