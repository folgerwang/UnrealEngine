// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaOutput.h"

#include "AjaMediaCapture.h"
#include "AjaMediaSettings.h"

/* UAjaMediaOutput
*****************************************************************************/

UAjaMediaOutput::UAjaMediaOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OutputType(EAjaMediaOutputType::FillOnly)
	, bOutputWithAutoCirculating(false)
	, TimecodeFormat(EAjaMediaTimecodeFormat::LTC)
	, PixelFormat(EAjaMediaOutputPixelFormat::PF_10BIT_RGB)
	, bWaitForSyncEvent(false)
	, bEncodeTimecodeInTexel(false)
{
}

bool UAjaMediaOutput::Validate(FString& OutFailureReason) const
{
	if (!Super::Validate(OutFailureReason))
	{
		return false;
	}
	
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
	const FAjaMediaMode CurrentMode = GetMediaMode();
	if (!CurrentMode.IsValid())
	{
		const FString OverrideString = !bIsDefaultModeOverriden ? TEXT("The project settings haven't been set for this port.") : TEXT("");
		OutFailureReason = FString::Printf(TEXT("The MediaMode of '%s' is invalid. %s"), *GetName(), *OverrideString);
		return false;
	}

	if (CurrentMode.DeviceIndex != FillPort.DeviceIndex)
	{
		OutFailureReason = FString::Printf(TEXT("The MediaMode & FillPort of '%s' are not on the same device."), *GetName());
		return false;
	}

	return true;
}

FAjaMediaMode UAjaMediaOutput::GetMediaMode() const
{
	FAjaMediaMode CurrentMode;
	if (bIsDefaultModeOverriden == false)
	{
		CurrentMode = GetDefault<UAjaMediaSettings>()->GetOutputMediaMode(FillPort);
	}
	else
	{
		CurrentMode = MediaMode;
	}

	return CurrentMode;
}

FIntPoint UAjaMediaOutput::GetRequestedSize() const
{
	return GetMediaMode().TargetSize;
}

EPixelFormat UAjaMediaOutput::GetRequestedPixelFormat() const
{
	EPixelFormat Result = EPixelFormat::PF_A2B10G10R10;
	switch (PixelFormat)
	{
	case EAjaMediaOutputPixelFormat::PF_8BIT_ARGB:
		Result = EPixelFormat::PF_B8G8R8A8;
		break;
	case EAjaMediaOutputPixelFormat::PF_10BIT_RGB:
		Result = EPixelFormat::PF_A2B10G10R10;
		break;
	}
	return Result;
}

UMediaCapture* UAjaMediaOutput::CreateMediaCaptureImpl()
{
	return NewObject<UAjaMediaCapture>();
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
