// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimationBudgetBlueprintLibrary.generated.h"

/**
 * Function library to expose the budget allocator to Blueprints
 */
UCLASS(meta = (ScriptName = "AnimationBudgetLibrary"))
class UAnimationBudgetBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/**
	 * Enable/disable the animation budgeting system.
	 * Note that the system can also be disabled 'globally' via CVar, which overrides this setting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation Budget", meta=(WorldContext="WorldContextObject"))
	static void EnableAnimationBudget(UObject* WorldContextObject, bool bEnabled);
};