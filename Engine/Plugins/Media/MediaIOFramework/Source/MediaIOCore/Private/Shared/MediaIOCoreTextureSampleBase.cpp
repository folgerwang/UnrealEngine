// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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


bool FMediaIOCoreTextureSampleBase::Initialize(const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	FreeSample();

	if ((InVideoBuffer == nullptr) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
	{
		return false;
	}

	Buffer.Reset(InBufferSize);
	Buffer.Append(reinterpret_cast<const uint8*>(InVideoBuffer), InBufferSize);
	Stride = InStride;
	Width = InWidth;
	Height = InHeight;
	SampleFormat = InSampleFormat;
	Time = InTime;
	Timecode = InTimecode;

	return true;
}


bool FMediaIOCoreTextureSampleBase::Initialize(TArray<uint8> InVideoBuffer, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	FreeSample();

	if ((InVideoBuffer.Num() == 0) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
	{
		return false;
	}

	Buffer = MoveTemp(InVideoBuffer);
	Stride = InStride;
	Width = InWidth;
	Height = InHeight;
	SampleFormat = InSampleFormat;
	Time = InTime;
	Timecode = InTimecode;

	return true;
}


bool FMediaIOCoreTextureSampleBase::InitializeWithEvenOddLine(bool bUseEvenLine, const void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, uint32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	FreeSample();

	if (InVideoBuffer == nullptr || InSampleFormat == EMediaTextureSampleFormat::Undefined)
	{
		return false;
	}

	Buffer.Reset(InBufferSize / 2);
	Stride = InStride;
	Width = InWidth;
	Height = InHeight / 2;
	SampleFormat = InSampleFormat;
	Timecode = InTimecode;
	Time = InTime;

	for (uint32 IndexY = (bUseEvenLine ? 0 : 1); IndexY < InHeight; IndexY += 2)
	{
		const uint8* Source = reinterpret_cast<const uint8*>(InVideoBuffer) + (IndexY*Stride);
		Buffer.Append(Source, Stride);
	}

	return true;
}
