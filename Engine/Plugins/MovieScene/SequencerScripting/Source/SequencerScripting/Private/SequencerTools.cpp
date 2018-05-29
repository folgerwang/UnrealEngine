// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerTools.h"
#include "SequencerScripting.h"
#include "MovieSceneCapture.h"
#include "MovieSceneCaptureDialogModule.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneTimeHelpers.h"
#include "UObject/Stack.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "SequencerTools"

bool USequencerToolsFunctionLibrary::RenderMovie(UMovieSceneCapture* InCaptureSettings)
{
	IMovieSceneCaptureDialogModule& MovieSceneCaptureModule = FModuleManager::Get().LoadModuleChecked<IMovieSceneCaptureDialogModule>("MovieSceneCaptureDialog");
	
	// Because this comes from the Python/BP layer we need to soft-validate the state before we pass it onto functions that do a assert-based validation.
	if (!InCaptureSettings)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot start Render Sequence to Movie with null capture settings."), ELogVerbosity::Error);
		return false;
	}

	if (IsRenderingMovie())
	{
		FFrame::KismetExecutionMessage(TEXT("Capture already in progress."), ELogVerbosity::Error);
		return false;
	}

	// If they're capturing a level sequence we'll do some additional checking as there are more parameters on the Automated Level Sequence capture.
	UAutomatedLevelSequenceCapture* LevelSequenceCapture = Cast<UAutomatedLevelSequenceCapture>(InCaptureSettings);
	if (LevelSequenceCapture)
	{
		if (!LevelSequenceCapture->LevelSequenceAsset.IsValid())
		{
			// UE_LOG(LogTemp, Warning, TEXT("No Level Sequence Asset specified in UAutomatedLevelSequenceCapture."));
			FFrame::KismetExecutionMessage(TEXT("No Level Sequence Asset specified in UAutomatedLevelSequenceCapture."), ELogVerbosity::Error);
			return false;
		}

		if (!LevelSequenceCapture->bUseCustomStartFrame && !LevelSequenceCapture->bUseCustomEndFrame)
		{
			// If they don't want to use a custom start/end frame we override the default values to be the length of the sequence, as the default is [0,1)
			ULevelSequence* LevelSequence = Cast<ULevelSequence>(LevelSequenceCapture->LevelSequenceAsset.TryLoad());
			if (!LevelSequence)
			{
				const FString ErrorMessage = FString::Printf(TEXT("Specified Level Sequence Asset failed to load. Specified Asset Path: %s"), *LevelSequenceCapture->LevelSequenceAsset.GetAssetPathString());
				FFrame::KismetExecutionMessage(*ErrorMessage, ELogVerbosity::Error);
				return false;
			}

			FFrameRate DisplayRate = LevelSequence->GetMovieScene()->GetDisplayRate();
			FFrameRate TickResolution = LevelSequence->GetMovieScene()->GetTickResolution();
			
			LevelSequenceCapture->Settings.FrameRate = DisplayRate;
			LevelSequenceCapture->Settings.bUseRelativeFrameNumbers = false;
			TRange<FFrameNumber> Range = LevelSequence->GetMovieScene()->GetPlaybackRange();

			FFrameNumber StartFrame = MovieScene::DiscreteInclusiveLower(Range);
			FFrameNumber EndFrame = MovieScene::DiscreteExclusiveUpper(Range);

			FFrameNumber RoundedStartFrame = FFrameRate::TransformTime(StartFrame, TickResolution, DisplayRate).CeilToFrame();
			FFrameNumber RoundedEndFrame = FFrameRate::TransformTime(EndFrame, TickResolution, DisplayRate).CeilToFrame();

			LevelSequenceCapture->CustomStartFrame = RoundedStartFrame;
			LevelSequenceCapture->CustomEndFrame = RoundedEndFrame;
		}
	}

	MovieSceneCaptureModule.StartCapture(InCaptureSettings);
	return true;
}

void USequencerToolsFunctionLibrary::CancelMovieRender()
{
	IMovieSceneCaptureDialogModule& MovieSceneCaptureModule = FModuleManager::Get().LoadModuleChecked<IMovieSceneCaptureDialogModule>("MovieSceneCaptureDialog");
	TSharedPtr<FMovieSceneCaptureBase> CurrentCapture = MovieSceneCaptureModule.GetCurrentCapture();
	if (CurrentCapture.IsValid())
	{
		// We just invoke the capture's Cancel function. This will cause a shut-down of the capture (the same as the UI)
		// which will invoke all of the necessary callbacks as well. We don't null out CurrentCapture because that is done
		// as the result of its shutdown callbacks.
		CurrentCapture->Cancel();
	}
}
#undef LOCTEXT_NAMESPACE // "SequencerTools"