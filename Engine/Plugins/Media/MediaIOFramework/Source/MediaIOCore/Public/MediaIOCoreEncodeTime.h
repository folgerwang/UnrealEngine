// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"

enum class MEDIAIOCORE_API EMediaIOCoreEncodePixelFormat
{
	A2B10G10R10,
	CharBGRA,
	CharUYVY,
};

class MEDIAIOCORE_API FMediaIOCoreEncodeTime
{
public:
	FMediaIOCoreEncodeTime(EMediaIOCoreEncodePixelFormat InFormat, void* InBuffer, uint32 InPitch, uint32 InHeight);
	void Render(uint32 InX, uint32 InY, uint32 InHours, uint32 InMinutes, uint32 InSeconds, uint32 InFrames) const;

protected:
	using TColor = int32;
	void Fill(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight, TColor InColor) const;
	void FillChecker(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight, TColor InColor0, TColor InColor1) const;
	void DrawTime(uint32 InX, uint32 InY, uint32 inTime, TColor InColor) const;

	inline TColor& At(uint32 InX, uint32 InY) const
	{
		return *(reinterpret_cast<TColor*>(reinterpret_cast<char*>(Buffer) + (Pitch * InY)) + InX);
	}

protected:
	/** Pixel format */
	EMediaIOCoreEncodePixelFormat Format;

	/** Pointer to pixels */
	void* Buffer;

	/** Pitch of image */
	uint32 Pitch;

	/** Height of image */
	uint32 Height;

protected:
	TColor ColorBlack;
	TColor ColorRed;
	TColor ColorWhite;
};