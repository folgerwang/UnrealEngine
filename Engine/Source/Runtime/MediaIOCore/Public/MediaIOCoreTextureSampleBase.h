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
	{
	}

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoBuffer The audio frame data.
	 * @param InBufferSize The size of the video buffer.
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InTimecode The sample timecode if available.
	 */
	bool Initialize(void* InVideoBuffer, uint32 InBufferSize, uint32 InStride, uint32 InWidth, int32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
	{
		FreeSample();

		if ((InVideoBuffer == nullptr) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
		{
			return false;
		}

		Buffer.Reset(InBufferSize);
		Buffer.Append(reinterpret_cast<uint8*>(InVideoBuffer), InBufferSize);
		PixelBuffer = Buffer.GetData(); //@TODO: Temp for Blackmagic.
		Stride = InStride;
		Width = InWidth;
		Height = InHeight;
		SampleFormat = InSampleFormat;
		Time = InTime;
		Timecode = InTimecode;

		return true;
	}

	/**
	 * Initialize the sample.
	 *
	 * @param InVideoBuffer The audio frame data.
	 * @param InStride The number of channel of the video buffer.
	 * @param InWidth The sample rate of the video buffer.
	 * @param InHeight The sample rate of the video buffer.
	 * @param InSampleFormat The sample format of the video buffer.
	 * @param InTime The sample time (in the player's own clock).
	 * @param InTimecode The sample timecode if available.
	 */
	bool Initialize(TArray<uint8> InVideoBuffer, uint32 InStride, uint32 InWidth, int32 InHeight, EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
	{
		FreeSample();

		if ((InVideoBuffer.Num() == 0) || (InSampleFormat == EMediaTextureSampleFormat::Undefined))
		{
			return false;
		}

		Buffer = MoveTemp(InVideoBuffer);
		PixelBuffer = Buffer.GetData(); //@TODO: Temp for Blackmagic.
		Stride = InStride;
		Width = InWidth;
		Height = InHeight;
		SampleFormat = InSampleFormat;
		Time = InTime;
		Timecode = InTimecode;

		return true;
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

	virtual TOptional<FTimecode> GetTimecode() const override
	{
		return Timecode;
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
	virtual void FreeSample()
	{
		Buffer.Reset();
	}

protected:
	/** Duration for which the sample is valid. */
	FTimespan Duration;

	/** Sample format. */
	EMediaTextureSampleFormat SampleFormat;

	/** Sample time. */
	FTimespan Time;

	/** Sample timecode. */
	TOptional<FTimecode> Timecode;

	/** Image dimensions */
	uint32_t Stride;
	uint32_t Width;
	uint32_t Height;

	/** Pointer to raw pixels */
	TArray<uint8> Buffer;
	void* PixelBuffer; //@TODO: Temp for Blackmagic.
};

