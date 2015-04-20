/*
 * Timer driver for PIC32 and PIC24
 *
 * Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *                    Signal 11 Software
 *
 * 2015-04-17
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
#include "timer.h"

#if defined(__XC8) || defined (__XC16__) || defined (__XC32__)
	#include <xc.h>
#else
	#error "Compiler not supported"
#endif

void timer_start(uint16_t timeout_milliseconds)
{
#ifdef _PIC18
	uint16_t timeout = OSCILLATOR_HZ / 4 /
	                             256 / 1000 * timeout_milliseconds;

	/* Timer sets TMR0IF at overflow, so TMR0 needs to be set to the
	 * number of ticks _before_ overflow. */
	timeout = 65535 - timeout + 1;

	INTCONbits.TMR0IF = 0; /* Turn off the interrupt flag */
	T0CONbits.TMR0ON = 0;  /* 0 = timer off */
	T0CONbits.T08BIT = 0;  /* 0 = 16-bit timer */
	T0CONbits.T0CS = 0;    /* clock select: 0 = Fosc/4 */
	T0CONbits.PSA = 0;     /* 0 = use prescaler */
	T0CONbits.T0PS= 7;     /* 7 = 1:256 prescaler */
	TMR0H = timeout >> 8 & 0xff;
	TMR0L = timeout & 0xff;
	T0CONbits.TMR0ON = 1;  /* timer on */

#elif __XC16__
	uint16_t timeout = OSCILLATOR_HZ / 2 /
	                             256 / 1000 * timeout_milliseconds;
	IFS0bits.T1IF = 0;   /* Turn off the interrupt flag */
	TMR1 = 0;
	T1CONbits.TON = 1;
	T1CONbits.TCKPS = 3; /* Prescalar: 3 = 1:256 */
	PR1 = timeout;

#elif __PIC32MX__
	uint8_t divisor = 1 << OSCCONbits.PBDIV;
	uint32_t peripheral_clock = OSCILLATOR_HZ / divisor;
	uint32_t timeout = peripheral_clock / 256 / 1000 * timeout_milliseconds;

	/* Use timers 2 and 3 combined to get a 32-bit timer. In this mode
	 * TMR2 gets the configuration values and the interrupt flag will
	 * come on TMR3. */
	IFS0bits.T3IF = 0;      /* Turn off the interrupt flag */
	TMR2 = 0;
	TMR3 = 0;
	T2CONbits.T32 = 1;      /* 1 = 32-bit timer combining T2 and T3 */
	T2CONbits.TCKPS = 7;    /* Prescalar: 7 = 1:256 */
	PR2 = timeout & 0xffff;
	PR3 = timeout >> 16 & 0xffff;
	T2CONbits.ON = 1;

#else
	#error "Processor family not supported"

#endif
}

bool timer_expired(void)
{
#ifdef _PIC18
	return (INTCONbits.TMR0IF != 0);

#elif __XC16__
	return (IFS0bits.T1IF != 0);

#elif __PIC32MX__
	return (IFS0bits.T3IF != 0);

#else
	#error "Processor family not supported"
#endif
}

void timer_stop(void)
{
#ifdef _PIC18
	T0CONbits.TMR0ON = 0;

#elif __XC16__
	T1CONbits.TON = 0;

#elif __PIC32MX__
	T2CONbits.ON = 0;

#else
	#error "Processor family not supported"
#endif
}
