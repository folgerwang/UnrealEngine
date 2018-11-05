// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneCaptureDialogModule.h"
#include "SequencerTools.generated.h"

/** 
 * This is a set of helper functions to access various parts of the Sequencer API via Python. Because Sequencer itself is not suitable for exposing, most functionality
 * gets wrapped by UObjects that have an easier API to work with. This UObject provides access to these wrapper UObjects where needed. 
 */
UCLASS(Transient, meta=(ScriptName="SequencerTools"))
class SEQUENCERSCRIPTINGEDITOR_API USequencerToolsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	* Attempts to render a sequence to movie based on the specified settings. This will automatically detect
	* if we're rendering via a PIE instance or a new process based on the passed in settings. Will return false
	* if the state is not valid (ie: null or missing required parameters, capture in progress, etc.), true otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools")
	static bool RenderMovie(class UMovieSceneCapture* InCaptureSettings);

	/** 
	* Returns if Render to Movie is currently in progress.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools")
	static bool IsRenderingMovie()
	{
		IMovieSceneCaptureDialogModule& MovieSceneCaptureModule = FModuleManager::Get().LoadModuleChecked<IMovieSceneCaptureDialogModule>("MovieSceneCaptureDialog");
		return MovieSceneCaptureModule.GetCurrentCapture().IsValid();
	}

	/**
	* Attempts to cancel an in-progress Render to Movie. Does nothing if there is no render in progress.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools")
	static void CancelMovieRender();
};