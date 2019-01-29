// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "IInputDevice.h"
#include "IMagicLeapInputDevice.h"
#include "IMotionController.h"
#include "XRMotionControllerBase.h"
#include "IMagicLeapControllerPlugin.h"
#include <Containers/Queue.h>
#include "Misc/ScopeLock.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_input.h>
#include <ml_controller.h>
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
class FMagicLeapController : public IMagicLeapInputDevice, public FXRMotionControllerBase
{
private:
#if WITH_MLSDK
	class FControllerMapper
	{
	friend class FMagicLeapController;
	private:
		TMap<FName, int32> MotionSourceToInputControllerIndex;
		FName InputControllerIndexToMotionSource[MLInput_MaxControllers];
		TMap<EControllerHand, FName> HandToMotionSource;
		TMap<FName, EControllerHand> MotionSourceToHand;
		FCriticalSection CriticalSection;
		EControllerHand DefaultInputControllerIndexToHand[MLInput_MaxControllers];

	protected:
		void UpdateMotionSourceInputIndexPairing(const MLInputControllerState ControllerState[MLInput_MaxControllers]);
	public:
		FControllerMapper();

		void MapHandToMotionSource(EControllerHand Hand, FName MotionSource);

		FName GetMotionSourceForHand(EControllerHand Hand) const;
		EControllerHand GetHandForMotionSource(FName MotionSource) const;

		FName GetMotionSourceForInputControllerIndex(uint8 controller_id) const;
		uint8 GetInputControllerIndexForMotionSource(FName MotionSource) const;

		EControllerHand GetHandForInputControllerIndex(uint8 controller_id) const;
		uint8 GetInputControllerIndexForHand(EControllerHand Hand) const;
		EMLControllerType MotionSourceToControllerType(FName InMotionSource);

		void SwapHands();
	};
#endif //WITH_MLSDK
public:
	FMagicLeapController(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FMagicLeapController();

	/** IMagicLeapInputDevice interface */
	void Tick(float DeltaTime) override;
	void SendControllerEvents() override;
	void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	bool IsGamepadAttached() const override;
	void Enable() override;
	bool SupportsExplicitEnable() const override;
	void Disable() override;

	/** IMotionController interface */
	bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const FName MotionSource) const override;
	FName GetMotionControllerDeviceTypeName() const override;
	void EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const override;

	/** IInputDevice interface */
	void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override { }
	void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override { }

	EMLControllerTrackingMode GetControllerTrackingMode();
	bool SetControllerTrackingMode(EMLControllerTrackingMode TrackingMode);

	void RegisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver);
	void UnregisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver);

	bool PlayLEDPattern(FName MotionSource, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayLEDEffect(FName MotionSource, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayHapticPattern(FName MotionSource, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);

#if WITH_MLSDK
	// Has to be public so button functions can use it
	FControllerMapper ControllerMapper;
#endif //WITH_MLSDK

	bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;
	EMLControllerType GetMLControllerType(EControllerHand Hand) const;
	bool PlayControllerLED(EControllerHand Hand, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);
	bool PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);

private:
	void UpdateTrackerData();
	void UpdateControllerStateFromInputTracker(const class FAppFramework& AppFramework, FName MotionSource);
	void UpdateControllerStateFromControllerTracker(const class FAppFramework& AppFramework, FName MotionSource);
	void AddKeys();
	void ReadConfigParams();
	void InitializeInputCallbacks();
	void SendControllerEventsForHand(EControllerHand Hand);

	TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;
	int32 DeviceIndex;

#if WITH_MLSDK
	MLHandle InputTracker;
	MLHandle ControllerTracker;
	MLInputControllerDof ControllerDof;
	EMLControllerTrackingMode TrackingMode;
	MLInputControllerState InputControllerState[MLInput_MaxControllers];
	MLControllerSystemState ControllerSystemState;
	MLInputControllerCallbacks InputControllerCallbacks;
	TMap<FName, FMagicLeapControllerState> CurrMotionSourceControllerState;
	TMap<FName, FMagicLeapControllerState> PrevMotionSourceControllerState;
#endif //WITH_MLSDK

	bool bIsInputStateValid;

	float TriggerKeyIsConsideredPressed;
	float TriggerKeyIsConsideredReleased;

	TArray<IMagicLeapTouchpadGestures*> TouchpadGestureReceivers;

	TQueue<TPair<FName, bool>> PendingButtonEvents;
};

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapController, Display, All);
