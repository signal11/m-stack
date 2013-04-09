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

#define EP_0_OUT_LEN 64
#define EP_0_IN_LEN 64

#define EP_1_OUT_LEN 64
#define EP_1_IN_LEN 64

#define EP_2_OUT_LEN 64
#define EP_2_IN_LEN 64


#endif /* USB_CONFIG_H__ */
