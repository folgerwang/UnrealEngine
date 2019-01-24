// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructOnlyHeader.generated.h"

USTRUCT()
struct FSomeStruct
{
#pragma region X

	GENERATED_BODY()

#pragma endregion X
};

USTRUCT()
struct alignas(8) FAlignedStruct
{
	GENERATED_BODY()
};
