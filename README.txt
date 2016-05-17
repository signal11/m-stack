
   M-Stack: Free USB Stack for PIC 16F, 18F, 24F, and 32MX Microcontrollers
  ==========================================================================

About
======

A USB device stack is the software necessary to drive the USB device
peripheral hardware on a microcontroller (MCU) or other device.  Typically a
USB peripheral only supports the transaction level and below of the USB
protocol.  Enumeration and transfers are left to the firmware or software to
implement.  The Microchip PIC line of microcontrollers work exactly this
way.

M-Stack is a functional, well-documented, open source implementation of a
USB stack for Microchip PIC platforms.  It performs the following
operations:
 * USB device hardware initialization
 * USB interrupt handling
 * Management of the serial interface engine SIE
 * Management of endpoints
 * Fragmentation and reassembly of control transfers
 * Handling and sending standard setup requests
 * Enumeration

The following device classes are supported:
 * HID - Human Interface Device
 * CDC-ACM - Communication Device Class - Abstract Control Model
 * Mass Storage Class (MSC) with MMC/SD card support

Other features include:
 * Clean, well-documented examples (device, and PC-host (libusb))
 * Fast PIC24 bootloader

While having a working USB stack is of great benefit when starting a USB
project, know that there is no substitute for actually knowing the details
of how USB works.  The USB specification should be consulted frequently when
creating a USB device.

Home
-----
The master web page for this project can be found at:
	http://www.signal11.us/oss/m-stack/

License
========

The software is dual-licensed under the GNU Lesser General Public License
(LGPL) version 3.0 and the Apache License, version 2.0.  It may be used
under the terms of either one of these licenses as seen fit.  It may be
used in commercial and open hardware projects so long as the conditions of
one of these licenses can be met.

Separate commercial licenses are available for purchase for companies and
projects which cannot or wish to not comply with the terms of the LGPL or
Apache License.

Contribution
-------------
Code contributions to the project are welcome, but know that copyright on
code submitted will need to be assigned to Signal 11 Software.  Shared
copyright is permissible.

Support
--------
Free support for this product is be somewhat limited. A mailing list will
be setup soon.

Paid support is available through Signal 11 Software.

Consulting Services
--------------------
USB is hard. There's no getting around it. There's a good chance if you're
reading this that USB is not the main focus of your project.  USB is a means
to an end, and your specialization is likely more related to the end than to
the means.  Given that, doesn't it makes sense to hire someone to help you
with the USB aspect of your project?  Many find that hiring specialized
consultants for specialized jobs can drastically reduce the total cost of a
project.  Signal 11 can help you.  This software testifies to the expertise
Signal 11 Software has with respect to USB and software development.  See
the website at http://www.signal11.us for more information.

Getting Started
================

Downloading the Software
-------------------------
M-Stack's source code is currently hosted on Github at:
	https://github.com/signal11/m-stack

To get the latest version, run:
	git clone https://github.com/signal11/m-stack.git

Documentation
--------------
M-Stack has Doxygen-generated documentation at:
      http://www.signal11.us/oss/m-stack/m-stack/html/group__public__api.html

Supported hardware
-------------------
M-Stack has currently been tested on PIC16F, PIC18F PIC24F, and PIC32MX
devices.  Microchip has obviously made a conscious effort to make the
register-level interfaces to their USB peripherals as similar as possible
across MCUs and even across MCU families.  While many devices should be able
to be easily supported with this software, there are often times small
differences which need to be worked out, the biggest of which being buffer
descriptor and data buffer locations with respect to the DMA capabilities of
the microcontroller being used.

The following MCU's and configurations have been tested:
 * PIC32MX460F512L - PIC32 USB Starter Board
 * PIC24FJ64GB002
 * PIC24FJ256DA206
 * PIC18F46J50 - PIC18F Starter Kit
 * PIC16F1459
 * PIC16F1454 - similar to PIC16F1459

If your hardware is not supported, and it's in the PIC16F/18F/24F/32MX
family, I can probably easily make you a port without very much trouble. 
The easiest way is for you to send me a development board.  If your hardware
is in another MCU family which is not currently supported, I can also make
you a port, but it will be more effort.  In either case, I'd be happy to
talk with you about it.

Supported Software
-------------------
The USB stack is supported by the following software:
 * Microchip XC8 compiler
 * Microchip XC16 compiler
 * Microchip XC32 compiler
 * Microchip MPLAB X IDE

Note that the C18 compiler is not currently supported. There are some
#defines in the code for C18 because this project came from code that was
originally done on a PIC18F4550 using C18.  It has not yet been determined
whether a port to C18 will be made, as C18 has been deprecated by Microchip.
Further, C18 has some properties which make a port somewhat more difficult
than other compilers.

Building the Unit Test Firmware
--------------------------------
Open the MPLAB.X project in apps/unit_test in the MPLAB X IDE. Select a
configuration and build. Make sure a supported compiler is installed.

Building the Test Software
---------------------------
The host_test/ directory contains Libusb-based test programs for testing the
functionality of a USB device running the unit test firmware.  Currently the
Libusb test software has only been tested Linux, but since its only
dependency is the cross-platform Libusb library, it is easily portable to
other operating systems on which Libusb is supported.

With Libusb installed, run "make" in the host_test/ directory to build the
test software.

Running the Test Software
--------------------------
./control_transfer_in [number_of_bytes]
	* Execute an IN control transfer requesting number_of_bytes bytes
	  from the device. The data returned will be printed. The unit test
	  firmware supports control transfers up to 512 bytes.
./control_transfer_out [number_of_bytes]
	* Execute an OUT control transfer sending number_of_bytes bytes
	  to the device. A message will be printed. The unit test
	  firmware supports control transfers up to 512 bytes.
./test [number_of_bytes]
	* Send and then ask for number_of_bytes bytes on EP 1 OUT and EP 1
	  IN, respectively. The data is printed out. The unit test firmware
	  will support up to 128-bytes of this kind of operation.
./feature <clear>
	* Set the Endpoint halt feature on Endpoint 1 IN. Passing the
	  "clear" parameter clears endpoint halt.

Source Tree Structure
----------------------
(root)
 |
 +- usb/                   <- USB stack software
 |   +- include/           <- API include file directory
 |   +- src/               <- Source files
 +- storage/               <- MMC/SD card implementation
     +- include/           <- API include files
     +- src/               <- Source files
 +- apps/                  <- Firmware USB device applications,
     |                        examples, and tests
     +- unit_test/         <- Unit test firmware
     +- hid_mouse/         <- HID Mouse example
     +- cdc_acm/           <- CDC/ACM virtual COM port example
     +- msc_test/          <- Mass Storage Class example
     +- bootloader/        <- USB bootloader firmware and software
 +- host_test/             <- Software applications to run from a PC Host

USB Stack Source Files
-----------------------
usb/src/usb.c         - The implementation of the USB stack.
usb/src/usb_hal.h     - Hardware abstraction layer (HAL) containing
                        differences specific to each platform.
usb/src/usb_hid.c     - Implementation of the HID class.
usb/include/usb.h     - The API header for the USB stack. Applications should
                        #include this file.
usb/include/usb_ch9.h - Enums and structs from Chapter 9 of the USB
                        specification which deals with control transfers and
                        enumeration.  An application should #include this
                        file from their usb_descriptors.c and from any file
                        which deals with control transfers.
usb/src/usb_hid.h     - Enums, structs, and callbacks for the HID class

Application Source Files (Unit Test Example)
---------------------------------------------
apps/unit_test/main.c            - Main program file
apps/unit_test/usb_config.h      - USB stack configuration file. The USB
                                   stack will include this file and use it
                                   for configuration.  The application
                                   should set the #defines in this file to
                                   suit the application's needs.
apps/unit_test/usb_descriptors.c - The application's descriptors. The
                                   application should set the descriptors in
                                   this file as desired to suit the
                                   application's needs.
apps/hid_mouse/*                 - HID mouse example (same files as above)

Making Your Own Project
------------------------
The easiest way to create a project using M-Stack is to simply copy one of
the examples and modify it accordingly.  Sometimes it's better though to do
things the hard way in order to understand better.

To create a new project, perform the following steps:
1. Create a new project with MPLAB X.
2. Copy and add the usb/ directory as a subdirectory of your project.
3. Add the usb/include directory to the include path of your project. (Note
   that the include paths are relative to the Makefile.  If you set up your
   project like the examples, with an MPLAB.X/ subdirectory, you'll need to
   add an additional ../ to the beginning of the include path).
4. Add . to the include path of your project (same note from #3 applies).
5. Copy a usb_config.h and a usb_descriptors.c file from one of the example
   projects into your main project directory.
6. Modify usb_config.h to match your desired device configuration.
7. Modify usb_descriptors.c to match your device configuration.
8. If you're using a PIC16F/18F platform, add an interrupt handler similar
   to one of the examples.
9. Reference main.c in one of the examples, and the Doxygen-generated
   documentation to add your application logic.
10. Make sure to configure the MCU for your board (__CONFIG registers, etc.).


Limitations
============

Nothing's perfect. Here are the known limitations:
 * Control transfers are supported on endpoint 0 only.
 * Isochronous transfers are not supported.
 * Remote wake-up is not supported.


Future Plans
=============

The following features are on the horizon:
	* Support for more specific MCUs
	* dsPIC33E and PIC24E support
	* Isochronous transfers


References
===========

USB
----
USB Implementers Forum:  http://www.usb.org
USB Specification, 2.0:  http://www.usb.org/developers/docs
Jan Axelson's "USB Complete," 4th Edition:  http://www.lvr.com/usbc.htm
Jan Axelson's online USB resources:  http://www.lvr.com/usb.htm

Microchip PIC
--------------
Microchip Libraries for Applications: http://www.microchip.com/mla
Microchip MPLAB X: http://www.microchip.com/mplabx

Contacts
=========

Please contact Signal 11 Software directly for support or consulting help.
Sometime in the near future there will be a mailing list.

Alan Ott
Signal 11 Software
alan@signal11.us
http://www.signal11.us
+1 407-222-6975

2013-04-26
2013-05-20
