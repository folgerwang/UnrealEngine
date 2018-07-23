// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterOperationMode.generated.h"

/**
 * Display cluster operation mode
 */
UENUM(BlueprintType)
enum class EDisplayClusterOperationMode : uint8
{
	Cluster,
	Standalone,
	Editor,
	Disabled
};
