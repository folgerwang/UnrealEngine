// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "IMagicLeapInputDevice.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_gesture.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

#include "MagicLeapGestureTypes.h"
#include "XRMotionControllerBase.h"
#include "InputCoreTypes.h"
#include "AppEventHandler.h"

/**
  * MagicLeap Gestures
  */
class FMagicLeapGestures : public IMagicLeapInputDevice, public FXRMotionControllerBase, public MagicLeap::IAppEventHandler
{
public:
	struct FGestureData
	{
		EStaticGestures Gesture = EStaticGestures::NoHand;
		TArray<FTransform> Keypoints;
		FTransform HandCenter;
		FVector HandCenterNormalized;
		FTransform HandPointer;
		FTransform HandSecondary;
		bool HandCenterValid = false;
		float GestureConfidence = 0.0f;
	};

public:
	FMagicLeapGestures(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FMagicLeapGestures();

	/** IMotionController interface */
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;
	virtual FName GetMotionControllerDeviceTypeName() const override;

	/** IMagicLeapInputDevice interface */
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {};
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override {};
	virtual bool IsGamepadAttached() const override;
	virtual void Enable() override;
	virtual bool SupportsExplicitEnable() const override;
	virtual void Disable() override;

	const FGestureData& CurrentLeftGestureData() const;
	const FGestureData& CurrentRightGestureData() const;
	bool IsGestureStateValid() const;

	bool SetConfiguration(const TArray<EStaticGestures>& StaticGesturesToActivate, EGestureKeypointsFilterLevel KeypointsFilterLevel, EGestureRecognitionFilterLevel GestureFilterLevel, EGestureRecognitionFilterLevel HandSwitchingFilterLevel);
	bool GetConfiguration(TArray<EStaticGestures>& ActiveStaticGestures, EGestureKeypointsFilterLevel& KeypointsFilterLevel, EGestureRecognitionFilterLevel& GestureFilterLevel, EGestureRecognitionFilterLevel& HandSwitchingFilterLevel);

	void SetStaticGestureConfidenceThreshold(EStaticGestures Gesture, float Confidence);
	float GetStaticGestureConfidenceThreshold(EStaticGestures Gesture) const;

private:
	void UpdateTrackerData();
	void AddKeys();
	void ConditionallyEnable();
	void OnAppPause() override;
	void OnAppResume() override;

#if WITH_MLSDK
	static EStaticGestures TranslateGestureEnum(MLGestureStaticHandState HandState);
#endif //WITH_MLSDK

	TSharedPtr<FGenericApplicationMessageHandler> MessageHandler;
	int32 DeviceIndex;

#if WITH_MLSDK
	MLHandle GestureTracker;
	MLGestureData GestureData;
	MLGestureData OldData;
	MLGestureStaticData StaticData;
#endif //WITH_MLSDK

	FGestureData LeftHand;
	FGestureData RightHand;

	bool bIsGestureStateValid;

	TArray<float> StaticGestureConfidenceThresholds;

	TArray<FName> LeftStaticGestureMap;
	TArray<FName> RightStaticGestureMap;

	struct FStaticGestures
	{
		static const FKey Left_Finger;
		static const FKey Left_Fist;
		static const FKey Left_Pinch;
		static const FKey Left_Thumb;
		static const FKey Left_L;
		static const FKey Left_OpenHandBack;
		static const FKey Left_Ok;
		static const FKey Left_C;
		static const FKey Left_NoHand;

		static const FKey Right_Finger;
		static const FKey Right_Fist;
		static const FKey Right_Pinch;
		static const FKey Right_Thumb;
		static const FKey Right_L;
		static const FKey Right_OpenHandBack;
		static const FKey Right_Ok;
		static const FKey Right_C;
		static const FKey Right_NoHand;
	};
};

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapGestures, Display, All);
