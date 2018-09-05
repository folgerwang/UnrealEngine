// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "PixelStreamingSettings.generated.h"

UCLASS(config = PixelStreaming, defaultconfig, meta = (DisplayName = "PixelStreaming"))
class PIXELSTREAMING_API UPixelStreamingSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	/**
	* Pixel streaming always requires a default software cursor as it needs to
	* be shown on the browser to allow the user to click UI elements.
	*/
	UPROPERTY(config, EditAnywhere, Category = PixelStreaming)
	FSoftClassPath PixelStreamingDefaultCursorClassName;

	// Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	// END UDeveloperSettings Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};