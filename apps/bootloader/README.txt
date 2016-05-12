
PIC24F/32MX Bootloader
=======================

This is the PIC24F/32MX USB bootloader.  It contains firmware and software
to implement a USB bootloader using M-Stack.  It differs from the Microchip
bootloaders in several key ways in both licensing and implementation.

License
--------
Like the rest of M-Stack, this bootloader is free software, and its firmware
and software are usable under the terms of either the LGPL version 3, or the
Apache License, version 2.0.

The Linker scripts for the PIC24 come from the PIC Linker Script Generator
(linked below) and have a permissive license. See the file header for the
included linker scripts.

The PIC32 linker scripts are derived from the Linker scripts which are
distributed with the XC32 toolchain, version 1.40.  These linker scripts are
copyrighted by Microchip and are used under a BSD-style license.  See the
file header on these scripts for details.  This is separate from the license
of M-Stack.

Firmware Implementation
------------------------
This bootloader's implementation does not use HID, as the Microchip
bootloader does, and instead uses custom-class control transfers.  This
difference makes this bootloader faster, and gives it a more robust
interface, since control transfers can be rejected by the firmware if they
are not correct.

Another difference from the Microchip bootloader is that for PIC24F it uses
linker scripts from the Signal 11 PIC Linker Script Generator at
https://github.com/signal11/pic_linker_script .  The generated scripts for
PIC24F and the hand-modified scripts for PIC32MX make use of both the C
preprocessor and more advanced linker script features to create linker
scripts which can be used in the same form for both the bootloader and the
application to be loaded by the bootloader.  This way only a single linker
script is needed for both projects (bootloader and application) which is
less prone to the inherent errors of manual edits.  A single #define in the
MPLABX project is used to select whether the linker script is being used for
the bootloader or for the application (define either BOOTLOADER or
BOOTLOADER_APP to select the mode).  See the PIC Linker Script Generator
README for more details.

Also different in the implementation is that the linker script passes flash
addresses to the bootloader firmware.  This means that for MCUs with the
same flash layout (such as the entire PIC24F family and the entire PIC32MX
family), the bootloader firmware and software source code does not have to
change between different MCUs.  Only the linker script does, with data that
comes directly from the datasheets.  This means there are no #defines in the
source code with memory addresses and ranges for each MCU.

Software Implementation
------------------------
The software implementation is straight-forward. A hex file reader parses
the program data in Intel Hex file format, and libusb is used to send the
program data to the bootloader firmware.

Supported Platforms
--------------------
Currently tested platforms are:
	PIC24FJ64GB002
	PIC32MX460F512L (PIC32 USB Starter Board)

In theory, any PIC24F or PIC32MX MCU should work as long as a suitable
linker script is made.

Contributing
-------------
More platforms need to be added. If you want to use this bootloader on a
different PIC24F MCU, please add the platform to the Linker Script
Generator.  See the Linker Script Generator README for more information.

Future Plans
-------------
Future goals include the following:
	* More PIC24F platforms
	* PIC16F PIC18F PIC24E, dsPIC33E
Note that the USB stack will first have to be ported to several of those
platforms before te bootloader can be ported. Patches are welcome.


Alan Ott
Signal 11 Software
alan@signal11.us
http://www.signal11.us
407-222-6975
