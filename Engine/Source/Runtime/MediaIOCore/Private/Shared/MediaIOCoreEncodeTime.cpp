// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreEncodeTime.h"

FMediaIOCoreEncodeTime::FMediaIOCoreEncodeTime(EMediaTextureSampleFormat InFormat, FColor* InBuffer, uint32 InWidth, uint32 InHeight)
	: Format(InFormat)
	, Buffer(InBuffer)
	, Width(InWidth)
	, Height(InHeight)
{
	check(Buffer);

	if (Format == EMediaTextureSampleFormat::CharUYVY)
	{
		// YUVU Encoding of colors
		ColorBlack = FColor(0x00800080);
		ColorRed = FColor(0x38e4385e);
		ColorWhite = FColor(0xff80ff80);
	}
	else
	{
		ColorBlack = FColor::Black;
		ColorRed = FColor::Red;
		ColorWhite = FColor::White;
	}
}

void FMediaIOCoreEncodeTime::Fill(uint32 InX, uint32 InY, uint32  InWidth, uint32 InHeight, FColor InColor) const
{
	check(InX < Width);
	check(InX + InWidth <= Width);
	check(InY < Height);
	check(InY + InHeight <= Height);

	for (uint32 Line = InY; Line < InY + InHeight; ++Line)
	{
		for (uint32 Column = InX; Column < InX + InWidth; ++Column)
		{
			*(Buffer + (Line * Width) + Column) = InColor;
		}
	}
}

void FMediaIOCoreEncodeTime::FillChecker(uint32 InX, uint32 InY, uint32  InWidth, uint32 InHeight, FColor InColor0, FColor InColor1) const
{
	check(InX < Width);
	check(InX + InWidth <= Width);
	check(InY < Height);
	check(InY + InHeight <= Height);

	for (uint32 Line = InY; Line < InY + InHeight; ++Line)
	{
		for (uint32 Column = InX; Column < InX + InWidth; ++Column)
		{
			*(Buffer + (Line * Width) + Column) = (Column % 2) ? InColor0 : InColor1;
		}
	}
}

void FMediaIOCoreEncodeTime::DrawTime(uint32 InX, uint32 InY, uint32  InTime, FColor InColor) const
{
	uint32 Tenth = (InTime / 10);
	uint32 Unit = (InTime % 10);
	if (Tenth > 0)
	{
		*At(InX + Tenth - 1, InY) = InColor;
	}
	*At(InX + Unit, InY + 1) = InColor;
}

void FMediaIOCoreEncodeTime::Render(uint32 InX, uint32 InY, uint32 InHours, uint32 InMinutes, uint32 InSeconds, uint32 InFrames) const
{
	// clear space
	Fill(InX, InY, 12, 12, ColorBlack);

	// render patern
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
