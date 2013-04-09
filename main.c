/**********************************
USB Test Program

Alan Ott
Signal 11 Software
2013-04-08
***********************************/


#include "usb.h"
#include <xc.h>
#include <string.h>
#include "usb_config.h"

_CONFIG1(WDTPS_PS16 & FWPSA_PR32 & WINDIS_OFF & FWDTEN_OFF & ICS_PGx1 & GWRP_OFF & GCP_OFF & JTAGEN_OFF)
_CONFIG2(POSCMOD_NONE & I2C1SEL_PRI & IOL1WAY_OFF & OSCIOFNC_OFF & FCKSM_CSDCMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_NODIV & IESO_OFF)
_CONFIG3(WPFP_WPFP0 & SOSCSEL_IO & WUTSEL_LEG & WPDIS_WPDIS & WPCFG_WPCFGDIS & WPEND_WPENDMEM)
_CONFIG4(DSWDTPS_DSWDTPS3 & DSWDTOSC_SOSC & RTCOSC_SOSC & DSBOREN_OFF & DSWDTEN_OFF)

int main(void)
{
	unsigned int pll_startup_counter = 600;
	CLKDIVbits.PLLEN = 1;
	while(pll_startup_counter--);

	usb_init();
	
	uchar *buf = usb_get_in_buffer(1);
	memset(buf, 0xa0, EP_1_IN_LEN);

	while (1) {
	if (!usb_in_endpoint_busy(1))
		usb_send_in_buffer(1, 64);

		usb_service();
	}

	return 0;
}
