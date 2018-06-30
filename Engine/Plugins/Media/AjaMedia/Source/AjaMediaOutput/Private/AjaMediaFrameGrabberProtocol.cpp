// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaFrameGrabberProtocol.h"

#include "IAjaMediaOutputModule.h"
#include "IMovieSceneCaptureProtocol.h"

#define LOCTEXT_NAMESPACE "AjaMediaOutput"


/**
 * UAjaFrameGrabberProtocolSettings
 */

UAjaFrameGrabberProtocolSettings::UAjaFrameGrabberProtocolSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Information("FrameRate, Resolution, Output Directory and Filename Format options won't be used with AJA output")
{
}

/**
 * FAjaFrameGrabberProtocol
 */

FAjaFrameGrabberProtocol::FAjaFrameGrabberProtocol()
	: bInitialized(false)
{
}

bool FAjaFrameGrabberProtocol::Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host)
{
	bInitialized = false;
	Finalize();

	UAjaFrameGrabberProtocolSettings* AjaSettings = Cast<UAjaFrameGrabberProtocolSettings>(InSettings.ProtocolSettings);
	if (AjaSettings == nullptr)
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Setting provided don't match with the AjaFramGrabberProtocol"));
		return bInitialized;
	}

	if (AjaSettings->MediaOutput == nullptr)
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Couldn't start the capture. No Media Output was provided."));
		return bInitialized;
	}

	MediaOutput = TStrongObjectPtr<UAjaMediaOutput>(AjaSettings->MediaOutput);

	if (MediaOutput->GetMediaMode().FrameRate != Host.GetCaptureFrameRate())
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("AjaMediaOutput %s FrameRate doesn't match sequence FrameRate."), *AjaSettings->MediaOutput->GetName());
		return false;
	}

	MediaCapture.Reset(CastChecked<UAjaMediaCapture>(MediaOutput->CreateMediaCapture(), ECastCheckedType::NullAllowed));
	if (MediaCapture.IsValid())
	{
		bool bResult = MediaCapture->CaptureSceneViewport(InSettings.SceneViewport);
		if (!bResult)
		{
			UE_LOG(LogAjaMediaOutput, Error, TEXT("Could not initialize the Media Capture."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Could not create the Media Capture."));
		return false;
	}

	bInitialized = true;
	return bInitialized;
}

bool FAjaFrameGrabberProtocol::HasFinishedProcessing() const
{
	return !bInitialized || !MediaCapture.IsValid() || MediaCapture->HasFinishedProcessing();
}

void FAjaFrameGrabberProtocol::Finalize()
{
	if (MediaCapture.IsValid())
	{
		MediaCapture->StopCapture(true);
		MediaCapture.Reset();
	}
	MediaOutput.Reset();
	bInitialized = false;
}

#undef LOCTEXT_NAMESPACE
