// 2021 Cooper Dalrymple
// Code sourced from https://github.com/mpod/raspi-bare-metal

#include <stdio.h>
#include "rpi-base.h"
#include "rpi-gpio.h"
#include "stdlib.h"

#include "rpiHardware.h"

#define SPI0_BASE        (PERIPHERAL_BASE + 0x204000)

// GPIO register offsets from SPI0_BASE.
// Offsets into the SPI Peripheral block in bytes per 10.5 SPI Register Map
#define SPI0_CS         0x0000 // SPI Master Control and Status
#define SPI0_FIFO       0x0004 // SPI Master TX and RX FIFOs
#define SPI0_CLK        0x0008 // SPI Master Clock Divider
#define SPI0_DLEN       0x000c // SPI Master Data Length
#define SPI0_LTOH       0x0010 // SPI LOSSI mode TOH
#define SPI0_DC         0x0014 // SPI DMA DREQ Controls

// Register masks for SPI0_CS
#define SPI0_CS_LEN_LONG             0x02000000 // Enable Long data word in Lossi mode if DMA_LEN is set
#define SPI0_CS_DMA_LEN              0x01000000 // Enable DMA mode in Lossi mode
#define SPI0_CS_CSPOL2               0x00800000 // Chip Select 2 Polarity
#define SPI0_CS_CSPOL1               0x00400000 // Chip Select 1 Polarity
#define SPI0_CS_CSPOL0               0x00200000 // Chip Select 0 Polarity
#define SPI0_CS_RXF                  0x00100000 // RXF - RX FIFO Full
#define SPI0_CS_RXR                  0x00080000 // RXR RX FIFO needs Reading ( full)
#define SPI0_CS_TXD                  0x00040000 // TXD TX FIFO can accept Data
#define SPI0_CS_RXD                  0x00020000 // RXD RX FIFO contains Data
#define SPI0_CS_DONE                 0x00010000 // Done transfer Done
#define SPI0_CS_TE_EN                0x00008000 // Unused
#define SPI0_CS_LMONO                0x00004000 // Unused
#define SPI0_CS_LEN                  0x00002000 // LEN LoSSI enable
#define SPI0_CS_REN                  0x00001000 // REN Read Enable
#define SPI0_CS_ADCS                 0x00000800 // ADCS Automatically Deassert Chip Select
#define SPI0_CS_INTR                 0x00000400 // INTR Interrupt on RXR
#define SPI0_CS_INTD                 0x00000200 // INTD Interrupt on Done
#define SPI0_CS_DMAEN                0x00000100 // DMAEN DMA Enable
#define SPI0_CS_TA                   0x00000080 // Transfer Active
#define SPI0_CS_CSPOL                0x00000040 // Chip Select Polarity
#define SPI0_CS_CLEAR                0x00000030 // Clear FIFO Clear RX and TX
#define SPI0_CS_CLEAR_RX             0x00000020 // Clear FIFO Clear RX
#define SPI0_CS_CLEAR_TX             0x00000010 // Clear FIFO Clear TX
#define SPI0_CS_CPOL                 0x00000008 // Clock Polarity
#define SPI0_CS_CPHA                 0x00000004 // Clock Phase
#define SPI0_CS_CS                   0x00000003 // Chip Select

// Specifies the SPI data bit ordering for RPI_SPISetBitOrder()
typedef enum
{
	SPI_BIT_ORDER_LSBFIRST = 0,  // LSB First
	SPI_BIT_ORDER_MSBFIRST = 1   // MSB First
} SPIBitOrder;

// Specify the SPI data mode to be passed to RPI_SPISetDataMode()
typedef enum
{
	SPI_MODE0 = 0,  // CPOL = 0, CPHA = 0
	SPI_MODE1 = 1,  // CPOL = 0, CPHA = 1
	SPI_MODE2 = 2,  // CPOL = 1, CPHA = 0
	SPI_MODE3 = 3,  // CPOL = 1, CPHA = 1
} SPIMode;

// Specify the SPI chip select pin(s)
typedef enum
{
	SPI_CS0 = 0,     // Chip Select 0
	SPI_CS1 = 1,     // Chip Select 1
	SPI_CS2 = 2,     // Chip Select 2 (ie pins CS1 and CS2 are asserted)
	SPI_CS_NONE = 3, // No CS, control it yourself
} SPIChipSelect;

// Specifies the divider used to generate the SPI clock from the system clock.
// Figures below give the divider, clock period and clock frequency.
// Clock divided is based on nominal base clock rate of 250MHz
// It is reported that (contrary to the documentation) any even divider may used.
// The frequencies shown for each divider have been confirmed by measurement
typedef enum
{
	SPI_CLOCK_DIVIDER_65536 = 0,       // 65536 = 262.144us = 3.814697260kHz
	SPI_CLOCK_DIVIDER_32768 = 32768,   // 32768 = 131.072us = 7.629394531kHz
	SPI_CLOCK_DIVIDER_16384 = 16384,   // 16384 = 65.536us = 15.25878906kHz
	SPI_CLOCK_DIVIDER_8192  = 8192,    // 8192 = 32.768us = 30/51757813kHz
	SPI_CLOCK_DIVIDER_4096  = 4096,    // 4096 = 16.384us = 61.03515625kHz
	SPI_CLOCK_DIVIDER_2048  = 2048,    // 2048 = 8.192us = 122.0703125kHz
	SPI_CLOCK_DIVIDER_1024  = 1024,    // 1024 = 4.096us = 244.140625kHz
	SPI_CLOCK_DIVIDER_512   = 512,     // 512 = 2.048us = 488.28125kHz
	SPI_CLOCK_DIVIDER_256   = 256,     // 256 = 1.024us = 976.5625MHz
	SPI_CLOCK_DIVIDER_128   = 128,     // 128 = 512ns = = 1.953125MHz
	SPI_CLOCK_DIVIDER_64    = 64,      // 64 = 256ns = 3.90625MHz
	SPI_CLOCK_DIVIDER_32    = 32,      // 32 = 128ns = 7.8125MHz
	SPI_CLOCK_DIVIDER_16    = 16,      // 16 = 64ns = 15.625MHz
	SPI_CLOCK_DIVIDER_8     = 8,       // 8 = 32ns = 31.25MHz
	SPI_CLOCK_DIVIDER_4     = 4,       // 4 = 16ns = 62.5MHz
	SPI_CLOCK_DIVIDER_2     = 2,       // 2 = 8ns = 125MHz, fastest you can get
	SPI_CLOCK_DIVIDER_1     = 1,       // 1 = 262.144us = 3.814697260kHz, same as 0/65536
} SPIClockDivider;

// safe read from peripheral
uint32_t RPI_PeriRead(volatile uint32_t* paddr)
{
	// Make sure we dont return the _last_ read which might get lost
	// if subsequent code changes to a different peripheral
	uint32_t ret = *paddr;
	*paddr; // Read without assigneing to an unused variable
	return ret;
}

// read from peripheral without the read barrier
uint32_t RPI_PeriRead_nb(volatile uint32_t* paddr)
{
	return *paddr;
}

// Safe write to peripheral
void RPI_PeriWrite(volatile uint32_t* paddr, uint32_t value)
{
    // Make sure we don't rely on the first write, which may get
    // lost if the previous access was to a different peripheral.
    *paddr = value;
    *paddr = value;
}

// write to peripheral without the write barrier
void RPI_PeriWrite_nb(volatile uint32_t* paddr, uint32_t value)
{
    *paddr = value;
}

// Set/clear only the bits in value covered by the mask
void RPI_PeriSetBits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
    uint32_t v = RPI_PeriRead(paddr);
    v = (v & ~mask) | (value & mask);
    RPI_PeriWrite(paddr, v);
}

void RPI_SPIBegin(void)
{
    // Set the SPI0 pins to the Alt 0 function to enable SPI0 access on them
    RPI_SetGpioPinFunction(RPI_GPIO26, FS_ALT0); // CE1
    RPI_SetGpioPinFunction(RPI_GPIO24, FS_ALT0); // CE0
    RPI_SetGpioPinFunction(RPI_GPIO21, FS_ALT0); // MISO
    RPI_SetGpioPinFunction(RPI_GPIO19, FS_ALT0); // MOSI
    RPI_SetGpioPinFunction(RPI_GPIO23, FS_ALT0); // CLK

	// Set the SPI CS register to the some sensible defaults
	volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	RPI_PeriWrite(paddr, 0); // All 0s

	// Clear TX and RX fifos
	RPI_PeriWrite_nb(paddr, SPI0_CS_CLEAR);
}

void RPI_SPIEnd(void)
{
    // Set all the SPI0 pins back to input
    RPI_SetGpioPinFunction(RPI_GPIO26, FS_INPUT); // CE1
    RPI_SetGpioPinFunction(RPI_GPIO24, FS_INPUT); // CE0
    RPI_SetGpioPinFunction(RPI_GPIO21, FS_INPUT); // MISO
    RPI_SetGpioPinFunction(RPI_GPIO19, FS_INPUT); // MOSI
    RPI_SetGpioPinFunction(RPI_GPIO23, FS_INPUT); // CLK
}

void RPI_SPISetBitOrder(uint8_t order)
{
    // SPI_BIT_ORDER_MSBFIRST is the only one suported by SPI0
}

// defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
void RPI_SPISetClockDivider(uint16_t divider)
{
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CLK/4;
	RPI_PeriWrite(paddr, divider);
}

void RPI_SPISetDataMode(uint8_t mode)
{
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	// Mask in the CPO and CPHA bits of CS
	RPI_PeriSetBits(paddr, mode << 2, SPI0_CS_CPOL | SPI0_CS_CPHA);
}

uint8_t RPI_SPITransfer(uint8_t value)
{
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	volatile uint32_t* fifo = SPI0_BASE + SPI0_FIFO/4;

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
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	volatile uint32_t* fifo = SPI0_BASE + SPI0_FIFO/4;
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
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	volatile uint32_t* fifo = SPI0_BASE + SPI0_FIFO/4;

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

void RPI_SPITransferPacket(char* buf, uint32_t len)
{
    RPI_SPITransferPacket(buf, buf, len);
}

void RPI_SPIChipSelect(uint8_t cs)
{
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	// Mask in the CS bits of CS
	RPI_PeriSetBits(paddr, cs, SPI0_CS_CS);
}

void RPI_SPISetChipSelectPolarity(uint8_t cs, uint8_t active)
{
    volatile uint32_t* paddr = SPI0_BASE + SPI0_CS/4;
	uint8_t shift = 21 + cs;
	// Mask in the appropriate CSPOLn bit
	RPI_PeriSetBits(paddr, active << shift, 1 << shift);
}
