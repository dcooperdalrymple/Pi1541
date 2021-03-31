// 2021 Cooper Dalrymple
// Code sourced from https://github.com/mpod/raspi-bare-metal

#ifndef RPI_SPI_H
#define RPI_SPI_H

#include "rpi-base.h"

extern void RPI_SPIBegin(void);
extern void RPI_SPIEnd(void);
extern void RPI_SPISetBitOrder(uint8_t order);
extern void RPI_SPISetClockDivider(uint16_t divider);
extern void RPI_SPISetDataMode(uint8_t mode);
extern uint8_t RPI_SPITransfer(uint8_t value);
extern void RPI_SPITransferPacket(char* tbuf, char* rbuf, uint32_t len);
extern void RPI_SPIWritePacket(char* tbuf, uint32_t len);
extern void RPI_SPITransferPacket(char* buf, uint32_t len);
extern void RPI_SPIChipSelect(uint8_t cs);
extern void RPI_SPISetChipSelectPolarity(uint8_t cs, uint8_t active);

#endif
