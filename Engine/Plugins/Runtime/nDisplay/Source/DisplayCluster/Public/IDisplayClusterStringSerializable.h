// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * String serialization interface
 */
struct IDisplayClusterStringSerializable
{
	virtual ~IDisplayClusterStringSerializable() = 0
	{ }

	virtual FString SerializeToString() const = 0;
	virtual bool    DeserializeFromString(const FString& ar) = 0;
};
