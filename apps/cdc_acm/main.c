/*
 * USB CDC-ACM Demo
 *
 * This file may be used by anyone for any purpose and may be used as a
 * starting point making your own application using M-Stack.
 *
 * It is worth noting that M-Stack itself is not under the same license as
 * this file.
 *
 * M-Stack is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  For details, see sections 7, 8, and 9
 * of the Apache License, version 2.0 which apply to this file.  If you have
 * purchased a commercial license for this software from Signal 11 Software,
 * your commercial license supersedes the information in this header.
 *
 * Alan Ott
 * Signal 11 Software
 * 2014-05-12
 */

#include "usb.h"
#include <xc.h>
#include <string.h>
#include "usb_config.h"
#include "usb_ch9.h"
#include "usb_cdc.h"
#include "hardware.h"

#ifdef MULTI_CLASS_DEVICE
static uint8_t cdc_interfaces[] = { 0 };
#endif

static void send_string_sync(uint8_t endpoint, const char *str)
{
	char *in_buf = (char*) usb_get_in_buffer(endpoint);

	while (usb_in_endpoint_busy(endpoint))
		;

	strcpy(in_buf, str);
	/* Hack: Get the length from strlen(). This is inefficient, but it's
	 * just a demo. strlen()'s return excludes the terminating NULL. */
	usb_send_in_buffer(endpoint, strlen(in_buf));
}

int main(void)
{

    hardware_init();

#ifdef MULTI_CLASS_DEVICE
	cdc_set_interface_list(cdc_interfaces, sizeof(cdc_interfaces));
#endif
	usb_init();

	uint8_t char_to_send = 'A';
	bool send = true;
	bool loopback = false;

	while (1) {

		/* Send data to the PC */
		if (usb_is_configured() &&
		    !usb_in_endpoint_halted(2) &&
		    !usb_in_endpoint_busy(2) && send) {

			int i;
			unsigned char *buf = usb_get_in_buffer(2);

			for (i = 0; i < 16; i++) {
				buf[i] = char_to_send++;
				if (char_to_send > 'Z')
					char_to_send = 'A';
			}
			buf[i++] = '\r';
			buf[i++] = '\n';
			usb_send_in_buffer(2, i);
		}

		/* Handle data received from the host */
		if (usb_is_configured() &&
		    !usb_out_endpoint_halted(2) &&
		    usb_out_endpoint_has_data(2)) {
			const unsigned char *out_buf;
			size_t out_buf_len;
			int i;

			/* Check for an empty transaction. */
			out_buf_len = usb_get_out_buffer(2, &out_buf);
			if (out_buf_len <= 0)
				goto empty;

			if (send) {
				/* Stop sendng if a key was hit. */
				send = false;
				send_string_sync(2, "Data send off ('h' for help)\r\n");
			}
			else if (loopback) {
				/* Loop data back to the PC */

				/* Wait until the IN endpoint can accept it */
				while (usb_in_endpoint_busy(2))
					;

				/* Copy contents of OUT buffer to IN buffer
				 * and send back to host. */
				memcpy(usb_get_in_buffer(2), out_buf, out_buf_len);
				usb_send_in_buffer(2, out_buf_len);

				/* Send a zero-length packet if the transaction
				 * length was the same as the endpoint
				 * length. This is for demo purposes. In real
				 * life, you only need to do this if the data
				 * you're transferring ends on a multiple of
				 * the endpoint length. */
				if (out_buf_len == EP_2_LEN) {
					/* Wait until the IN endpoint can accept it */
					while (usb_in_endpoint_busy(2))
						;
					usb_send_in_buffer(2, 0);
				}

				/* Scan for ~ character to end loopback mode */
				for (i = 0; i < out_buf_len; i++) {
					if (out_buf[i] == '~') {
						loopback = false;
						send_string_sync(2, "\r\nLoopback off ('h' for help)\r\n");
						break;
					}
				}
			}
			else {
				/* Scan for commands if not in loopback or
				 * send mode.
				 *
				 * This is a hack. One should really scan the
				 * entire string. In this case, since this
				 * is a demo, assume that the user is using
				 * a terminal program and typing the input,
				 * all but ensuring the data will come in
				 * single-character transactions. */
				if (out_buf[0] == 'h' || out_buf[0] == '?') {
					/* Show help.
					 * Make sure to not try to send more
					 * than 63 bytes of data in one
					 * transaction */
					send_string_sync(2,
						"\r\nHelp:\r\n"
						"\ts: send data\r\n"
						"\tl: loopback\r\n");
					send_string_sync(2,
						"\tn: send notification\r\n"
						"\th: help\r\n");
				}
				else if (out_buf[0] == 's')
					send = true;
				else if (out_buf[0] == 'l') {
					loopback = true;
					send_string_sync(2, "loopback enabled; press ~ to disable\r\n");
				}
				else if (out_buf[0] == 'n') {
					/* Send a Notification on Endpoint 1 */
					struct cdc_serial_state_notification *n =
						(struct cdc_serial_state_notification *)
							usb_get_in_buffer(1);

					n->header.REQUEST.bmRequestType = 0xa1;
					n->header.bNotification = CDC_SERIAL_STATE;
					n->header.wValue = 0;
					n->header.wIndex = 1; /* Interface */
					n->header.wLength = 2;
					n->data.serial_state = 0; /* Zero the whole bit mask */
					n->data.bits.bRxCarrier = 1;
					n->data.bits.bTxCarrier = 1;
					n->data.bits.bBreak = 0;
					n->data.bits.bRingSignal = 0;
					n->data.bits.bFraming = 0;
					n->data.bits.bParity = 0;
					n->data.bits.bOverrun = 0;

					/* Wait for the endpoint to be free */
					while (usb_in_endpoint_busy(1))
						;

					/* Send to to host */
					usb_send_in_buffer(1, sizeof(*n));

					send_string_sync(2, "Notification Sent\r\n");
				}
			}
empty:
			usb_arm_out_endpoint(2);
		}

		#ifndef USB_USE_INTERRUPTS
		usb_service();
		#endif
	}

	return 0;
}

/* Callbacks. These function names are set in usb_config.h. */
void app_set_configuration_callback(uint8_t configuration)
{

}

uint16_t app_get_device_status_callback()
{
	return 0x0000;
}

void app_endpoint_halt_callback(uint8_t endpoint, bool halted)
{

}

int8_t app_set_interface_callback(uint8_t interface, uint8_t alt_setting)
{
	return 0;
}

int8_t app_get_interface_callback(uint8_t interface)
{
	return 0;
}

void app_out_transaction_callback(uint8_t endpoint)
{

}

void app_in_transaction_complete_callback(uint8_t endpoint)
{

}

int8_t app_unknown_setup_request_callback(const struct setup_packet *setup)
{
	/* To use the CDC device class, have a handler for unknown setup
	 * requests and call process_cdc_setup_request() (as shown here),
	 * which will check if the setup request is CDC-related, and will
	 * call the CDC application callbacks defined in usb_cdc.h. For
	 * composite devices containing other device classes, make sure
	 * MULTI_CLASS_DEVICE is defined in usb_config.h and call all
	 * appropriate device class setup request functions here.
	 */
	return process_cdc_setup_request(setup);
}

int16_t app_unknown_get_descriptor_callback(const struct setup_packet *pkt, const void **descriptor)
{
	return -1;
}

void app_start_of_frame_callback(void)
{

}

void app_usb_reset_callback(void)
{

}

/* CDC Callbacks. See usb_cdc.h for documentation. */

int8_t app_send_encapsulated_command(uint8_t interface, uint16_t length)
{
	return -1;
}

int16_t app_get_encapsulated_response(uint8_t interface,
                                      uint16_t length, const void **report,
                                      usb_ep0_data_stage_callback *callback,
                                      void **context)
{
	return -1;
}

int8_t app_set_comm_feature_callback(uint8_t interface,
                                     bool idle_setting,
                                     bool data_multiplexed_state)
{
	return -1;
}

int8_t app_clear_comm_feature_callback(uint8_t interface,
                                       bool idle_setting,
                                       bool data_multiplexed_state)
{
	return -1;
}

int8_t app_get_comm_feature_callback(uint8_t interface,
                                     bool *idle_setting,
                                     bool *data_multiplexed_state)
{
	return -1;
}

static struct cdc_line_coding line_coding =
{
	115200,
	CDC_CHAR_FORMAT_1_STOP_BIT,
	CDC_PARITY_NONE,
	8,
};

int8_t app_set_line_coding_callback(uint8_t interface,
                                    const struct cdc_line_coding *coding)
{
	line_coding = *coding;
	return 0;
}

int8_t app_get_line_coding_callback(uint8_t interface,
                                    struct cdc_line_coding *coding)
{
	/* This is where baud rate, data, stop, and parity bits are set. */
	*coding = line_coding;
	return 0;
}

int8_t app_set_control_line_state_callback(uint8_t interface,
                                           bool dtr, bool dts)
{
	return 0;
}

int8_t app_send_break_callback(uint8_t interface, uint16_t duration)
{
	return 0;
}


#ifdef _PIC14E
void interrupt isr()
{
	usb_service();
}
#elif _PIC18

#ifdef __XC8
void interrupt high_priority isr()
{
	usb_service();
}
#elif _PICC18
#error need to make ISR
#endif

#endif
