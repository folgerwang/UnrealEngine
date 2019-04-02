// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "Math/Color.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "GenericPlatform/ICursor.h"

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
	static FString GetGamepadControllerName(int32 ControllerId);

	/** IInputInterface interface */
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) override;
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values);
	virtual void SetLightColor(int32 ControllerId, struct FColor Color) override {};
	virtual void ResetLightColor(int32 ControllerId) override {};

	void SetGamepadsAllowed(bool bAllowed) {}
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

