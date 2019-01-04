// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NavAreas/NavArea_LowHeight.h"

UNavArea_LowHeight::UNavArea_LowHeight(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	DefaultCost = BIG_NUMBER;
	DrawColor = FColor(0, 0, 128);

	// can't traverse
	AreaFlags = 0;
}
