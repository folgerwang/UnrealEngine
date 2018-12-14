// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NavAreas/NavArea.h"
#include "NavArea_Default.generated.h"

/** Regular navigation area, applied to entire navigation data by default */
UCLASS(Config=Engine)
class NAVIGATIONSYSTEM_API UNavArea_Default : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
