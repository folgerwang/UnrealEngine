// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"


/**
 * Memory serialization interface
 */
struct IDisplayClusterSerializable
{
	virtual ~IDisplayClusterSerializable() = 0
	{ }

	virtual bool Serialize  (FMemoryWriter& ar) = 0;
	virtual bool Deserialize(FMemoryReader& ar) = 0;
};
