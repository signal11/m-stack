/*
 *  M-Stack Private Header File - Don't use outside of this directory
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2015-04-04
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

#ifndef USB_PRIV_H__
#define USB_PRIV_H__

#include <usb_config.h>

/* Private M-Stack functions */
#ifdef USB_USE_INTERRUPTS
/* Manipulate the transaction (token) interrupt.  There is no stack used
 * here, so care must be used to ensure that calls to these functions are
 * not nested.  */
void usb_disable_transaction_interrupt();
void usb_enable_transaction_interrupt();
#else
#define usb_disable_transaction_interrupt
#define usb_enable_transaction_interrupt
#endif

#endif /* USB_PRIV_H__ */
