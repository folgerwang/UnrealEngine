// Copyright (c) Microsoft Corporation. All rights reserved.

#include "CoreMinimal.h"

#pragma once

UENUM()
enum class ESpatialInputSourceKind
{
	Other = 0,
	Hand = 1,
	Voice = 2,
	Controller = 3
};
