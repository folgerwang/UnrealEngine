// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Abilities/GameplayAbilityTargetDataFilter.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayTargetDataFilter
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayTargetDataFilter::InitializeFilterContext(AActor* FilterActor)
{
	SelfActor = FilterActor;
}
