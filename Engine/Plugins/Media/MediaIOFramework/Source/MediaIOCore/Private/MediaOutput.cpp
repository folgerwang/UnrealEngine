// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaOutput.h"

#include "MediaCapture.h"
#include "MediaIOCoreModule.h"

/* IMediaOptions interface
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
		UE_LOG(LogMediaIOCore, Error, TEXT("Couldn't create the media capture. %s."), *FailureReason);
	}

	return Result;
}

bool UMediaOutput::Validate(FString& OutFailureReason) const
{
	bool bResult = (NumberOfTextureBuffers >= 1 && NumberOfTextureBuffers <= 4);

	if (!bResult)
	{
		OutFailureReason = TEXT("NumberOfBuffer is not valid.");
	}

	return bResult;
}
