// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Tickable.h"

#include "BlackmagicMediaViewportOutput.generated.h"

class UBlackmagicMediaOutput;
class FBlackmagicMediaViewportOutputImpl;

/**
 * Class to manage to output the viewport
 */
UCLASS(BlueprintType)
class BLACKMAGICMEDIAOUTPUT_API UBlackmagicMediaViewportOutput : public UObject, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:
	//~ FTickableGameObject interface
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltatTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBlackmagicMediaViewportOutput, STATGROUP_Tickables); }

	//~ UObject interface
	virtual void BeginDestroy() override;
	
	UFUNCTION(BlueprintCallable, Category=BLACKMAGIC)
	void ActivateOutput(UBlackmagicMediaOutput* MediaOutput);

	UFUNCTION(BlueprintCallable, Category=BLACKMAGIC)
	void DeactivateOutput();

private:
	TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> Implementation;
};
