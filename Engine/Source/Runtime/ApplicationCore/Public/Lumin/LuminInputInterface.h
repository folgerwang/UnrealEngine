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

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "Math/Color.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"

/**
 * Interface class for Lumin input devices.
 */
class FAndroidInputInterface : public IInputInterface
{
public:
	virtual ~FAndroidInputInterface() = default;

	static TSharedRef<FAndroidInputInterface> Create(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, const TSharedPtr< ICursor >& InCursor);

	/** Tick the interface (i.e check for new controllers) */
	void Tick(float DeltaTime);
  
	/** Poll for controller state and send events if needed */
	void SendControllerEvents();

	void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);

	static void ResetGamepadAssignments();
	static void ResetGamepadAssignmentToController(int32 ControllerId);
	static bool IsControllerAssignedToGamepad(int32 ControllerId);

	/** IInputInterface interface */
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) override;
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values);
	virtual void SetLightColor(int32 ControllerId, struct FColor Color) override {};

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice>& InputDevice);
	virtual bool IsGamepadAttached() const;

	const TSharedPtr< ICursor > GetCursor() const { return nullptr; }

private:
	FAndroidInputInterface(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);

private:
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** Lumin Gamepad + List of input devices implemented in external modules. */
	TArray<TSharedPtr<class IInputDevice>> LuminInputDevices;
};

