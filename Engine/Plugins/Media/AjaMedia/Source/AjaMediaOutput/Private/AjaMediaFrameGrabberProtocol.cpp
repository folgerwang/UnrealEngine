// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaFrameGrabberProtocol.h"

#include "AjaMediaOutput.h"
#include "AjaMediaViewportOutputImpl.h"
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
	Implementation = FAjaMediaViewportOutputImpl::CreateShared(MediaOutput.Get(), InSettings.SceneViewport);

	if (!Implementation.IsValid())
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Could not initialize the Output interface."));
		Finalize();
		return bInitialized;
	}

	if (Implementation->GetOutputFrameRate() != Host.GetCaptureFrameRate())
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("AjaMediaOutput %s FrameRate doesn't match sequence FrameRate."), *AjaSettings->MediaOutput->GetName());
	}

	bInitialized = true;
	return bInitialized;
}

void FAjaFrameGrabberProtocol::CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host)
{
	if (bInitialized)
	{
		check(Implementation.IsValid());

		const FFrameNumber FrameNumber = (int32)FrameMetrics.FrameNumber;
		FTimecode Timecode = FTimecode::FromFrameNumber(FrameNumber, Host.GetCaptureFrameRate(), FTimecode::IsDropFormatTimecodeSupported(Host.GetCaptureFrameRate()));
		Implementation->Tick(Timecode);
	}
}

bool FAjaFrameGrabberProtocol::HasFinishedProcessing() const
{
	return !bInitialized || !Implementation.IsValid() || Implementation->HasFinishedProcessing();
}

void FAjaFrameGrabberProtocol::Finalize()
{
	if (Implementation.IsValid())
	{
		Implementation->Shutdown();
		Implementation.Reset();
	}
	MediaOutput.Reset();
	bInitialized = false;
}

#undef LOCTEXT_NAMESPACE
