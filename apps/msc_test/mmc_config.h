/*
 * Sample MMC Configuration
 *
 * This file may be used by anyone for any purpose and may be used as a
 * starting point making your own application using M-Stack.
 *
 * It is worth noting that M-Stack itself is not under the same license
 * as this file.
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
 * 2014-07-17
 */

#ifndef MMC_CONFIG_H__
#define MMC_CONFIG_H__

/* Callbacks from mmc.h */
#define MMC_SPI_TRANSFER  app_spi_transfer
#define MMC_SPI_SET_SPEED app_spi_set_speed
#define MMC_SPI_SET_CS    app_spi_set_cs

#define MMC_USE_TIMER
#define MMC_TIMER_START app_timer_start
#define MMC_TIMER_EXPIRED app_timer_expired
#define MMC_TIMER_STOP  app_timer_stop

#endif /* MMC_CONFIG_H__ */
