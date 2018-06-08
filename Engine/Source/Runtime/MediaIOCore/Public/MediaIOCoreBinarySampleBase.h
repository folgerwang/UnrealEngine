// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaBinarySample.h"
#include "MediaObjectPool.h"

/**
 * Implements a media binary data sample for AjaMedia.
 */
class FMediaIOCoreBinarySampleBase
	: public IMediaBinarySample
	, public IMediaPoolable
{
public:

	/** Default constructor. */
	FMediaIOCoreBinarySampleBase()
		: Time(FTimespan::Zero())
	{ }

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
};
