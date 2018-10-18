// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreEncodeTime.h"

FMediaIOCoreEncodeTime::FMediaIOCoreEncodeTime(EMediaIOCoreEncodePixelFormat InFormat, void* InBuffer, uint32 InPitch, uint32 InHeight)
	: Format(InFormat)
	, Buffer(InBuffer)
	, Pitch(InPitch)
	, Height(InHeight)
{
	check(Buffer);

	if (Format == EMediaIOCoreEncodePixelFormat::A2B10G10R10)
	{
		// 10bit Encoding of colors
		ColorBlack = 0x3;
		ColorRed = 0xFFF;
		ColorWhite = 0xFFFFFFFF;
	}
	else if (Format == EMediaIOCoreEncodePixelFormat::CharUYVY)
	{
		// YUVU Encoding of colors
		ColorBlack = 0x00800080;
		ColorRed = 0x38e4385e;
		ColorWhite = 0xff80ff80;
	}
	else if (Format == EMediaIOCoreEncodePixelFormat::CharBGRA)
	{
		ColorBlack = FColor::Black.ToPackedARGB();
		ColorRed = FColor::Red.ToPackedARGB();
		ColorWhite = FColor::White.ToPackedARGB();
	}
	else
	{
		check(false);
	}
}

void FMediaIOCoreEncodeTime::Fill(uint32 InX, uint32 InY, uint32  InWidth, uint32 InHeight, TColor InColor) const
{
	check(InX < Pitch);
	check(InX + InWidth <= Pitch);
	check(InY < Height);
	check(InY + InHeight <= Height);

	for (uint32 Line = InY; Line < InY + InHeight; ++Line)
	{
		for (uint32 Column = InX; Column < InX + InWidth; ++Column)
		{
			At(Column, Line) = InColor;
		}
	}
}

void FMediaIOCoreEncodeTime::FillChecker(uint32 InX, uint32 InY, uint32  InWidth, uint32 InHeight, TColor InColor0, TColor InColor1) const
{
	check(InX < Pitch);
	check(InX + InWidth <= Pitch);
	check(InY < Height);
	check(InY + InHeight <= Height);

	for (uint32 Line = InY; Line < InY + InHeight; ++Line)
	{
		for (uint32 Column = InX; Column < InX + InWidth; ++Column)
		{
			At(Column, Line) = (Column % 2) ? InColor0 : InColor1;
		}
	}
}

void FMediaIOCoreEncodeTime::DrawTime(uint32 InX, uint32 InY, uint32  InTime, TColor InColor) const
{
	uint32 Tenth = (InTime / 10);
	uint32 Unit = (InTime % 10);
	At(InX + Tenth, InY) = InColor;
	At(InX + Unit, InY + 1) = InColor;
}

void FMediaIOCoreEncodeTime::Render(uint32 InX, uint32 InY, uint32 InHours, uint32 InMinutes, uint32 InSeconds, uint32 InFrames) const
{
	if (InX + 12 >= Pitch/sizeof(TColor) || InY + 12 >= Height)
	{
		return;
	}

	// clear space
	Fill(InX, InY, 12, 12, ColorBlack);

	// render pattern
	FillChecker(InX, InY +  0,  2, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY +  1, 10, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY +  3,  6, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY +  4, 10, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY +  6,  6, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY +  7, 10, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY +  9,  6, 1, ColorRed, ColorBlack);
	FillChecker(InX, InY + 10, 10, 1, ColorRed, ColorBlack);

	// render time
	DrawTime(InX, InY + 0, InHours, ColorWhite);
	DrawTime(InX, InY + 3, InMinutes, ColorWhite);
	DrawTime(InX, InY + 6, InSeconds, ColorWhite);
	DrawTime(InX, InY + 9, InFrames, ColorWhite);
}
