/*
 *  M-Stack USB Mass Storage Device Class Structures
 *  Copyright (C) 2014 Alan Ott <alan@signal11.us>
 *  Copyright (C) 2014 Signal 11 Software
 *
 *  2014-06-04
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

#ifndef USB_MSC_H__
#define USB_MSC_H__

/** @file usb_msc.h
 *  @brief USB Mass Storage Class Enumerations and Structures
 *  @defgroup public_api Public API
 */

/** @addtogroup public_api
 *  @{
 */

#include <stdint.h>
#include "usb_config.h"

#if defined(__XC16__) || defined(__XC32__)
#pragma pack(push, 1)
#elif __XC8
    #if __XC8_VERSION >= 2000
	// This is needed, even though the XC8 manual says its unnecessary
	#pragma pack(push, 1)
    #endif
#else
#error "Compiler not supported"
#endif

/** @defgroup msc_items USB MSC Class Enumerations and Descriptors
 *  @brief Packet structs, constants, and callback functions implementing
 *  the "Universal Serial Bus Mass Storage Class", revision 1.4, and the
 *  "Universal Serial Bus Mass Storage Class Bulk-Only Transport", revision
 *  1.0.
 *
 *  An indespensible reference is Jan Axelson's "USB Mass Storage" book.
 *  The major value in this book is the real-life perspective regarding
 *  which parts of the SCSI standards are actually used in USB devices, what
 *  they are used for, and how the SCSI commands and terminology map onto USB
 *  devices.
 *
 *  Document sections listed in the comments are prefixed with the document
 *  they reference as follows:\n
 *  \b MSCO: USB Mass Storage Class, revision 1.4\n
 *  \b BOT:  USB Mass Storage Class Bulk-Only Transport, revision 1.0\n
 *  \b Axelson: Jan Axelson's "USB Mass Storage" book\n
 *
 *  For more information, see the above referenced document, available from
 *  http://www.usb.org .
 *  @addtogroup msc_items
 *  @{
 */

#define MSC_DEVICE_CLASS 0x08 /* http://www.usb.org/developers/defined_class */
#define MSC_SCSI_TRANSPARENT_COMMAND_SET_SUBCLASS 0x06
/* Many of the subclass codes (MSCO: sec 2) are omitted here. Get in
 * contact with Signal 11 if you need something specific.  */

#define MSC_PROTOCOL_CODE_BBB 0x50 /* Bulk-Only */
/* Many of the protocol codes (MSCO: sec 3) are omitted here. Get in
 * contact with Signal 11 if you need something specific.  */

#if MSC_MAX_LUNS_PER_INTERFACE <= 8
	typedef uint8_t msc_lun_mask_t;
#else
	typedef uint16_t msc_lun_mask_t;
#endif


/** MSC Class Requests
 *
 * These are the class requests needed for MSC Bulk-Only Transport (MSCO:
 * sec 4, table 3).  Others are omitted.  Get in contact with Signal 11 if
 * you need something specific.
 */
enum MSCRequests {
	MSC_GET_MAX_LUN = 0xfe,
	MSC_BULK_ONLY_MASS_STORAGE_RESET = 0xff,
};

/** MSC Command Block Status Values
 *
 * See BOT, 5.2
 */
enum MSCStatus {
	MSC_STATUS_PASSED = 0, /* Success */
	MSC_STATUS_FAILED = 1,
	MSC_STATUS_PHASE_ERROR = 2,
};

/** MSC Command Block Flags (Direction is the only used bit)
 *
 * See BOT, 5.1
 */
enum MSCDirection {
	MSC_DIRECTION_OUT = 0x0,
	MSC_DIRECTION_IN_BIT = 0x80,
};

/** MSC Bulk-Only Data Command Block Wrapper
 *
 * See BOT, 5.1
 */
struct msc_command_block_wrapper {
	uint32_t dCBWSignature; /* Set to 0x43425355 */
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength; /** Data to be transfered */
	uint8_t bmCBWFlags; /** bit 0x80=data-in, 0x00=data-out */
	uint8_t bCBWLUN; /**< Lower 4 bits only */
	uint8_t bCBWCBLength ; /**< Lower 4 bits only; length of CBWCB */
	uint8_t CBWCB[16];
};

/** MSC Bulk-Only Data Command Block Wrapper
 *
 * See BOT, 5.2
 */
struct msc_command_status_wrapper {
	uint32_t dCSWSignature; /**< Set to 0x53425355 */
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t  bCSWStatus; /**< @see enum MSCStatus */
};

/* SCSI Definitions and Structures */

enum MSCSCSICommands {
	MSC_SCSI_FORMAT_UNIT = 0x04,
	MSC_SCSI_INQUIRY = 0x12,
	MSC_SCSI_MODE_SELECT_6 = 0x15,
	MSC_SCSI_MODE_SELECT_10 = 0x55,
	MSC_SCSI_MODE_SENSE_6 = 0x1a,
	MSC_SCSI_MODE_SENSE_10 = 0x5a,
	MSC_SCSI_START_STOP_UNIT = 0x1b,
	MSC_SCSI_READ_6 = 0x08,
	MSC_SCSI_READ_10 = 0x28,
	MSC_SCSI_READ_CAPACITY_10 = 0x25,
	MSC_SCSI_REPORT_LUNS = 0xa0,
	MSC_SCSI_REQUEST_SENSE = 0x03,
	MSC_SCSI_SEND_DIAGNOSTIC = 0x1d,
	MSC_SCSI_TEST_UNIT_READY = 0x00,
	MSC_SCSI_VERIFY = 0x2f,
	MSC_SCSI_WRITE_6 = 0x0a,
	MSC_SCSI_WRITE_10 = 0x2a,
};

struct msc_scsi_inquiry_command {
	uint8_t operation_code; /* 0x12 */
	uint8_t evpd; /* bit 0 only */
	uint8_t page_code;
	uint16_t allocation_length;
	uint8_t control;
};

struct msc_scsi_request_sense_command {
	uint8_t operation_code; /* 0x3 */
	uint8_t desc; /* bit 0 only */
	uint8_t reserved[2];
	uint8_t allocation_length;
	uint8_t control;
};

struct msc_scsi_mode_sense_6_command {
	uint8_t operation_code; /* 0x1a */
	uint8_t dbd_reserved;
	uint8_t pc_page_code; /* bits 6-7: PC, bits 0-5: Page Code */
	uint8_t subpage_code;
	uint8_t allocation_length;
	uint8_t control;
};

struct msc_scsi_start_stop_unit {
	uint8_t operation_code;
	uint8_t immed; /* Bit 0 only */
	uint8_t reserved[2];
	uint8_t command; /* bit 0: start, bit 1: LOEJ, bits 4-7: power cond */
	uint8_t control;
};

struct msc_scsi_read_10_command {
	uint8_t opcode;
	uint8_t unused_flags;
	uint32_t logical_block_address;
	uint8_t group_number;
	uint16_t transfer_length;
	uint8_t control_flags;
};

struct msc_scsi_write_10_command {
	uint8_t operation_code;
	uint8_t wrprotect_flags;
	uint32_t logical_block_address;
	uint8_t group_number;
	uint16_t transfer_length;
	uint8_t control;
};

enum MSCSCSIVersion {
	MSC_SCSI_SPC_VERSION_2 = 4,
	MSC_SCSI_SPC_VERSION_3 = 5,
};


struct scsi_inquiry_response {
	uint8_t peripheral; /**< Set to 0x0 */
	uint8_t rmb; /**< 0x80 for removable media */
	uint8_t version; /**< enum MSCSCSIVersion */
	uint8_t response_data_format; /**< Set to 0x2 */
	uint8_t additional_length; /**< 4 less than sizeof(inquiry_response) */
	uint8_t unused[3]; /**< Not used in our implementation */
	char vendor[8]; /**< ASCII, no zero-termination */
	char product[16]; /**< ASCII, no zero-termination */
	char revision[4]; /**< ASCII, no zero-termination */
};

struct scsi_capacity_response {
	uint32_t last_block;
	uint32_t block_length;
};

enum SCSISenseResponseCode {
	SCSI_SENSE_CURRENT_ERRORS = 0x70,
	SCSI_SENSE_DEFERRED_ERRORS = 0x71,
	SCSI_SENSE_INFORMATION_VALID = 0x80,
};

enum SCSISenseFlags {
	SCSI_SENSE_FILEMARK = 0x80,
	SCSI_SENSE_EOM = 0x40,
	SCSI_SENSE_ILI = 0x20,
	SCSI_SENSE_KEY_MASK = 0x0f,
};

struct scsi_mode_sense_response {
	uint8_t mode_data_length; /**< sizeof(scsi_mode_sense_response) - 1 */
	uint8_t medium_type; /**< Set to 0x00 for SBC devices */
	uint8_t device_specific_parameter; /**< set to 0x80 for write-protect, else 0x0 */
	uint8_t block_descriptor_length; /**< Set to 0x0 */
};

enum SCSISenseKeys {
	SCSI_SENSE_KEY_NOT_READY = 0x2,
	SCSI_SENSE_KEY_MEDIUM_ERROR = 0x3,
	SCSI_SENSE_KEY_ILLEGAL_REQUEST = 0x5,
	SCSI_SENSE_KEY_UNIT_ATTENTION = 0x6,
	SCSI_SENSE_KEY_DATA_PROTECT = 0x7,
};

enum SCSIAdditionalSenseCodes {
	/* ILLEGAL_REQUEST */
	SCSI_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE = 0x21,
	SCSI_ASC_INVALID_COMMAND_OPERATION_CODE = 0x20,
	SCSI_ASC_INVALID_FIELD_IN_COMMAND_PACKET = 0x24,
	SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED = 0x25,

	/* MEDIUM_ERROR */
	SCSI_ASC_PERIPHERAL_DEVICE_WRITE_FAULT = 0x03,
	SCSI_ASC_UNRECOVERED_READ_ERROR = 0x11,

	/* UNIT_ATTENTION */
	SCSI_ASC_WRITE_ERROR = 0x0c,
	SCSI_ASC_MEDIUM_NOT_PRESENT = 0x3a,

	/* WRITE_DATA_PROTECT */
	SCSI_ASC_WRITE_PROTECTED = 0x27,
};

struct scsi_sense_response {
	uint8_t response_code; /**< 0x70=current_errors, 0x71=deferred_errors
	                         *  0x80 bit = information_valid */
	uint8_t obsolete;
	uint8_t flags; /**< Bitmask of SCSISenseFlags OR'd with one from SCSISenseKeys */
	uint32_t information;
	uint8_t additional_sense_length;
	uint32_t command_specific_information;
	uint8_t additional_sense_code; /**< SCSIAdditionalSenseCodes */
	uint8_t additional_sense_code_qualifier;
	uint8_t field_replaceable_unit_code;
	uint8_t sense_key_specific[3];
	/* Additional, vendor-specific sense data goes here. */
};

#if defined(__XC16__) || defined(__XC32__)
#pragma pack(pop)
#elif __XC8
#else
#error "Compiler not supported"
#endif


/** MSC Application States
 *
 * The states the transport goes through.
 */
enum MSCApplicationStates {
	MSC_IDLE,
	MSC_DATA_TRANSPORT_IN,  /**< Next transactions will be data sent */
	MSC_DATA_TRANSPORT_OUT, /**< Next transactions will be data received */
	MSC_STALL,              /**< Next transaction needs to stall */
	MSC_CSW,                /**< Next transaction will contain the CSW */
	MSC_NEEDS_RESET_RECOVERY, /**< Reset recovery is required */
};

/** Return codes used by application callbacks.
 *
 * Success is defined to be 0 and all failures are < 0.
 */
enum MSCReturnCodes {
	MSC_SUCCESS                  =  0,
	MSC_ERROR_MEDIUM_NOT_PRESENT = -1, /**< The medium is not physically present */
	MSC_ERROR_INVALID_LUN        = -2, /**< The LUN is out of range */
	MSC_ERROR_INVALID_ADDRESS    = -3, /**< The LBA address is out of range */
	MSC_ERROR_WRITE_PROTECTED    = -4, /**< The medium is write protected */
	MSC_ERROR_READ               = -5, /**< Error while reading the medium */
	MSC_ERROR_WRITE              = -6, /**< Error while writing the medium */
	MSC_ERROR_MEDIUM             = -7, /**< Unspecified medium error */
};

/* Forward declare struct msc_application_data to enable the following:
 * 1. The struct be used by the callback,
 * 2. The callback can be used by the struct. */
struct msc_application_data;

/** @brief MSC Transmission Complete Callback
 *
 * This is the callback function type expected to be passed to @p
 * msc_start_send_to_host(). Callback functions will be called by the stack
 * when the transmission of a block of data has completed.
 *
 * @param app_data      Pointer to application data for this interface.
 * @param transfer_ok   @a true if transaction completed successfully, or
 *                      @a false if there was an error
 */
typedef void (*msc_completion_callback) (struct msc_application_data *app_data,
                                         bool transfer_ok);

/** MSC Applicaiton Data
 *
 * The application shall provide one of these structures for each interface
 * when the MSC class is initialized and keep it as a persistent object for
 * the lifetime of the application. Global variables work well for this.
 *
 * The Application shall initialize the variables in the first section. The
 * variables in the second section are used by the MSC class to keep track of
 * the state of the connection.
 *
 * The application will pass this structure to the MSC class whenever it needs
 * the MSC class to process data for an interface.
 */
struct msc_application_data {
	/* Application should initialize the following: */
	uint8_t interface;
	uint8_t max_lun; /**< The maximally numbered LUN. One-less than the number of LUNs. */
	uint8_t in_endpoint;
	uint8_t out_endpoint;
	uint8_t in_endpoint_size; /**< Size in bytes for IN endpoint */
	msc_lun_mask_t media_is_removable_mask; /**< bitmask, one bit for each LUN */
	const char *vendor; /**< SCSI-assigned vendor. Pointer to global or constant. */
	const char *product; /**< Pointer to global or constant. */
	const char *revision; /**< Pointer to global or constant. */

	/* MSC Class handler will initialize and use the following. The
	 * applicaiton should ignore these: */
	uint8_t state; /**< enum MSCApplicationStates */
	uint32_t current_tag;
	/* Error-reporting codes */
	uint8_t sense_key;
	uint8_t additional_sense_code;
	/* CSW fields */
	uint32_t residue;
	uint8_t status; /**< enum MSCStatus */
	/* READ command handling fields */
	uint32_t requested_bytes; /* Bytes requested by SCSI */
	uint32_t requested_bytes_cbw; /* Bytes requested by the MSC (CBW) */
	uint32_t transferred_bytes;
	/* Block size for each LUN */
	uint32_t block_size[MSC_MAX_LUNS_PER_INTERFACE];

	/* Asynchronous transmit/receive data */
	union {
		const uint8_t *tx_buf; /**< Data to be sent to the host. */
		uint8_t *rx_buf;       /**< Data received from the host. */
	};
	uint16_t tx_len_remaining; /**< TX data remaining in the current block */
#ifdef MSC_WRITE_SUPPORT
	uint8_t *rx_buf_cur; /**< Current position in the RX buffer */
	size_t rx_buf_len;   /**< Length of the application's block RX buffer */
	/* Endpoint buffer management. */
	uint8_t out_ep_missed_transactions; /**< Number of out transactions not processed */
#endif
	msc_completion_callback operation_complete_callback;
};

/** Initialize the MSC class for all interfaces
 *
 * Initialize all instances of the MSC class. Call this function with an
 * array containing an @class msc_application_data for each
 * MSC interface. The @p app_data pointer should point to an array of valid
 * @class msc_application_data structures which have been filled out properly.
 *
 * The array should be valid for the lifetime of the application. Global
 * variables work well for this.
 *
 * @param app_data  Pointer to an array of application data structures
 * @param count     Number of structures (size of array) in app_data
 *
 * @returns
 *   Returns 0 on success, or -1 if some of the data is invalid. On a -1
 *   return, assume that none of the interfaces have been initialized
 *   properly and fail.
 */
uint8_t msc_init(struct msc_application_data *app_data, uint8_t count);

/** Process MSC Setup Request
 *
 * Process a setup request which has been unhandled as if it is potentially
 * an MSC setup request. This function will then call appropriate callbacks
 * into the appliction if the setup packet is one recognized by the MSC
 * specification.
 *
 * @param setup          A setup packet to handle
 *
 * @returns
 *   Returns 0 if the setup packet could be processed or -1 if it could not.
 */
int8_t process_msc_setup_request(const struct setup_packet *setup);

/** Clear Halt on MSC Endpoint
 *
 * Notify the MSC class that a CLEAR_FEATURE ENDPOINT_HALT has been
 * received for an MSC endpoint.
 *
 * @param endpoint_num    The endpoint number affected
 * @param direction       The endpoint direction affected. 0=out, 1=in
 *
 */
void msc_clear_halt(uint8_t endpoint_num, uint8_t direction);

/** Notify of a transaction completing on the Data-IN endpoint
 *
 * Notify the MSC class that an IN transaction has completed on the Data-IN
 * endpoint.  If using interrupts, call this function from the
 * @p IN_TRANSACTION_COMPLETE_CALLBACK.
 *
 * This function will not block.
 *
 * @param endpoint_num    The endpoint number affected
 */
void msc_in_transaction_complete(uint8_t endpoint_num);

/** Notify of a transaction completing on the Data-OUT endpoint
 *
 * Notify the MSC class that an OUT transaction has completed on the
 * Data-OUT endpoint.  This function will process the data which is
 * available on endpoint @p endpoint_num and will re-arm the endpoint when
 * appropriate.
 *
 * If using interrupts, call this function from the
 * @p OUT_TRANSACTION_CALLBACK.
 *
 * This function will not block.
 *
 * @param endpoint_num    The endpoint number affected
 */
void msc_out_transaction_complete(uint8_t endpoint_num);

/** Send data read from the medium to the host
 *
 * Start transmission of data that has been read from the storage medium to
 * the host. This function simply starts the transmission and returns control
 * immediately to the caller without blocking. When the transmission has
 * completed, the @p completion_callback will be called. The application
 * should call this function as soon as data is available from the storage
 * medium, and should not call it again until the @p completion_callback has
 * been called. For example, if 8 blocks are requested by the host, the
 * application could read a single block, call @p msc_start_send_to_host(),
 * wait for the @p completion_callback to be called, then call
 * @p msc_start_send_to_host() with the second block, and so on..
 *
 * This function does not block.
 *
 * @p completion_callback will be called from interrupt context and must
 * not block.
 *
 * @p len needs to be multiple of the IN endpoint size for all calls to
 * this function except the last in response to an @p MSC_READ() callback.
 *
 * @param app_data             Pointer to application data for this interface.
 * @param data                 Pointer to the data to send.
 * @param len                  Data length in bytes. Must be a multiple of
 *                             the IN endpoint size.
 * @param completion_callback  Pointer to a function which is called when
 *                             the transmission has completed.
 *
 * @returns
 *   Returns 0 if the transmission could be started or -1 if it could not.
 */
uint8_t msc_start_send_to_host(struct msc_application_data *app_data,
                               const uint8_t *data, uint16_t len,
                               msc_completion_callback completion_callback);

/** Notify the MSC class that a Read Operation has Completed
 *
 * Tell the MSC class that a read operation has been completed with either
 * success or failure.
 *
 * In the case of success, pass true to @p passed, indicating that the data
 * requested by an @p MSC_READ() callback has all been passed to the MSC
 * class using @p msc_start_send_to_host().  This will cause the transfer to
 * complete successfully.
 *
 * If the read failed, pass false to @p passed.  This will cause a SCSI
 * MEDIUM_ERROR to be returned to the host.
 *
 * @param app_data       Pointer to application data for this interface.
 * @param passed         Whether the read operation completed successfully
 */
void msc_notify_read_operation_complete(
                                    struct msc_application_data *app_data,
                                    bool passed);

#ifdef MSC_WRITE_SUPPORT
/** Notify Write Data Handled
 *
 * Tell the MSC class that a block of data provided to a write callback
 * has been handled, and that the buffer is no longer in use by the
 * applicaiton. The MSC stack will now provide the next block to be written
 * into the same buffer and call the callback provided to @p
 * MSC_START_WRITE().
 *
 * Note that calling this function does not necessarily indicate that the
 * data was successfully written to the medium. It only indicates that the
 * application has received the buffer and dealt with it, and requests that
 * the MSC stack put the next block of data into the buffer. It is possible
 * the application does not know that a write operation has failed (or will
 * fail) until the end of the write operation.
 *
 * Note that if there are OUT transfers pending (and waiting because the
 * application is using the buffer), and if the application's buffer is
 * sufficiently small, then the MSC stack could potentially call the @p
 * msc_completion_callback() from this function. With small buffers, this is
 * a typical case.
 *
 * If there was an error in handling the data, call @p
 * msc_notify_write_operation_complete() instead to cancel the transport.
 *
 * @param app_data       Pointer to application data for this interface.
 */
void msc_notify_write_data_handled(struct msc_application_data *app_data);

/** Notify Write Operation Complete
 *
 * Tell the MSC class that a write operation has completed, either with
 * full success, with partial success, or with failure. The MSC stack can now
 * complete the data transport and report the status to the host.
 *
 * In the case of a complete success, set @p passed to true, and set @p
 * bytes_processed to the number of bytes which were asked for and written.
 *
 * In the case of a failure or a partial success, set @p passed to false
 * and set @p bytes_processed to the number of bytes which were acutally
 * written successfully.
 *
 * Pass true as @p passed if the write succeeded or false if it failed.
 *
 * @param app_data         Pointer to application data for this interface.
 * @param passed           Whether the write operation fully completed
 *                         successfully, writing all desired data.
 * @param bytes_processed  The number of bytes successfully written. This may
 *                         be fewer than the number of bytes asked for by the
 *                         host.
 */
void msc_notify_write_operation_complete(struct msc_application_data *app_data,
                                         bool passed,
                                         uint32_t bytes_processed);
#endif

/** MSC Bulk-Only Mass Storage Reset callback
 *
 * The MSC class will call this function when a Bulk-Only Mass Storage Reset
 * request has been received from the host.  This function should perform
 * a reset of the device indicated by @p interfce and not return until this
 * has been completed.
 *
 * @param interface      The interface for which the command is intended
 *
 * @returns
 *   Return 0 if the request can be handled or -1 if it cannot. Returning -1
 *   will cause STALL to be returned to the host.
 */
extern int8_t MSC_BULK_ONLY_MASS_STORAGE_RESET_CALLBACK(uint8_t interface);

#ifdef MSC_GET_STORAGE_INFORMATION
/** MSC Get Storage Information Callback
 *
 * The USB Stack will call this function when the host requests information
 * about the storage device.
 *
 * SCSI (and therefore USB) drives are required to support LBA (Logical Block
 * Addressing) which means every block on the drive can be addressed as an
 * array of fixed-sized blocks.
 *
 * This function may be called more than once as data is requested by the
 * host.
 *
 * @param app_data       Pointer to application data for this interface.
 * @param lun            The Logical Unit Number (LUN) of the medium requested.
 * @param block_size     The block size for this medium. This block size is
 *                       used for both reading and writing. It should be a
 *                       multiple of both the IN and OUT endpoint sizes, but
 *                       does not strictly have to be. If it is not, the
 *                       handling of reads and writes on the application side
 *                       will likely be more complex. The block_size must be
 *                       below 2^24.
 * @param num_blocks     The number of blocks the medium contains.
 * @param write_protect  Whether write-protection is enabled.
 *
 *
 * @returns
 *   Return a code from @p MSCReturnCodes. Returning non-success will cause
 *   an error to be returned to the host.
 */
extern int8_t MSC_GET_STORAGE_INFORMATION(
			const struct msc_application_data *app_data,
			uint8_t lun,
			uint32_t *block_size,
			uint32_t *num_blocks,
			bool *write_protect);
#else
#error "You must define MSC_GET_STORAGE_INFORMATION in your usb_config.h"
#endif

#ifdef MSC_UNIT_READY
/** MSC Check if Medium is Ready Callback
 *
 * The USB Stack will call this function when the host requests information
 * regarding whether the medium is ready or not.
 *
 * This function may be called more than once as data is requested by the
 * host.
 *
 * @param app_data       Pointer to application data for this interface.
 * @param lun            The Logical Unit Number (LUN) of the medium requested.
 *
 * @returns
 *   Return a code from @p MSCReturnCodes. Returning non-success will cause
 *   an error to be returned to the host.
 */
extern int8_t MSC_UNIT_READY(
			const struct msc_application_data *app_data,
			uint8_t lun);
#else
#error "You must define MSC_UNIT_READY in your usb_config.h"
#endif

#ifdef MSC_START_STOP_UNIT
/** Start or Stop an MSC Unit
 *
 * The USB Stack will call this function when the host requests that a
 * logical unit start or stop. This is mostly used for stopping and ejecting
 * the medium.
 *
 * @param app_data       Pointer to application data for this interface.
 * @param lun            The Logical Unit Number (LUN) of the medium requested.
 * @param start          Whether to start or stop the medium. true means to
 *                       start, and false means to stop the medium.
 * @param load_eject     Whether to load or eject the medium. The desired
 *                       behavior depends on the value of @p start. When
 *                       load_eject is true, then the application should load
 *                       media if start is true, or eject media if start is
 *                       false.
 *
 * The four cases for @p start and @p load_eject:
 * if (start  && load_eject):   load the medium
 * if (start  && !load_eject):  neither load nor eject
 * if (!start && load_eject):   eject the medium
 * if (!start && !load_eject):  neither load nor eject
 *
 * @returns
 *   Return a code from @p MSCReturnCodes. Returning non-success will cause
 *   an error to be returned to the host.
 */
extern int8_t MSC_START_STOP_UNIT(
			const struct msc_application_data *app_data,
			uint8_t lun,
			bool start,
			bool load_eject);
#else
#error "You must define MSC_START_STOP_UNIT in your usb_config.h"
#endif

#ifdef MSC_START_READ
/** MSC Read Callback
 *
 * The USB Stack will call this function when the host requests a read
 * operation from the device. This callback will initiate reading data from
 * the medium. The application will then call @p msc_start_send_to_host()
 * repeatedly until all data has been given to the MSC class. At that point,
 * the application must call @p msc_notify_read_operation_complete() to
 * indicate that the Data-Transport is complete.
 *
 * See the documentation for @p msc_start_send_to_host() for additional
 * restrictions.
 *
 * Note that this funciton must simply kick-off the reading/sending of the
 * data and return quickly. In other words, this function must not block.
 *
 * @param app_data       Pointer to application data for this interface.
 * @param lun            The Logical Unit Number (LUN) of the medium requested.
 * @param lba_address    Logical Block Address to start reading from.
 * @param num_blocks     Number of blocks to read.
 *
 * @returns
 *   Return a code from @p MSCReturnCodes. Returning non-success will cause
 *   an error to be returned to the host.
 */
extern int8_t MSC_START_READ(
		struct msc_application_data *app_data,
		uint8_t lun,
		uint32_t lba_address,
		uint16_t num_blocks);
#else
#error "You must define MSC_START_READ in your usb_config.h"
#endif

#ifdef MSC_WRITE_SUPPORT
#ifdef MSC_START_WRITE
/** MSC Write Callback
 *
 * The USB Stack will call this function when the host requests to write
 * data to the device.  The application's implementation of this function
 * will provide a buffer (@p buffer) where the USB stack will place the data
 * as it is received.  Once the buffer is full, the USB stack will call the
 * provided callback function (@p callback), notifying the application that
 * it can begin processing the data (ie: writing it to the medium).  Once
 * the buffer has been processed (and the application is done with the
 * buffer), the application must then call @p
 * msc_notify_block_write_complete() to notify the USB stack that it is
 * ready for the next buffer-full of data.
 *
 * Note that this funciton must simply set any necessary state on the
 * appliction side and return the requested data quickly. In other words,
 * this function must not block.
 *
 * @param app_data       Pointer to application data for this interface.
 * @param lun            The Logical Unit Number (LUN) of the medium requested.
 * @param lba_address    Logical Block Address the data is intended for.
 * @param num_blocks     Number of blocks which will eventually be written.
 * @param buffer         The place to put the data from the USB bus
 * @param buffer_len     The size of the buffer in bytes. It must be a
 *                       multiple of the OUT endpoint size.
 * @param callback       A function to be called when the data has been
 *                       received from the host. It will be called from
 *                       interrupt context and must not block.
 *
 * @returns
 *   Return a code from @p MSCReturnCodes. Returning non-success will cause
 *   an error to be returned to the host.
 */
extern int8_t MSC_START_WRITE(
		struct msc_application_data *app_data,
		uint8_t lun,
		uint32_t lba_address,
		uint16_t num_blocks,
		uint8_t **buffer,
		size_t *buffer_len,
		msc_completion_callback *callback);
#else
#error "You must either define MSC_START_WRITE in your usb_config.h or make this MSC class read-only."
#endif /* MSC_START_WRITE */
#endif /* MSC_WRITE_SUPPORT */

/* Doxygen end-of-group for msc_items */
/** @}*/

/* Doxygen end-of-group for public_api */
/** @}*/

#endif /* USB_MSC_H__ */
