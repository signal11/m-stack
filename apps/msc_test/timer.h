/*
 * SPI driver for PIC18, PIC24, and PIC32MX
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

#ifndef S11_PIC_TIMER_H__
#define S11_PIC_TIMER_H__

#include <stdint.h>
#include <stdbool.h>
#include "board.h"

/* Basic Timer Implementation for MMC
 *
 * This is a simple timer implementation intended to be used for the
 * timeouts in the MMC component. It has millisecond resolution.
 *
 * This timer interface intended to be the back-end for the MMC component.
 * It is not a generic timer implementation. To create such an implementation
 * is beyond the scope of M-Stack. Further, a more generic implementation
 * would require more code space than would be acceptable on some of the
 * smaller supported microcontrollers (namely the 8-bit PICs).
 *
 * Thus, this implementation really only exists to serve the supported boards
 * in their specific configurations. However, modification for a custom
 * project should be easy and straight forward.
 *
 * The following hardware timer resources are used:
 *     PIC18: Timer 1 is used (16-bit timer)
 *     PIC24: Timer 1 is used (16-bit timer)
 *     PIC32: Timers 2 and 3 are combined to form a 32-bit timer
 *
 * The following are the maximum supported timeouts:
 *     PIC18: 2^16 / (OSCILLATOR_HZ / 4 / 256) seconds
 *     PIC24: 2^16 / (OSCILLATOR_HZ / 2 / 256) seconds
 *     PIC32: 2^32 / (OSCILLATOR_HZ / peripheral_clock_divisor / 256) seconds
 *
 * On all architectures, this timer is good for at least 1 second, which is
 * what is necessary for the MMC implementation.
 *
 * This timer interface is configured from board.h, which contains definitions
 * specific to the supported boards.
 */

/* Start the timer and set it to expire in timeout_milliseconds */
void timer_start(uint16_t timeout_milliseconds);

/* Return true if the timer has expired or false if it has not */
bool timer_expired(void);

/* Stop the timer from running */
void timer_stop(void);

#endif /* S11_PIC_TIMER_H__ */
