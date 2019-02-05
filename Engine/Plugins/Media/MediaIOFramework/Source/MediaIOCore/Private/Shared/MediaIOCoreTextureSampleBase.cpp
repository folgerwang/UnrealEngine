// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreTextureSampleBase.h"

FMediaIOCoreTextureSampleBase::FMediaIOCoreTextureSampleBase()
	: Duration(FTimespan::Zero())
	, SampleFormat(EMediaTextureSampleFormat::Undefined)
	, Time(FTimespan::Zero())
	, Stride(0)
	, Width(0)
	, Height(0)
{
}


bool FMediaIOCoreTextureSampleBase::Initialize(const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode)
{
	FreeSample();

	if (!SetProperties(InStride, InWidth, InHeight, InSampleFormat, InTime, InFrameRate, InTimecode))
	{
		return false;
	}

	return SetBuffer(InVideoBuffer, InBufferSize);
}


bool FMediaIOCoreTextureSampleBase::Initialize(TArray<uint8> InVideoBuffer, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode)
{
	FreeSample();

	if (!SetProperties(InStride, InWidth, InHeight, InSampleFormat, InTime, InFrameRate, InTimecode))
	{
		return false;
	}

	return SetBuffer(MoveTemp(InVideoBuffer));
}


bool FMediaIOCoreTextureSampleBase::SetBuffer(const void* InVideoBuffer, uint32 InBufferSize)
{
	if (InVideoBuffer == nullptr)
	{
		return false;
	}

	Buffer.Reset(InBufferSize);
	Buffer.Append(reinterpret_cast<const uint8*>(InVideoBuffer), InBufferSize);

	return true;
}


bool FMediaIOCoreTextureSampleBase::SetBuffer(TArray<uint8> InVideoBuffer)
{
	if (InVideoBuffer.Num() == 0)
	{
		return false;
	}

	Buffer = MoveTemp(InVideoBuffer);

	return true;
}


bool FMediaIOCoreTextureSampleBase::SetProperties(uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode)
{
	if (InSampleFormat == EMediaTextureSampleFormat::Undefined)
	{
		return false;
	}

	Stride = InStride;
	Width = InWidth;
	Height = InHeight;
	SampleFormat = InSampleFormat;
	Time = InTime;
	Duration = FTimespan(ETimespan::TicksPerSecond * InFrameRate.AsInterval());
	Timecode = InTimecode;

	return true;
}


bool FMediaIOCoreTextureSampleBase::InitializeWithEvenOddLine(bool bUseEvenLine, const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const FFrameRate& InFrameRate, const TOptional<FTimecode>& InTimecode)
{
	FreeSample();

	if (!SetProperties(InStride, InWidth, InHeight/2, InSampleFormat, InTime, InFrameRate, InTimecode))
	{
		return false;
	}

	return SetBufferWithEvenOddLine(bUseEvenLine, InVideoBuffer, InBufferSize, InStride, InHeight);
}


bool FMediaIOCoreTextureSampleBase::SetBufferWithEvenOddLine(bool bUseEvenLine, const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InHeight)
{
	Buffer.Reset(InBufferSize / 2);

	for (uint32 IndexY = (bUseEvenLine ? 0 : 1); IndexY < InHeight; IndexY += 2)
	{
		const uint8* Source = reinterpret_cast<const uint8*>(InVideoBuffer) + (IndexY*InStride);
		Buffer.Append(Source, InStride);
	}

	return true;
}


void* FMediaIOCoreTextureSampleBase::RequestBuffer(uint32 InBufferSize)
{
	FreeSample();
	Buffer.SetNumUninitialized(InBufferSize); // Reset the array without shrinking (Does not destruct items, does not de-allocate memory).
	return Buffer.GetData();
}
