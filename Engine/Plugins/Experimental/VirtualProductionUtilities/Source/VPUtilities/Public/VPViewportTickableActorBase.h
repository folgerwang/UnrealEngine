// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

#include "VPViewportTickableActorBase.generated.h"

/**
 * Actor that tick in the Editor viewport with the event EditorTick.
 */
UCLASS(Abstract)
class AVPViewportTickableActorBase : public AActor
{
	GENERATED_BODY()

public:
	AVPViewportTickableActorBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "Tick")
	void EditorTick(float DeltaSeconds);

	/** If true, actor is ticked even if TickType==LEVELTICK_ViewportsOnly */
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaSeconds) override;
};
