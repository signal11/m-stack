/************************************
USB Device Hardware Abstraction

Alan Ott
Signal 11 Software
2013-04-08
*************************************/


#ifndef USB_HAL_H__
#define UAB_HAL_H__

#ifdef _PIC18
#define NEEDS_PULL /* Whether to pull up D+/D- with SFR_PULL_EN. */
#define HAS_LOW_SPEED

#define BDNADR_TYPE              uint16_t

#define SFR_FULL_SPEED_EN        UCFGbits.FSEN
#define SFR_PULL_EN              UCFGbits.UPUEN
#define SFR_ON_CHIP_XCVR_DIS     UCFGbits.UTRDIS
#define SET_PING_PONG_MODE(n)    do { UCFGbits.PPB0 = n & 1; UCFGbits.PPB1 = n & 2; } while (0)
#define SFR_TOKEN_COMPLETE       UIRbits.TRNIF

#define SFR_USB_INTERRUPT_FLAGS  UIR
#define SFR_USB_RESET_IF         UIRbits.URSTIF
#define SFR_USB_STALL_IF         UIRbits.STALLIF
#define SFR_USB_TOKEN_IF         UIRbits.TRNIF
#define SFR_USB_SOF_IF           UIRbits.SOFIF
#define SFR_USB_IF               PIR2bits.USBIF

#define SFR_USB_INTERRUPT_EN     UIE
#define SFR_TRANSFER_IE          UIE.TRNIE
#define SFR_RESET_IE             UIE.URSTIE
#define SFR_SOF_IE               UIE.SOFIE

#define SFR_USB_EXTENDED_INTERRUPT_EN UEIE

#define SFR_EP_MGMT_TYPE         UEP1bits_t /* TODO test */
#define SFR_EP_MGMT(n)           UEP##n##bits
#define SFR_EP_MGMT_HANDSHAKE    EPHSHK
#define SFR_EP_MGMT_STALL        EPSTALL
#define SFR_EP_MGMT_OUT_EN       EPOUTEN
#define SFR_EP_MGMT_IN_EN        EPINEN
#define SFR_EP_MGMT_CON_DIS      EPCONDIS /* disable control transfers */

#define SFR_USB_ADDR             UADDR
#define SFR_USB_EN               UCONbits.USBEN
#define SFR_USB_PKT_DIS          UCONbits.PKTDIS

#define SFR_USB_STATUS           USTAT
#define SFR_USB_STATUS_EP        USTATbits.ENDP
#define SFR_USB_STATUS_DIR       USTATbits.DIR
#define SFR_USB_STATUS_PPBI      USTATbits.PPBI

#define CLEAR_ALL_USB_IF()       SFR_USB_INTERRUPT_FLAGS = 0 //TODO TEST!
#define CLEAR_USB_RESET_IF()     SFR_USB_RESET_IF = 0
#define CLEAR_USB_STALL_IF()     SFR_USB_STALL_IF = 0
#define CLEAR_USB_TOKEN_IF()     SFR_USB_TOKEN_IF = 0
#define CLEAR_USB_SOF_IF()       SFR_USB_SOF_IF = 0

#ifdef _18F46J50
#define BD_ADDR 0x400
//#define BUFFER_ADDR
#else
#error "CPU not supported yet"
#endif

/* Compiler stuff. Probably should be somewhere else. */
#ifdef __C18
	#define FAR far
	#define memcpy_from_rom(x,y,z) memcpypgm2ram(x,(rom void*)y,z);
	#define XC8_BD_ADDR_TAG
	#define XC8_BUFFER_ADDR_TAG
#elif defined __XC8
	#define memcpy_from_rom(x,y,z) memcpy(x,y,z);
	#define FAR
	#define XC8_BD_ADDR_TAG @##BD_ADDR
	#ifdef BUFFER_ADDR
		#define XC8_BUFFER_ADDR_TAG @##BUFFER_ADDR
	#else
		#define XC8_BUFFER_ADDR_TAG
	#endif
#endif

#elif __XC16__

#define USB_NEEDS_POWER_ON
#define USB_NEEDS_SET_BD_ADDR_REG

#define BDNADR_TYPE              void *

#define SFR_PULL_EN              /* Not used on PIC24 */
#define SFR_ON_CHIP_XCVR_DIS     U1CNFG2bits.UTRDIS
#define SET_PING_PONG_MODE(n)    U1CNFG1bits.PPB = n
#define SFR_TOKEN_COMPLETE       U1IRbits.TRNIF

#define SFR_USB_INTERRUPT_FLAGS  U1IR
#define SFR_USB_RESET_IF         U1IRbits.URSTIF
#define SFR_USB_STALL_IF         U1IRbits.STALLIF
#define SFR_USB_TOKEN_IF         U1IRbits.TRNIF
#define SFR_USB_SOF_IF           U1IRbits.SOFIF
#define SFR_USB_IF               IFS5bits.USB1IF

#define SFR_USB_INTERRUPT_EN     U1IE
#define SFR_TRANSFER_IE          U1IE.TRNIE
#define SFR_RESET_IE             U1IE.URSTIE
#define SFR_SOF_IE               U1IE.SOFIE

#define SFR_USB_EXTENDED_INTERRUPT_EN U1EIE

#define SFR_EP_MGMT_TYPE         U1EP1BITS
#define SFR_EP_MGMT(n)           U1EP##n##bits
#define SFR_EP_MGMT_HANDSHAKE    EPHSHK
#define SFR_EP_MGMT_STALL        EPSTALL
#define SFR_EP_MGMT_IN_EN        EPTXEN   /* In/out from HOST perspective */
#define SFR_EP_MGMT_OUT_EN       EPRXEN
#define SFR_EP_MGMT_CON_DIS      EPCONDIS /* disable control transfers */
                                 /* Ignoring RETRYDIS and LSPD for now */
#define SFR_USB_ADDR             U1ADDR
#define SFR_USB_EN               U1CONbits.USBEN
#define SFR_USB_PKT_DIS          U1CONbits.PKTDIS


#define SFR_USB_STATUS           U1STAT
#define SFR_USB_STATUS_EP        U1STATbits.ENDPT
#define SFR_USB_STATUS_DIR       U1STATbits.DIR
#define SFR_USB_STATUS_PPBI      U1STATbits.PPBI

#define SFR_USB_POWER            U1PWRCbits.USBPWR
#define SFR_BD_ADDR_REG          U1BDTP1

#define BDnCNT                   STAT.BC /* buffer descriptor */

#define CLEAR_ALL_USB_IF()       do { SFR_USB_INTERRUPT_FLAGS = 0xff; U1EIR = 0xff; } while(0)
#define CLEAR_USB_RESET_IF()     SFR_USB_INTERRUPT_FLAGS = 0x1
#define CLEAR_USB_STALL_IF()     SFR_USB_INTERRUPT_FLAGS = 0x80
#define CLEAR_USB_TOKEN_IF()     SFR_USB_INTERRUPT_FLAGS = 0x08
#define CLEAR_USB_SOF_IF()       SFR_USB_INTERRUPT_FLAGS = 0x4

#define BD_ADDR
#define BUFFER_ADDR
#define XC8_BD_ADDR_TAG
#define XC8_BUFFER_ADDR_TAG

/* Compiler stuff. Probably should be somewhere else. */
#define FAR
#define memcpy_from_rom(x,y,z) memcpy(x,y,z);

#endif



#endif /* USB_HAL_H__ */