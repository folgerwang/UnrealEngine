// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaOutput.h"


/* UAjaMediaOutput
*****************************************************************************/

UAjaMediaOutput::UAjaMediaOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OutputType(EAjaMediaOutputType::FillOnly)
	, bOutputWithAutoCirculating(false)
	, bOutputTimecode(true)
	, bCopyVideoOnRenderThread(true)
	, bWaitForSyncEvent(false)
	, bClearBuffer(false)
	, ClearBufferColor(FColor::Green)
	, bEncodeTimecodeInTexel(false)
{
}

bool UAjaMediaOutput::Validate(FString& OutFailureReason) const
{
	if (!FillPort.IsValid())
	{
		OutFailureReason = FString::Printf(TEXT("The FillPort of '%s' is invalid ."), *GetName());
		return false;
	}

	if (OutputReference == EAjaMediaOutputReferenceType::Input)
	{
		if (!SyncPort.IsValid())
		{
			OutFailureReason = FString::Printf(TEXT("The SyncPort of '%s' is invalid ."), *GetName());
			return false;
		}

		if (FillPort.DeviceIndex != SyncPort.DeviceIndex)
		{
			OutFailureReason = FString::Printf(TEXT("The FillPort & SyncPort of '%s' are not on the same device."), *GetName());
			return false;
		}
	}

	if (OutputType == EAjaMediaOutputType::FillAndKey)
	{
		if (!KeyPort.IsValid())
		{
			OutFailureReason = FString::Printf(TEXT("The KeyPort of '%s' is invalid ."), *GetName());
			return false;
		}

		if (FillPort.DeviceIndex != KeyPort.DeviceIndex)
		{
			OutFailureReason = FString::Printf(TEXT("The FillPort & KeyPort of '%s' are not on the same device."), *GetName());
			return false;
		}
	}

	if (!MediaMode.IsValid())
	{
		OutFailureReason = FString::Printf(TEXT("The MediaMode of '%s' is invalid ."), *GetName());
		return false;
	}

	if (MediaMode.DeviceIndex != FillPort.DeviceIndex)
	{
		OutFailureReason = FString::Printf(TEXT("The MediaMode & FillPort of '%s' are not on the same device."), *GetName());
		return false;
	}

	return true;
}

#if WITH_EDITOR
bool UAjaMediaOutput::CanEditChange(const UProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAjaMediaOutput, KeyPort))
	{
		return OutputType == EAjaMediaOutputType::FillAndKey;
	}
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAjaMediaOutput, SyncPort))
	{
		return OutputReference == EAjaMediaOutputReferenceType::Input;
	}

	return true;
}

void UAjaMediaOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAjaMediaOutput, bOutputWithAutoCirculating))
	{
		if (!bOutputWithAutoCirculating)
		{
			bWaitForSyncEvent = false;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR
