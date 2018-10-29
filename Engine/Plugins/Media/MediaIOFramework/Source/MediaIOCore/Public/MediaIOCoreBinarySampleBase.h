// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaBinarySample.h"
#include "MediaObjectPool.h"

/**
 * Implements a media binary data sample for AjaMedia.
 */
class MEDIAIOCORE_API FMediaIOCoreBinarySampleBase
	: public IMediaBinarySample
	, public IMediaPoolable
{
public:

	/** Default constructor. */
	FMediaIOCoreBinarySampleBase();

	/**
	 * Initialize the sample.
	 *
	 * @param InBinaryBuffer The metadata frame data.
	 * @param InBufferSize The size of the InBinaryBuffer.
	 * @param InTimecode The sample timecode if available.
	 * @param InTime The sample time (in the player's own clock).
	 */
	bool Initialize(const uint8* InBinaryBuffer, uint32 InBufferSize, FTimespan InTime, const TOptional<FTimecode>& InTimecode);

	/**
	 * Initialize the sample.
	 *
	 * @param InBinaryBuffer The metadata frame data.
	 * @param InTimecode The sample timecode if available.
	 * @param InTime The sample time (in the player's own clock).
	 */
	bool Initialize(TArray<uint8> InBinaryBuffer, FTimespan InTime, const TOptional<FTimecode>& InTimecode);

public:

	//~ IMediaBinarySample interface

	virtual const void* GetData() override
	{
		return Buffer.GetData();
	}

	virtual FTimespan GetDuration() const override
	{
		return FTimespan::Zero();
	}

	virtual uint32 GetSize() const override
	{
		return Buffer.Num();
	}

	virtual FTimespan GetTime() const override
	{
		return Time;
	}

	virtual TOptional<FTimecode> GetTimecode() const override
	{
		return Timecode;
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

	/** The sample's frame buffer. */
	TArray<uint8> Buffer;

	/** Sample time. */
	FTimespan Time;

	/** Sample timecode. */
	TOptional<FTimecode> Timecode;
};
