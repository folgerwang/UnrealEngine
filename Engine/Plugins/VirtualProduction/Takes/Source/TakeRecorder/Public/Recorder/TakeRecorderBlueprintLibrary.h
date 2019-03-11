// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Recorder/TakeRecorder.h"
#include "TakeRecorderBlueprintLibrary.generated.h"

class UTakeRecorder;
class UTakeMetaData;
class ULevelSequence;
class UTakeRecorderPanel;

UCLASS()
class TAKERECORDER_API UTakeRecorderBlueprintLibrary : public UBlueprintFunctionLibrary
{
public:

	GENERATED_BODY()


	/**
	 * Is the Take Recorder enabled in the build
	 */
	UFUNCTION(BlueprintPure, Category="Take Recorder")
	static bool IsTakeRecorderEnabled();

	/**
	 * Start a new recording using the specified parameters. Will fail if a recording is currently in progress
	 *
	 * @param LevelSequence         The base level sequence to use for the recording. Will be played back during the recording and duplicated to create the starting point for the resulting asset.
	 * @param Sources               The sources to use for the recording
	 * @param MetaData              Meta-data pertaining to this recording, duplicated into the resulting recorded sequence
	 * @param Parameters            Configurable parameters for this recorder instance
	 * @return The recorder responsible for the recording, or None if a a recording could not be started
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static UTakeRecorder* StartRecording(ULevelSequence* LevelSequence, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& Parameters);


	/**
	 * Get the default recorder parameters according to the project and user settings
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static FTakeRecorderParameters GetDefaultParameters();


	/**
	 * Check whether a recording is currently active
	 */
	UFUNCTION(BlueprintPure, Category="Take Recorder")
	static bool IsRecording();


	/**
	 * Retrieve the currently active recorder, or None if there none are active
	 */
	UFUNCTION(BlueprintPure, Category="Take Recorder")
	static UTakeRecorder* GetActiveRecorder();


	/**
	 * Stop recording if there is a recorder currently active
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static void StopRecording();


	/**
	 * Get the currently open take recorder panel, if one is open
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Take Recorder")
	static UTakeRecorderPanel* GetTakeRecorderPanel();


	/**
	 * Get the currently open take recorder panel, if one is open, opening a new one if not
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static UTakeRecorderPanel* OpenTakeRecorderPanel();


	DECLARE_DYNAMIC_DELEGATE(FOnTakeRecorderPanelChanged);

	/** Called when a Take Panel is constructed or destroyed. */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static void SetOnTakeRecorderPanelChanged(FOnTakeRecorderPanelChanged OnTakeRecorderPanelChanged);


	/**
	 * Internal function to assign a new take recorder panel singleton.
	 * NOTE: Only to be called by STakeRecorderTabContent::Construct.
	 */
	static void SetTakeRecorderPanel(UTakeRecorderPanel* InNewPanel);
};
