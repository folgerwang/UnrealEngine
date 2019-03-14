// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaOutput.h"

#include "MediaCapture.h"
#include "MediaIOCoreModule.h"

const FIntPoint UMediaOutput::RequestCaptureSourceSize = FIntPoint::ZeroValue;

/* UMediaOutput
 *****************************************************************************/

UMediaOutput::UMediaOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NumberOfTextureBuffers(2)
{
}

UMediaCapture* UMediaOutput::CreateMediaCapture()
{
	UMediaCapture* Result = nullptr;

	FString FailureReason;
	if (Validate(FailureReason))
	{
		Result = CreateMediaCaptureImpl();
	}
	else
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Couldn't create the media capture. %s"), *FailureReason);
	}

	return Result;
}

bool UMediaOutput::Validate(FString& OutFailureReason) const
{
	FIntPoint RequestedSize = GetRequestedSize();
	if (RequestedSize.X < 1 || RequestedSize.Y < 1)
	{
		OutFailureReason = TEXT("The requested size is invalid.");
		return false;
	}

	if (NumberOfTextureBuffers < 1 && NumberOfTextureBuffers > 4)
	{
		OutFailureReason = TEXT("NumberOfTextureBuffers is not valid.");
		return false;
	}

	return true;
}
