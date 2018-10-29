// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreBinarySampleBase.h"


FMediaIOCoreBinarySampleBase::FMediaIOCoreBinarySampleBase()
	: Time(FTimespan::Zero())
{ }


bool FMediaIOCoreBinarySampleBase::Initialize(const uint8* InBinaryBuffer, uint32 InBufferSize, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	if (InBinaryBuffer == nullptr)
	{
		Buffer.Reset();
		return false;
	}

	Buffer.Reset(InBufferSize);
	Buffer.Append(InBinaryBuffer, InBufferSize);
	Time = InTime;
	Timecode = InTimecode;

	return true;
}


bool FMediaIOCoreBinarySampleBase::Initialize(TArray<uint8> InBinaryBuffer, FTimespan InTime, const TOptional<FTimecode>& InTimecode)
{
	Buffer = MoveTemp(InBinaryBuffer);
	Time = InTime;
	Timecode = InTimecode;

	return true;
}
