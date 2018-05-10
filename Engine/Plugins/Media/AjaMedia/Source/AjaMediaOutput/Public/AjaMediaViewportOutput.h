// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Tickable.h"

#include "AjaMediaViewportOutput.generated.h"

class UAjaMediaOutput;
class FAjaMediaViewportOutputImpl;

/**
 * Class to manage to output the viewport to AJASDI 
 */
UCLASS(BlueprintType)
class AJAMEDIAOUTPUT_API UAjaMediaViewportOutput : public UObject, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:
	//~ FTickableGameObject interface
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAjaMediaViewportOutput, STATGROUP_Tickables); }

	//~ UObject interface
	virtual void BeginDestroy() override;
	
	UFUNCTION(BlueprintCallable, Category=AJA)
	void ActivateOutput(UAjaMediaOutput* MediaOutput);

	UFUNCTION(BlueprintCallable, Category=AJA)
	void DeactivateOutput();

private:
	TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> Implementation;
};
