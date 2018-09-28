// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Serialization/Archive.h"

/**
* Base class for serializing bitstreams.
*/
class CORE_API FBitArchive : public FArchive
{
public:
	virtual void SerializeBitsWithOffset( void* Src, int32 SourceBit, int64 LengthBits ) PURE_VIRTUAL(FBitArchive::SerializeBitsWithOffset,);
};