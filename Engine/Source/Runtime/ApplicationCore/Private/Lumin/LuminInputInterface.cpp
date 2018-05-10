// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 201x Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "Lumin/LuminInputInterface.h"
#include "LuminGamepadInterface.h"
#include "GenericPlatform/GenericApplication.h"
#include "IInputDevice.h"
#include "IHapticDevice.h"
#include "CoreMinimal.h"

TSharedRef<FAndroidInputInterface> FAndroidInputInterface::Create(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
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
