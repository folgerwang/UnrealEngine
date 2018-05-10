// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTextureSample.h"
#include "MediaObjectPool.h"

/**
 * Implements the IMediaTextureSample/IMediaPoolable interface.
 */
class FMediaIOCoreTextureSampleBase
	: public IMediaTextureSample
	, public IMediaPoolable
{

protected:
	FMediaIOCoreTextureSampleBase()
		: Duration(FTimespan::Zero())
		, SampleFormat(EMediaTextureSampleFormat::Undefined)
		, Time(FTimespan::Zero())

		, Stride(0)
		, Width(0)
		, Height(0)
		, PixelBuffer(nullptr)
	{
	}

public:
	//~ IMediaTextureSample interface

	virtual const void* GetBuffer() override
	{
		return PixelBuffer;
	}

	virtual FIntPoint GetDim() const override
	{
		return FIntPoint(Width, Height);
	}

	virtual FTimespan GetDuration() const override
	{
		return FTimespan(ETimespan::TicksPerSecond / 60);
	}

	virtual EMediaTextureSampleFormat GetFormat() const override
	{
		return SampleFormat;
	}

	virtual FIntPoint GetOutputDim() const override
	{
		return FIntPoint(Width, Height);
	}

	virtual uint32 GetStride() const override
	{
		return Stride;
	}

#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override
	{
		return nullptr;
	}
#endif //WITH_ENGINE

	virtual FTimespan GetTime() const override
	{
		return Time;
	}

	virtual bool IsCacheable() const override
	{
		return true;
	}

	virtual bool IsOutputSrgb() const override
	{
		return true;
	}

public:
	//~ IMediaPoolable interface

	virtual void ShutdownPoolable() override
	{
		FreeSample();
	}

protected:
	virtual void FreeSample() = 0;

protected:
	/** Duration for which the sample is valid. */
	FTimespan Duration;

	/** Sample format. */
	EMediaTextureSampleFormat SampleFormat;

	/** Sample time. */
	FTimespan Time;

	/** Image dimensions */
	uint32_t Stride;
	uint32_t Width;
	uint32_t Height;

	/** Pointer to raw pixels */
	void* PixelBuffer;
};

