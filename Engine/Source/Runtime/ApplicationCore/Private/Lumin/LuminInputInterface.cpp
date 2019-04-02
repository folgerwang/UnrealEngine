// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Lumin/LuminInputInterface.h"
#include "LuminGamepadInterface.h"
#include "GenericPlatform/GenericApplication.h"
#include "IInputDevice.h"
#include "IHapticDevice.h"
#include "CoreMinimal.h"

TSharedRef<FAndroidInputInterface> FAndroidInputInterface::Create(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
{
	return MakeShareable(new FAndroidInputInterface(InMessageHandler));
}

void FAndroidInputInterface::Tick(float DeltaTime)
{
	for (TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		InputDevice->Tick(DeltaTime);
	}
}

void FAndroidInputInterface::SendControllerEvents()
{
	for (TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		InputDevice->SendControllerEvents();
	}
}

void FAndroidInputInterface::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
	for (TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		InputDevice->SetMessageHandler(InMessageHandler);
	}	
}

void FAndroidInputInterface::ResetGamepadAssignments()
{

}

void FAndroidInputInterface::ResetGamepadAssignmentToController(int32 ControllerId)
{

}

bool FAndroidInputInterface::IsControllerAssignedToGamepad(int32 ControllerId)
{
	// @todo Lumin: Well, we only support gamepads really, so always true?
	return true;
}

FString FAndroidInputInterface::GetGamepadControllerName(int32 ControllerId)
{
	return FString(TEXT("Generic"));
}

void FAndroidInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	for (TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		InputDevice->SetChannelValue(ControllerId, ChannelType, Value);
	}
}

void FAndroidInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	for (TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		InputDevice->SetChannelValues(ControllerId, Values);
	}
}

void FAndroidInputInterface::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	for (TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		IHapticDevice* HapticDevice = InputDevice->GetHapticDevice();
		if (HapticDevice != nullptr)
		{
			HapticDevice->SetHapticFeedbackValues(ControllerId, Hand, Values);
		}
	}
}

void FAndroidInputInterface::AddExternalInputDevice(TSharedPtr<IInputDevice>& InputDevice)
{
	if (InputDevice.IsValid())
	{
		LuminInputDevices.Add(InputDevice);
	}
}

bool FAndroidInputInterface::IsGamepadAttached() const
{
	for (const TSharedPtr<IInputDevice>& InputDevice : LuminInputDevices)
	{
		return InputDevice->IsGamepadAttached();
	}

	return false;
}

FAndroidInputInterface::FAndroidInputInterface(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	: MessageHandler(InMessageHandler)
{
	// Removed Xbox controller support from Lumin since it was directly accessing /dev/input. which is a security violation.
	// LuminInputDevices.Add(FLuminGamepadInterface::Create(InMessageHandler));
}
