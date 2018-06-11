// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"

class MEDIAIOCORE_API FMediaIOCoreEncodeTime
{
public:
	FMediaIOCoreEncodeTime(EMediaTextureSampleFormat InFormat, FColor* InBuffer, uint32 InWidth, uint32 InHeight);
	void Render(uint32 InX, uint32 InY, uint32 InHours, uint32 InMinutes, uint32 InSeconds, uint32 InFrames) const;

protected:
	void Fill(uint32 InX, uint32 InY, uint32  InWidth, uint32 InHeight, FColor InColor) const;
	void FillChecker(uint32 InX, uint32 InY, uint32  InWidth, uint32 InHeight, FColor InColor0, FColor InColor1) const;
	void DrawTime(uint32 InX, uint32 InY, uint32  inTime, FColor InColor) const;

	inline FColor* At(uint32 InX, uint32 InY) const
	{
		check(InX < Width);
		check(InY < Height);
		return Buffer + Width * InY + InX;
	}

protected:
	/** Pixel format */
	EMediaTextureSampleFormat Format;

	/** Pointer to pixels */
	FColor* Buffer;

	/** Width of image */
	uint32 Width;

	/** Height of image */
	uint32 Height;

protected:
	FColor ColorBlack;
	FColor ColorRed;
	FColor ColorWhite;
};