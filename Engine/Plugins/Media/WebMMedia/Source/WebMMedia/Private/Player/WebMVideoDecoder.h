// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "MediaShaders.h"
#include "WebMMediaTextureSample.h"

THIRD_PARTY_INCLUDES_START
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
THIRD_PARTY_INCLUDES_END

class FMediaSamples;
struct FWebMFrame;

class FWebMVideoDecoder
{
public:
	FWebMVideoDecoder(TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> InSamples);
	~FWebMVideoDecoder();

public:
	void Initialize();
	void DecodeVideoFramesAsync(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames);

private:
	struct FConvertParams
	{
		TSharedPtr<FWebMMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
		const vpx_image_t* Image;
	};

	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TUniquePtr<FWebMMediaTextureSamplePool> VideoSamplePool;
	TRefCountPtr<FRHITexture2D> DecodedY;
	TRefCountPtr<FRHITexture2D> DecodedU;
	TRefCountPtr<FRHITexture2D> DecodedV;
	FGraphEventRef VideoDecodingTask;
	bool bTexturesCreated;

	vpx_codec_ctx_t Context;

	void ConvertYUVToRGBAndSubmit(const FConvertParams& Params);
	void DoDecodeVideoFrames(const TArray<TSharedPtr<FWebMFrame>>& VideoFrames);
	void CreateTextures(const vpx_image_t* Image);
};
