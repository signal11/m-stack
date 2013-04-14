/***********************************
 Sample USB Configuration
 
 Alan Ott
 Signal 11 Software
 2013-04-08
***********************************/


#ifndef USB_CONFIG_H__
#define USB_CONFIG_H__

/* Number of endpoint numbers besides endpoint zero. It's worth noting that
   and endpoint NUMBER does not completely describe an endpoint, but the
   along with the DIRECTION does (eg: EP 1 IN).  The #define below turns on
   BOTH IN and OUT endpoints for endpoint numbers (besides zero) up to the
   value specified.  For example, setting NUM_ENDPOINT_NUMBERS to 2 will
   activate endpoints EP 1 IN, EP 1 OUT, EP 2 IN, EP 2 OUT.  */
#define NUM_ENDPOINT_NUMBERS 1

/* Only 8, 16, 32 and 64 are supported for endpoint zero length. */
#define EP_0_OUT_LEN 8
#define EP_0_IN_LEN 8

#define EP_1_OUT_LEN 64
#define EP_1_IN_LEN 64

#define EP_2_OUT_LEN 64
#define EP_2_IN_LEN 64

#define NUMBER_OF_CONFIGURATIONS 1

/* Comment the following line to use polling USB operation. You are responsible
   then for calling usb_service() periodically from your application. */
#define USB_USE_INTERRUPTS

/* Objects from usb_descriptors.c */
#define USB_DEVICE_DESCRIPTOR this_device_descriptor
#define USB_CONFIG_DESCRIPTOR_MAP usb_application_config_descs
#define USB_STRING_DESCRIPTOR_FUNC usb_application_get_string

/* Optional callbacks from usb.c. Leave them commented if you don't want to
   use them. For the prototypes and documentation for each one, see usb.h. */

#define SET_CONFIGURATION_CALLBACK app_set_configuration_callback



#endif /* USB_CONFIG_H__ */
