// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenXRInput.h"
#include "OpenXRHMD.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/InputSettings.h"

#include <openxr/openxr.h>

#define XR_ENSURE(x) ensure(XR_SUCCEEDED(x))

// Hack to prefer emitting MotionController keys for action events
static bool MatchKeyNamePrefix(const FKey& Key, const TCHAR* Prefix)
{
	return Key.GetFName().ToString().StartsWith(Prefix);
};

static bool MatchKeyNameSuffix(const FKey& Key, const TCHAR* Suffix)
{
	return Key.GetFName().ToString().EndsWith(Suffix);
};

FORCEINLINE XrPath GetPath(XrInstance Instance, const char* PathString)
{
	XrPath Path = XR_NULL_PATH;
	XrResult Result = xrStringToPath(Instance, PathString, &Path);
	check(XR_SUCCEEDED(Result));
	return Path;
}

FORCEINLINE void FilterActionName(const char* InActionName, char* OutActionName)
{
	// Ensure the action name is a well-formed path
	size_t i;
	for (i = 0; InActionName[i] != '\0' && i < XR_MAX_ACTION_NAME_SIZE - 1; i++)
	{
		unsigned char c = InActionName[i];
		OutActionName[i] = (c == ' ') ? '-' : isalnum(c) ? tolower(c) : '_';
	}
	OutActionName[i] = '\0';
}

TSharedPtr< class IInputDevice > FOpenXRInputPlugin::CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	if (InputDevice)
		InputDevice->SetMessageHandler(InMessageHandler);
	return InputDevice;
}

IMPLEMENT_MODULE(FOpenXRInputPlugin, OpenXRInput)

FOpenXRInputPlugin::FOpenXRInputPlugin()
	: InputDevice()
{
}

FOpenXRInputPlugin::~FOpenXRInputPlugin()
{
}

FOpenXRHMD* FOpenXRInputPlugin::GetOpenXRHMD() const
{
	static FName SystemName(TEXT("OpenXR"));
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
	{
		return static_cast<FOpenXRHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

void FOpenXRInputPlugin::StartupModule()
{
	IOpenXRInputPlugin::StartupModule();

	FOpenXRHMD* OpenXRHMD = GetOpenXRHMD();
	if (OpenXRHMD)
	{
		InputDevice = TSharedPtr<FOpenXRInput>(new FOpenXRInputPlugin::FOpenXRInput(OpenXRHMD));
	}
}

FOpenXRInputPlugin::FOpenXRAction::FOpenXRAction(XrActionSet InSet, XrActionType InType, const FName& InName)
	: Set(InSet)
	, Type(InType)
	, Name(InName)
	, ActionKey()
	, Handle()
{
	char ActionName[NAME_SIZE];
	Name.GetPlainANSIString(ActionName);

	XrActionCreateInfo Info;
	Info.type = XR_TYPE_ACTION_CREATE_INFO;
	Info.next = nullptr;
	FilterActionName(ActionName, Info.actionName);
	Info.actionType = Type;
	Info.countSubactionPaths = 0;
	Info.subactionPaths = nullptr;
	FCStringAnsi::Strcpy(Info.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE, ActionName);
	ensure(xrCreateAction(Set, &Info, &Handle) >= XR_SUCCESS);
}

FOpenXRInputPlugin::FOpenXRAction::FOpenXRAction(XrActionSet InSet, const FInputActionKeyMapping& InActionKey)
	: FOpenXRAction(InSet, XR_INPUT_ACTION_TYPE_BOOLEAN, InActionKey.ActionName)
{
	ActionKey = InActionKey.Key.GetFName();
}

FOpenXRInputPlugin::FOpenXRAction::FOpenXRAction(XrActionSet InSet, const FInputAxisKeyMapping& InAxisKey)
	: FOpenXRAction(InSet, XR_INPUT_ACTION_TYPE_VECTOR1F, InAxisKey.AxisName)
{
	ActionKey = InAxisKey.Key.GetFName();
}

FOpenXRInputPlugin::FOpenXRController::FOpenXRController(FOpenXRHMD* HMD, XrActionSet InSet, const char* InName)
	: Set(InSet)
	, Pose(XR_NULL_HANDLE)
	, Vibration(XR_NULL_HANDLE)
	, DeviceId(-1)
{
	XrActionCreateInfo Info;
	Info.type = XR_TYPE_ACTION_CREATE_INFO;
	Info.next = nullptr;
	FCStringAnsi::Strcpy(Info.localizedActionName, XR_MAX_ACTION_NAME_SIZE, InName);
	FCStringAnsi::Strcat(Info.localizedActionName, XR_MAX_ACTION_NAME_SIZE, " Pose");
	FilterActionName(Info.localizedActionName, Info.actionName);
	Info.actionType = XR_INPUT_ACTION_TYPE_POSE;
	Info.countSubactionPaths = 0;
	Info.subactionPaths = nullptr;
	XR_ENSURE(xrCreateAction(Set, &Info, &Pose));

	FCStringAnsi::Strcpy(Info.localizedActionName, XR_MAX_ACTION_NAME_SIZE, InName);
	FCStringAnsi::Strcat(Info.localizedActionName, XR_MAX_ACTION_NAME_SIZE, " Vibration");
	FilterActionName(Info.localizedActionName, Info.actionName);
	Info.actionType = XR_OUTPUT_ACTION_TYPE_VIBRATION;
	XR_ENSURE(xrCreateAction(Set, &Info, &Vibration));

	if (HMD)
	{
		DeviceId = HMD->AddActionDevice(Pose);
	}
}

FOpenXRInputPlugin::FOpenXRInput::FOpenXRInput(FOpenXRHMD* HMD)
	: OpenXRHMD(HMD)
	, ActionSets()
	, Actions()
	, Controllers()
	, MessageHandler(new FGenericApplicationMessageHandler())
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	check(OpenXRHMD);

	XrSession Session = OpenXRHMD->GetSession();
	XrInstance Instance = OpenXRHMD->GetInstance();
	check(Session && Instance);

	XrActionSet ActionSet;
	XrActionSetCreateInfo SetInfo;
	SetInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	SetInfo.next = nullptr;
	FCStringAnsi::Strcpy(SetInfo.actionSetName, XR_MAX_ACTION_SET_NAME_SIZE, "ue4");
	FCStringAnsi::Strcpy(SetInfo.localizedActionSetName, XR_MAX_ACTION_SET_NAME_SIZE, "Unreal Engine 4");
	XR_ENSURE(xrCreateActionSet(Session, &SetInfo, &ActionSet));

	// Controller poses
	TArray<XrActionSuggestedBinding> Bindings;
	Controllers.Add(EControllerHand::Left, FOpenXRController(OpenXRHMD, ActionSet, "Left Controller"));
	Controllers.Add(EControllerHand::Right, FOpenXRController(OpenXRHMD, ActionSet, "Right Controller"));

	Bindings.Add(XrActionSuggestedBinding{ Controllers[EControllerHand::Left].Pose, GetPath(Instance, "/user/hand/left/input/palm") });
	Bindings.Add(XrActionSuggestedBinding{ Controllers[EControllerHand::Right].Pose, GetPath(Instance, "/user/hand/right/input/palm") });

	Bindings.Add(XrActionSuggestedBinding{ Controllers[EControllerHand::Left].Vibration, GetPath(Instance, "/user/hand/left/output/haptic") });
	Bindings.Add(XrActionSuggestedBinding{ Controllers[EControllerHand::Right].Vibration, GetPath(Instance, "/user/hand/right/output/haptic") });

	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Shoulder, GetPath(Instance, "/user/hand/left/input/menu/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Trigger, GetPath(Instance, "/user/hand/left/input/trigger/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_TriggerAxis, GetPath(Instance, "/user/hand/left/input/trigger/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Grip1, GetPath(Instance, "/user/hand/left/input/grip/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Grip1Axis, GetPath(Instance, "/user/hand/left/input/grip/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Grip1, GetPath(Instance, "/user/hand/left/input/grip/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Grip1Axis, GetPath(Instance, "/user/hand/left/input/grip/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Thumbstick_X, GetPath(Instance, "/user/hand/left/input/thumbstick/x"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Thumbstick_Y, GetPath(Instance, "/user/hand/left/input/thumbstick/y"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_Thumbstick, GetPath(Instance, "/user/hand/left/input/thumbstick/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_FaceButton1, GetPath(Instance, "/user/hand/left/input/x/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Left_FaceButton1, GetPath(Instance, "/user/hand/left/input/y/click"));

	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Shoulder, GetPath(Instance, "/user/hand/right/input/menu/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Trigger, GetPath(Instance, "/user/hand/right/input/trigger/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_TriggerAxis, GetPath(Instance, "/user/hand/right/input/trigger/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Grip1, GetPath(Instance, "/user/hand/right/input/grip/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Grip1Axis, GetPath(Instance, "/user/hand/right/input/grip/value"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Grip1, GetPath(Instance, "/user/hand/right/input/grip/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Grip1Axis, GetPath(Instance, "/user/hand/right/input/grip/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Thumbstick_X, GetPath(Instance, "/user/hand/right/input/thumbstick/x"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Thumbstick_Y, GetPath(Instance, "/user/hand/right/input/thumbstick/y"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_Thumbstick, GetPath(Instance, "/user/hand/right/input/thumbstick/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_FaceButton1, GetPath(Instance, "/user/hand/right/input/a/click"));
	InteractionMappings.Add(FGamepadKeyNames::MotionController_Right_FaceButton2, GetPath(Instance, "/user/hand/right/input/b/click"));

	auto InputSettings = GetDefault<UInputSettings>();
	if (InputSettings != nullptr)
	{
		TArray<FName> ActionNames;
		InputSettings->GetActionNames(ActionNames);
		for (const auto& ActionName : ActionNames)
		{
			TArray<FInputActionKeyMapping> Mappings;
			InputSettings->GetActionMappingByName(ActionName, Mappings);
			AddAction(ActionSet, Mappings, Bindings);
		}

		TArray<FName> AxisNames;
		InputSettings->GetAxisNames(AxisNames);
		for (const auto& AxisName : AxisNames)
		{
			TArray<FInputAxisKeyMapping> Mappings;
			InputSettings->GetAxisMappingByName(AxisName, Mappings);
			AddAction(ActionSet, Mappings, Bindings);
		}

		// Open console
		{
			const FKey* ConsoleKey = InputSettings->ConsoleKeys.FindByPredicate([](FKey& Key) { return Key.IsValid(); });
			if (ConsoleKey != nullptr)
			{
				Actions.Add(FOpenXRAction(ActionSet, FName(TEXT("Open Console")), ConsoleKey->GetFName()));
			}
		}
	}

	TArray<XrPath> Profiles;
	Profiles.Add(GetPath(Instance, "/interaction_profiles/khr/simple_controller"));
	Profiles.Add(GetPath(Instance, "/interaction_profiles/microsoft/motion_controller"));
	Profiles.Add(GetPath(Instance, "/interaction_profiles/oculus/touch_controller"));
	Profiles.Add(GetPath(Instance, "/interaction_profiles/valve/knuckles_controller"));

	for (XrPath Profile : Profiles)
	{
		XrInteractionProfileSuggestedBinding InteractionProfile;
		InteractionProfile.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		InteractionProfile.next = nullptr;
		InteractionProfile.interactionProfile = Profile;
		InteractionProfile.countSuggestedBindings = Bindings.Num();
		InteractionProfile.suggestedBindings = Bindings.GetData();
		XR_ENSURE(xrSetInteractionProfileSuggestedBindings(Session, &InteractionProfile));
	}

	XrActiveActionSet ActiveSet;
	ActiveSet.type = XR_TYPE_ACTIVE_ACTION_SET;
	ActiveSet.next = nullptr;
	ActiveSet.actionSet = ActionSet;
	ActiveSet.subactionPath = XR_NULL_PATH;
	ActionSets.Add(ActiveSet);
}

FOpenXRInputPlugin::FOpenXRInput::~FOpenXRInput()
{
	for (XrActiveActionSet& ActionSet : ActionSets)
	{
		xrDestroyActionSet(ActionSet.actionSet);
	}
}

FORCEINLINE FName GetName(const FInputActionKeyMapping& ActionKey) { return ActionKey.ActionName; }
FORCEINLINE FName GetName(const FInputAxisKeyMapping& AxisKey) { return AxisKey.AxisName; }

template<typename T>
void FOpenXRInputPlugin::FOpenXRInput::AddAction(XrActionSet ActionSet, const TArray<T>& Mappings, TArray<XrActionSuggestedBinding>& OutSuggestedBindings)
{
	// Find all the motion controller keys so we can suggest bindings for them
	TArray<T> KeyMappings = Mappings.FilterByPredicate([](const T& Mapping)
	{
		return MatchKeyNamePrefix(Mapping.Key, TEXT("MotionController"));
	});

	// We need at least one key to be able to trigger the action
	// TODO: Refactor the input API so we can trigger actions directly
	if (KeyMappings.Num() == 0)
	{
		KeyMappings = Mappings.FilterByPredicate([](const T& Mapping) { return Mapping.Key.IsValid(); });
	}

	if (KeyMappings.Num() > 0)
	{
		// Build the action based on the primary mapping
		FOpenXRAction Action(ActionSet, KeyMappings[0]);

		// Add suggested bindings for every mapping
		for (T InputKey : KeyMappings)
		{
			XrPath* Path = InteractionMappings.Find(InputKey.Key.GetFName());
			if (Path)
			{
				OutSuggestedBindings.Add(XrActionSuggestedBinding{ Action.Handle, *Path });
			}
		}

		Actions.Add(Action);
	}
}

void FOpenXRInputPlugin::FOpenXRInput::Tick(float DeltaTime)
{
	if (OpenXRHMD->IsRunning())
	{
		XR_ENSURE(xrSyncActionData(OpenXRHMD->GetSession(), ActionSets.Num(), ActionSets.GetData()));
	}
}

void FOpenXRInputPlugin::FOpenXRInput::SendControllerEvents()
{
	if (!OpenXRHMD->IsRunning())
	{
		return;
	}

	for (auto& Action : Actions)
	{
		switch (Action.Type)
		{
		case XR_INPUT_ACTION_TYPE_BOOLEAN:
		{
			XrActionStateBoolean State;
			State.type = XR_TYPE_ACTION_STATE_BOOLEAN;
			State.next = nullptr;
			XrResult Result = xrGetActionStateBoolean(Action.Handle, 0, XR_NULL_PATH, &State);

			if (Result >= XR_SUCCESS && State.changedSinceLastSync)
			{
				if (State.currentState)
				{
					MessageHandler->OnControllerButtonPressed(Action.ActionKey, 0, /*IsRepeat =*/false);
				}
				else
				{
					MessageHandler->OnControllerButtonReleased(Action.ActionKey, 0, /*IsRepeat =*/false);
				}
			}
		}
		break;
		case XR_INPUT_ACTION_TYPE_VECTOR1F:
		{
			XrActionStateVector1f State;
			State.type = XR_TYPE_ACTION_STATE_VECTOR1F;
			State.next = nullptr;
			XrResult Result = xrGetActionStateVector1f(Action.Handle, 0, XR_NULL_PATH, &State);
			if (Result >= XR_SUCCESS && State.changedSinceLastSync)
			{
				MessageHandler->OnControllerAnalog(Action.ActionKey, 0, State.currentState);
			}
		}
		break;
		default:
			// Other action types are currently unsupported.
			break;
		}
	}
}

void FOpenXRInputPlugin::FOpenXRInput::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FOpenXRInputPlugin::FOpenXRInput::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

void FOpenXRInputPlugin::FOpenXRInput::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	// Large channel type maps to amplitude. We are interested in amplitude.
	if ((ChannelType == FForceFeedbackChannelType::LEFT_LARGE) ||
		(ChannelType == FForceFeedbackChannelType::RIGHT_LARGE))
	{
		FHapticFeedbackValues Values(XR_FREQUENCY_UNSPECIFIED, Value);
		SetHapticFeedbackValues(ControllerId, ChannelType == FForceFeedbackChannelType::LEFT_LARGE ? (int32)EControllerHand::Left : (int32)EControllerHand::Right, Values);
	}
}

void FOpenXRInputPlugin::FOpenXRInput::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values)
{
	FHapticFeedbackValues leftHaptics = FHapticFeedbackValues(
		values.LeftSmall,		// frequency
		values.LeftLarge);		// amplitude
	FHapticFeedbackValues rightHaptics = FHapticFeedbackValues(
		values.RightSmall,		// frequency
		values.RightLarge);		// amplitude

	SetHapticFeedbackValues(
		ControllerId,
		(int32)EControllerHand::Left,
		leftHaptics);

	SetHapticFeedbackValues(
		ControllerId,
		(int32)EControllerHand::Right,
		rightHaptics);
}

FName FOpenXRInputPlugin::FOpenXRInput::GetMotionControllerDeviceTypeName() const
{
	return FName(TEXT("OpenXR"));
}
bool FOpenXRInputPlugin::FOpenXRInput::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	if (ControllerIndex == 0 && (DeviceHand == EControllerHand::Left || DeviceHand == EControllerHand::Right))
	{
		FQuat Orientation;
		OpenXRHMD->GetCurrentPose(Controllers[DeviceHand].DeviceId, Orientation, OutPosition);
		OutOrientation = FRotator(Orientation);
		return true;
	}

	return false;
}

ETrackingStatus FOpenXRInputPlugin::FOpenXRInput::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	if (ControllerIndex == 0 && (DeviceHand == EControllerHand::Left || DeviceHand == EControllerHand::Right || DeviceHand == EControllerHand::AnyHand))
	{
		return ETrackingStatus::Tracked;
	}

	return ETrackingStatus::NotTracked;
}

// TODO: Refactor API to change the Hand type to EControllerHand
void FOpenXRInputPlugin::FOpenXRInput::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	XrHapticVibration hapticValue;
	hapticValue.type = XR_TYPE_HAPTIC_VIBRATION;
	hapticValue.next = nullptr;
	hapticValue.duration = MaxFeedbackDuration;
	hapticValue.frequency = Values.Frequency;
	hapticValue.amplitude = Values.Amplitude;

	if (ControllerId == 0)
	{
		if (Hand == (int32)EControllerHand::Left || Hand == (int32)EControllerHand::AnyHand)
		{
			if (Values.Amplitude <= 0.0f || Values.Frequency < XR_FREQUENCY_UNSPECIFIED)
			{
				XR_ENSURE(xrStopHapticFeedback(Controllers[EControllerHand::Left].Vibration, 0, nullptr));
			}
			else
			{
				XR_ENSURE(xrApplyHapticFeedback(Controllers[EControllerHand::Left].Vibration, 0, nullptr, (XrHapticBaseHeader*)&hapticValue));
			}
		}
		if (Hand == (int32)EControllerHand::Right || Hand == (int32)EControllerHand::AnyHand)
		{
			if (Values.Amplitude <= 0.0f || Values.Frequency < XR_FREQUENCY_UNSPECIFIED)
			{
				XR_ENSURE(xrStopHapticFeedback(Controllers[EControllerHand::Right].Vibration, 0, nullptr));
			}
			else
			{
				XR_ENSURE(xrApplyHapticFeedback(Controllers[EControllerHand::Right].Vibration, 0, nullptr, (XrHapticBaseHeader*)&hapticValue));
			}
		}
	}
}

void FOpenXRInputPlugin::FOpenXRInput::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = XR_FREQUENCY_UNSPECIFIED;
	MaxFrequency = XR_FREQUENCY_UNSPECIFIED;
}

float FOpenXRInputPlugin::FOpenXRInput::GetHapticAmplitudeScale() const
{
	return 1.0f;
}
