// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
#include "RHIResources.h"

struct FVideoEncoderSettings
{
	FVideoEncoderSettings()
		: AverageBitRate(20000000) // TODO(andriy): the initial value should be dictated by the receiver
		, FrameRate(60)
		, Width(1920)
		, Height(1080)
	{}

	FORCEINLINE bool operator==(const FVideoEncoderSettings& Other) const
	{
		return (AverageBitRate == Other.AverageBitRate) 
			&& (FrameRate == Other.FrameRate) 
			&& (Width == Other.Width) 
			&& (Height == Other.Height);
	}

	FORCEINLINE bool operator!=(const FVideoEncoderSettings& Other) const
	{
		return (AverageBitRate != Other.AverageBitRate) 
			|| (FrameRate != Other.FrameRate) 
			|| (Width != Other.Width) 
			|| (Height != Other.Height);
	}

	uint32	AverageBitRate;
	uint32	FrameRate;
	uint32	Width;
	uint32	Height;
};

class IVideoEncoder
{
public:
	using FEncodedFrameReadyCallback = TFunction<void(uint64, bool, const uint8*, uint32)>;

	virtual ~IVideoEncoder() = default;

	/**
	* Return name of the encoder.
	*/
	virtual FString GetName() const = 0;

	/**
	* If encoder is supported.
	*/
	virtual bool IsSupported() const = 0;

	/**
	* Get Sps/Pps header data.
	*/
	virtual const TArray<uint8>& GetSpsPpsHeader() const = 0;

	/**
	* Actions to take before resizing back buffer.
	*/
	virtual void PreResizeBackBuffer() {}

	/**
	* Actions to take after back buffer is resized.
	*/
	virtual void PostResizeBackBuffer() {}

	/**
	* Encode an input back buffer.
	*/
	virtual void EncodeFrame(const FVideoEncoderSettings& Settings, const FTexture2DRHIRef& BackBuffer, uint64 CaptureMs) = 0;

	/**
	* Force the next frame to be an IDR frame.
	*/
	virtual void ForceIdrFrame() = 0;

	/**
	* If encoder is running in async/sync mode.
	*/
	virtual bool IsAsyncEnabled() const = 0;
};

