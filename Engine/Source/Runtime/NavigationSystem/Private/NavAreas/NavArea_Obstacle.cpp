// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NavAreas/NavArea_Obstacle.h"

UNavArea_Obstacle::UNavArea_Obstacle(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DrawColor = FColor(127, 51, 0);	// brownish
	DefaultCost = 1000000.0f;
}
