// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapGestures.h"
#include "MagicLeapHMD.h"
#include "MagicLeapMath.h"
#include "Framework/Application/SlateApplication.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"
#include "MagicLeapGestureTypes.h"
#include "IMagicLeapGesturesPlugin.h"

#define LOCTEXT_NAMESPACE "MagicLeapGestures"

class FMagicLeapGesturesPlugin : public IMagicLeapGesturesPlugin
{
public:
	FMagicLeapGesturesPlugin()
		: InputDevice(nullptr)
	{}

	virtual void StartupModule() override
	{
		IMagicLeapGesturesPlugin::StartupModule();

		// HACK: Generic Application might not be instantiated at this point so we create the input device with a
		// dummy message handler. When the Generic Application creates the input device it passes a valid message
		// handler to it which is further on used for all the controller events. This hack fixes issues caused by
		// using a custom input device before the Generic Application has instantiated it. Eg. within BeginPlay()
		//
		// This also fixes the warnings that pop up on the custom input keys when the blueprint loads. Those
		// warnings are caused because Unreal loads the bluerints before the input device has been instantiated
		// and has added its keys, thus leading Unreal to believe that those keys don't exist. This hack causes
		// an earlier instantiation of the input device, and consequently, the custom keys.
		TSharedPtr<FGenericApplicationMessageHandler> DummyMessageHandler(new FGenericApplicationMessageHandler());
		CreateInputDevice(DummyMessageHandler.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		IMagicLeapGesturesPlugin::ShutdownModule();
	}

	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		if (!InputDevice.IsValid())
		{
			TSharedPtr<IInputDevice> GesturesInputDevice(new FMagicLeapGestures(InMessageHandler));
			InputDevice = GesturesInputDevice;
			return InputDevice;
		}
		else
		{
			InputDevice.Get()->SetMessageHandler(InMessageHandler);
			return InputDevice;
		}
		return nullptr;
	}

	virtual TSharedPtr<IInputDevice> GetInputDevice() override
	{
		if (!InputDevice.IsValid())
		{
			InputDevice = CreateInputDevice(FSlateApplication::Get().GetPlatformApplication()->GetMessageHandler());
		}
		return InputDevice;
	}

private:
	TSharedPtr<IInputDevice> InputDevice;
};

IMPLEMENT_MODULE(FMagicLeapGesturesPlugin, MagicLeapGestures);

//////////////////////////////////////////////////////////////////////////

// Left Static Gestures
const FKey FMagicLeapGestures::FStaticGestures::Left_Finger("MagicLeap_Left_Finger");
const FName FMagicLeapGestureKeyNames::Left_Finger_Name("MagicLeap_Left_Finger");

const FKey FMagicLeapGestures::FStaticGestures::Left_Fist("MagicLeap_Left_Fist");
const FName FMagicLeapGestureKeyNames::Left_Fist_Name("MagicLeap_Left_Fist");

const FKey FMagicLeapGestures::FStaticGestures::Left_Pinch("MagicLeap_Left_Pinch");
const FName FMagicLeapGestureKeyNames::Left_Pinch_Name("MagicLeap_Left_Pinch");

const FKey FMagicLeapGestures::FStaticGestures::Left_Thumb("MagicLeap_Left_Thumb");
const FName FMagicLeapGestureKeyNames::Left_Thumb_Name("MagicLeap_Left_Thumb");

const FKey FMagicLeapGestures::FStaticGestures::Left_L("MagicLeap_Left_L");
const FName FMagicLeapGestureKeyNames::Left_L_Name("MagicLeap_Left_L");

const FKey FMagicLeapGestures::FStaticGestures::Left_OpenHandBack("MagicLeap_Left_OpenHandBack");
const FName FMagicLeapGestureKeyNames::Left_OpenHandBack_Name("MagicLeap_Left_OpenHandBack");

const FKey FMagicLeapGestures::FStaticGestures::Left_Ok("MagicLeap_Left_Ok");
const FName FMagicLeapGestureKeyNames::Left_Ok_Name("MagicLeap_Left_Ok");

const FKey FMagicLeapGestures::FStaticGestures::Left_C("MagicLeap_Left_C");
const FName FMagicLeapGestureKeyNames::Left_C_Name("MagicLeap_Left_C");

const FKey FMagicLeapGestures::FStaticGestures::Left_NoHand("MagicLeap_Left_NoHand");
const FName FMagicLeapGestureKeyNames::Left_NoHand_Name("MagicLeap_Left_NoHand");

// Right Static Gestures
const FKey FMagicLeapGestures::FStaticGestures::Right_Finger("MagicLeap_Right_Finger");
const FName FMagicLeapGestureKeyNames::Right_Finger_Name("MagicLeap_Right_Finger");

const FKey FMagicLeapGestures::FStaticGestures::Right_Fist("MagicLeap_Right_Fist");
const FName FMagicLeapGestureKeyNames::Right_Fist_Name("MagicLeap_Right_Fist");

const FKey FMagicLeapGestures::FStaticGestures::Right_Pinch("MagicLeap_Right_Pinch");
const FName FMagicLeapGestureKeyNames::Right_Pinch_Name("MagicLeap_Right_Pinch");

const FKey FMagicLeapGestures::FStaticGestures::Right_Thumb("MagicLeap_Right_Thumb");
const FName FMagicLeapGestureKeyNames::Right_Thumb_Name("MagicLeap_Right_Thumb");

const FKey FMagicLeapGestures::FStaticGestures::Right_L("MagicLeap_Right_L");
const FName FMagicLeapGestureKeyNames::Right_L_Name("MagicLeap_Right_L");

const FKey FMagicLeapGestures::FStaticGestures::Right_OpenHandBack("MagicLeap_Right_OpenHandBack");
const FName FMagicLeapGestureKeyNames::Right_OpenHandBack_Name("MagicLeap_Right_OpenHandBack");

const FKey FMagicLeapGestures::FStaticGestures::Right_Ok("MagicLeap_Right_Ok");
const FName FMagicLeapGestureKeyNames::Right_Ok_Name("MagicLeap_Right_Ok");

const FKey FMagicLeapGestures::FStaticGestures::Right_C("MagicLeap_Right_C");
const FName FMagicLeapGestureKeyNames::Right_C_Name("MagicLeap_Right_C");

const FKey FMagicLeapGestures::FStaticGestures::Right_NoHand("MagicLeap_Right_NoHand");
const FName FMagicLeapGestureKeyNames::Right_NoHand_Name("MagicLeap_Right_NoHand");

#if WITH_MLSDK
EGestureKeypointsFilterLevel MLToUnrealKeypointsFilterLevel(MLGestureFilterLevel FilterLevel)
{
	switch (FilterLevel)
	{
	case MLGestureFilterLevel_0:
		return EGestureKeypointsFilterLevel::NoFilter;
	case MLGestureFilterLevel_1:
		return EGestureKeypointsFilterLevel::SimpleSmoothing;
	case MLGestureFilterLevel_2:
		return EGestureKeypointsFilterLevel::PredictiveSmoothing;
	}

	return EGestureKeypointsFilterLevel::NoFilter;
}

EGestureRecognitionFilterLevel MLToUnrealGestureFilterLevel(MLGestureFilterLevel FilterLevel)
{
	switch (FilterLevel)
	{
	case MLGestureFilterLevel_0:
		return EGestureRecognitionFilterLevel::NoFilter;
	case MLGestureFilterLevel_1:
		return EGestureRecognitionFilterLevel::SlightRobustnessToFlicker;
	case MLGestureFilterLevel_2:
		return EGestureRecognitionFilterLevel::MoreRobustnessToFlicker;
	}

	return EGestureRecognitionFilterLevel::NoFilter;
}

MLGestureFilterLevel UnrealToMLKeypointsFilterLevel(EGestureKeypointsFilterLevel FilterLevel)
{
	switch (FilterLevel)
	{
	case EGestureKeypointsFilterLevel::NoFilter:
		return MLGestureFilterLevel_0;
	case EGestureKeypointsFilterLevel::SimpleSmoothing:
		return MLGestureFilterLevel_1;
	case EGestureKeypointsFilterLevel::PredictiveSmoothing:
		return MLGestureFilterLevel_2;
	}

	return MLGestureFilterLevel_0;
}

MLGestureFilterLevel UnrealToMLGestureFilterLevel(EGestureRecognitionFilterLevel FilterLevel)
{
	switch (FilterLevel)
	{
	case EGestureRecognitionFilterLevel::NoFilter:
		return MLGestureFilterLevel_0;
	case EGestureRecognitionFilterLevel::SlightRobustnessToFlicker:
		return MLGestureFilterLevel_1;
	case EGestureRecognitionFilterLevel::MoreRobustnessToFlicker:
		return MLGestureFilterLevel_2;
	}

	return MLGestureFilterLevel_0;
}
#endif //WITH_MLSDK

FMagicLeapGestures::FMagicLeapGestures(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	: MessageHandler(InMessageHandler)
	, DeviceIndex(0)
#if WITH_MLSDK
	, GestureTracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	, bIsGestureStateValid(false)
{
#if WITH_MLSDK
	// Initialize gesture data to default values.
	GestureData.left_hand_state.static_gesture_category = MLGestureStaticHandState_NoHand;
	GestureData.right_hand_state.static_gesture_category = MLGestureStaticHandState_NoHand;

	StaticGestureConfidenceThresholds.AddZeroed(static_cast<int32>(MLGestureStaticHandState_Count));
#endif //WITH_MLSDK

	// Register "MotionController" modular feature manually
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	AddKeys();

	// We're implicitly requiring that the MagicLeapPlugin has been loaded and
	// initialized at this point.
	auto HMD = IMagicLeapPlugin::Get().GetHMD().Pin();
	if (HMD.IsValid())
	{
		HMD->RegisterMagicLeapInputDevice(this);
	}
}

FMagicLeapGestures::~FMagicLeapGestures()
{
	// Normally, the MagicLeapPlugin will be around during unload,
	// but it isn't an assumption that we should make.
	if (IMagicLeapPlugin::IsAvailable())
	{
		auto HMD = IMagicLeapPlugin::Get().GetHMD().Pin();
		if (HMD.IsValid())
		{
			HMD->UnregisterMagicLeapInputDevice(this);
		}
	}

	Disable();

	// Unregister "MotionController" modular feature manually
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

bool FMagicLeapGestures::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	bool bControllerTracked = false;
	if (ControllerIndex == DeviceIndex)
	{
		if (GetControllerTrackingStatus(ControllerIndex, DeviceHand) != ETrackingStatus::NotTracked)
		{
			// only these are supported by gestures.
			// 1 is Left, 2 is right, then we go to pointers, then to secondarys
			const FTransform* ControllerTransform = nullptr;
			switch (DeviceHand)
			{
			case EControllerHand::Special_1:
				ControllerTransform = &LeftHand.HandCenter;
				break;
			case EControllerHand::Special_3:
				ControllerTransform = &LeftHand.HandPointer;
				break;
			case EControllerHand::Special_5:
				ControllerTransform = &LeftHand.HandSecondary;
				break;
			case EControllerHand::Special_2:
				ControllerTransform = &RightHand.HandCenter;
				break;
			case EControllerHand::Special_4:
				ControllerTransform = &RightHand.HandPointer;
				break;
			case EControllerHand::Special_6:
				ControllerTransform = &RightHand.HandSecondary;
				break;
			default:
				check(false);
			}

			check(ControllerTransform);
			OutPosition = ControllerTransform->GetLocation();
			OutOrientation = ControllerTransform->GetRotation().Rotator();

			bControllerTracked = true;
		}
	}

	return bControllerTracked;
}

ETrackingStatus FMagicLeapGestures::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	ETrackingStatus status = ETrackingStatus::NotTracked;
	if (bIsGestureStateValid)
	{
		if (DeviceHand == EControllerHand::Special_1 || DeviceHand == EControllerHand::Special_3 || DeviceHand == EControllerHand::Special_5)
		{
			if (LeftHand.Gesture != EStaticGestures::NoHand && LeftHand.HandCenterValid)
			{
				status = ETrackingStatus::Tracked;
			}
		}
		else if (DeviceHand == EControllerHand::Special_2 || DeviceHand == EControllerHand::Special_4 || DeviceHand == EControllerHand::Special_6)
		{
			if (RightHand.Gesture != EStaticGestures::NoHand && RightHand.HandCenterValid)
			{
				status = ETrackingStatus::Tracked;
			}
		}
	}

	UE_LOG(LogMagicLeapGestures, Log, TEXT("FMagicLeapGestures::GetControllerTrackingStatus %i"), (int32)status);
	return status;
}

FName FMagicLeapGestures::GetMotionControllerDeviceTypeName() const
{
	const static FName DefaultName(TEXT("MagicLeapGesture"));
	return DefaultName;
}

void FMagicLeapGestures::Tick(float DeltaTime)
{
	UpdateTrackerData();
}

void FMagicLeapGestures::SendControllerEvents()
{
#if WITH_MLSDK
	if (bIsGestureStateValid)
	{
		// Left Hand
		int32 gestureIndex = static_cast<int32>(GestureData.left_hand_state.static_gesture_category);
		if (GestureData.left_hand_state.static_gesture_category != OldData.left_hand_state.static_gesture_category)
		{
			FMagicLeapHMD::EnableInput EnableInputFromHMD;
			// fixes unreferenced parameter error for Windows package builds.
			(void)EnableInputFromHMD;
			MessageHandler->OnControllerButtonReleased(LeftStaticGestureMap[static_cast<int32>(OldData.left_hand_state.static_gesture_category)], DeviceIndex, false);
			if (StaticGestureConfidenceThresholds[gestureIndex] <= GestureData.left_hand_state.gesture_confidence)
			{
				MessageHandler->OnControllerButtonPressed(LeftStaticGestureMap[gestureIndex], DeviceIndex, false);
			}
		}
		else if (OldData.left_hand_state.gesture_confidence < StaticGestureConfidenceThresholds[gestureIndex] && GestureData.left_hand_state.gesture_confidence >= StaticGestureConfidenceThresholds[gestureIndex])
		{
			FMagicLeapHMD::EnableInput EnableInputFromHMD;
			// fixes unreferenced parameter error for Windows package builds.
			(void)EnableInputFromHMD;
			MessageHandler->OnControllerButtonPressed(LeftStaticGestureMap[gestureIndex], DeviceIndex, false);
		}

		// Right Hand
		gestureIndex = static_cast<int32>(GestureData.right_hand_state.static_gesture_category);
		if (GestureData.right_hand_state.static_gesture_category != OldData.right_hand_state.static_gesture_category)
		{
			FMagicLeapHMD::EnableInput EnableInputFromHMD;
			// fixes unreferenced parameter error for Windows package builds.
			(void)EnableInputFromHMD;
			MessageHandler->OnControllerButtonReleased(RightStaticGestureMap[OldData.right_hand_state.static_gesture_category], DeviceIndex, false);
			if (StaticGestureConfidenceThresholds[gestureIndex] <= GestureData.right_hand_state.gesture_confidence)
			{
				MessageHandler->OnControllerButtonPressed(RightStaticGestureMap[gestureIndex], DeviceIndex, false);
			}
		}
		else if (OldData.right_hand_state.gesture_confidence < StaticGestureConfidenceThresholds[gestureIndex] && GestureData.right_hand_state.gesture_confidence >= StaticGestureConfidenceThresholds[gestureIndex])
		{
			FMagicLeapHMD::EnableInput EnableInputFromHMD;
			// fixes unreferenced parameter error for Windows package builds.
			(void)EnableInputFromHMD;
			MessageHandler->OnControllerButtonPressed(RightStaticGestureMap[gestureIndex], DeviceIndex, false);
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapGestures::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FMagicLeapGestures::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

bool FMagicLeapGestures::IsGamepadAttached() const
{
#if WITH_MLSDK
	return MLHandleIsValid(GestureTracker);
#else
	return false;
#endif //WITH_MLSDK
}

void FMagicLeapGestures::Enable()
{
	checkf(false, TEXT("FMagicLeapGestures::Enable is not supported! Check 'SupportsExplicitEnable()' first!"));
}

bool FMagicLeapGestures::SupportsExplicitEnable() const
{
	return false;
}

void FMagicLeapGestures::Disable()
{
#if WITH_MLSDK
	if (MLHandleIsValid(GestureTracker))
	{
		if (MLGestureTrackingDestroy(GestureTracker))
		{
			GestureTracker = ML_INVALID_HANDLE;
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Error destroying gesture tracker."));
		}
	}
#endif
}

const FMagicLeapGestures::FGestureData& FMagicLeapGestures::CurrentLeftGestureData() const
{
	return LeftHand;
}

const FMagicLeapGestures::FGestureData& FMagicLeapGestures::CurrentRightGestureData() const
{
	return RightHand;
}

bool FMagicLeapGestures::IsGestureStateValid() const
{
	return bIsGestureStateValid;
}

bool FMagicLeapGestures::SetConfiguration(const TArray<EStaticGestures>& StaticGesturesToActivate, EGestureKeypointsFilterLevel KeypointsFilterLevel, EGestureRecognitionFilterLevel GestureFilterLevel, EGestureRecognitionFilterLevel HandSwitchingFilterLevel)
{
#if WITH_MLSDK
	ConditionallyEnable();

	if (!MLHandleIsValid(GestureTracker))
	{
		return false;
	}

	MLGestureConfiguration config;
	for (uint32 i = 0; i < MLGestureStaticHandState_Count; ++i)
	{
		config.static_gestures_config[i] = false;
	}

	bool enableGesturePipeline = false;

	for (const EStaticGestures& staticGesture : StaticGesturesToActivate)
	{
		if (staticGesture <= EStaticGestures::NoHand)
		{
			config.static_gestures_config[static_cast<uint32>(staticGesture)] = true;
			enableGesturePipeline = true;
		}
	}

	config.gesture_pipeline_enabled = enableGesturePipeline;
	config.keypoints_filter_level = UnrealToMLKeypointsFilterLevel(KeypointsFilterLevel);
	config.pose_filter_level = UnrealToMLGestureFilterLevel(GestureFilterLevel);
	config.handtype_filter_level = UnrealToMLGestureFilterLevel(HandSwitchingFilterLevel);

	return MLGestureSetConfiguration(GestureTracker, &config);
#else
	return false;
#endif //WITH_MLSDK
}

bool FMagicLeapGestures::GetConfiguration(TArray<EStaticGestures>& ActiveStaticGestures, EGestureKeypointsFilterLevel& KeypointsFilterLevel, EGestureRecognitionFilterLevel& GestureFilterLevel, EGestureRecognitionFilterLevel& HandSwitchingFilterLevel)
{
#if WITH_MLSDK
	ConditionallyEnable();

	if (!MLHandleIsValid(GestureTracker))
	{
		return false;
	}

	MLGestureConfiguration config;
	bool validResult = MLGestureGetConfiguration(GestureTracker, &config);

	if (validResult)
	{
		ActiveStaticGestures.Empty(MLGestureStaticHandState_Count);
		for (uint32 i = 0; i < MLGestureStaticHandState_Count; ++i)
		{
			if (config.static_gestures_config[i])
			{
				ActiveStaticGestures.Add(static_cast<EStaticGestures>(i));
			}
		}

		KeypointsFilterLevel = MLToUnrealKeypointsFilterLevel(config.keypoints_filter_level);
		GestureFilterLevel = MLToUnrealGestureFilterLevel(config.pose_filter_level);
		HandSwitchingFilterLevel = MLToUnrealGestureFilterLevel(config.handtype_filter_level);
	}

	return validResult;
#else
	return false;
#endif //WITH_MLSDK

}

void FMagicLeapGestures::SetStaticGestureConfidenceThreshold(EStaticGestures Gesture, float Confidence)
{
	if (Gesture <= EStaticGestures::NoHand)
	{
		StaticGestureConfidenceThresholds[static_cast<int32>(Gesture)] = Confidence;
	}
}

float FMagicLeapGestures::GetStaticGestureConfidenceThreshold(EStaticGestures Gesture) const
{
	if (Gesture <= EStaticGestures::NoHand)
	{
		return StaticGestureConfidenceThresholds[static_cast<int32>(Gesture)];
	}
	return 0.0f;
}

void FMagicLeapGestures::UpdateTrackerData()
{
#if WITH_MLSDK
	if (MLHandleIsValid(GestureTracker))
	{
		OldData = GestureData;
		bIsGestureStateValid = MLGestureGetData(GestureTracker, &GestureData);
	}
	else
	{
		bIsGestureStateValid = false;
	}

	if (bIsGestureStateValid)
	{
		const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
		check(AppFramework.IsInitialized());

		EFailReason FailReason = EFailReason::None;
		LeftHand.Gesture = TranslateGestureEnum(GestureData.left_hand_state.static_gesture_category);
		LeftHand.HandCenterValid = (GestureData.left_hand_state.static_gesture_category != MLGestureStaticHandState_NoHand);
		if (LeftHand.HandCenterValid)
		{
			bool bResult = AppFramework.GetTransform(StaticData.left_hand_center, LeftHand.HandCenter, FailReason);
			if (!bResult)
			{
				LeftHand.HandCenterValid = false;
				if (FailReason == EFailReason::NaNsInTransform)
				{
					UE_LOG(LogMagicLeapGestures, Error, TEXT("NaNs in left hand center transform."));
				}
			}
		}

		RightHand.Gesture = TranslateGestureEnum(GestureData.right_hand_state.static_gesture_category);
		RightHand.HandCenterValid = (GestureData.right_hand_state.static_gesture_category != MLGestureStaticHandState_NoHand);
		if (RightHand.HandCenterValid)
		{
			bool bResult = AppFramework.GetTransform(StaticData.right_hand_center, RightHand.HandCenter, FailReason);
			if (!bResult)
			{
				RightHand.HandCenterValid = false;
				if (FailReason == EFailReason::NaNsInTransform)
				{
					UE_LOG(LogMagicLeapGestures, Error, TEXT("NaNs in right hand center transform."));
				}
			}
		}

		LeftHand.HandCenterNormalized = MagicLeap::ToFVector(GestureData.left_hand_state.hand_center_normalized, 1.0f);
		RightHand.HandCenterNormalized = MagicLeap::ToFVector(GestureData.right_hand_state.hand_center_normalized, 1.0f);

		if (LeftHand.HandCenterNormalized.ContainsNaN())
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("LeftHand.HandCenterNormalized received NaNs from the device. Zeroing out the vector."));
			LeftHand.HandCenterNormalized = FVector::ZeroVector;
		}
		if (RightHand.HandCenterNormalized.ContainsNaN())
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("RightHand.HandCenterNormalized received NaNs from the device. Zeroing out the vector."));
			RightHand.HandCenterNormalized = FVector::ZeroVector;
		}

		LeftHand.Keypoints.Empty(GestureData.left_hand_state.num_key_points);
		for (uint32 i = 0; i < GestureData.left_hand_state.num_key_points; ++i)
		{
			FTransform LeftHandKeypointPose;
			bool bResult = AppFramework.GetTransform(StaticData.left_frame[i], LeftHandKeypointPose, FailReason);
			if (bResult)
			{
				LeftHand.Keypoints.Add(LeftHandKeypointPose);
			}
			else if (FailReason == EFailReason::NaNsInTransform)
			{
				UE_LOG(LogMagicLeapGestures, Error, TEXT("NaNs in left keypoint [%d] transform."), i);
			}
		}	
		LeftHand.HandPointer = LeftHand.Keypoints.Num() > 0 ? LeftHand.Keypoints[0] : LeftHand.HandCenter;
		LeftHand.HandSecondary = LeftHand.Keypoints.Num() > 1 ? LeftHand.Keypoints[1] : LeftHand.HandCenter;

		RightHand.Keypoints.Empty(GestureData.right_hand_state.num_key_points);
		for (uint32 i = 0; i < GestureData.right_hand_state.num_key_points; ++i)
		{
			FTransform RightHandKeypointPose;
			bool bResult = AppFramework.GetTransform(StaticData.right_frame[i], RightHandKeypointPose, FailReason);
			if (bResult)
			{
				RightHand.Keypoints.Add(RightHandKeypointPose);
			}
			else if (FailReason == EFailReason::NaNsInTransform)
			{
				UE_LOG(LogMagicLeapGestures, Error, TEXT("NaNs in right keypoint [%d] transform."), i);
			}
		}
		RightHand.HandPointer = RightHand.Keypoints.Num() > 0 ? RightHand.Keypoints[0] : RightHand.HandCenter;
		RightHand.HandSecondary = RightHand.Keypoints.Num() > 1 ? RightHand.Keypoints[1] : RightHand.HandCenter;

		LeftHand.GestureConfidence = GestureData.left_hand_state.gesture_confidence;
		RightHand.GestureConfidence = GestureData.right_hand_state.gesture_confidence;
	}
#endif //WITH_MLSDK
}

void FMagicLeapGestures::AddKeys()
{
	// Left Static Gestures
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_Finger, LOCTEXT("MagicLeap_Left_Finger", "ML Left Finger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_Fist, LOCTEXT("MagicLeap_Left_Fist", "ML Left Fist"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_Pinch, LOCTEXT("MagicLeap_Left_Pinch", "ML Left Pinch"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_Thumb, LOCTEXT("MagicLeap_Left_Thumb", "ML Left Thumb"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_L, LOCTEXT("MagicLeap_Left_L", "ML Left L"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_OpenHandBack, LOCTEXT("MagicLeap_Left_OpenHandBack", "ML Left Open Hand Back"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_Ok, LOCTEXT("MagicLeap_Left_Ok", "ML Left Ok"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_C, LOCTEXT("MagicLeap_Left_C", "ML Left C"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Left_NoHand, LOCTEXT("MagicLeap_Left_NoHand", "ML Left No Hand"), FKeyDetails::GamepadKey));

	// Right Static Gestures
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_Finger, LOCTEXT("MagicLeap_Right_Finger", "ML Right Finger"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_Fist, LOCTEXT("MagicLeap_Right_Fist", "ML Right Fist"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_Pinch, LOCTEXT("MagicLeap_Right_Pinch", "ML Right Pinch"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_Thumb, LOCTEXT("MagicLeap_Right_Thumb", "ML Right Thumb"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_L, LOCTEXT("MagicLeap_Right_L", "ML Right L"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_OpenHandBack, LOCTEXT("MagicLeap_Right_OpenHandBack", "ML Right Open Hand Back"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_Ok, LOCTEXT("MagicLeap_Right_Ok", "ML Right Ok"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_C, LOCTEXT("MagicLeap_Right_C", "ML Right C"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FStaticGestures::Right_NoHand, LOCTEXT("MagicLeap_Right_NoHand", "ML Right No Hand"), FKeyDetails::GamepadKey));

	// Create mapping of static and dynamic gesture enums to their key names.

#if WITH_MLSDK
	LeftStaticGestureMap.AddDefaulted(static_cast<int32>(MLGestureStaticHandState_Count));

	RightStaticGestureMap.AddDefaulted(static_cast<int32>(MLGestureStaticHandState_Count));

	// Left Static Gestures
	LeftStaticGestureMap[MLGestureStaticHandState_Finger] = FMagicLeapGestureKeyNames::Left_Finger_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_Fist] = FMagicLeapGestureKeyNames::Left_Fist_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_Pinch] = FMagicLeapGestureKeyNames::Left_Pinch_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_Thumb] = FMagicLeapGestureKeyNames::Left_Thumb_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_L] = FMagicLeapGestureKeyNames::Left_L_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_OpenHandBack] = FMagicLeapGestureKeyNames::Left_OpenHandBack_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_Ok] = FMagicLeapGestureKeyNames::Left_Ok_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_C] = FMagicLeapGestureKeyNames::Left_C_Name;
	LeftStaticGestureMap[MLGestureStaticHandState_NoHand] = FMagicLeapGestureKeyNames::Left_NoHand_Name;

	// Right Static Gestures
	RightStaticGestureMap[MLGestureStaticHandState_Finger] = FMagicLeapGestureKeyNames::Right_Finger_Name;
	RightStaticGestureMap[MLGestureStaticHandState_Fist] = FMagicLeapGestureKeyNames::Right_Fist_Name;
	RightStaticGestureMap[MLGestureStaticHandState_Pinch] = FMagicLeapGestureKeyNames::Right_Pinch_Name;
	RightStaticGestureMap[MLGestureStaticHandState_Thumb] = FMagicLeapGestureKeyNames::Right_Thumb_Name;
	RightStaticGestureMap[MLGestureStaticHandState_L] = FMagicLeapGestureKeyNames::Right_L_Name;
	RightStaticGestureMap[MLGestureStaticHandState_OpenHandBack] = FMagicLeapGestureKeyNames::Right_OpenHandBack_Name;
	RightStaticGestureMap[MLGestureStaticHandState_Ok] = FMagicLeapGestureKeyNames::Right_Ok_Name;
	RightStaticGestureMap[MLGestureStaticHandState_C] = FMagicLeapGestureKeyNames::Right_C_Name;
	RightStaticGestureMap[MLGestureStaticHandState_NoHand] = FMagicLeapGestureKeyNames::Right_NoHand_Name;
#endif //WITH_MLSDK

}

#if WITH_MLSDK
EStaticGestures FMagicLeapGestures::TranslateGestureEnum(MLGestureStaticHandState HandState)
{
	switch (HandState)
	{
	case MLGestureStaticHandState_Finger:
		return EStaticGestures::Finger;
	case MLGestureStaticHandState_Fist:
		return EStaticGestures::Fist;
	case MLGestureStaticHandState_Pinch:
		return EStaticGestures::Pinch;
	case MLGestureStaticHandState_Thumb:
		return EStaticGestures::Thumb;
	case MLGestureStaticHandState_L:
		return EStaticGestures::L;
	case MLGestureStaticHandState_OpenHandBack:
		return EStaticGestures::OpenHandBack;
	case MLGestureStaticHandState_Ok:
		return EStaticGestures::Ok;
	case MLGestureStaticHandState_C:
		return EStaticGestures::C;
	case MLGestureStaticHandState_NoHand:
		return EStaticGestures::NoHand;
	default:
		check(false);
		return EStaticGestures::NoHand;
	}
}
#endif //WITH_MLSDK

void FMagicLeapGestures::ConditionallyEnable()
{
#if WITH_MLSDK
	if (!MLHandleIsValid(GestureTracker) && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
	{
		GestureTracker = MLGestureTrackingCreate();

		if (MLHandleIsValid(GestureTracker))
		{
			MLGestureGetStaticData(GestureTracker, &StaticData);
		}
		else
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Error creating gesture tracker."));
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapGestures::OnAppPause()
{
#if WITH_MLSDK
	if (!MLHandleIsValid(GestureTracker))
	{
		UE_LOG(LogMagicLeapGestures, Error, TEXT("Gesture tracker was invalid on application pause."));
	}
	else
	{
		MLGestureConfiguration GestureConfig;

		if (!MLGestureGetConfiguration(GestureTracker, &GestureConfig))
		{
			UE_LOG(LogMagicLeapGestures, Error, TEXT("Failed to retrieve gesture tracking configuration on application pause."));
		}
		else
		{
			bWasSystemEnabledOnPause = GestureConfig.gesture_pipeline_enabled;

			if (!bWasSystemEnabledOnPause)
			{
				UE_LOG(LogMagicLeapGestures, Log, TEXT("Gesture tracking was not enabled at time of application pause."));
			}
			else
			{
				GestureConfig.gesture_pipeline_enabled = false;

				if (!MLGestureSetConfiguration(GestureTracker, &GestureConfig))
				{
					UE_LOG(LogMagicLeapGestures, Error, TEXT("Failed to disable gesture tracking on application pause."));
				}
				else
				{
					UE_LOG(LogMagicLeapGestures, Log, TEXT("Gesture tracking paused until app resumes."));
				}
			}
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapGestures::OnAppResume()
{
#if WITH_MLSDK
	if (!MLHandleIsValid(GestureTracker))
	{
		UE_LOG(LogMagicLeapGestures, Error, TEXT("Gesture tracker was invalid on application resume."));
	}
	else
	{
		MLGestureConfiguration GestureConfig;

		if (!bWasSystemEnabledOnPause)
		{
			UE_LOG(LogMagicLeapGestures, Log, TEXT("Not resuming gesture tracking as it was not enabled at time of application pause."));
		}
		else
		{
			if (!MLGestureGetConfiguration(GestureTracker, &GestureConfig))
			{
				UE_LOG(LogMagicLeapGestures, Error, TEXT("Failed to retrieve gesture tracking configuration on application resume."));
			}
			else
			{
				GestureConfig.gesture_pipeline_enabled = true;

				if (!MLGestureSetConfiguration(GestureTracker, &GestureConfig))
				{
					UE_LOG(LogMagicLeapGestures, Error, TEXT("Failed to re-enable gesture tracking on application resume."));
				}
				else
				{
					UE_LOG(LogMagicLeapGestures, Log, TEXT("Gesture tracking resumed."));
				}
			}
		}
	}
#endif //WITH_MLSDK
}

#undef LOCTEXT_NAMESPACE
