// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/FrameRate.h"
#include "Math/IntPoint.h"

class FString;


class OPENEXRWRAPPER_API FOpenExr
{
public:

	static void SetGlobalThreadCount(uint16 ThreadCount);
};


class OPENEXRWRAPPER_API FRgbaInputFile
{
public:

	FRgbaInputFile(const FString& FilePath);
	FRgbaInputFile(const FString& FilePath, uint16 ThreadCount);
	~FRgbaInputFile();

public:

	const TCHAR* GetCompressionName() const;
	FIntPoint GetDataWindow() const;
	FFrameRate GetFrameRate(const FFrameRate& DefaultValue) const;
	int32 GetUncompressedSize() const;
	bool IsComplete() const;
	void ReadPixels(int32 StartY, int32 EndY);
	void SetFrameBuffer(void* Buffer, const FIntPoint& Stride);

private:

	void* InputFile;
};
