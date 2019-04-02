// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCaptureEnvironment.h"
#include "MovieSceneCapture.h"
#include "MovieSceneCaptureModule.h"
#include "Protocols/UserDefinedCaptureProtocol.h"

int32 UMovieSceneCaptureEnvironment::GetCaptureFrameNumber()
{
	UMovieSceneCapture* Capture = static_cast<UMovieSceneCapture*>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	return Capture ? Capture->GetMetrics().Frame : 0;
}

float UMovieSceneCaptureEnvironment::GetCaptureElapsedTime()
{
	UMovieSceneCapture* Capture = static_cast<UMovieSceneCapture*>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	return Capture ? Capture->GetMetrics().ElapsedSeconds : 0.f;
}

bool UMovieSceneCaptureEnvironment::IsCaptureInProgress()
{
	return IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture() != nullptr;
}

UMovieSceneImageCaptureProtocolBase* UMovieSceneCaptureEnvironment::FindImageCaptureProtocol()
{
	UMovieSceneCapture* Capture = static_cast<UMovieSceneCapture*>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	return Capture ? Cast<UMovieSceneImageCaptureProtocolBase>(Capture->GetImageCaptureProtocol()) : nullptr;
}

UMovieSceneAudioCaptureProtocolBase* UMovieSceneCaptureEnvironment::FindAudioCaptureProtocol()
{
	UMovieSceneCapture* Capture = static_cast<UMovieSceneCapture*>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	return Capture ? Cast<UMovieSceneAudioCaptureProtocolBase>(Capture->GetAudioCaptureProtocol()) : nullptr;
}