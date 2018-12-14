// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class to expose a custom per encoding decompression context and a common interface.
 */
class FAnimEncodingDecompressionContext
{
public:
	virtual ~FAnimEncodingDecompressionContext() {}

	virtual void Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime) = 0;
};
