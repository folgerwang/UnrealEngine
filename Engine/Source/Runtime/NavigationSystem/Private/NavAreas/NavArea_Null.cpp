// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NavAreas/NavArea_Null.h"

UNavArea_Null::UNavArea_Null(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultCost = BIG_NUMBER;
	AreaFlags = 0;
}
