// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "IInputDevice.h"
#include "IMagicLeapInputDevice.h"
#include "IHapticDevice.h"
#include "IMotionController.h"
#include "XRMotionControllerBase.h"
#include "IMagicLeapControllerPlugin.h"
#include "Misc/ScopeLock.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_input.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "MagicLeapControllerKeys.h"
#include "MagicLeapInputState.h"

//function to force the linker to include this cpp
void MagicLeapTestReferenceFunction();

class IMagicLeapTouchpadGestures;

/**
 * MagicLeap Motion Controller
 */
class FMagicLeapController : public IMagicLeapInputDevice, public IHapticDevice, public FXRMotionControllerBase
{
public:
	FMagicLeapController(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FMagicLeapController();

	/** IMagicLeapInputDevice interface */
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;
	virtual class IHapticDevice* GetHapticDevice() override;
	virtual bool IsGamepadAttached() const override;
	virtual void Enable() override;
	virtual bool SupportsExplicitEnable() const override;
	virtual void Disable() override;

	/** IHapticDevice interface */
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) override;
	virtual void GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const override;
	virtual float GetHapticAmplitudeScale() const override;

private:
	void InternalSetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values, bool bFromHapticInterface);
public:

	/** IMotionController interface */
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;
	virtual FName GetMotionControllerDeviceTypeName() const override;

	/** FMagicLeapController interface */
	bool IsInputStateValid() const;

	void OnChar(uint32 CharUTF32);
#if WITH_MLSDK
	void OnKeyDown(MLKeyCode KeyCode);
	void OnKeyUp(MLKeyCode KeyCode);
	void OnButtonDown(uint8 ControllerID, MLInputControllerButton Button);
	void OnButtonUp(uint8 ControllerID, MLInputControllerButton Button);
#endif //WITH_MLSDK
	void OnControllerConnected(EControllerHand Hand);
	void OnControllerDisconnected(EControllerHand Hand);

	void SetControllerIsConnected(EControllerHand Hand, bool bConnected);

	bool GetControllerMapping(int32 ControllerIndex, EControllerHand& Hand) const;
	void InvertControllerMapping();
	EMLControllerType GetMLControllerType(EControllerHand Hand) const;
	void CalibrateControllerNow(EControllerHand Hand, const FVector& StartPosition, const FRotator& StartOrientation);

	bool PlayControllerLED(EControllerHand Hand, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);

	void RegisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver);
	void UnregisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver);

private:
	void UpdateTrackerData();
	void SendControllerEventsForHand(EControllerHand Hand);
	void AddKeys();
	void DebouncedButtonMessageHandler(bool NewButtonState, bool OldButtonState, const FName& ButtonName);
#if WITH_MLSDK
	const FName& MagicLeapButtonToUnrealButton(int32 ControllerID, MLInputControllerButton ml_button);
#endif //WITH_MLSDK
	const FName& MagicLeapTouchToUnrealThumbstickAxis(EControllerHand Hand, uint32 TouchIndex);
	const FName& MagicLeapTouchToUnrealThumbstickButton(EControllerHand Hand);
	const FName& MagicLeapTriggerToUnrealTriggerAxis(EControllerHand Hand);
	const FName& MagicLeapTriggerToUnrealTriggerKey(EControllerHand Hand);
#if WITH_MLSDK
	MLInputControllerFeedbackPatternLED UnrealToMLPatternLED(EMLControllerLEDPattern LEDPattern) const;
	MLInputControllerFeedbackEffectLED UnrealToMLEffectLED(EMLControllerLEDEffect LEDEffect) const;
	MLInputControllerFeedbackColorLED UnrealToMLColorLED(EMLControllerLEDColor LEDColor) const;
	MLInputControllerFeedbackEffectSpeedLED UnrealToMLSpeedLED(EMLControllerLEDSpeed LEDSpeed) const;
	MLInputControllerFeedbackPatternVibe UnrealToMLPatternVibe(EMLControllerHapticPattern HapticPattern) const;
	MLInputControllerFeedbackIntensity UnrealToMLHapticIntensity(EMLControllerHapticIntensity HapticIntensity) const;
#endif //WITH_MLSDK
	void InitializeInputCallbacks();

	TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;
	int32 DeviceIndex;

#if WITH_MLSDK
	MLHandle InputTracker;
	MLInputControllerState InputState[MLInput_MaxControllers];
	MLInputControllerState OldInputState[MLInput_MaxControllers];
	MLInputControllerCallbacks InputControllerCallbacks;
	MLInputKeyboardCallbacks InputKeyboardCallbacks;
#endif //WITH_MLSDK

	bool bIsInputStateValid;
	bool bTriggerState;
	bool bTriggerKeyPressing;

	float triggerKeyIsConsideredPressed;
	float triggerKeyIsConsideredReleased;

	FTransform LeftControllerTransform;
	FTransform RightControllerTransform;

	FTransform LeftControllerCalibration;
	FTransform RightControllerCalibration;

	TMap<int32, EControllerHand> ControllerIDToHand;
	TMap<EControllerHand, int32> HandToControllerID;

	TMap<int32, FMagicLeapControllerState> ControllerIDToControllerState;

	struct FMagicLeapKeys
	{
		static const FKey MotionController_Left_Thumbstick_Z;
		static const FKey Left_MoveButton;
		static const FKey Left_AppButton;
		static const FKey Left_HomeButton;

		static const FKey MotionController_Right_Thumbstick_Z;
		static const FKey Right_MoveButton;
		static const FKey Right_AppButton;
		static const FKey Right_HomeButton;
	};

	TArray<IMagicLeapTouchpadGestures*> TouchpadGestureReceivers;

#if WITH_MLSDK
	TArray<MLKeyCode> PendingKeyDowns;
	TArray<MLKeyCode> PendingKeyUps;
#endif //WITH_MLSDK
	TArray<uint32> PendingCharKeys;
	FCriticalSection KeyCriticalSection;

	struct FButtonMap
	{
#if WITH_MLSDK
		MLInputControllerButton Button;
#endif //WITH_MLSDK
		uint8 ControllerID;
		bool bPressed;
	};

	TArray<FButtonMap> PendingButtonStates;
	FCriticalSection ButtonCriticalSection;
};

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapController, Display, All);
