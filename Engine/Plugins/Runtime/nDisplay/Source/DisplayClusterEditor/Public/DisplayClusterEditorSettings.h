// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DisplayClusterEditorSettings.generated.h"


/**
 * Implements the settings for the nDisplay
 **/
UCLASS(config = Engine, defaultconfig)
class DISPLAYCLUSTEREDITOR_API UDisplayClusterEditorSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category = Main)
	bool bEnabled;

public:
	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

};
