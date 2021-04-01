// 2021 Cooper Dalrymple

#ifndef SCREENTFT_H
#define SCREENTFT_H

#include "Screen.h"

class ScreenTFT : public Screen
{

public:
	ScreenTFT()
		: Screen()
	{
	}

	void Open();

    void DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour);

	void ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2);

	void WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour);
	u32 PrintText(bool petscii, u32 xPos, u32 yPos, char *ptr, RGBA TxtColour = RGBA(0xff, 0xff, 0xff, 0xff), RGBA BkColour = RGBA(0, 0, 0, 0xFF), bool measureOnly = false, u32* width = 0, u32* height = 0);

	void DrawLine(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour);
	void DrawLineV(u32 x, u32 y1, u32 y2, RGBA colour);
	void PlotPixel(u32 x, u32 y, RGBA colour);

	void PlotImage(u32* image, int x, int y, int w, int h);

	void SwapBuffers();

private:
    void MakeDirty(u32 x1, u32 y1, u32 x2, u32 y2);
    u32 dirty_x1;
    u32 dirty_y1;
    u32 dirty_x2;
    u32 dirty_y2;

    void UpdateDisplay();

};

#endif
