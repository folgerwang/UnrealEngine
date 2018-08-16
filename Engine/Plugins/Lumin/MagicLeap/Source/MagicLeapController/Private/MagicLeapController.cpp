// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapController.h"
#include "IMagicLeapControllerPlugin.h"
#include "IMagicLeapPlugin.h"
#include "MagicLeapHMD.h"
#include "MagicLeapMath.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Engine/Engine.h"
#include "MagicLeapControllerKeys.h"
#include "Framework/Application/SlateApplication.h"
#include "Async/Async.h"
#include "AppFramework.h"
#include "MagicLeapPluginUtil.h"
#include "TouchpadGesturesComponent.h"
#include "AssetData.h"

#define LOCTEXT_NAMESPACE "MagicLeapController"

//@TODO - would be easier to make sure the Epic enum matches the ML enum if there was a MLInputControllerFeedbackPatternVibe_MAX = MLInputControllerFeedbackPatternVibe_SecondForceDown
//for now this just guards against insertions
#if WITH_MLSDK
static_assert((uint32)EMLControllerHapticPattern::SecondForceDown == (uint32)MLInputControllerFeedbackPatternVibe_SecondForceDown, "Enum entries must match.");
#endif //WITH_MLSDK


class FMagicLeapControllerPlugin : public IMagicLeapControllerPlugin
{
public:

	virtual void StartupModule() override
	{
		IMagicLeapControllerPlugin::StartupModule();

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

		// Ideally, we should be able to query GetDefault<UMagicLeapSettings>()->bEnableZI directly.
		// Unfortunately, the UObject system hasn't finished initialization when this module has been loaded.
		bool bIsVDZIEnabled = false;
		GConfig->GetBool(TEXT("/Script/MagicLeap.MagicLeapSettings"), TEXT("bEnableZI"), bIsVDZIEnabled, GEngineIni);

		APISetup.Startup(bIsVDZIEnabled);
#if WITH_MLSDK
		APISetup.LoadDLL(TEXT("ml_input"));
#endif
	}

	virtual void ShutdownModule() override
	{
		APISetup.Shutdown();
		IMagicLeapControllerPlugin::ShutdownModule();
	}

	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		if (!InputDevice.IsValid())
		{
			TSharedPtr<IInputDevice> MagicLeapController(new FMagicLeapController(InMessageHandler));
			InputDevice = MagicLeapController;
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
	FMagicLeapAPISetup APISetup;
	TSharedPtr<IInputDevice> InputDevice;
};

IMPLEMENT_MODULE(FMagicLeapControllerPlugin, MagicLeapController);

//////////////////////////////////////////////////////////////////////////

const FKey FMagicLeapController::FMagicLeapKeys::MotionController_Left_Thumbstick_Z("MotionController_Left_Thumbstick_Z");
const FName FMagicLeapControllerKeyNames::MotionController_Left_Thumbstick_Z_Name("MotionController_Left_Thumbstick_Z");

const FKey FMagicLeapController::FMagicLeapKeys::Left_MoveButton("MagicLeap_Left_MoveButton");
const FName FMagicLeapControllerKeyNames::Left_MoveButton_Name("MagicLeap_Left_MoveButton");

const FKey FMagicLeapController::FMagicLeapKeys::Left_AppButton("MagicLeap_Left_AppButton");
const FName FMagicLeapControllerKeyNames::Left_AppButton_Name("MagicLeap_Left_AppButton");

const FKey FMagicLeapController::FMagicLeapKeys::Left_HomeButton("MagicLeap_Left_HomeButton");
const FName FMagicLeapControllerKeyNames::Left_HomeButton_Name("MagicLeap_Left_HomeButton");

const FKey FMagicLeapController::FMagicLeapKeys::MotionController_Right_Thumbstick_Z("MotionController_Right_Thumbstick_Z");
const FName FMagicLeapControllerKeyNames::MotionController_Right_Thumbstick_Z_Name("MotionController_Right_Thumbstick_Z");

const FKey FMagicLeapController::FMagicLeapKeys::Right_MoveButton("MagicLeap_Right_MoveButton");
const FName FMagicLeapControllerKeyNames::Right_MoveButton_Name("MagicLeap_Right_MoveButton");

const FKey FMagicLeapController::FMagicLeapKeys::Right_AppButton("MagicLeap_Right_AppButton");
const FName FMagicLeapControllerKeyNames::Right_AppButton_Name("MagicLeap_Right_AppButton");

const FKey FMagicLeapController::FMagicLeapKeys::Right_HomeButton("MagicLeap_Right_HomeButton");
const FName FMagicLeapControllerKeyNames::Right_HomeButton_Name("MagicLeap_Right_HomeButton");

FMagicLeapController::FMagicLeapController(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	: MessageHandler(InMessageHandler)
	, DeviceIndex(0)  // Input Controller Index for Unreal is hardcoded to 0. Ideally it should be incremented for each registered InputDevice.
#if WITH_MLSDK
	, InputTracker(ML_INVALID_HANDLE)
	, ControllerTracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	, bIsInputStateValid(false)
	, bTriggerState(false)
	, bTriggerKeyPressing(false)
	, triggerKeyIsConsideredPressed(80.0f)
	, triggerKeyIsConsideredReleased(20.0f)
{
	InitializeInputCallbacks();

	//hack call to empty function to force the automation tests to compile in
	MagicLeapTestReferenceFunction();

	int32 ControllerIndex = 0;
	// Make Right hand the default hand (Controller index 0), for backwards compatibility with the previous versions.
	ControllerIDToHand.Add(ControllerIndex++, EControllerHand::Right);
	ControllerIDToHand.Add(ControllerIndex++, EControllerHand::Left);
#if WITH_MLSDK
	check(ControllerIndex <= MLInput_MaxControllers);
#endif //WITH_MLSDK

	// Make Right hand the default hand (Controller index 0), for backwards compatibility with the previous versions.
	ControllerIndex = 0;
	HandToControllerID.Add(EControllerHand::Right, ControllerIndex++);
	HandToControllerID.Add(EControllerHand::Left, ControllerIndex++);
#if WITH_MLSDK
	check(ControllerIndex <= MLInput_MaxControllers);
#endif //WITH_MLSDK

	// Shadow state for the controllers
	ControllerIndex = 0;
	ControllerIDToControllerState.Add(ControllerIndex++, FMagicLeapControllerState(EControllerHand::Right));
	ControllerIDToControllerState.Add(ControllerIndex++, FMagicLeapControllerState(EControllerHand::Left));
#if WITH_MLSDK
	check(ControllerIndex <= MLInput_MaxControllers);
#endif //WITH_MLSDK

	if (GConfig)
	{
		float floatValueReceived = 0.0f;
		GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("TriggerKeyIsConsideredPressed"), floatValueReceived, GInputIni);
		triggerKeyIsConsideredPressed = floatValueReceived;

		GConfig->GetFloat(TEXT("/Script/Engine.InputSettings"), TEXT("TriggerKeyIsConsideredReleased"), floatValueReceived, GInputIni);
		triggerKeyIsConsideredReleased = floatValueReceived;
	}

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

FMagicLeapController::~FMagicLeapController()
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

	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

void FMagicLeapController::OnChar(uint32 CharUTF32)
{
	FMagicLeapHMD::EnableInput EnableInputFromHMD;
	// fixes unreferenced parameter error for Windows package builds.
	(void)EnableInputFromHMD;
	MessageHandler->OnKeyChar(CharUTF32, false);
}

#if WITH_MLSDK
void FMagicLeapController::OnKeyDown(MLKeyCode KeyCode)
{
	FKey pressedKey = FInputKeyManager::Get().GetKeyFromCodes(static_cast<uint32>(KeyCode), static_cast<uint32>(KeyCode));
	if (pressedKey != EKeys::Invalid)
	{
		FMagicLeapHMD::EnableInput EnableInputFromHMD;
		// fixes unreferenced parameter error for Windows package builds.
		(void)EnableInputFromHMD;
		MessageHandler->OnControllerButtonPressed(pressedKey.GetFName(), DeviceIndex, false);
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("Key not supported in Unreal. Keycode = %d"), static_cast<uint32>(KeyCode));
	}
}

void FMagicLeapController::OnKeyUp(MLKeyCode KeyCode)
{
	FKey pressedKey = FInputKeyManager::Get().GetKeyFromCodes(static_cast<uint32>(KeyCode), static_cast<uint32>(KeyCode));
	if (pressedKey != EKeys::Invalid)
	{
		FMagicLeapHMD::EnableInput EnableInputFromHMD;
		// fixes unreferenced parameter error for Windows package builds.
		(void)EnableInputFromHMD;
		MessageHandler->OnControllerButtonReleased(pressedKey.GetFName(), DeviceIndex, false);
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("Key not supported in Unreal. Keycode = %d"), static_cast<uint32>(KeyCode));
	}
}

#define TOUCH_GESTURE_CASE(x) case MLInputControllerTouchpadGestureType_##x: { return EMagicLeapTouchpadGestureType::x; }

EMagicLeapTouchpadGestureType MLToUnrealTouchpadGestureType(MLInputControllerTouchpadGestureType GestureType)
{
	switch (GestureType)
	{
		TOUCH_GESTURE_CASE(Tap)
		TOUCH_GESTURE_CASE(ForceTapDown)
		TOUCH_GESTURE_CASE(ForceTapUp)
		TOUCH_GESTURE_CASE(ForceDwell)
		TOUCH_GESTURE_CASE(SecondForceDown)
		TOUCH_GESTURE_CASE(LongHold)
		TOUCH_GESTURE_CASE(RadialScroll)
		TOUCH_GESTURE_CASE(Swipe)
		TOUCH_GESTURE_CASE(Scroll)
		TOUCH_GESTURE_CASE(Pinch)
	}
	return EMagicLeapTouchpadGestureType::None;
}

#define TOUCH_DIR_CASE(x) case MLInputControllerTouchpadGestureDirection_##x: { return EMagicLeapTouchpadGestureDirection::x; }

EMagicLeapTouchpadGestureDirection MLToUnrealTouchpadGestureDirection(MLInputControllerTouchpadGestureDirection Direction)
{
	switch (Direction)
	{
		TOUCH_DIR_CASE(Up)
		TOUCH_DIR_CASE(Down)
		TOUCH_DIR_CASE(Left)
		TOUCH_DIR_CASE(Right)
		TOUCH_DIR_CASE(In)
		TOUCH_DIR_CASE(Out)
		TOUCH_DIR_CASE(Clockwise)
		TOUCH_DIR_CASE(CounterClockwise)
	}
	return EMagicLeapTouchpadGestureDirection::None;
}

FMagicLeapTouchpadGesture MLToUnrealTouchpadGesture(EControllerHand hand, const MLInputControllerTouchpadGesture& touchpad_gesture)
{
	FMagicLeapTouchpadGesture gesture;
	gesture.Hand = hand;
	gesture.Type = MLToUnrealTouchpadGestureType(touchpad_gesture.type);
	gesture.Direction = MLToUnrealTouchpadGestureDirection(touchpad_gesture.direction);
	gesture.PositionAndForce = FVector(touchpad_gesture.pos_and_force.x, touchpad_gesture.pos_and_force.y, touchpad_gesture.pos_and_force.z);
	gesture.Speed = touchpad_gesture.speed;
	gesture.Distance = touchpad_gesture.distance;
	gesture.FingerGap = touchpad_gesture.finger_gap;
	gesture.Radius = touchpad_gesture.radius;
	gesture.Angle = touchpad_gesture.angle;

	return gesture;
}

void FMagicLeapController::OnButtonDown(uint8 ControllerID, MLInputControllerButton Button)
{
	FMagicLeapHMD::EnableInput EnableInputFromHMD;
	// fixes unreferenced parameter error for Windows package builds.
	(void)EnableInputFromHMD;
	MessageHandler->OnControllerButtonPressed(MagicLeapButtonToUnrealButton(ControllerID, Button), DeviceIndex, false);
}

void FMagicLeapController::OnButtonUp(uint8 ControllerID, MLInputControllerButton Button)
{
	FMagicLeapHMD::EnableInput EnableInputFromHMD;
	// fixes unreferenced parameter error for Windows package builds.
	(void)EnableInputFromHMD;
	MessageHandler->OnControllerButtonReleased(MagicLeapButtonToUnrealButton(ControllerID, Button), DeviceIndex, false);
}
#endif //WITH_MLSDK

void FMagicLeapController::OnControllerConnected(EControllerHand Hand)
{
	UE_LOG(LogCore, Log, TEXT("CONTROLLER CONNECTED"));
	SetControllerIsConnected(Hand, true);
}

void FMagicLeapController::OnControllerDisconnected(EControllerHand Hand)
{
	UE_LOG(LogCore, Log, TEXT("CONTROLLER DISCONNECTED"));
	SetControllerIsConnected(Hand, false);
}

void FMagicLeapController::SetControllerIsConnected(EControllerHand Hand, bool bConnected)
{
#if WITH_MLSDK
	const int32* ControllerIndex = HandToControllerID.Find((EControllerHand)Hand);
	if (ControllerIndex != nullptr)
	{
		//get the shadow state from the controller id
		FMagicLeapControllerState* ControllerState = ControllerIDToControllerState.Find(*ControllerIndex);
		if (ControllerState != nullptr)
		{
			ControllerState->bIsConnected = bConnected;

			//default to no tracking
			ControllerState->bIsOrientationTracked = false;
			ControllerState->bIsPositionTracked = false;

			switch (InputState[*ControllerIndex].dof)
			{
			case MLInputControllerDof_3:
				ControllerState->bIsOrientationTracked = true;
				break;
			case MLInputControllerDof_6:
				ControllerState->bIsOrientationTracked = true;
				ControllerState->bIsPositionTracked = true;
				break;
			default:
				break;
			}
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapController::Tick(float DeltaTime)
{
	UpdateTrackerData();
}

void FMagicLeapController::SendControllerEvents()
{
#if WITH_MLSDK
	if (bIsInputStateValid && MessageHandler.IsValid())
	{
		SendControllerEventsForHand(EControllerHand::Left);
		SendControllerEventsForHand(EControllerHand::Right);

		{
			FScopeLock Lock(&ButtonCriticalSection);
			for (const FButtonMap& buttonmap : PendingButtonStates)
			{
				if (buttonmap.bPressed)
				{
					OnButtonDown(buttonmap.ControllerID, buttonmap.Button);
				}
				else
				{
					OnButtonUp(buttonmap.ControllerID, buttonmap.Button);
				}
			}
			PendingButtonStates.Empty();
		}

		{
			FScopeLock Lock(&KeyCriticalSection);
			for (MLKeyCode keyCode : PendingKeyDowns)
			{
				OnKeyDown(keyCode);
			}
			for (MLKeyCode keyCode : PendingKeyUps)
			{
				OnKeyUp(keyCode);
			}
			for (uint32 charUTF32 : PendingCharKeys)
			{
				OnChar(charUTF32);
			}

			PendingKeyDowns.Empty();
			PendingKeyUps.Empty();
			PendingCharKeys.Empty();
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapController::SendControllerEventsForHand(EControllerHand Hand)
{
#if WITH_MLSDK
	const int32* Controller = HandToControllerID.Find(Hand);
	if (Controller != nullptr)
	{
		const int32 ControllerID = *Controller;

		// Controller connected or disconnected
		if (InputState[ControllerID].is_connected && !OldInputState[ControllerID].is_connected)
		{
			OnControllerConnected(Hand);
		}
		else if (!InputState[ControllerID].is_connected && OldInputState[ControllerID].is_connected)
		{
			OnControllerDisconnected(Hand);
		}

		if (InputState[ControllerID].is_connected)
		{
			// Touch 0 maps to Motion Controller Thumbstick
			if (InputState[ControllerID].is_touch_active[0])
			{
				FMagicLeapHMD::EnableInput EnableInputFromHMD;
				// fixes unreferenced parameter error for Windows package builds.
				(void)EnableInputFromHMD;
				MessageHandler->OnControllerAnalog(MagicLeapTouchToUnrealThumbstickAxis(Hand, 0),
					DeviceIndex, InputState[ControllerID].touch_pos_and_force[0].x);
				MessageHandler->OnControllerAnalog(MagicLeapTouchToUnrealThumbstickAxis(Hand, 1),
					DeviceIndex, InputState[ControllerID].touch_pos_and_force[0].y);
				MessageHandler->OnControllerAnalog(MagicLeapTouchToUnrealThumbstickAxis(Hand, 2),
					DeviceIndex, InputState[ControllerID].touch_pos_and_force[0].z);
			}
			else
			{
				FMagicLeapHMD::EnableInput EnableInputFromHMD;
				// fixes unreferenced parameter error for Windows package builds.
				(void)EnableInputFromHMD;
				MessageHandler->OnControllerAnalog(MagicLeapTouchToUnrealThumbstickAxis(Hand, 0), DeviceIndex, 0.0f);
				MessageHandler->OnControllerAnalog(MagicLeapTouchToUnrealThumbstickAxis(Hand, 1), DeviceIndex, 0.0f);
				MessageHandler->OnControllerAnalog(MagicLeapTouchToUnrealThumbstickAxis(Hand, 2), DeviceIndex, 0.0f);
			}

			DebouncedButtonMessageHandler(InputState[ControllerID].is_touch_active[0],
				OldInputState[ControllerID].is_touch_active[0], MagicLeapTouchToUnrealThumbstickButton(Hand));

			// Analog trigger
			if (OldInputState[ControllerID].trigger_normalized != InputState[ControllerID].trigger_normalized)
			{
				FMagicLeapHMD::EnableInput EnableInputFromHMD;
				// fixes unreferenced parameter error for Windows package builds.
				(void)EnableInputFromHMD;
				MessageHandler->OnControllerAnalog(MagicLeapTriggerToUnrealTriggerAxis(Hand),
					DeviceIndex, InputState[ControllerID].trigger_normalized);

				// TODO: This is a temporary hack until the C-API can send trigger down messages
				const float triggerValue = InputState[ControllerID].trigger_normalized;
				const bool isTriggerKeyPressing = (triggerValue > triggerKeyIsConsideredPressed) && !bTriggerKeyPressing;
				const bool isTriggerKeyReleasing = (triggerValue < triggerKeyIsConsideredReleased) && bTriggerKeyPressing;

				if (isTriggerKeyPressing)
				{
					// Analog trigger pressed
					MessageHandler->OnControllerButtonPressed(MagicLeapTriggerToUnrealTriggerKey(Hand), DeviceIndex, false);
					bTriggerKeyPressing = true;
				}
				else if (isTriggerKeyReleasing)
				{
					// Analog trigger released
					MessageHandler->OnControllerButtonReleased(MagicLeapTriggerToUnrealTriggerKey(Hand), DeviceIndex, false);
					bTriggerKeyPressing = false;
				}
			}
		}
	}
#endif //WITH_MLSDK
}

void FMagicLeapController::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FMagicLeapController::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

void FMagicLeapController::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	const EControllerHand Hand = (ChannelType == FForceFeedbackChannelType::LEFT_LARGE || ChannelType == FForceFeedbackChannelType::LEFT_SMALL) ? EControllerHand::Left : EControllerHand::Right;

	bool bFromHapticInterface = false;
	FHapticFeedbackValues HapticValues;
	HapticValues.Frequency = 1.0f;
	HapticValues.Amplitude = Value;
	InternalSetHapticFeedbackValues(ControllerId, static_cast<int32>(Hand), HapticValues, bFromHapticInterface);
}

//Convert directly from Force Feedback Values to haptic values.  Small = Frequency, Large = Amplitude
void FMagicLeapController::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values)
{
	bool bFromHapticInterface = false;
	FHapticFeedbackValues HapticValues;

	//body
	HapticValues.Frequency = values.LeftSmall;
	HapticValues.Amplitude = values.LeftLarge;
	InternalSetHapticFeedbackValues(ControllerId, static_cast<int32>(EControllerHand::Left), HapticValues, bFromHapticInterface);

	//touchpad
	HapticValues.Frequency = values.RightSmall;
	HapticValues.Amplitude = values.RightLarge;
	InternalSetHapticFeedbackValues(ControllerId, static_cast<int32>(EControllerHand::Right), HapticValues, bFromHapticInterface);
}

IHapticDevice* FMagicLeapController::GetHapticDevice()
{
	return this;
}

bool FMagicLeapController::IsGamepadAttached() const
{
#if WITH_MLSDK
	if (bIsInputStateValid)
	{
		// TODO: replace with flags set in OnControllerConnected and OnControllerDisconnected.
		bool bDeviceConnected = false;
		const int32* Controller_L = HandToControllerID.Find(EControllerHand::Left);
		const int32* Controller_R = HandToControllerID.Find(EControllerHand::Right);

		if (Controller_L != nullptr)
		{
			bDeviceConnected = bDeviceConnected || InputState[*Controller_L].is_connected;
		}
		if (Controller_R != nullptr)
		{
			bDeviceConnected = bDeviceConnected || InputState[*Controller_R].is_connected;
		}
		return bDeviceConnected;
	}
#endif //WITH_MLSDK
	return false;
}

void FMagicLeapController::Enable()
{
#if WITH_MLSDK
	// Default to CFUID
	TrackingMode = EMLControllerTrackingMode::CoordinateFrameUID;

	// Pull preference from config file
	const static UEnum* TrackingModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMLControllerTrackingMode"));

	FString EnumVal;
	GConfig->GetString(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"),
		TEXT("ControllerTrackingMode"), EnumVal, GEngineIni);

	if (EnumVal.Len() > 0)
	{
		TrackingMode = static_cast<EMLControllerTrackingMode>(TrackingModeEnum->GetValueByNameString(EnumVal));
	}

	// Attempt to create the Controller Tracker. We always do this b/c we do not want to create
	// the tracker on the fly, and the mode can be changed on the fly.
	if (!MLHandleIsValid(ControllerTracker))
	{
		MLControllerConfiguration ControllerConfig = { false };

		ControllerConfig.enable_fused6dof = true;

		MLResult Result = MLControllerCreate(ControllerConfig, &ControllerTracker);

		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapController, Error, 
				TEXT("MLControllerCreate failed with error %s."), 
				UTF8_TO_TCHAR(MLGetResultString(Result)));
			ControllerTracker = ML_INVALID_HANDLE;

			// Revert to input
			TrackingMode = EMLControllerTrackingMode::InputService;
		}
	}

	// Attempt to create the Input Tracker
	if (!MLHandleIsValid(InputTracker))
	{
		MLResult Result = MLInputCreate(nullptr, &InputTracker);
		if (Result == MLResult_Ok)
		{
			FMemory::Memset(&InputState, 0, sizeof(InputState));
			Result = MLInputSetControllerCallbacks(InputTracker, &InputControllerCallbacks, this);
			if (Result == MLResult_Ok)
			{
				Result = MLInputSetKeyboardCallbacks(InputTracker, &InputKeyboardCallbacks, this);
				if (Result != MLResult_Ok)
				{
					UE_LOG(LogMagicLeapController, Error, 
						TEXT("MLInputSetKeyboardCallbacks failed with error %s."), 
						UTF8_TO_TCHAR(MLGetResultString(Result)));
				}

				// Poll to get startup status
				UpdateTrackerData();
			}
			else
			{
				UE_LOG(LogMagicLeapController, Error, 
					TEXT("MLInputSetControllerCallbacks failed with error %s."), 
					UTF8_TO_TCHAR(MLGetResultString(Result)));
			}
		}
		else
		{
			UE_LOG(LogMagicLeapController, Error, 
				TEXT("MLInputCreate failed with error %s."), 
				UTF8_TO_TCHAR(MLGetResultString(Result)));
		}
	}
#endif //WITH_MLSDK
}

bool FMagicLeapController::SupportsExplicitEnable() const
{
	return true;
}

void FMagicLeapController::Disable()
{
#if WITH_MLSDK
	if (MLHandleIsValid(InputTracker))
	{
		MLResult Result = MLInputDestroy(InputTracker);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapController, Error, 
				TEXT("MLInputDestroy failed with error %s!"), 
				UTF8_TO_TCHAR(MLGetResultString(Result)));
		}
		InputTracker = ML_INVALID_HANDLE;
		bIsInputStateValid = false;
	}
	if (MLHandleIsValid(ControllerTracker))
	{
		MLResult Result = MLControllerDestroy(ControllerTracker);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapController, Error, 
				TEXT("MLControllerDestroy failed with error %s!"), 
				UTF8_TO_TCHAR(MLGetResultString(Result)));
		}
		ControllerTracker = ML_INVALID_HANDLE;
	}
#endif //WITH_MLSDK
}

void FMagicLeapController::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	bool bFromHapticInterface = true;
	InternalSetHapticFeedbackValues(ControllerId, Hand, Values, bFromHapticInterface);
}

void FMagicLeapController::InternalSetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values, bool bFromHapticInterface)
{
	if (ControllerId != DeviceIndex)
	{
		return;
	}

	const int32* ControllerIndex = HandToControllerID.Find((EControllerHand)Hand);
	if (ControllerIndex != nullptr)
	{
		//get the shadow state from the controller id
		FMagicLeapControllerState* ControllerState = ControllerIDToControllerState.Find(*ControllerIndex);
		if ((ControllerState != nullptr) && (ControllerState->bIsConnected) && (FApp::HasVRFocus()))
		{
			//early out if this is force feedback trying to run while haptics are running
			if (ControllerState->bPlayingHapticEffect && !bFromHapticInterface)
			{
				return;
			}

			//clear to apply change to haptics
			FHapticFeedbackBuffer* HapticBuffer = Values.HapticBuffer;
			if (HapticBuffer) // && HapticBuffer->SamplingRate == SampleRateHz)
			{
				//@TODO - Setup buffer transfer
				//if (bFromHapticInterface)
				//{
				//	ControllerState.bPlayingHapticEffect = (Amplitude != 0.f) && (Frequency != 0.f);
				//}
			}
			else
			{
				float FreqMin, FreqMax = 0.f;
				GetHapticFrequencyRange(FreqMin, FreqMax);

				const float Frequency = FMath::Lerp(FreqMin, FreqMax, FMath::Clamp(Values.Frequency, 0.f, 1.f));
				const float Amplitude = Values.Amplitude * GetHapticAmplitudeScale();

				if (ControllerState->HapticAmplitude != Amplitude || ControllerState->HapticFrequency != Frequency)
				{
					ControllerState->HapticAmplitude = Amplitude;
					ControllerState->HapticFrequency = Frequency;

					UE_LOG(LogMagicLeapController, Warning, TEXT("Disabling haptic feedback pending a lower level API.  please use UMagicLeapControllerFunctionLibrary::PlayControllerHapticFeedback for now!"));

					if ((EControllerHand(Hand) == EControllerHand::Left))
					{
						// TODO: What to do with duration?
						//MLResult Result = MLInputHapticsStartControllerBody(InputTracker, static_cast<uint8>(*ControllerIndex), MLInputControllerFeedbackPatternVibe_Buzz, intensity, 8);
						//if (Result != MLResult_Ok)
						//{
						//	UE_LOG(LogMagicLeapController, Error, TEXT("MLInputHapticsStartControllerBody() failed."));
						//}
					}
					else
					{
						// TODO: What to do with duration?
						//MLResult Result = MLInputHapticsStartControllerTouchpad(InputTracker, static_cast<uint8>(*Controller), MLInputControllerFeedbackPatternVibe_Buzz, intensity, 8);
						//if (Result != MLResult_Ok)
						//{
						//	UE_LOG(LogMagicLeapController, Error, TEXT("MLInputHapticsStartControllerTouchpad() failed."));
						//}
					}

					//only toggle when called from the haptics interface, not force feedback
					if (bFromHapticInterface)
					{
						ControllerState->bPlayingHapticEffect = (Amplitude != 0.f) && (Frequency != 0.f);
					}
				}
			}
		}
		else
		{
#if PLATFORM_LUMIN
			static int32 bHasErrored = 0;
			if (!bHasErrored)
			{
				bHasErrored = true;
				UE_LOG(LogMagicLeapController, Error, TEXT("Haptic controller not attached"));
			}
#endif
		}
	}
}

void FMagicLeapController::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = 0.0f;
	MaxFrequency = 1.0f;
}

float FMagicLeapController::GetHapticAmplitudeScale() const
{
	return 1.0f;
}

bool FMagicLeapController::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	bool bControllerTracked = false;
	if (ControllerIndex == DeviceIndex)
	{
		if (GetControllerTrackingStatus(ControllerIndex, DeviceHand) != ETrackingStatus::NotTracked)
		{
			const FTransform* ControllerTransform = &LeftControllerTransform;

			if (DeviceHand == EControllerHand::Right)
			{
				ControllerTransform = &RightControllerTransform;
			}

			OutPosition = ControllerTransform->GetLocation();
			OutOrientation = ControllerTransform->GetRotation().Rotator();

			bControllerTracked = true;
		}
	}

	return bControllerTracked;
}

ETrackingStatus FMagicLeapController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	ETrackingStatus status = ETrackingStatus::NotTracked;
#if WITH_MLSDK
	if (ControllerIndex == DeviceIndex)
	{
		const int32* ControllerID = HandToControllerID.Find(DeviceHand);
		if (ControllerID != nullptr)
		{
			if (bIsInputStateValid && InputState[*ControllerID].is_connected)
			{
				switch (InputState[*ControllerID].dof)
				{
				case MLInputControllerDof_3:
					status = ETrackingStatus::InertialOnly;
					break;
				case MLInputControllerDof_6:
					status = ETrackingStatus::Tracked;
					break;
				default:
					status = ETrackingStatus::NotTracked;
					break;
				}
			}
		}
	}
#endif //WITH_MLSDK
	return status;
}

FName FMagicLeapController::GetMotionControllerDeviceTypeName() const
{
	const static FName DefaultName(TEXT("MagicLeapController"));
	return DefaultName;
}

void FMagicLeapController::UpdateControllerTransformFromInputTracker(const FAppFramework& AppFramework, FTransform& ControllerTransform, EControllerHand ControllerHand)
{
#if WITH_MLSDK
	int32 ControllerID = *HandToControllerID.Find(ControllerHand);
	ETrackingStatus ControllerTrackingStatus = GetControllerTrackingStatus(DeviceIndex, ControllerHand);
	if (ControllerTrackingStatus == ETrackingStatus::Tracked)
	{
		ControllerTransform.SetLocation(MagicLeap::ToFVector(InputState[ControllerID].position, AppFramework.GetWorldToMetersScale()));
		ControllerTransform.SetRotation(MagicLeap::ToFQuat(InputState[ControllerID].orientation));
	}
	else if (ControllerTrackingStatus == ETrackingStatus::InertialOnly)
	{
		ControllerTransform.SetRotation(MagicLeap::ToFQuat(InputState[ControllerID].orientation));
	}

	if (ControllerTransform.ContainsNaN())
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("Transform for controller index %d has NaNs."), ControllerID);
		InputState[ControllerID].dof = MLInputControllerDof_None;
	}
	else
	{
		if (!ControllerTransform.GetRotation().IsNormalized())
		{
			FQuat rotation = ControllerTransform.GetRotation();
			rotation.Normalize();
			ControllerTransform.SetRotation(rotation);
		}
	}
#endif //WITH_MLSDK
}

#if WITH_MLSDK
void FMagicLeapController::UpdateControllerTransformFromControllerTracker(const FAppFramework& AppFramework, const MLControllerSystemState& ControllerSystemState, FTransform& ControllerTransform, int32 InDeviceIndex)
{
	const auto& ControllerStream = ControllerSystemState.
		controller_state[InDeviceIndex].stream[MLControllerMode_Fused6Dof];

	if (ControllerStream.is_active)
	{
		EFailReason FailReason = EFailReason::None;
		if (!AppFramework.GetTransform(ControllerStream.coord_frame_controller, ControllerTransform, FailReason))
		{
			UE_LOG(LogMagicLeapController, Error, 
				TEXT("UpdateControllerTransformFromControllerTracker: AppFramework."
				"GetTransform returned false, fail reason = %d."), 
				static_cast<uint32>(FailReason));
		}
	}
}
#endif //WITH_MLSDK

void FMagicLeapController::UpdateTrackerData()
{
#if WITH_MLSDK
	if (!IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
	{
		return;
	}

	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>
		(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	if (!AppFramework.IsInitialized())
	{
		return;
	}

	// First pull data from input tracker. Note that this is not conditional based on the tracking
	// type because we also need to get button, touchpad, etc.
	if (MLHandleIsValid(InputTracker))
	{
		FMemory::Memcpy(&OldInputState, &InputState, sizeof(InputState));

		bIsInputStateValid = MLInputGetControllerState(InputTracker, InputState) == MLResult_Ok;

		if (bIsInputStateValid)
		{
			UpdateControllerTransformFromInputTracker(AppFramework, 
				LeftControllerTransform, EControllerHand::Left);
			UpdateControllerTransformFromInputTracker(AppFramework, 
				RightControllerTransform, EControllerHand::Right);
		}
	}

	// If mode is set to CFUID tracking overwrite the input data
	if (bIsInputStateValid && MLHandleIsValid(ControllerTracker) && (TrackingMode == EMLControllerTrackingMode::CoordinateFrameUID))
	{
		MLControllerSystemState ControllerSystemState;
		MLResult Result = MLControllerGetState(ControllerTracker, &ControllerSystemState);
		if (MLResult_Ok == Result)
		{
			int32 ControllerID;

			ControllerID = HandToControllerID[EControllerHand::Left];
			if (InputState[ControllerID].type == MLInputControllerType_Device)
			{
				UpdateControllerTransformFromControllerTracker(AppFramework, ControllerSystemState,
					LeftControllerTransform, InputState[ControllerID].hardware_index);
			}

			ControllerID = HandToControllerID[EControllerHand::Right];
			if (InputState[ControllerID].type == MLInputControllerType_Device)
			{
				UpdateControllerTransformFromControllerTracker(AppFramework, ControllerSystemState,
					RightControllerTransform, InputState[ControllerID].hardware_index);
			}
		}
		else
		{
			UE_LOG(LogMagicLeapController, Error, 
				TEXT("MLControllerGetState failed with error %s."), 
				UTF8_TO_TCHAR(MLGetResultString(Result)));
		}
	}
#endif //WITH_MLSDK
}

bool FMagicLeapController::IsInputStateValid() const
{
	return bIsInputStateValid;
}

bool FMagicLeapController::GetControllerMapping(int32 ControllerIndex, EControllerHand& Hand) const
{
	const EControllerHand* possibleHand = ControllerIDToHand.Find(ControllerIndex);
	if (possibleHand != nullptr)
	{
		Hand = *possibleHand;
		return true;
	}
	Hand = EControllerHand::Special_9;
	return false;
}

void FMagicLeapController::InvertControllerMapping()
{
	EControllerHand tempHand = ControllerIDToHand[0];
	ControllerIDToHand[0] = ControllerIDToHand[1];
	ControllerIDToHand[1] = tempHand;

	int32 tempID = HandToControllerID[EControllerHand::Left];
	HandToControllerID[EControllerHand::Left] = HandToControllerID[EControllerHand::Right];
	HandToControllerID[EControllerHand::Right] = tempID;
}

EMLControllerType FMagicLeapController::GetMLControllerType(EControllerHand Hand) const
{
#if WITH_MLSDK
	const int32* ControllerID = HandToControllerID.Find(Hand);
	if (ControllerID != nullptr)
	{
		switch (InputState[*ControllerID].type)
		{
		case MLInputControllerType_Device:
			return EMLControllerType::Device;
		case MLInputControllerType_MobileApp:
			return EMLControllerType::MobileApp;
		}
	}
#endif //WITH_MLSDK
	return EMLControllerType::None;
}

bool FMagicLeapController::PlayControllerLED(EControllerHand Hand, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec)
{
#if WITH_MLSDK
	if (IsGamepadAttached())
	{
		const int32* ControllerID = HandToControllerID.Find(Hand);
		if (ControllerID != nullptr)
		{
			return MLInputStartControllerFeedbackPatternLED(InputTracker, static_cast<uint8>(*ControllerID), UnrealToMLPatternLED(LEDPattern), UnrealToMLColorLED(LEDColor), static_cast<uint32>(DurationInSec * 1000)) == MLResult_Ok;
		}
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("LED controller not attached"));
	}
#endif //WITH_MLSDK

	return false;
}

bool FMagicLeapController::PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec)
{
#if WITH_MLSDK
	if (IsGamepadAttached())
	{
		const int32* ControllerID = HandToControllerID.Find(Hand);
		if (ControllerID != nullptr)
		{
			return MLInputStartControllerFeedbackPatternEffectLED(InputTracker, static_cast<uint8>(*ControllerID), UnrealToMLEffectLED(LEDEffect), UnrealToMLSpeedLED(LEDSpeed), UnrealToMLPatternLED(LEDPattern), UnrealToMLColorLED(LEDColor), static_cast<uint32>(DurationInSec * 1000)) == MLResult_Ok;
		}
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("LED controller not attached"));
	}
#endif //WITH_MLSDK

	return false;
}

bool FMagicLeapController::PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity)
{
#if WITH_MLSDK
	if (IsGamepadAttached())
	{
		const int32* ControllerID = HandToControllerID.Find(Hand);
		if (ControllerID != nullptr)
		{
			return MLInputStartControllerFeedbackPatternVibe(InputTracker, static_cast<uint8>(*ControllerID), UnrealToMLPatternVibe(HapticPattern), UnrealToMLHapticIntensity(Intensity)) == MLResult_Ok;
		}
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("Haptic controller not attached"));
	}
#endif //WITH_MLSDK

	return false;
}

bool FMagicLeapController::SetControllerTrackingMode(EMLControllerTrackingMode InTrackingMode)
{
#if WITH_MLSDK
	if (IsGamepadAttached())
	{
		TrackingMode = InTrackingMode;
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("Haptic controller not attached"));
	}
#endif //WITH_MLSDK

	return false;
}

EMLControllerTrackingMode FMagicLeapController::GetControllerTrackingMode()
{
#if WITH_MLSDK
	if (IsGamepadAttached())
	{
		return TrackingMode;
	}
	else
	{
		UE_LOG(LogMagicLeapController, Error, TEXT("Haptic controller not attached"));
	}
#endif //WITH_MLSDK

	return EMLControllerTrackingMode::InputService;
}

void FMagicLeapController::RegisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver)
{
	if (Receiver != nullptr)
	{
		TouchpadGestureReceivers.Add(Receiver);
	}
}

void FMagicLeapController::UnregisterTouchpadGestureReceiver(IMagicLeapTouchpadGestures* Receiver)
{
	TouchpadGestureReceivers.Remove(Receiver);
}

void FMagicLeapController::AddKeys()
{
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::MotionController_Left_Thumbstick_Z, LOCTEXT("MotionController_Left_Thumbstick_Z", "MotionController (L) Thumbstick Z"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::Left_MoveButton, LOCTEXT("MagicLeap_Left_MoveButton", "ML (L) Move Button"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::Left_AppButton, LOCTEXT("MagicLeap_Left_AppButton", "ML (L) App Button"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::Left_HomeButton, LOCTEXT("MagicLeap_Left_HomeButton", "ML (L) Home Button"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::MotionController_Right_Thumbstick_Z, LOCTEXT("MotionController_Right_Thumbstick_Z", "MotionController (R) Thumbstick Z"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::Right_MoveButton, LOCTEXT("MagicLeap_Right_MoveButton", "ML (R) Move Button"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::Right_AppButton, LOCTEXT("MagicLeap_Right_AppButton", "ML (R) App Button"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FMagicLeapKeys::Right_HomeButton, LOCTEXT("MagicLeap_Right_HomeButton", "ML (R) Home Button"), FKeyDetails::GamepadKey));
}

void FMagicLeapController::DebouncedButtonMessageHandler(bool NewButtonState, bool OldButtonState, const FName& ButtonName)
{
	if (NewButtonState && !OldButtonState)
	{
		FMagicLeapHMD::EnableInput EnableInputFromHMD;
		// fixes unreferenced parameter error for Windows package builds.
		(void)EnableInputFromHMD;
		MessageHandler->OnControllerButtonPressed(ButtonName, DeviceIndex, false);
	}
	else if (!NewButtonState && OldButtonState)
	{
		FMagicLeapHMD::EnableInput EnableInputFromHMD;
		// fixes unreferenced parameter error for Windows package builds.
		(void)EnableInputFromHMD;
		MessageHandler->OnControllerButtonReleased(ButtonName, DeviceIndex, false);
	}
}

#if WITH_MLSDK
const FName& FMagicLeapController::MagicLeapButtonToUnrealButton(int32 ControllerID, MLInputControllerButton ml_button)
{
	static const FName empty;

	const EControllerHand* possibleHand = ControllerIDToHand.Find(ControllerID);
	if (possibleHand == nullptr)
	{
		return empty;
	}

	const EControllerHand hand = *possibleHand;

	switch (ml_button)
	{
	case MLInputControllerButton_Move:
		if (hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::Left_MoveButton_Name;
		}
		return FMagicLeapControllerKeyNames::Right_MoveButton_Name;
	case MLInputControllerButton_App:
		if (hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::Left_AppButton_Name;
		}
		return FMagicLeapControllerKeyNames::Right_AppButton_Name;
	case MLInputControllerButton_Bumper:
		if (hand == EControllerHand::Left)
		{
			return FGamepadKeyNames::MotionController_Left_Shoulder;
		}
		return FGamepadKeyNames::MotionController_Right_Shoulder;
	case MLInputControllerButton_HomeTap:
		if (hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::Left_HomeButton_Name;
		}
		return FMagicLeapControllerKeyNames::Right_HomeButton_Name;
	default:
		break;
	}
	return empty;
}
#endif //WITH_MLSDK


const FName& FMagicLeapController::MagicLeapTouchToUnrealThumbstickAxis(EControllerHand Hand, uint32 TouchIndex)
{
	static const FName empty;

	switch (TouchIndex)
	{
	case 0:
		if (Hand == EControllerHand::Left)
		{
			return FGamepadKeyNames::MotionController_Left_Thumbstick_X;
		}
		return FGamepadKeyNames::MotionController_Right_Thumbstick_X;
	case 1:
		if (Hand == EControllerHand::Left)
		{
			return FGamepadKeyNames::MotionController_Left_Thumbstick_Y;
		}
		return FGamepadKeyNames::MotionController_Right_Thumbstick_Y;
	case 2:
		if (Hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::MotionController_Left_Thumbstick_Z_Name;
		}
		return FMagicLeapControllerKeyNames::MotionController_Right_Thumbstick_Z_Name;
	default:
		return empty;
	}

	return empty;
}

const FName& FMagicLeapController::MagicLeapTouchToUnrealThumbstickButton(EControllerHand Hand)
{
	static const FName empty;

	switch (Hand)
	{
	case EControllerHand::Left:
		return FGamepadKeyNames::MotionController_Left_Thumbstick;
	case EControllerHand::Right:
		return FGamepadKeyNames::MotionController_Right_Thumbstick;
	}

	return empty;
}

const FName& FMagicLeapController::MagicLeapTriggerToUnrealTriggerAxis(EControllerHand Hand)
{
	static const FName empty;

	switch (Hand)
	{
	case EControllerHand::Left:
		return FGamepadKeyNames::MotionController_Left_TriggerAxis;
	case EControllerHand::Right:
		return FGamepadKeyNames::MotionController_Right_TriggerAxis;
	}

	return empty;
}

const FName& FMagicLeapController::MagicLeapTriggerToUnrealTriggerKey(EControllerHand Hand)
{
	static const FName empty;
	switch (Hand)
	{
	case EControllerHand::Left:
		return FGamepadKeyNames::MotionController_Left_Trigger;
	case EControllerHand::Right:
		return FGamepadKeyNames::MotionController_Right_Trigger;
	}
	return empty;
}

#if WITH_MLSDK

MLInputControllerFeedbackPatternLED FMagicLeapController::UnrealToMLPatternLED(EMLControllerLEDPattern LEDPattern) const
{
	switch (LEDPattern)
	{
		case EMLControllerLEDPattern::None:
			return MLInputControllerFeedbackPatternLED_None;
		case EMLControllerLEDPattern::Clock01:
			return MLInputControllerFeedbackPatternLED_Clock1;
		case EMLControllerLEDPattern::Clock02:
			return MLInputControllerFeedbackPatternLED_Clock2;
		case EMLControllerLEDPattern::Clock03:
			return MLInputControllerFeedbackPatternLED_Clock3;
		case EMLControllerLEDPattern::Clock04:
			return MLInputControllerFeedbackPatternLED_Clock4;
		case EMLControllerLEDPattern::Clock05:
			return MLInputControllerFeedbackPatternLED_Clock5;
		case EMLControllerLEDPattern::Clock06:
			return MLInputControllerFeedbackPatternLED_Clock6;
		case EMLControllerLEDPattern::Clock07:
			return MLInputControllerFeedbackPatternLED_Clock7;
		case EMLControllerLEDPattern::Clock08:
			return MLInputControllerFeedbackPatternLED_Clock8;
		case EMLControllerLEDPattern::Clock09:
			return MLInputControllerFeedbackPatternLED_Clock9;
		case EMLControllerLEDPattern::Clock10:
			return MLInputControllerFeedbackPatternLED_Clock10;
		case EMLControllerLEDPattern::Clock11:
			return MLInputControllerFeedbackPatternLED_Clock11;
		case EMLControllerLEDPattern::Clock12:
			return MLInputControllerFeedbackPatternLED_Clock12;
		case EMLControllerLEDPattern::Clock01_07:
			return MLInputControllerFeedbackPatternLED_Clock1And7;
		case EMLControllerLEDPattern::Clock02_08:
			return MLInputControllerFeedbackPatternLED_Clock2And8;
		case EMLControllerLEDPattern::Clock03_09:
			return MLInputControllerFeedbackPatternLED_Clock3And9;
		case EMLControllerLEDPattern::Clock04_10:
			return MLInputControllerFeedbackPatternLED_Clock4And10;
		case EMLControllerLEDPattern::Clock05_11:
			return MLInputControllerFeedbackPatternLED_Clock5And11;
		case EMLControllerLEDPattern::Clock06_12:
			return MLInputControllerFeedbackPatternLED_Clock6And12;    
		default:
			UE_LOG(LogMagicLeapController, Error, TEXT("Unhandled LED Pattern type %d"), static_cast<int32>(LEDPattern));
			break;
	}
	return MLInputControllerFeedbackPatternLED_Ensure32Bits;
}

MLInputControllerFeedbackEffectLED FMagicLeapController::UnrealToMLEffectLED(EMLControllerLEDEffect LEDEffect) const
{
	switch (LEDEffect)
	{
		case EMLControllerLEDEffect::RotateCW:
			return MLInputControllerFeedbackEffectLED_RotateCW;
		case EMLControllerLEDEffect::RotateCCW:
			return MLInputControllerFeedbackEffectLED_RotateCCW;
		case EMLControllerLEDEffect::Pulse:
			return MLInputControllerFeedbackEffectLED_Pulse;
		case EMLControllerLEDEffect::PaintCW:
			return MLInputControllerFeedbackEffectLED_PaintCW;
		case EMLControllerLEDEffect::PaintCCW:
			return MLInputControllerFeedbackEffectLED_PaintCCW;
		case EMLControllerLEDEffect::Blink:
			return MLInputControllerFeedbackEffectLED_Blink;
		default:
			UE_LOG(LogMagicLeapController, Error, TEXT("Unhandled LED effect type %d"), static_cast<int32>(LEDEffect));
		break;
	}
	return MLInputControllerFeedbackEffectLED_Ensure32Bits;
}

#define LED_COLOR_CASE(x) case EMLControllerLEDColor::x: { return MLInputControllerFeedbackColorLED_##x; }
MLInputControllerFeedbackColorLED FMagicLeapController::UnrealToMLColorLED(EMLControllerLEDColor LEDColor) const
{
	switch (LEDColor)
	{
#if MLSDK_VERSION_MINOR >= 16
		LED_COLOR_CASE(BrightMissionRed)
		LED_COLOR_CASE(PastelMissionRed)
#else
		case EMLControllerLEDColor::BrightMissionRed: { return MLInputControllerFeedbackColorLED_BrightRed; }
		case EMLControllerLEDColor::PastelMissionRed: { return MLInputControllerFeedbackColorLED_PastelRed; }
#endif // MLSDK_VERSION_MINOR >= 16
		LED_COLOR_CASE(BrightFloridaOrange)
		LED_COLOR_CASE(PastelFloridaOrange)
		LED_COLOR_CASE(BrightLunaYellow)
		LED_COLOR_CASE(PastelLunaYellow)
		LED_COLOR_CASE(BrightNebulaPink)
		LED_COLOR_CASE(PastelNebulaPink)
		LED_COLOR_CASE(BrightCosmicPurple)
		LED_COLOR_CASE(PastelCosmicPurple)
		LED_COLOR_CASE(BrightMysticBlue)
		LED_COLOR_CASE(PastelMysticBlue)
		LED_COLOR_CASE(BrightCelestialBlue)
		LED_COLOR_CASE(PastelCelestialBlue)
		LED_COLOR_CASE(BrightShaggleGreen)
		LED_COLOR_CASE(PastelShaggleGreen)
		default:
			UE_LOG(LogMagicLeapController, Error, TEXT("Unhandled LED color type %d"), static_cast<int32>(LEDColor));
			break;
	}
	return MLInputControllerFeedbackColorLED_Ensure32Bits;
}

MLInputControllerFeedbackEffectSpeedLED FMagicLeapController::UnrealToMLSpeedLED(EMLControllerLEDSpeed LEDSpeed) const
{
	switch (LEDSpeed)
	{
		case EMLControllerLEDSpeed::Slow:
			return MLInputControllerFeedbackEffectSpeedLED_Slow;
		case EMLControllerLEDSpeed::Medium:
			return MLInputControllerFeedbackEffectSpeedLED_Medium;
		case EMLControllerLEDSpeed::Fast:
			return MLInputControllerFeedbackEffectSpeedLED_Fast;
		default:
			UE_LOG(LogMagicLeapController, Error, TEXT("Unhandled LED speed type %d"), static_cast<int32>(LEDSpeed));
			break;
	}
	return MLInputControllerFeedbackEffectSpeedLED_Ensure32Bits;  
}

MLInputControllerFeedbackPatternVibe FMagicLeapController::UnrealToMLPatternVibe(EMLControllerHapticPattern HapticPattern) const
{
	switch (HapticPattern)
	{
		case EMLControllerHapticPattern::None:
			return MLInputControllerFeedbackPatternVibe_None;
		case EMLControllerHapticPattern::Click:
			return MLInputControllerFeedbackPatternVibe_Click;
		case EMLControllerHapticPattern::Bump:
			return MLInputControllerFeedbackPatternVibe_Bump;
		case EMLControllerHapticPattern::DoubleClick:
			return MLInputControllerFeedbackPatternVibe_DoubleClick;
		case EMLControllerHapticPattern::Buzz:
			return MLInputControllerFeedbackPatternVibe_Buzz;
		case EMLControllerHapticPattern::Tick:
			return MLInputControllerFeedbackPatternVibe_Tick;
		case EMLControllerHapticPattern::ForceDown:
			return MLInputControllerFeedbackPatternVibe_ForceDown;
		case EMLControllerHapticPattern::ForceUp:
			return MLInputControllerFeedbackPatternVibe_ForceUp;
		case EMLControllerHapticPattern::ForceDwell:
			return MLInputControllerFeedbackPatternVibe_ForceDwell;
		case EMLControllerHapticPattern::SecondForceDown:
			return MLInputControllerFeedbackPatternVibe_SecondForceDown;
		default:
			UE_LOG(LogMagicLeapController, Error, TEXT("Unhandled Haptic Pattern type %d"), static_cast<int32>(HapticPattern));
			break;
	}
	return MLInputControllerFeedbackPatternVibe_Ensure32Bits;
}

MLInputControllerFeedbackIntensity FMagicLeapController::UnrealToMLHapticIntensity(EMLControllerHapticIntensity HapticIntensity) const
{
	switch (HapticIntensity)
	{
	case EMLControllerHapticIntensity::Low:
		return MLInputControllerFeedbackIntensity_Low;
	case EMLControllerHapticIntensity::Medium:
		return MLInputControllerFeedbackIntensity_Medium;
	case EMLControllerHapticIntensity::High:
		return MLInputControllerFeedbackIntensity_High;
	default:
		UE_LOG(LogMagicLeapController, Error, TEXT("Unhandled Haptic Intensity type %d"), static_cast<int32>(HapticIntensity));
		break;
	}
	return MLInputControllerFeedbackIntensity_Ensure32Bits;
}
#endif //WITH_MLSDK

void FMagicLeapController::InitializeInputCallbacks()
{
#if WITH_MLSDK
	FMemory::Memset(&InputKeyboardCallbacks, 0, sizeof(InputKeyboardCallbacks));

	InputKeyboardCallbacks.on_char = [](uint32_t char_utf32, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			{
				FScopeLock Lock(&controller->KeyCriticalSection);
				controller->PendingCharKeys.Add(char_utf32);
			}
		}
	};

	InputKeyboardCallbacks.on_key_down = [](MLKeyCode key_code, uint32 modifier_mask, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			{
				FScopeLock Lock(&controller->KeyCriticalSection);
				controller->PendingKeyDowns.Add(key_code);
			}
		}
	};

	InputKeyboardCallbacks.on_key_up = [](MLKeyCode key_code, uint32 modifier_mask, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			{
				FScopeLock Lock(&controller->KeyCriticalSection);
				controller->PendingKeyUps.Add(key_code);
			}
		}
	};

	FMemory::Memset(&InputControllerCallbacks, 0, sizeof(InputControllerCallbacks));
	// Creating an async task to be fired on the game thread in these callbacks was causing
	// intermitted blocks on the game thread. Until that is investigated and fixed, use polling.
	InputControllerCallbacks.on_touchpad_gesture_start = [](uint8 controller_id, const MLInputControllerTouchpadGesture *touchpad_gesture, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			EControllerHand hand;
			controller->GetControllerMapping(controller_id, hand);
			FMagicLeapTouchpadGesture gesture = MLToUnrealTouchpadGesture(hand, *touchpad_gesture);
			for (IMagicLeapTouchpadGestures* Receiver : controller->TouchpadGestureReceivers)
			{
				// NOTE: This is temporary; Epic has a Jira task to pipe touchpad gestures through correctly but we need
				// something for the interim - rmobbs
				Receiver->OnTouchpadGestureStartCallback(gesture);
			}
		}
	};

	InputControllerCallbacks.on_touchpad_gesture_continue = [](uint8 controller_id, const MLInputControllerTouchpadGesture *touchpad_gesture, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			EControllerHand hand;
			controller->GetControllerMapping(controller_id, hand);
			FMagicLeapTouchpadGesture gesture = MLToUnrealTouchpadGesture(hand, *touchpad_gesture);
			for (IMagicLeapTouchpadGestures* Receiver : controller->TouchpadGestureReceivers)
			{
				// NOTE: This is temporary; Epic has a Jira task to pipe touchpad gestures through correctly but we need
				// something for the interim - rmobbs
				Receiver->OnTouchpadGestureContinueCallback(gesture);
			}
		}
	};

	InputControllerCallbacks.on_touchpad_gesture_end = [](uint8 controller_id, const MLInputControllerTouchpadGesture *touchpad_gesture, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			EControllerHand hand;
			controller->GetControllerMapping(controller_id, hand);
			FMagicLeapTouchpadGesture gesture = MLToUnrealTouchpadGesture(hand, *touchpad_gesture);
			for (IMagicLeapTouchpadGestures* Receiver : controller->TouchpadGestureReceivers)
			{
				// NOTE: This is temporary; Epic has a Jira task to pipe touchpad gestures through correctly but we need
				// something for the interim - rmobbs
				Receiver->OnTouchpadGestureEndCallback(gesture);
			}
		}
	};

	InputControllerCallbacks.on_button_down = [](uint8_t controller_id, MLInputControllerButton button, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			FScopeLock Lock(&controller->ButtonCriticalSection);
			FButtonMap ButtonMap;
			ButtonMap.ControllerID = controller_id;
			ButtonMap.Button = button;
			ButtonMap.bPressed = true;
			controller->PendingButtonStates.Add(ButtonMap);
		}
	};

	InputControllerCallbacks.on_button_up = [](uint8_t controller_id, MLInputControllerButton button, void *data)
	{
		auto controller = reinterpret_cast<FMagicLeapController*>(data);
		if (controller)
		{
			FScopeLock Lock(&controller->ButtonCriticalSection);
			FButtonMap ButtonMap;
			ButtonMap.ControllerID = controller_id;
			ButtonMap.Button = button;
			ButtonMap.bPressed = false;
			controller->PendingButtonStates.Add(ButtonMap);
		}
	};

	InputControllerCallbacks.on_connect = nullptr;
	InputControllerCallbacks.on_disconnect = nullptr;
#endif //WITH_MLSDK
}

#undef LOCTEXT_NAMESPACE
