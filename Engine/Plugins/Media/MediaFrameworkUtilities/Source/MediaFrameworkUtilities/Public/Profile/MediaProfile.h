// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "MediaProfile.generated.h"

class UMediaOutput;
class UMediaSource;

/**
 * A media profile that configures the inputs, outputs, timecode provider and custom time step.
 */
UCLASS(BlueprintType)
class MEDIAFRAMEWORKUTILITIES_API UMediaProfile : public UObject
{
	GENERATED_BODY()

private:

	/** Media sources. */
	UPROPERTY(EditAnywhere, Instanced, Category="Inputs")
	TArray<UMediaSource*> MediaSources;

	/** Media outputs. */
	UPROPERTY(EditAnywhere, Instanced, Category="Outputs")
	TArray<UMediaOutput*> MediaOutputs;

public:

	/**
	 * Get the media source for the selected proxy.
	 *
	 * @return The media source, or nullptr if not set.
	 */
	UMediaSource* GetMediaSource(int32 Index) const;

	/**
	 * Get the media output for the selected proxy.
	 *
	 * @return The media output, or nullptr if not set.
	 */
	UMediaOutput* GetMediaOutput(int32 Index) const;

public:

	/**
	 * Apply the media profile as the current profile.
	 * Will change the engine's timecode provider & custom time step and redirect the media profile source/output proxy for the correct media source/output.
	 */
	void Apply();


	/**
	 * Apply the media profile as the current profile.
	 * Will change the engine's timecode provider & custom time step and redirect the media profile source/output proxy for the correct media source/output.
	 */
	bool IsMediaSourceAffectedByProfile(UMediaSource* InMediaSource);
};
