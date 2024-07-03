/**
 * CT-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2023 Harumakkin.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "hardware/spi.h"

// SPI
#define SPIDEV spi0
#define MMC_SPI_TX_PIN  	19		// GP19 spi0 TX	 pin.25
#define MMC_SPI_RX_PIN  	16		// GP16 spi0 RX	 pin.21
#define MMC_SPI_SCK_PIN 	18		// GP18 spi0 SCK pin.24
#define MMC_SPI_CSN_PIN 	17		// GP17 spi0 CSn pin.22

#ifdef MGS_MUSE_MACHINA
const uint32_t MMM_D0		= 2;
const uint32_t MMM_D1		= 3;
const uint32_t MMM_D2		= 4;
const uint32_t MMM_D3		= 5;
const uint32_t MMM_D4		= 6;
const uint32_t MMM_D5		= 7;
const uint32_t MMM_D6		= 8;
const uint32_t MMM_D7		= 9;
const uint32_t MMM_AEX1		= 11;
const uint32_t MMM_ADDT_SCC	= 12;
const uint32_t MMM_CSWR_PSG	= 13;
const uint32_t MMM_CSWR_FM	= 14;
const uint32_t MMM_CSWR_SCC	= 15;
const uint32_t MMM_S_RESET	= 20;	// RESET
const uint32_t MMM_AEX0		= 21;	// A0
const uint32_t MMM_MODESW	= 22;	// 
#else
const uint32_t MSX_A0_D0		= 0;
const uint32_t MSX_A1_D1		= 1;
const uint32_t MSX_A2_D2		= 2;
const uint32_t MSX_A3_D3		= 3;
const uint32_t MSX_A4_D4		= 4;
const uint32_t MSX_A5_D5		= 5;
const uint32_t MSX_A6_D6		= 6;
const uint32_t MSX_A7_D7		= 7;
const uint32_t MSX_A8_IORQ		= 8;
const uint32_t MSX_A9_MREQ		= 9;
const uint32_t MSX_A10_RW		= 10;
const uint32_t MSX_A11_RD		= 11;
const uint32_t MSX_A12_SLTSL	= 12;
const uint32_t MSX_A13_C1		= 13;
const uint32_t MSX_A14_C12		= 14;
const uint32_t MSX_A15_RESET	= 15;
const uint32_t MSX_DDIR			= 20;
const uint32_t MSX_LATCH_A		= 21;
const uint32_t MSX_LATCH_C		= 22;
#endif

// SW
const uint32_t MGSPICO_SW1		= 28;
const uint32_t MGSPICO_SW2		= 27;
const uint32_t MGSPICO_SW3		= 26;

// Pico's LED.
const uint32_t GP_PICO_LED		= 25;

