// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaFrameGrabberProtocol.h"

#include "BlackmagicMediaOutput.h"
#include "BlackmagicHardwareSync.h"

#include "BlackmagicMediaViewportOutputImpl.h"
#include "IBlackmagicMediaOutputModule.h"
#include "MovieSceneCaptureProtocolBase.h"

#define LOCTEXT_NAMESPACE "BlackmagicMediaOutput"

/**
 * UBlackmagicFrameGrabberProtocol
 */
UBlackmagicFrameGrabberProtocol::UBlackmagicFrameGrabberProtocol(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, Information("FrameRate, Resolution, Output Directory and Filename Format options won't be used with output")
	, TransientMediaOutputPtr(nullptr)
{
}

bool UBlackmagicFrameGrabberProtocol::StartCaptureImpl()
{
	if (MediaOutput == nullptr)
	{
		UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Couldn't start the capture. No Media Output was provided."));
		return false;
	}

	TransientMediaOutputPtr = Cast<UBlackmagicMediaOutput>(MediaOutput.TryLoad());
	if (TransientMediaOutputPtr == nullptr)
	{
		UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Couldn't start the capture. No Media Output was provided."));
		return false;
	}

	Implementation = FBlackmagicMediaViewportOutputImpl::CreateShared(TransientMediaOutputPtr, InitSettings->SceneViewport);

	if (!Implementation.IsValid())
	{
		UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Could not initialize the Output interface."));
		return false;
	}

	if (Implementation->GetOutputFrameRate() != CaptureHost->GetCaptureFrameRate())
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("BlackmagicMediaOutput %s FrameRate doesn't match sequence FrameRate."), *TransientMediaOutputPtr->GetName());
	}

	return true;
}

void UBlackmagicFrameGrabberProtocol::CaptureFrameImpl(const FFrameMetrics& FrameMetrics)
{
	check(Implementation.IsValid());

	const FFrameNumber FrameNumber = (int32)FrameMetrics.FrameNumber;
	FTimecode Timecode = FTimecode::FromFrameNumber(FrameNumber, CaptureHost->GetCaptureFrameRate(), FTimecode::IsDropFormatTimecodeSupported(CaptureHost->GetCaptureFrameRate()));
	Implementation->Tick(Timecode);
}

bool UBlackmagicFrameGrabberProtocol::HasFinishedProcessingImpl() const
{
	return !Implementation.IsValid() || Implementation->HasFinishedProcessing();
}

void UBlackmagicFrameGrabberProtocol::FinalizeImpl()
{
	if (Implementation.IsValid())
	{
		Implementation->Shutdown();
		Implementation.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
