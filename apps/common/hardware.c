/*
 * Hardware Initialization
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
 * your commerical license superceeds the information in this header.
 *
 * Alan Ott
 * Signal 11 Software
 * 2016-05-02
 */

#include <xc.h>
#include <stdint.h>
#include "usb_config.h"
#include "hardware.h"

#if __PIC24FJ64GB002__ || __PIC24FJ32GB002__
_CONFIG1(WDTPS_PS16 & FWPSA_PR32 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx1 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & I2C1SEL_PRI & IOL1WAY_OFF & OSCIOFNC_OFF & FCKSM_CSDCMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP0 & SOSCSEL_IO & WUTSEL_LEG & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)
_CONFIG4(DSWDTPS_DSWDTPS3 & DSWDTOSC_SOSC & RTCOSC_SOSC & DSBOREN_OFF & DSWDTEN_OFF)

#elif __PIC24FJ256DA206__
_CONFIG1(WDTPS_PS32768 & FWPSA_PR128 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx2 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & IOL1WAY_OFF & OSCIOFNC_ON & FCKSM_CSECMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP255 & SOSCSEL_SOSC & WUTSEL_LEG & ALTPMP_ALPMPDIS & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)

#elif _18F46J50
#pragma config PLLDIV = 3 /* 3 = Divide by 3. 12MHz crystal => 4MHz */
#pragma config XINST = OFF
#pragma config WDTEN = OFF
#pragma config CPUDIV = OSC1
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config LPT1OSC = OFF
#pragma config T1DIG = ON
#pragma config OSC = ECPLL
#pragma config DSWDTEN = OFF
#pragma config IOL1WAY = OFF
#pragma config WPDIS = OFF /* This pragma seems backwards */

#elif _16F1459 || _16F1454
#pragma config FOSC = INTOSC
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config MCLRE = ON
#pragma config CP = OFF
#pragma config BOREN = ON
#pragma config CLKOUTEN = OFF
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config WRT = OFF
#pragma config CPUDIV = NOCLKDIV
#pragma config USBLSCLK = 48MHz
#pragma config PLLMULT = 3x
#pragma config PLLEN = ENABLED
#pragma config STVREN = ON
#pragma config BORV = LO
#pragma config LPBOR = ON
#pragma config LVP = OFF

#elif __32MX460F512L__
#pragma config DEBUG = OFF, ICESEL = ICS_PGx2, PWP = OFF, BWP = OFF, CP = OFF
#pragma config FNOSC = PRIPLL, FSOSCEN = OFF, IESO = OFF, POSCMOD = HS, \
	       OSCIOFNC = OFF, FPBDIV = DIV_1, FCKSM = CSDCMD, WDTPS = PS1, \
	       FWDTEN = OFF
#pragma config FPLLIDIV = DIV_2, FPLLMUL = MUL_15, UPLLIDIV = DIV_2, \
	       UPLLEN = ON, FPLLODIV = DIV_1

#elif __32MX795F512L__
// DEVCFG3
// USERID = No Setting
#pragma config FSRSSEL = PRIORITY_7     // SRS Select (SRS Priority 7)
#pragma config FMIIEN = ON              // Ethernet RMII/MII Enable (MII Enabled)
#pragma config FETHIO = ON              // Ethernet I/O Pin Select (Default Ethernet I/O)
#pragma config FCANIO = ON              // CAN I/O Pin Select (Default CAN I/O)
#pragma config FUSBIDIO = ON            // USB USID Selection (Controlled by the USB Module)
#pragma config FVBUSONIO = ON           // USB VBUS ON Selection (Controlled by USB Module)

// DEVCFG2
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider (2x Divider)
#pragma config FPLLMUL = MUL_15         // PLL Multiplier (15x Multiplier)
#pragma config UPLLIDIV = DIV_2         // USB PLL Input Divider (2x Divider)
#pragma config UPLLEN = ON              // USB PLL Enable (Enabled)
#pragma config FPLLODIV = DIV_1         // System PLL Output Clock Divider (PLL Divide by 1)

// DEVCFG1
#pragma config FNOSC = PRIPLL           // Oscillator Selection Bits (Primary Osc (XT,HS,EC), PLL)
#pragma config FSOSCEN = OFF            // Secondary Oscillator Enable (Disabled)
#pragma config IESO = OFF               // Internal/External Switch Over (Disabled)
#pragma config POSCMOD = HS             // Primary Oscillator Configuration (HS osc mode)
#pragma config OSCIOFNC = OFF           // CLKO Output Signal Active on the OSCO Pin (Disabled)
#pragma config FPBDIV = DIV_1           // Peripheral Clock Divisor (Pb_Clk is Sys_Clk/1)
#pragma config FCKSM = CSDCMD           // Clock Switching and Monitor Selection (Clock Switch Disable, FSCM Disabled)
#pragma config WDTPS = PS1048576        // Watchdog Timer Postscaler (1:1048576)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable (WDT Disabled (SWDTEN Bit Controls))

// DEVCFG0
#pragma config DEBUG = OFF           // Background Debugger Enable (Debugger is disabled)
#pragma config ICESEL = ICS_PGx2        // ICE/ICD Comm Channel Select (ICE EMUC2/EMUD2 pins shared with PGC2/PGD2)
#pragma config PWP = OFF                // Program Flash Write Protect (Disable)
#pragma config BWP = OFF                // Boot Flash Write Protect bit (Protection Disabled)
#pragma config CP = OFF                 // Code Protect (Protection Disabled)

#else
	#error "Config flags for your device not defined"

#endif


#ifdef __PIC32MX__
static void system_config_performance(uint32_t cpu_speed)
{
#define MAX_FLASH_SPEED 30000000
#define MAX_PERIPHERAL_BUS_SPEED 80000000

	uint8_t wait_states;

	/* Handle the peripheral bus first */
	if (cpu_speed > MAX_PERIPHERAL_BUS_SPEED) {
		uint32_t irq_state;
		uint32_t dma_suspend_state;

		/* Disable interrupts and store the state */
		asm volatile("di %0" : "=r" (irq_state));

		/* Suspend the DMA if it's not already suspended */
		dma_suspend_state = DMACONbits.SUSPEND;
		if (!dma_suspend_state) {
			DMACONSET = _DMACON_SUSPEND_MASK;
#if defined(__32MX460F512L__) || defined(__32MX470F512L__)
			/* Some PIC32MX don't have DMACONbits.BUSY. As far as
			   I know there is not a good way to test for this. */
			while (!DMACONbits.SUSPEND)
				;
#else
			/* If this breaks, your CPU might not have
			 * DMACONbits.busy. In that case, add your chip to the
			 * section above. It's not clear which PIC32MX lines
			 * have it from the family reference manual. */
			while (DMACONbits.DMABUSY)
				;
#endif
		}

		/* Unlock the OSCCON. The documentation says the writes to
		 * SYSKEY have to be "back-to-back," whatever that means. The
		 * only way to be sure is to do it in ASM. */
		__asm__ volatile(
	        "LI $t0, 0xaa996655\n"
	        "LI $t1, 0x556699aa\n"
	        "LA $t3, (SYSKEY)\n"
	        "SW $0,  ($t3)\n"
		"SW $t0, ($t3)\n"
	        "SW $t1, ($t3)\n"
	        : /* no outputs */
		: /* no inputs */
		: /* clobber */ "t0", "t1", "t3");

		/* Set the peripheral bus clock appropriately */
		if (cpu_speed <= MAX_PERIPHERAL_BUS_SPEED * 2)
			OSCCONbits.PBDIV = 0x1; /* 0x1 = 1:2 */
		else if (cpu_speed <= MAX_PERIPHERAL_BUS_SPEED * 4)
			OSCCONbits.PBDIV = 0x2; /* 0x2 = 1:4 */
		else
			OSCCONbits.PBDIV = 0x3; /* 0x3 = 1:8 */

		/* Re-lock the OSCON */
		SYSKEY = 0x33333333;

		/* Restore DMA suspend state */
		if (!dma_suspend_state)
			DMACONbits.SUSPEND = 0;

		/* Restore interrupts */
		if (irq_state & 0x1)
			asm volatile("ei");
	}

	/* Handle the PCACHE wait states and enable predictive prefetch.
	 * This sets predictive prefetch for all regions, including both
	 * KSEG0 and KSEG1. Caching can be done on the uncached region
	 * (KSEG1) because the PCACHE is invalidated automatically if
	 * there is a write to the flash memory. */
	wait_states = cpu_speed / MAX_FLASH_SPEED;
	CHECONbits.PFMWS = wait_states;
	CHECONbits.PREFEN = 0x3; /* 0x3 = predictive prefetch for all regions */
}
#endif /* __PIC32MX */


void hardware_init(void)
{
#if defined(__PIC24FJ64GB002__) || defined(__PIC24FJ32GB002__) || defined(__PIC24FJ256DA206__)
	unsigned int pll_startup_counter = 600;
	CLKDIVbits.PLLEN = 1;
	while(pll_startup_counter--);
#elif _18F46J50
	unsigned int pll_startup = 600;
	OSCTUNEbits.PLLEN = 1;
	while (pll_startup--)
		;
#elif _16F1459 || _16F1454
	OSCCONbits.IRCF = 0b1111; /* 0b1111 = 16MHz HFINTOSC postscalar */

	/* Enable Active clock-tuning from the USB */
	ACTCONbits.ACTSRC = 1; /* 1=USB */
	ACTCONbits.ACTEN = 1;
#elif __32MX460F512L__
	system_config_performance(80000000);
#elif __32MX795F512L__
	system_config_performance(60000000);
#else
	#error "Add configuration for your processor here"
#endif


/* Configure interrupts, per architecture */
#ifdef USB_USE_INTERRUPTS
	#if defined (_PIC18) || defined(_PIC14E)
		INTCONbits.PEIE = 1;
		INTCONbits.GIE = 1;
	#elif __PIC32MX__
		INTCONbits.MVEC = 1; /* Multi-vector interrupts */
		IPC11bits.USBIP = 4; /* Interrupt priority, must set to != 0. */
		__asm volatile("ei");
	#endif
#endif

#ifdef PIC18F_STARTER_KIT
	/* Turn on the APP_VDD power bus on the PIC18F starter kit. This turns
	 * on the MMC card socket, among other things (OLED, BMA150, etc). The
	 * documentation talks about inrush current, and the MLA provides code
	 * to gradually ramp up the voltage on APP_VDD to mitigate this, but
	 * in my testing, the MLA's ramp-up code doesn't do anything
	 * significantly different to the current consumption graph than just
	 * turning the voltage on, so that's what we do here.
	 *
	 * The whole board seems to draw < 100mA with the oLED and the BMA150
	 * untouched but the MMC used. */
	TRISCbits.TRISC0 = 0; /* Output */
	LATCbits.LATC0 = 0;
#endif

}
