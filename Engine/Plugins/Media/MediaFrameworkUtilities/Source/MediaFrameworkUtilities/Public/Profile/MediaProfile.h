// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "MediaProfile.generated.h"

class UEngineCustomTimeStep;
class UMediaOutput;
class UMediaSource;
class UTimecodeProvider;

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

	/** Override the Engine's Timecode provider defined in the project settings. */
	UPROPERTY(EditAnywhere, Category="Timecode Provider", meta=(DisplayName="Override Project Settings"))
	bool bOverrideTimecodeProvider;

	/** Timecode provider. */
	UPROPERTY(EditAnywhere, Instanced, Category="Timecode Provider", meta=(EditCondition="bOverrideTimecodeProvider"))
	UTimecodeProvider* TimecodeProvider;

	/** Override the Engine's Custom time step defined in the project settings. */
	UPROPERTY(EditAnywhere, Category="Genlock", meta=(DisplayName="Override Project Settings"))
	bool bOverrideCustomTimeStep;

	/** Custom time step */
	UPROPERTY(EditAnywhere, Instanced, Category="Genlock", meta=(EditCondition="bOverrideCustomTimeStep"))
	UEngineCustomTimeStep* CustomTimeStep;

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

	/**
	 * Get the timecode provider.
	 *
	 * @return The timecode provider, or nullptr if not set.
	 */
	UTimecodeProvider* GetTimecodeProvider() const;

	/**
	 * Get the custom time step.
	 *
	 * @return The custom time step, or nullptr if not set.
	 */
	UEngineCustomTimeStep* GetCustomTimeStep() const;

public:

	/**
	 * Apply the media profile.
	 * Will change the engine's timecode provider & custom time step and redirect the media profile source/output proxy for the correct media source/output.
	 */
	void Apply();

	/**
	 * Reset the media profile.
	 * Will reset the engine's timecode provider & custom time step and redirect the media profile source/output proxy for no media source/output.
	 */
	void Reset();


	/**
	 * Apply the media profile as the current profile.
	 * Will change the engine's timecode provider & custom time step and redirect the media profile source/output proxy for the correct media source/output.
	 */
	bool IsMediaSourceAffectedByProfile(UMediaSource* InMediaSource);
};
