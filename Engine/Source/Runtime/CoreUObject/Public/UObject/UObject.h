// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

UE_DEPRECATED(4.14, "Including UObject.h has been deprecated. Please include Object.h instead.")
inline void UObjectHeaderDeprecatedWarning()
{
}

inline void TriggerUObjectHeaderDeprecatedWarning()
{
	UObjectHeaderDeprecatedWarning();
}
