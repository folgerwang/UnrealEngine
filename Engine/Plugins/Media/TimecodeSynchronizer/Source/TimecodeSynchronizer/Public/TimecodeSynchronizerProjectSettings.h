// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "TimecodeSynchronizer.h"

#include "TimecodeSynchronizerProjectSettings.generated.h"


/**
 * Global settings for TimecodeSynchronizer
 */
UCLASS(config=Engine)
class TIMECODESYNCHRONIZER_API UTimecodeSynchronizerProjectSettings : public UObject
{
public:
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category="TimecodeSynchronizer")
	TSoftObjectPtr<UTimecodeSynchronizer> DefaultTimecodeSynchronizer;
};