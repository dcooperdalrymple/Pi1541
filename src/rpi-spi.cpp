// 2021 Cooper Dalrymple
// Code sourced from https://github.com/mpod/raspi-bare-metal

#include "rpi-spi.h"
#include <stdio.h>
#include "rpi-base.h"
#include "stdlib.h"

#include "rpiHardware.h"
#include "iec_bus.h"

// safe read from peripheral
uint32_t RPI_PeriRead(u32* paddr)
{
	// Make sure we dont return the _last_ read which might get lost
	// if subsequent code changes to a different peripheral
	uint32_t ret = *paddr;
	*paddr; // Read without assigneing to an unused variable
	return ret;
}

// read from peripheral without the read barrier
uint32_t RPI_PeriRead_nb(u32* paddr)
{
	return *paddr;
}

// Safe write to peripheral
void RPI_PeriWrite(u32* paddr, uint32_t value)
{
    // Make sure we don't rely on the first write, which may get
    // lost if the previous access was to a different peripheral.
    *paddr = value;
    *paddr = value;
}

// write to peripheral without the write barrier
void RPI_PeriWrite_nb(u32* paddr, uint32_t value)
{
    *paddr = value;
}

// Set/clear only the bits in value covered by the mask
void RPI_PeriSetBits(u32* paddr, uint32_t value, uint32_t mask)
{
    uint32_t v = RPI_PeriRead(paddr);
    v = (v & ~mask) | (value & mask);
    RPI_PeriWrite(paddr, v);
}

void RPI_SPIBegin(void)
{
    // Set the SPI0 pins to the Alt 0 function to enable SPI0 access on them
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_CS1, FS_ALT0); // CS1
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_CS0, FS_ALT0); // CS0
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_MISO, FS_ALT0); // MISO
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_MOSI, FS_ALT0); // MOSI
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_CLK, FS_ALT0); // CLK

	// Set the SPI CS register to the some sensible defaults
	u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	RPI_PeriWrite(paddr, 0); // All 0s

	// Clear TX and RX fifos
	RPI_PeriWrite_nb(paddr, SPI0_CS_CLEAR);
}

void RPI_SPIEnd(void)
{
    // Set all the SPI0 pins back to input
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_CS1, FS_INPUT); // CS1
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_CS0, FS_INPUT); // CS0
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_MISO, FS_INPUT); // MISO
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_MOSI, FS_INPUT); // MOSI
    RPI_SetGpioPinFunction((rpi_gpio_pin_t)PIGPIO_OUT_SPI0_CLK, FS_INPUT); // CLK
}

void RPI_SPISetBitOrder(u8 order)
{
    // SPI_BIT_ORDER_MSBFIRST is the only one suported by SPI0
}

// defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
void RPI_SPISetClockDivider(uint16_t divider)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CLK/4);
	RPI_PeriWrite(paddr, divider);
}

void RPI_SPISetDataMode(u8 mode)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	// Mask in the CPO and CPHA bits of CS
	RPI_PeriSetBits(paddr, mode << 2, SPI0_CS_CPOL | SPI0_CS_CPHA);
}

u8 RPI_SPITransfer(u8 value)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	u32* fifo = (u32*)(SPI0_BASE + SPI0_FIFO/4);

	// This is Polled transfer as per section 10.6.1
	// BUG ALERT: what happens if we get interupted in this section, and someone else
	// accesses a different peripheral?
	// Clear TX and RX fifos
	RPI_PeriSetBits(paddr, SPI0_CS_CLEAR, SPI0_CS_CLEAR);

	// Set TA = 1
	RPI_PeriSetBits(paddr, SPI0_CS_TA, SPI0_CS_TA);

	// Maybe wait for TXD
	while (!(RPI_PeriRead(paddr) & SPI0_CS_TXD))
		;

	// Write to FIFO, no barrier
	RPI_PeriWrite_nb(fifo, value);

	// Wait for DONE to be set
	while (!(RPI_PeriRead_nb(paddr) & SPI0_CS_DONE))
		;

	// Read any byte that was sent back by the slave while we sere sending to it
	uint32_t ret = RPI_PeriRead_nb(fifo);

	// Set TA = 0, and also set the barrier
	RPI_PeriSetBits(paddr, 0, SPI0_CS_TA);

	return ret;
}

void RPI_SPITransferPacket(char* tbuf, char* rbuf, uint32_t len)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	u32* fifo = (u32*)(SPI0_BASE + SPI0_FIFO/4);
	uint32_t TXCnt=0;
	uint32_t RXCnt=0;

	// This is Polled transfer as per section 10.6.1
	// BUG ALERT: what happens if we get interupted in this section, and someone else
	// accesses a different peripheral?

	// Clear TX and RX fifos
	RPI_PeriSetBits(paddr, SPI0_CS_CLEAR, SPI0_CS_CLEAR);

	// Set TA = 1
	RPI_PeriSetBits(paddr, SPI0_CS_TA, SPI0_CS_TA);

	// Use the FIFO's to reduce the interbyte times
	while((TXCnt < len)||(RXCnt < len))
	{
		// TX fifo not full, so add some more bytes
		while(((RPI_PeriRead(paddr) & SPI0_CS_TXD))&&(TXCnt < len ))
		{
			RPI_PeriWrite_nb(fifo, tbuf[TXCnt]);
			TXCnt++;
		}
		//Rx fifo not empty, so get the next received bytes
		while(((RPI_PeriRead(paddr) & SPI0_CS_RXD))&&( RXCnt < len ))
		{
			rbuf[RXCnt] = RPI_PeriRead_nb(fifo);
			RXCnt++;
		}
	}
	// Wait for DONE to be set
	while (!(RPI_PeriRead_nb(paddr) & SPI0_CS_DONE))
		;

	// Set TA = 0, and also set the barrier
	RPI_PeriSetBits(paddr, 0, SPI0_CS_TA);
}

void RPI_SPIWritePacket(char* tbuf, uint32_t len)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	u32* fifo = (u32*)(SPI0_BASE + SPI0_FIFO/4);

	// This is Polled transfer as per section 10.6.1
	// BUG ALERT: what happens if we get interupted in this section, and someone else
	// accesses a different peripheral?

	// Clear TX and RX fifos
	RPI_PeriSetBits(paddr, SPI0_CS_CLEAR, SPI0_CS_CLEAR);

	// Set TA = 1
	RPI_PeriSetBits(paddr, SPI0_CS_TA, SPI0_CS_TA);

	uint32_t i;
	for (i = 0; i < len; i++)
	{
		// Maybe wait for TXD
		while (!(RPI_PeriRead(paddr) & SPI0_CS_TXD))
			;

		// Write to FIFO, no barrier
		RPI_PeriWrite_nb(fifo, tbuf[i]);

		// Read from FIFO to prevent stalling
		while (RPI_PeriRead(paddr) & SPI0_CS_RXD)
			(void) RPI_PeriRead_nb(fifo);
	}

	// Wait for DONE to be set
	while (!(RPI_PeriRead_nb(paddr) & SPI0_CS_DONE)) {
		while (RPI_PeriRead(paddr) & SPI0_CS_RXD)
			(void) RPI_PeriRead_nb(fifo);
	};

	// Set TA = 0, and also set the barrier
	RPI_PeriSetBits(paddr, 0, SPI0_CS_TA);
}

/*
void RPI_SPITransferPacket(char* buf, uint32_t len)
{
    RPI_SPITransferPacket(buf, buf, len);
}
*/

void RPI_SPIChipSelect(u8 cs)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	// Mask in the CS bits of CS
	RPI_PeriSetBits(paddr, cs, SPI0_CS_CS);
}

void RPI_SPISetChipSelectPolarity(u8 cs, u8 active)
{
    u32* paddr = (u32*)(SPI0_BASE + SPI0_CS/4);
	u8 shift = 21 + cs;
	// Mask in the appropriate CSPOLn bit
	RPI_PeriSetBits(paddr, active << shift, 1 << shift);
}
