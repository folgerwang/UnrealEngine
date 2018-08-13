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

	UTimecodeSynchronizerProjectSettings()
		: bDisplayInToolbar(true)
	{ }

public:
	/** Display the timecode synchronizer icon in the editor toolbar. */
	UPROPERTY(Config, EditAnywhere, Category="TimecodeSynchronizer")
	bool bDisplayInToolbar;

	UPROPERTY(config, EditAnywhere, Category="TimecodeSynchronizer")
	TSoftObjectPtr<UTimecodeSynchronizer> DefaultTimecodeSynchronizer;
};

/**
 * Editor settings for TimecodeSynchronizer
 */
UCLASS(config= EditorPerProjectUserSettings)
class TIMECODESYNCHRONIZER_API UTimecodeSynchronizerEditorSettings : public UObject
{
public:
	GENERATED_BODY()

public:

	UPROPERTY(config, EditAnywhere, Category="TimecodeSynchronizer")
	TSoftObjectPtr<UTimecodeSynchronizer> UserTimecodeSynchronizer;
};
