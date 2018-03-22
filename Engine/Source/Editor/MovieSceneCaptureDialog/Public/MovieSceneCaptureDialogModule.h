// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/NumericTypeInterface.h"

class UMovieSceneCapture;

class IMovieSceneCaptureDialogModule : public IModuleInterface
{
public:
	static IMovieSceneCaptureDialogModule& Get()
	{
		static const FName ModuleName(TEXT("MovieSceneCaptureDialog"));
		return FModuleManager::LoadModuleChecked<IMovieSceneCaptureDialogModule>(ModuleName);
	}
	virtual void OpenDialog(const TSharedRef<class FTabManager>& TabManager, UMovieSceneCapture* CaptureObject, TSharedPtr<INumericTypeInterface<double>> InNumericTypeInterface) = 0;

	/** Get the world we're currently recording from, if an in process record is happening */
	virtual UWorld* GetCurrentlyRecordingWorld() = 0;
};

