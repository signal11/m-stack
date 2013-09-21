/*
 *  M-Stack Microsoft-Specific OS Descriptors
 *  Copyright (C) 2013 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2013 Signal 11 Software
 *
 *  2013-08-27
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

#ifndef USB_MICROSOFT_H__
#define USB_MICROSOFT_H__

/** @file usb.h
 *  @brief Microsoft-Specific OS Descriptors
 *  @defgroup public_api Public API
 */

/** @addtogroup public_api
 *  @{
 */

#include <stdint.h>

#if defined(__XC16__) || defined(__XC32__)
#pragma pack(push, 1)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/** @defgroup microsoft_items Microsoft-Specific Descriptors
 *  @brief Packet structs from Microsoft's documentation which deal with
 *  the Microsoft OS String Descriptor and the Extended Compat Descriptors.
 *
 *  For more information about these structures, see the Microsoft
 *  documentation at http://msdn.microsoft.com/library/windows/hardware/gg463182
 *  or search for "Microsoft OS Descriptors" on http://msdn.microsoft.com .
 *  Also see docs/winusb.txt in the M-Stack distribution.
 *
 *  @addtogroup microsoft_items
 *  @{
 */

/** OS String Descriptor
 *
 * This is the first descriptor Windows will request, as string number 0xee.
 */
struct microsoft_os_descriptor {
	uint8_t bLength;         /**< set to 0x12 */
	uint8_t bDescriptorType; /**< set to 0x3 */
	uint16_t qwSignature[7]; /**< set to "MSFT100" (Unicode) (no NULL) */
	uint8_t bMS_VendorCode;  /**< Set to the bRequest by which the host
	                              should ask for the Compat ID and
	                              Property descriptors. */
	uint8_t bPad;            /**< Set to 0x0 */
};

/** Extended Compat ID Header
 *
 * This is the header for the Extended Compat ID Descriptor
 */
struct microsoft_extended_compat_header {
	uint32_t dwLength;   /**< Total length of descriptor (header + functions) */
	uint16_t bcdVersion; /**< Descriptor version number, 0x0100. */
	uint16_t wIndex;     /**< This OS feature descriptor; set to 0x04. */
	uint8_t  bCount;     /**< Number of custom property sections */
	uint8_t  reserved[7];
};

/** Extended Compat ID Function
 *
 * This is the function struct for the Extended Compat ID Descriptor
 */
struct microsoft_extended_compat_function {
	uint8_t bFirstInterfaceNumber; /**< The interface or function number */
	uint8_t reserved;
	uint8_t compatibleID[8];       /**< Compatible String */
	uint8_t subCompatibleID[8];    /**< Subcompatible String */
	uint8_t reserved2[6];
};

/** Extended Properties Header
 *
 * This is the header for the Extended Properties Descriptor
 */
struct microsoft_extended_properties_header {
	uint32_t dwLength;   /**< Total length of descriptor (header + functions) */
	uint16_t bcdVersion; /**< Descriptor version number, 0x0100. */
	uint16_t wIndex;     /**< This OS feature descriptor; set to 0x04. */
	uint16_t bCount;     /**< Number of custom property sections */
};

/** Extended Property Section header
 *
 * This is the first part of the Extended Property Section, which is a
 * variable-length descriptor. The Variable-length types must be packed
 * manually after this section header.
 *
 */
struct microsoft_extended_property_section_header {
	uint32_t dwSize; /**< Size of this section (this struct + data) */
	uint32_t dwPropertyDataType; /**< Property Data Format */

/* Variable-length fields and lengths:
	uint16_t wPropertyNameLength;
	uint16_t bPropertyName[];
	uint32_t dwPropertyDataLength;
	uint8_t  bPropertyData[];
 */
};

#ifdef MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC
/** @brief Callback for the GET_MS_DESCRIPTOR/CompatID request
 *
 * MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC() is called when a @p
 * GET_MS_DESCRIPTOR request is received from the host with a wIndex of 0x0004.
 * The value of MS_GET_DESCRIPTOR request is defined by
 * MICROSOFT_OS_DESC_VENDOR_CODE which is set in usb_config.h, and reported to
 * the host as part of the @p microsoft_os_descriptor. See the MSDN
 * documentation on "Microsoft OS Descriptors" for more information.
 *
 * @param interface      The interface for which the descriptor is queried
 * @param descriptor     a pointer to a pointer which should be set to the
 *                       descriptor data.
 * @returns
 *   Return the length of the descriptor pointed to by @p *descriptor, or -1
 *   if the descriptor does not exist.
 */
uint16_t MICROSOFT_COMPAT_ID_DESCRIPTOR_FUNC(uint8_t interface,
                                              const void **descriptor);
#endif

#ifdef MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC
/** @brief Callback for the GET_MS_DESCRIPTOR/Custom_Property request
 *
 * MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC() is called when a @p
 * GET_MS_DESCRIPTOR request with a wIndex of 0x0005 is received from the host.
 * The value of the MS_GET_DESCRIPTOR request is defined by
 * MICROSOFT_OS_DESC_VENDOR_CODE which is set in usb_config.h, and reported to
 * the host as part of the @p microsoft_os_descriptor. See the MSDN
 * documentation on "Microsoft OS Descriptors" for more information.
 *
 * @param interface      The interface for which the descriptor is queried
 * @param descriptor     a pointer to a pointer which should be set to the
 *                       descriptor data.
 * @returns
 *   Return the length of the descriptor pointed to by @p *descriptor, or -1
 *   if the descriptor does not exist.
 */
uint16_t MICROSOFT_CUSTOM_PROPERTY_DESCRIPTOR_FUNC(uint8_t interface,
                                                   const void **descriptor);
#endif

/* Doxygen end-of-group for microsoft_items */
/** @}*/

#if defined(__XC16__) || defined(__XC32__)
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* USB_MICROSOFT_H__ */
