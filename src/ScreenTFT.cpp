// 2021 Cooper Dalrymple

#include "ScreenTFT.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "rpi-spi.h"
#include "debug.h"

#include "rpiHardware.h"
#include "iec_bus.h"

extern unsigned char* CBMFont;

static const int BitFontHt = 16;
static const int BitFontWth = 8;

void ScreenTFT::Open()
{
    ILI9340_Init();
	ILI9340_SetRotation(3);

    dirty_x1 = ILI9340_GetWidth();
	dirty_x2 = 0;
	dirty_y1 = ILI9340_GetHeight();
	dirty_y2 = 0;

    Screen::Open(ILI9340_GetWidth(), ILI9340_GetHeight(), 16);
}

void ScreenTFT::DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour)
{
    MakeDirty(x1, y1, x2, y2);
    Screen::DrawRectangle(x1, y1, x2, y2, colour);
}

void ScreenTFT::ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2)
{
    MakeDirty(x1, y1, x2, y2);
    Screen::ScrollArea(x1, y1, x2, y2);
}

void ScreenTFT::WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour)
{
    u32 fontHeight = BitFontHt;
    if (petscii && CBMFont) fontHeight = 8;

    MakeDirty(x, y, x+BitFontWth, y+fontHeight);
    Screen::WriteChar(petscii, x, y, c, colour);
}

u32 ScreenTFT::PrintText(bool petscii, u32 x, u32 y, char *ptr, RGBA TxtColour, RGBA BkColour, bool measureOnly, u32* width, u32* height)
{
    if (!measureOnly)
    {
        char *ptr2 = ptr;

        u32 xCursor = x;
        u32 yCursor = y;
        u32 fontHeight = BitFontHt;
        if (petscii && CBMFont) fontHeight = 8;

        u32 x1 = x;
        u32 y1 = y;
        u32 x2 = x+BitFontWth;
        u32 y2 = y+fontHeight;

        while (*ptr2 != 0)
        {
            char c = *ptr2++;
            if ((c != '\r') && (c != '\n'))
            {
                xCursor += BitFontWth;
            }
            else
            {
                xCursor = x;
                yCursor += fontHeight;
            }

            x1 = MIN(x1, xCursor);
            y1 = MIN(y1, yCursor);
            x2 = MAX(x2, xCursor + BitFontWth);
            y2 = MAX(y2, yCursor + fontHeight);
        }

        MakeDirty(x1, y1, x2, y2);
    }

    return Screen::PrintText(petscii, x, y, ptr, TxtColour, BkColour, measureOnly, width, height);
}

void ScreenTFT::DrawLine(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour)
{
    MakeDirty(x1, y1, x2, y2);
    Screen::DrawLine(x1, y1, x2, y2, colour);
}

void ScreenTFT::DrawLineV(u32 x, u32 y1, u32 y2, RGBA colour)
{
    MakeDirty(x, y1, x, y2);
    Screen::DrawLineV(x, y1, y2, colour);
}

void ScreenTFT::PlotPixel(u32 x, u32 y, RGBA colour)
{
    if (x >= 0 && y >= 0 && x < width && y < height)
		MakeDirty(x, y, x, y);
    Screen::PlotPixel(x, y, colour);
}

void ScreenTFT::PlotImage(u32* image, int x, int y, int w, int h)
{
    MakeDirty(x, y, x+w, y+h);
    Screen::PlotImage(image, x, y, w, h);
}

void ScreenTFT::SwapBuffers()
{
    UpdateDisplay();
}

void ScreenTFT::MakeDirty(u32 x1, u32 y1, u32 x2, u32 y2)
{
    ClipRect(x1, y1, x2, y2);
    if (x1 < dirty_x1) dirty_x1 = x1;
	if (y1 < dirty_y1) dirty_y1 = y1;
	if (x2 > dirty_x2) dirty_x2 = x2;
	if (y2 > dirty_y2) dirty_y2 = y2;
}

void ScreenTFT::UpdateDisplay()
{
    if (dirty_x1 >= width || dirty_x2 >= width ||
			dirty_y1 >= height || dirty_y2 >= height) {
		dirty_x1 = 0;
		dirty_x2 = width - 1;
		dirty_y1 = 0;
		dirty_y2 = height - 1;
	}
	if (dirty_x1 > dirty_x2) {
		dirty_x1 = 0;
		dirty_x2 = width - 1;
	}
	if (dirty_y1 > dirty_y2) {
		dirty_y1 = 0;
		dirty_y2 = height - 1;
	}

    ILI9340_SetAddrWindow(dirty_x1, dirty_y1, dirty_x2, dirty_y2);

    u32 offset = 2 * (dirty_y1 * width + dirty_x1);
    u32 len = 2 * (dirty_x2 - dirty_x1 + 1);

    while (dirty_y1 <= dirty_y2) {
        RPI_SPIWritePacket((char*)((u32)framebuffer + offset), len);
        offset += width << 1;
        dirty_y1++;
    }

    dirty_x1 = width;
    dirty_x2 = 0;
    dirty_y1 = height;
    dirty_y2 = 0;
}
