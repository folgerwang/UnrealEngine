// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimationBudgetBlueprintLibrary.h"
#include "AnimationBudgetAllocatorModule.h"
#include "Modules/ModuleManager.h"
#include "IAnimationBudgetAllocator.h"
#include "Engine/Engine.h"

void UAnimationBudgetBlueprintLibrary::EnableAnimationBudget(UObject* WorldContextObject, bool bEnabled)
{
	if(UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FAnimationBudgetAllocatorModule& AnimationBudgetAllocatorModule = FModuleManager::LoadModuleChecked<FAnimationBudgetAllocatorModule>("AnimationBudgetAllocator");
		IAnimationBudgetAllocator* AnimationBudgetAllocator = AnimationBudgetAllocatorModule.GetBudgetAllocatorForWorld(World);
		if(AnimationBudgetAllocator)
		{
			AnimationBudgetAllocator->SetEnabled(bEnabled);
		}
	}
}