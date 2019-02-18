// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ISteamVRControllerPlugin.h"
#include "ISteamVRPlugin.h"
#include "IInputDevice.h"
#include "IHapticDevice.h"
#include "XRMotionControllerBase.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "GenericPlatform/IInputInterface.h"
#include "GameFramework/InputSettings.h"
#include "../../SteamVR/Private/SteamVRHMD.h"
#include "SteamVRControllerLibrary.h" // for ESteamVRTouchDPadMapping
#include "HAL/FileManagerGeneric.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "UObject/UObjectIterator.h"
#include "GameFramework/PlayerInput.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "SteamVRController"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
#include "openvr.h"
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

DEFINE_LOG_CATEGORY_STATIC(LogSteamVRController, Log, All);

/** Total number of controllers in a set */
#define CONTROLLERS_PER_PLAYER	2

/** Player that generic trackers will be assigned to */
#define GENERIC_TRACKER_PLAYER_NUM 0

/** Controller axis mappings. @todo steamvr: should enumerate rather than hard code */
#define TOUCHPAD_AXIS					0
#define TRIGGER_AXIS					1
#define KNUCKLES_TOTAL_HAND_GRIP_AXIS	2
#define KNUCKLES_UPPER_HAND_GRIP_AXIS	3
#define KNUCKLES_LOWER_HAND_GRIP_AXIS	4
#define DOT_45DEG		0.7071f

//
// Gamepad thresholds
//
#define TOUCHPAD_DEADZONE  0.0f

// Controls whether or not we need to swap the input routing for the hands, for debugging
static TAutoConsoleVariable<int32> CVarSwapHands(
	TEXT("vr.SwapMotionControllerInput"),
	0,
	TEXT("This command allows you to swap the button / axis input handedness for the input controller, for debugging purposes.\n")
	TEXT(" 0: don't swap (default)\n")
	TEXT(" 1: swap left and right buttons"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarEnableVRInput(
	TEXT("vr.SteamVR.EnableVRInput"),
	0,
	TEXT("Enable the new Steam VR Input interface for mapping actions to events.\n")
	TEXT(" 0: use the legacy input mapping (default)\n")
	TEXT(" 1: use the new Input API. You will have to define input bindings for the controllers you want to support."),
	ECVF_ReadOnly);


namespace SteamVRControllerKeyNames
{
	const FGamepadKeyNames::Type Touch0("Steam_Touch_0");
	const FGamepadKeyNames::Type Touch1("Steam_Touch_1");
	const FGamepadKeyNames::Type GenericGrip("Steam_Generic_Grip");
	const FGamepadKeyNames::Type GenericTrigger("Steam_Generic_Trigger");
	const FGamepadKeyNames::Type GenericTouchpad("Steam_Generic_Touchpad");
	const FGamepadKeyNames::Type GenericMenu("Steam_Generic_Menu");
	const FGamepadKeyNames::Type GenericSystem("Steam_Generic_System");

	const FGamepadKeyNames::Type SteamVR_Knuckles_Left_HandGrip("SteamVR_Knuckles_Left_HandGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Left_IndexGrip("SteamVR_Knuckles_Left_IndexGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Left_MiddleGrip("SteamVR_Knuckles_Left_MiddleGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Left_RingGrip("SteamVR_Knuckles_Left_RingGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Left_PinkyGrip("SteamVR_Knuckles_Left_PinkyGrip");

	const FGamepadKeyNames::Type SteamVR_Knuckles_Right_HandGrip("SteamVR_Knuckles_Right_HandGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Right_IndexGrip("SteamVR_Knuckles_Right_IndexGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Right_MiddleGrip("SteamVR_Knuckles_Right_MiddleGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Right_RingGrip("SteamVR_Knuckles_Right_RingGrip");
	const FGamepadKeyNames::Type SteamVR_Knuckles_Right_PinkyGrip("SteamVR_Knuckles_Right_PinkyGrip");
}

namespace SteamVRControllerKeys
{
	const FKey SteamVR_Knuckles_Left_HandGrip("SteamVR_Knuckles_Left_HandGrip");
	const FKey SteamVR_Knuckles_Left_IndexGrip("SteamVR_Knuckles_Left_IndexGrip");
	const FKey SteamVR_Knuckles_Left_MiddleGrip("SteamVR_Knuckles_Left_MiddleGrip");
	const FKey SteamVR_Knuckles_Left_RingGrip("SteamVR_Knuckles_Left_RingGrip");
	const FKey SteamVR_Knuckles_Left_PinkyGrip("SteamVR_Knuckles_Left_PinkyGrip");

	const FKey SteamVR_Knuckles_Right_HandGrip("SteamVR_Knuckles_Right_HandGrip");
	const FKey SteamVR_Knuckles_Right_IndexGrip("SteamVR_Knuckles_Right_IndexGrip");
	const FKey SteamVR_Knuckles_Right_MiddleGrip("SteamVR_Knuckles_Right_MiddleGrip");
	const FKey SteamVR_Knuckles_Right_RingGrip("SteamVR_Knuckles_Right_RingGrip");
	const FKey SteamVR_Knuckles_Right_PinkyGrip("SteamVR_Knuckles_Right_PinkyGrip");
}

class FSteamVRController : public IInputDevice, public FXRMotionControllerBase, public IHapticDevice
{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	FSteamVRHMD* GetSteamVRHMD() const
	{
		static FName SystemName(TEXT("SteamVR"));
		if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
		{
			return static_cast<FSteamVRHMD*>(GEngine->XRSystem.Get());
		}

		return nullptr;
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

public:

	/** The maximum number of Unreal controllers.  Each Unreal controller represents a pair of motion controller devices */
	static const int32 MaxUnrealControllers = MAX_STEAMVR_CONTROLLER_PAIRS;

	/** Total number of motion controllers we'll support */
	// NOTE: This used to be MaxUnrealControllers * CONTROLLERS_PER_PLAYER, but we needed to support many more trackers than that
	static const int32 MaxControllers =
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		vr::k_unMaxTrackedDeviceCount;
#else
		0;
#endif

	/** The maximum number of Special hand designations available to use for generic trackers
	 *  Casting enums directly, so if the input model changes, this won't silently be invalid
	 */
	static const int32 MaxSpecialDesignations = (int32)EControllerHand::Special_9 - (int32)EControllerHand::Special_1 + 1;

	/**
	 * Buttons on the SteamVR controller
	 */
	struct ESteamVRControllerButton
	{
		enum Type
		{
			System,
			ApplicationMenu,
			TouchPadPress,
			TouchPadTouch,
			TriggerPress,
			Grip,
			TouchPadUp,
			TouchPadDown,
			TouchPadLeft,
			TouchPadRight,

			/** Max number of controller buttons.  Must be < 256 */
			TotalButtonCount
		};
	};

	FSteamVRController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
		: MessageHandler(InMessageHandler),
		SteamVRPlugin(nullptr)
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.SteamVR.EnableVRInput"));
		bEnableVRInput = (CVar->GetValueOnGameThread() != 0) ? true : false;

		FMemory::Memzero(ControllerStates, sizeof(ControllerStates));
		NumControllersMapped = 0;
		NumTrackersMapped = 0;

		InitialButtonRepeatDelay = 0.2f;
		ButtonRepeatDelay = 0.1f;

		InitControllerMappings();
		InitLegacyControllerKeys();
		BuildActionManifest();

		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
#endif // STEAMVRINPUT_SUPPORTED_PLATFORMS
	}

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	void InitControllerMappings()
	{
		for (int32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
		{
			DeviceToControllerMap[i] = INDEX_NONE;
		}

		for (int32 UnrealControllerIndex = 0; UnrealControllerIndex < MaxUnrealControllers; ++UnrealControllerIndex)
		{
			for (int32 HandIndex = 0; HandIndex < vr::k_unMaxTrackedDeviceCount; ++HandIndex)
			{
				SetUnrealControllerIdToControllerIndex(UnrealControllerIndex, (EControllerHand) HandIndex, INDEX_NONE);
			}
		}

		for (int32& Count : UnrealControllerHandUsageCount)
		{
			Count = 0;
		}
	}


	void InitLegacyControllerKeys()
	{

		if (!bEnableVRInput)
		{
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::System] = FGamepadKeyNames::SpecialLeft;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::ApplicationMenu] = FGamepadKeyNames::MotionController_Left_Shoulder;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadPress] = FGamepadKeyNames::MotionController_Left_Thumbstick;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadTouch] = SteamVRControllerKeyNames::Touch0;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TriggerPress] = FGamepadKeyNames::MotionController_Left_Trigger;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::Grip] = FGamepadKeyNames::MotionController_Left_Grip1;

			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::System] = FGamepadKeyNames::SpecialRight;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::ApplicationMenu] = FGamepadKeyNames::MotionController_Right_Shoulder;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadPress] = FGamepadKeyNames::MotionController_Right_Thumbstick;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadTouch] = SteamVRControllerKeyNames::Touch1;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TriggerPress] = FGamepadKeyNames::MotionController_Right_Trigger;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::Grip] = FGamepadKeyNames::MotionController_Right_Grip1;

			// Init Left & Right, TouchPadUp/Down/Left/Right button mappings
			SetTouchDPadMapping(DefaultDPadMapping);

			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::System] = SteamVRControllerKeyNames::GenericSystem;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::ApplicationMenu] = SteamVRControllerKeyNames::GenericMenu;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TouchPadPress] = SteamVRControllerKeyNames::GenericTouchpad;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TouchPadTouch] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TriggerPress] = SteamVRControllerKeyNames::GenericTrigger;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::Grip] = SteamVRControllerKeyNames::GenericGrip;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TouchPadUp] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TouchPadDown] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TouchPadLeft] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Pad][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::Invalid;

			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::System] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::ApplicationMenu] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TouchPadPress] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TouchPadTouch] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TriggerPress] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::Grip] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TouchPadUp] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TouchPadDown] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TouchPadLeft] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::ExternalCamera][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::Invalid;

			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::System] = SteamVRControllerKeyNames::GenericSystem;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::ApplicationMenu] = SteamVRControllerKeyNames::GenericMenu;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TouchPadPress] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TouchPadTouch] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TriggerPress] = SteamVRControllerKeyNames::GenericTrigger;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::Grip] = SteamVRControllerKeyNames::GenericGrip;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TouchPadUp] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TouchPadDown] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TouchPadLeft] = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Gun][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::Invalid;

			for (int32 SpecialIndex = (int32)EControllerHand::Special_1; SpecialIndex <= (int32)EControllerHand::Special_9; ++SpecialIndex)
			{
				Buttons[SpecialIndex][ESteamVRControllerButton::System] = SteamVRControllerKeyNames::GenericSystem;
				Buttons[SpecialIndex][ESteamVRControllerButton::ApplicationMenu] = SteamVRControllerKeyNames::GenericMenu;
				Buttons[SpecialIndex][ESteamVRControllerButton::TouchPadPress] = SteamVRControllerKeyNames::GenericTouchpad;
				Buttons[SpecialIndex][ESteamVRControllerButton::TouchPadTouch] = FGamepadKeyNames::Invalid;
				Buttons[SpecialIndex][ESteamVRControllerButton::TriggerPress] = SteamVRControllerKeyNames::GenericTrigger;
				Buttons[SpecialIndex][ESteamVRControllerButton::Grip] = SteamVRControllerKeyNames::GenericGrip;
				Buttons[SpecialIndex][ESteamVRControllerButton::TouchPadUp] = FGamepadKeyNames::Invalid;
				Buttons[SpecialIndex][ESteamVRControllerButton::TouchPadDown] = FGamepadKeyNames::Invalid;
				Buttons[SpecialIndex][ESteamVRControllerButton::TouchPadLeft] = FGamepadKeyNames::Invalid;
				Buttons[SpecialIndex][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::Invalid;
			}

			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Left_HandGrip, LOCTEXT("SteamVR_Knuckles_Left_HandGrip", "SteamVR Knuckles (L) Hand Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Left_IndexGrip, LOCTEXT("SteamVR_Knuckles_Left_IndexGrip", "SteamVR Knuckles (L) Index Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Left_MiddleGrip, LOCTEXT("SteamVR_Knuckles_Left_MiddleGrip", "SteamVR Knuckles (L) Middle Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Left_RingGrip, LOCTEXT("SteamVR_Knuckles_Left_RingGrip", "SteamVR Knuckles (L) Ring Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Left_PinkyGrip, LOCTEXT("SteamVR_Knuckles_Left_PinkyGrip", "SteamVR Knuckles (L) Pinky Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Right_HandGrip, LOCTEXT("SteamVR_Knuckles_Right_HandGrip", "SteamVR Knuckles (R) Hand Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Right_IndexGrip, LOCTEXT("SteamVR_Knuckles_Right_IndexGrip", "SteamVR Knuckles (R) Index Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Right_MiddleGrip, LOCTEXT("SteamVR_Knuckles_Right_MiddleGrip", "SteamVR Knuckles (R) Middle Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Right_RingGrip, LOCTEXT("SteamVR_Knuckles_Right_RingGrip", "SteamVR Knuckles (R) Ring Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
			EKeys::AddKey(FKeyDetails(SteamVRControllerKeys::SteamVR_Knuckles_Right_PinkyGrip, LOCTEXT("SteamVR_Knuckles_Right_PinkyGrip", "SteamVR Knuckles (R) Pinky Grip CapSense"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
		}
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

	virtual ~FSteamVRController()
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
#if WITH_EDITOR
		if (ActionMappingsChangedHandle.IsValid())
		{
			FEditorDelegates::OnActionAxisMappingsChanged.Remove(ActionMappingsChangedHandle);
			ActionMappingsChangedHandle.Reset();
		}
#endif
		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	}

	virtual void Tick(float DeltaTime) override
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		vr::IVRSystem* VRSystem = GetVRSystem();

		if (VRSystem != nullptr)
		{
			RegisterDeviceChanges(VRSystem);
			DetectHandednessSwap(VRSystem);
		}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	}

	virtual void SendControllerEvents() override
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		if (bEnableVRInput)
		{
			SendActionInputEvents();
		}
		else
		{
			SendLegacyControllerEvents();
		}
#endif
	}

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	void SendLegacyControllerEvents()
	{
		vr::VRControllerState_t VRControllerState;

		vr::IVRSystem* VRSystem = GetVRSystem();

		if (VRSystem != nullptr)
		{
			const double CurrentTime = FPlatformTime::Seconds();

			for (uint32 DeviceIndex = 0; DeviceIndex < vr::k_unMaxTrackedDeviceCount; ++DeviceIndex)
			{
				// see what kind of hardware this is
				vr::ETrackedDeviceClass DeviceClass = VRSystem->GetTrackedDeviceClass(DeviceIndex);

				// skip non-controller or non-tracker devices
				if (DeviceClass != vr::TrackedDeviceClass_Controller && DeviceClass != vr::TrackedDeviceClass_GenericTracker)
				{
					continue;
				}

				// get the controller index for this device
				int32 ControllerIndex = DeviceToControllerMap[DeviceIndex];
				FControllerState& ControllerState = ControllerStates[ DeviceIndex ];
				EControllerHand HandToUse = ControllerState.Hand;

				// see if this is a hand specific controller
				if (HandToUse == EControllerHand::Left || HandToUse == EControllerHand::Right)
				{
					// check to see if we need to swap input hands for debugging
					static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.SwapMotionControllerInput"));
					bool bSwapHandInput = (CVar->GetValueOnGameThread() != 0) ? true : false;
					if (bSwapHandInput)
					{
						HandToUse = (HandToUse == EControllerHand::Left) ? EControllerHand::Right : EControllerHand::Left;
					}
				}
				if (VRSystem->GetControllerState(DeviceIndex, &VRControllerState, sizeof(vr::VRControllerState_t)))
				{
					if (VRControllerState.unPacketNum != ControllerState.PacketNum )
					{
						bool CurrentStates[ ESteamVRControllerButton::TotalButtonCount ] = {0};

						// Get the current state of all buttons
						CurrentStates[ ESteamVRControllerButton::System ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_System));
						CurrentStates[ ESteamVRControllerButton::ApplicationMenu ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu));
						CurrentStates[ ESteamVRControllerButton::TouchPadPress ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad));
						CurrentStates[ ESteamVRControllerButton::TouchPadTouch ] = !!( VRControllerState.ulButtonTouched & vr::ButtonMaskFromId( vr::k_EButton_SteamVR_Touchpad ) );
						CurrentStates[ ESteamVRControllerButton::TriggerPress ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger));
						CurrentStates[ ESteamVRControllerButton::Grip ] = !!(VRControllerState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip));

						// If the touchpad isn't currently pressed or touched, zero put both of the axes
						if (!CurrentStates[ ESteamVRControllerButton::TouchPadTouch ])
						{
 							VRControllerState.rAxis[TOUCHPAD_AXIS].y = 0.0f;
 							VRControllerState.rAxis[TOUCHPAD_AXIS].x = 0.0f;
						}

						// D-pad emulation
						const FVector2D TouchDir = FVector2D(VRControllerState.rAxis[TOUCHPAD_AXIS].x, VRControllerState.rAxis[TOUCHPAD_AXIS].y).GetSafeNormal();
						const FVector2D UpDir(0.f, 1.f);
						const FVector2D RightDir(1.f, 0.f);

						const float VerticalDot = TouchDir | UpDir;
						const float RightDot = TouchDir | RightDir;

						const bool bPressed = !TouchDir.IsNearlyZero() && CurrentStates[ ESteamVRControllerButton::TouchPadPress ];
						
						CurrentStates[ ESteamVRControllerButton::TouchPadUp ]		= bPressed && (VerticalDot >= DOT_45DEG);
						CurrentStates[ ESteamVRControllerButton::TouchPadDown ]		= bPressed && (VerticalDot <= -DOT_45DEG);
						CurrentStates[ ESteamVRControllerButton::TouchPadLeft ]		= bPressed && (RightDot <= -DOT_45DEG);
						CurrentStates[ ESteamVRControllerButton::TouchPadRight ]	= bPressed && (RightDot >= DOT_45DEG);

						if ( ControllerState.TouchPadXAnalog != VRControllerState.rAxis[TOUCHPAD_AXIS].x)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_X : FGamepadKeyNames::MotionController_Right_Thumbstick_X;
							MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[TOUCHPAD_AXIS].x);
							ControllerState.TouchPadXAnalog = VRControllerState.rAxis[TOUCHPAD_AXIS].x;
						}

						if ( ControllerState.TouchPadYAnalog != VRControllerState.rAxis[TOUCHPAD_AXIS].y)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_Y : FGamepadKeyNames::MotionController_Right_Thumbstick_Y;
							// Invert the y to match UE4 convention
							const float Value = -VRControllerState.rAxis[TOUCHPAD_AXIS].y;
							MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, Value);
							ControllerState.TouchPadYAnalog = Value;
						}

						if ( ControllerState.TriggerAnalog != VRControllerState.rAxis[TRIGGER_AXIS].x)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_TriggerAxis : FGamepadKeyNames::MotionController_Right_TriggerAxis;
							MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[TRIGGER_AXIS].x);
							ControllerState.TriggerAnalog = VRControllerState.rAxis[TRIGGER_AXIS].x;
						}

						// Knuckles CapSense Grip Axes Updates
						{
							if (ControllerState.HandGripAnalog != VRControllerState.rAxis[KNUCKLES_TOTAL_HAND_GRIP_AXIS].x)
							{
								const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? SteamVRControllerKeyNames::SteamVR_Knuckles_Left_HandGrip : SteamVRControllerKeyNames::SteamVR_Knuckles_Right_HandGrip;
								MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[KNUCKLES_TOTAL_HAND_GRIP_AXIS].x);
								ControllerState.HandGripAnalog = VRControllerState.rAxis[KNUCKLES_TOTAL_HAND_GRIP_AXIS].x;
							}

							if (ControllerState.IndexGripAnalog != VRControllerState.rAxis[KNUCKLES_UPPER_HAND_GRIP_AXIS].x)
							{
								const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? SteamVRControllerKeyNames::SteamVR_Knuckles_Left_IndexGrip : SteamVRControllerKeyNames::SteamVR_Knuckles_Right_IndexGrip;
								MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[KNUCKLES_UPPER_HAND_GRIP_AXIS].x);
								ControllerState.IndexGripAnalog = VRControllerState.rAxis[KNUCKLES_UPPER_HAND_GRIP_AXIS].x;
							}

							if (ControllerState.MiddleGripAnalog != VRControllerState.rAxis[KNUCKLES_UPPER_HAND_GRIP_AXIS].y)
							{
								const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? SteamVRControllerKeyNames::SteamVR_Knuckles_Left_MiddleGrip : SteamVRControllerKeyNames::SteamVR_Knuckles_Right_MiddleGrip;
								MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[KNUCKLES_UPPER_HAND_GRIP_AXIS].y);
								ControllerState.MiddleGripAnalog = VRControllerState.rAxis[KNUCKLES_UPPER_HAND_GRIP_AXIS].y;
							}

							if (ControllerState.RingGripAnalog != VRControllerState.rAxis[KNUCKLES_LOWER_HAND_GRIP_AXIS].x)
							{
								const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? SteamVRControllerKeyNames::SteamVR_Knuckles_Left_RingGrip : SteamVRControllerKeyNames::SteamVR_Knuckles_Right_RingGrip;
								MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[KNUCKLES_LOWER_HAND_GRIP_AXIS].x);
								ControllerState.RingGripAnalog = VRControllerState.rAxis[KNUCKLES_LOWER_HAND_GRIP_AXIS].x;
							}

							if (ControllerState.PinkyGripAnalog != VRControllerState.rAxis[KNUCKLES_LOWER_HAND_GRIP_AXIS].y)
							{
								const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? SteamVRControllerKeyNames::SteamVR_Knuckles_Left_PinkyGrip : SteamVRControllerKeyNames::SteamVR_Knuckles_Right_PinkyGrip;
								MessageHandler->OnControllerAnalog(AxisButton, ControllerIndex, VRControllerState.rAxis[KNUCKLES_LOWER_HAND_GRIP_AXIS].y);
								ControllerState.PinkyGripAnalog = VRControllerState.rAxis[KNUCKLES_LOWER_HAND_GRIP_AXIS].y;
							}
						}

						// For each button check against the previous state and send the correct message if any
						for (int32 ButtonIndex = 0; ButtonIndex < ESteamVRControllerButton::TotalButtonCount; ++ButtonIndex)
						{
							if (CurrentStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
							{
								const FGamepadKeyNames::Type ButtonId = Buttons[(uint8)HandToUse][ButtonIndex];
								if (ButtonId != FGamepadKeyNames::Invalid)
								{
									if (CurrentStates[ButtonIndex])
									{
										MessageHandler->OnControllerButtonPressed(ButtonId, ControllerIndex, /*IsRepeat =*/false);
									}
									else
									{
										MessageHandler->OnControllerButtonReleased(ButtonId, ControllerIndex, /*IsRepeat =*/false);
									}
								}

								if (CurrentStates[ButtonIndex] != 0)
								{
									// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
									ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
								}
							}

							// Update the state for next time
							ControllerState.ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
						}

						ControllerState.PacketNum = VRControllerState.unPacketNum;
					}
				}

				for (int32 ButtonIndex = 0; ButtonIndex < ESteamVRControllerButton::TotalButtonCount; ++ButtonIndex)
				{
					if ( ControllerState.ButtonStates[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
					{
						const FGamepadKeyNames::Type ButtonId = Buttons[(uint8)HandToUse][ButtonIndex];
						if (ButtonId != FGamepadKeyNames::Invalid)
						{
							MessageHandler->OnControllerButtonPressed(ButtonId, ControllerIndex, /*IsRepeat =*/true);
						}

						// set the button's NextRepeatTime to the ButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
					}
				}
			}
		}
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	void SendActionInputEvents()
	{
		vr::IVRInput* VRInput = vr::VRInput();

		if (VRInput != nullptr)
		{
			vr::VRActiveActionSet_t ActiveActionSets[] = {
				{
					MainActionSet,
					vr::k_ulInvalidInputValueHandle,
					vr::k_ulInvalidActionSetHandle
				}
			};
			vr::EVRInputError Err = VRInput->UpdateActionState(ActiveActionSets, sizeof(vr::VRActiveActionSet_t), 1);
			if (Err != vr::VRInputError_None)
			{
				UE_LOG(LogSteamVRController, Warning, TEXT("UpdateActionState returned error: %d"), (int32)Err);
				return;
			}

			for (auto& Action : Actions)
			{
				switch (Action.Type)
				{
				case FSteamVRAction::Boolean:
				{
					vr::InputDigitalActionData_t Data;
					Err = VRInput->GetDigitalActionData(Action.Handle, &Data, sizeof(Data), vr::k_ulInvalidInputValueHandle);
					if (Err == vr::VRInputError_None)
					{
						if (Data.bState != Action.bState)
						{
							Action.bState = Data.bState;
							if (Action.bState)
							{
								MessageHandler->OnControllerButtonPressed(Action.ActionKey_X, 0, /*IsRepeat =*/false);
							}
							else
							{
								MessageHandler->OnControllerButtonReleased(Action.ActionKey_X, 0, /*IsRepeat =*/false);
							}
						}
					}
					// If the current error is the same as the last frame's error, don't log it again to avoid spamming the log
					else if (Err != Action.LastError)
					{
						UE_LOG(LogSteamVRController, Warning, TEXT("GetDigitalActionData for %s returned error: %d"), *Action.Name.ToString(), (int32)Err);

					}
					Action.LastError = Err;
				}
				break;
				case FSteamVRAction::Vector1:
				case FSteamVRAction::Vector2:
				case FSteamVRAction::Vector3:
				{
					vr::InputAnalogActionData_t Data;
					Err = VRInput->GetAnalogActionData(Action.Handle, &Data, sizeof(Data), vr::k_ulInvalidInputValueHandle);
					if (Err == vr::VRInputError_None)
					{
						if (!Action.ActionKey_X.IsNone() && Data.x != Action.Value.X)
						{
							Action.Value.X = Data.x;
							MessageHandler->OnControllerAnalog(Action.ActionKey_X, 0, Action.Value.X);
						}
						if (!Action.ActionKey_Y.IsNone() && Data.y != Action.Value.Y)
						{
							Action.Value.Y = Data.y;
							MessageHandler->OnControllerAnalog(Action.ActionKey_Y, 0, Action.Value.Y);
						}
						if (!Action.ActionKey_Z.IsNone() && Data.z != Action.Value.Z)
						{
							Action.Value.Z = Data.z;
							MessageHandler->OnControllerAnalog(Action.ActionKey_Z, 0, Action.Value.Z);
						}
					}
					// If the current error is the same as the last frame's error, don't log it again to avoid spamming the log
					else if (Err != Action.LastError)
					{
						UE_LOG(LogSteamVRController, Warning, TEXT("GetAnalogActionData for %s returned error: %d"), *Action.Name.ToString(), (int32)Err);
					}
					Action.LastError = Err;
				}
				break;
				default:
					// Other action types are currently unsupported.
					break;
				}
			}
		}
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

	void SetTouchDPadMapping(ESteamVRTouchDPadMapping NewMapping)
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		DefaultDPadMapping = NewMapping;

		switch (NewMapping)
		{
		case ESteamVRTouchDPadMapping::FaceButtons:
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadUp]     = FGamepadKeyNames::MotionController_Left_FaceButton1;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadDown]   = FGamepadKeyNames::MotionController_Left_FaceButton3;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadLeft]   = FGamepadKeyNames::MotionController_Left_FaceButton4;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadRight]  = FGamepadKeyNames::MotionController_Left_FaceButton2;

			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadUp]    = FGamepadKeyNames::MotionController_Right_FaceButton1;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadDown]  = FGamepadKeyNames::MotionController_Right_FaceButton3;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadLeft]  = FGamepadKeyNames::MotionController_Right_FaceButton4;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::MotionController_Right_FaceButton2;
			break;

		case ESteamVRTouchDPadMapping::ThumbstickDirections:
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadUp]     = FGamepadKeyNames::MotionController_Left_Thumbstick_Up;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadDown]   = FGamepadKeyNames::MotionController_Left_Thumbstick_Down;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadLeft]   = FGamepadKeyNames::MotionController_Left_Thumbstick_Left;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadRight]  = FGamepadKeyNames::MotionController_Left_Thumbstick_Right;

			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadUp]    = FGamepadKeyNames::MotionController_Right_Thumbstick_Up;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadDown]  = FGamepadKeyNames::MotionController_Right_Thumbstick_Down;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadLeft]  = FGamepadKeyNames::MotionController_Right_Thumbstick_Left;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::MotionController_Right_Thumbstick_Right;
			break;

		default:
			UE_LOG(LogSteamVRController, Warning, TEXT("Unsupported d-pad mapping (%d). Defaulting to disabled."), (int32)NewMapping);
		case ESteamVRTouchDPadMapping::Disabled:
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadUp]     = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadDown]   = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadLeft]   = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Left][ESteamVRControllerButton::TouchPadRight]  = FGamepadKeyNames::Invalid;

			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadUp]    = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadDown]  = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadLeft]  = FGamepadKeyNames::Invalid;
			Buttons[(int32)EControllerHand::Right][ESteamVRControllerButton::TouchPadRight] = FGamepadKeyNames::Invalid;
			break;
		}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	}

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	int32 UnrealControllerIdToControllerIndex(const int32 UnrealControllerId, const EControllerHand Hand) const
	{
		return UnrealControllerIdAndHandToDeviceIdMap[UnrealControllerId][(int32)Hand];
	}

	void SetUnrealControllerIdToControllerIndex(const int32 UnrealControllerId, const EControllerHand Hand, int32 value)
	{
		UnrealControllerIdAndHandToDeviceIdMap[UnrealControllerId][(int32)Hand] = value;
	}

#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	
	void SetChannelValue(int32 UnrealControllerId, FForceFeedbackChannelType ChannelType, float Value) override
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		// Skip unless this is the left or right large channel, which we consider to be the only SteamVRController feedback channel
		if( ChannelType != FForceFeedbackChannelType::LEFT_LARGE && ChannelType != FForceFeedbackChannelType::RIGHT_LARGE )
		{
			return;
		}

		const EControllerHand Hand = ( ChannelType == FForceFeedbackChannelType::LEFT_LARGE ) ? EControllerHand::Left : EControllerHand::Right;
		const int32 ControllerIndex = UnrealControllerIdToControllerIndex( UnrealControllerId, Hand );

		if ((ControllerIndex >= 0) && ( ControllerIndex < MaxControllers))
		{
			FControllerState& ControllerState = ControllerStates[ ControllerIndex ];

			ControllerState.ForceFeedbackValue = Value;

			UpdateVibration( ControllerIndex );
		}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	}

	void SetChannelValues(int32 UnrealControllerId, const FForceFeedbackValues& Values) override
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		const int32 LeftControllerIndex = UnrealControllerIdToControllerIndex( UnrealControllerId, EControllerHand::Left );
		if ((LeftControllerIndex >= 0) && ( LeftControllerIndex < MaxControllers))
		{
			FControllerState& ControllerState = ControllerStates[ LeftControllerIndex ];
			ControllerState.ForceFeedbackValue = Values.LeftLarge;

			UpdateVibration( LeftControllerIndex );
		}

		const int32 RightControllerIndex = UnrealControllerIdToControllerIndex( UnrealControllerId, EControllerHand::Right );
		if( ( RightControllerIndex >= 0 ) && ( RightControllerIndex < MaxControllers ) )
		{
			FControllerState& ControllerState = ControllerStates[ RightControllerIndex ];
			ControllerState.ForceFeedbackValue = Values.RightLarge;

			UpdateVibration( RightControllerIndex );
		}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	}

	virtual IHapticDevice* GetHapticDevice() override
	{
		return this;
	}	

	virtual void SetHapticFeedbackValues(int32 UnrealControllerId, int32 Hand, const FHapticFeedbackValues& Values) override
	{
#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		if (Hand != (int32)EControllerHand::Left && Hand != (int32)EControllerHand::Right)
		{
			return;
		}

		const int32 ControllerIndex = UnrealControllerIdToControllerIndex(UnrealControllerId, (EControllerHand)Hand);
		if (ControllerIndex >= 0 && ControllerIndex < MaxControllers)
		{
			FControllerState& ControllerState = ControllerStates[ControllerIndex];
			ControllerState.ForceFeedbackValue = (Values.Frequency > 0.0f) ? Values.Amplitude : 0.0f;

			UpdateVibration(ControllerIndex);
		}
#endif
	}

	virtual void GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const override
	{
		MinFrequency = 0.0f;
		MaxFrequency = 1.0f;
	}
	
	virtual float GetHapticAmplitudeScale() const override
	{
		return 1.0f;
	}

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
	void UpdateVibration( const int32 ControllerIndex )
	{
		const FControllerState& ControllerState = ControllerStates[ ControllerIndex ];
		vr::IVRSystem* VRSystem = GetVRSystem();

		if (VRSystem == nullptr)
		{
			return;
		}

		// Map the float values from [0,1] to be more reasonable values for the SteamController.  The docs say that [100,2000] are reasonable values
 		const float LeftIntensity = FMath::Clamp(ControllerState.ForceFeedbackValue * 2000.f, 0.f, 2000.f);
		if (LeftIntensity > 0.f)
		{
			VRSystem->TriggerHapticPulse(ControllerIndex, TOUCHPAD_AXIS, LeftIntensity);
		}
	}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override
	{
		MessageHandler = InMessageHandler;
	}

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		return false;
	}

	virtual FName GetMotionControllerDeviceTypeName() const override
	{
		return DeviceTypeName;
	}
	static FName DeviceTypeName;

	virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
	{
		bool RetVal = false;

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
 		FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
 		if (SteamVRHMD)
 		{
			int32 DeviceId = UnrealControllerIdToControllerIndex(ControllerIndex, DeviceHand);
 			FQuat DeviceOrientation = FQuat::Identity;
			// Steam handles WorldToMetersScale when it reads the controller posrot, so we do not need to use it again here.  Debugging found that they are the same.
			RetVal = SteamVRHMD->GetCurrentPose(DeviceId, DeviceOrientation, OutPosition);
 			OutOrientation = DeviceOrientation.Rotator();
 		}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

		return RetVal;
	}

	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
	{
		ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
		FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
 		if (SteamVRHMD)
 		{
			int32 DeviceId = UnrealControllerIdToControllerIndex(ControllerIndex, DeviceHand);
			TrackingStatus = SteamVRHMD->GetControllerTrackingStatus(DeviceId);
 		}
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

		return TrackingStatus;
	}

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

	virtual bool IsGamepadAttached() const override
	{
		FSteamVRHMD* SteamVRSystem = GetSteamVRHMD();

		if (SteamVRSystem != nullptr)
		{
			// Check if at least one motion controller is tracked
			// Only need to check for at least one player (player index 0)
			int32 PlayerIndex = 0;
			ETrackingStatus LeftHandTrackingStatus = GetControllerTrackingStatus(PlayerIndex, EControllerHand::Left);
			ETrackingStatus RightHandTrackingStatus = GetControllerTrackingStatus(PlayerIndex, EControllerHand::Right);

			return LeftHandTrackingStatus == ETrackingStatus::Tracked || RightHandTrackingStatus == ETrackingStatus::Tracked;
		}

		return false;
	}

	static ESteamVRTouchDPadMapping DefaultDPadMapping;
private:

	inline vr::IVRSystem* GetVRSystem() const
	{
		if (SteamVRPlugin == nullptr)
		{
			SteamVRPlugin = &FModuleManager::LoadModuleChecked<ISteamVRPlugin>(TEXT("SteamVR"));
		}

		return SteamVRPlugin->GetVRSystem();
	}

	void RegisterDeviceChanges(vr::IVRSystem* VRSystem)
	{
		for (uint32 DeviceIndex = 0; DeviceIndex < vr::k_unMaxTrackedDeviceCount; ++DeviceIndex)
		{
			// see what kind of hardware this is
			vr::ETrackedDeviceClass DeviceClass = VRSystem->GetTrackedDeviceClass(DeviceIndex);

			switch (DeviceClass)
			{
			case vr::TrackedDeviceClass_Controller:
			{
				// Check connection status
				if (VRSystem->IsTrackedDeviceConnected(DeviceIndex))
				{
				// has the controller not been mapped yet
				if (DeviceToControllerMap[DeviceIndex] == INDEX_NONE)
				{
						RegisterController(DeviceIndex, VRSystem);
					}
				}
				// the controller has been disconnected, unmap it 
				else if (DeviceToControllerMap[DeviceIndex] != INDEX_NONE)
				{
					UnregisterController(DeviceIndex);
				}
			}
			break;
			case vr::TrackedDeviceClass_GenericTracker:
			{
				// Check connection status
				if (VRSystem->IsTrackedDeviceConnected(DeviceIndex))
				{
				// has the tracker not been mapped yet
				if (DeviceToControllerMap[DeviceIndex] == INDEX_NONE)
				{
						RegisterTracker(DeviceIndex);
					}
				}
				// the tracker has been disconnected, unmap it 
				else if (DeviceToControllerMap[DeviceIndex] != INDEX_NONE)
				{
					UnregisterTracker(DeviceIndex);
				}
			}
			break;
			case vr::TrackedDeviceClass_Invalid:
				// falls through
			case vr::TrackedDeviceClass_HMD:
				// falls through
			case vr::TrackedDeviceClass_TrackingReference:
				// falls through
			case vr::TrackedDeviceClass_DisplayRedirect:
				break;
			default:
				UE_LOG(LogSteamVRController, Warning, TEXT("Encountered unsupported device class of %i!"), (int32)DeviceClass);
				break;
			}
		}
	}

	bool RegisterController(uint32 DeviceIndex, vr::IVRSystem* VRSystem)
	{
		// don't map too many controllers
		if (NumControllersMapped >= MaxControllers)
		{
			UE_LOG(LogSteamVRController, Warning, TEXT("Found more controllers than we support (%i vs %i)!  Probably need to fix this."), NumControllersMapped + 1, MaxControllers);
			return false;
		}

		// Decide which hand to associate this controller with
		EControllerHand ChosenHand = EControllerHand::Special_9;
		{
			vr::ETrackedControllerRole Role = VRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex);
			UE_LOG(LogSteamVRController, Verbose, TEXT("Controller role for device %i is %i (invalid=0, left=1, right=2)."), DeviceIndex, (int32)Role);

			switch (Role)
			{
			case vr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
				ChosenHand = EControllerHand::Left;
				break;
			case vr::ETrackedControllerRole::TrackedControllerRole_RightHand:
				ChosenHand = EControllerHand::Right;
				break;
			case vr::ETrackedControllerRole::TrackedControllerRole_Invalid:
				// falls through
			default:
				return false;
			}
		}

		// determine which player controller to assign the device to
		int32 ControllerIndex = FMath::FloorToInt(NumControllersMapped / CONTROLLERS_PER_PLAYER);

		UE_LOG(LogSteamVRController, Verbose, TEXT("Controller device %i is being assigned unreal hand %i (left=0, right=1), for player %i."), DeviceIndex, (int32)ChosenHand, ControllerIndex);
		ControllerStates[DeviceIndex].Hand = ChosenHand;
		UnrealControllerHandUsageCount[(int32)ChosenHand] += 1;

		DeviceToControllerMap[DeviceIndex] = ControllerIndex;

		++NumControllersMapped;

		SetUnrealControllerIdToControllerIndex(DeviceToControllerMap[DeviceIndex], ControllerStates[DeviceIndex].Hand, DeviceIndex);

		return true;
	}

	void DetectHandednessSwap(vr::IVRSystem* VRSystem)
	{
		const uint32 LeftDeviceIndex = VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
		const uint32 RightDeviceIndex = VRSystem->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

		// both hands need to be assigned
		if (LeftDeviceIndex != vr::k_unTrackedDeviceIndexInvalid && RightDeviceIndex != vr::k_unTrackedDeviceIndexInvalid)
		{
			// see if our mappings don't match
			if (ControllerStates[LeftDeviceIndex].Hand != EControllerHand::Left || ControllerStates[RightDeviceIndex].Hand != EControllerHand::Right)
			{
				// explicitly assign the handedness
				ControllerStates[LeftDeviceIndex].Hand = EControllerHand::Left;
				ControllerStates[RightDeviceIndex].Hand = EControllerHand::Right;

				int32 ControllerIndex = DeviceToControllerMap[LeftDeviceIndex];

				SetUnrealControllerIdToControllerIndex(ControllerIndex, EControllerHand::Left, LeftDeviceIndex);
				SetUnrealControllerIdToControllerIndex(ControllerIndex, EControllerHand::Right, RightDeviceIndex);
			}
		}
	}

	bool RegisterTracker(uint32 DeviceIndex)
	{
		// check to see if there are any Special designations left, skip mapping it if there are not
		if (NumTrackersMapped >= MaxSpecialDesignations)
		{
			// go ahead and increment, so we can display a little more info in the log
			++NumTrackersMapped;
			UE_LOG(LogSteamVRController, Warning, TEXT("Unable to map VR tracker (#%i) to Special hand designation!"), NumTrackersMapped);
			return false;
		}

		// add the tracker to player 0
		DeviceToControllerMap[DeviceIndex] = GENERIC_TRACKER_PLAYER_NUM;

		// select next special #
		switch (NumTrackersMapped)
		{
		case 0:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_1;
			break;
		case 1:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_2;
			break;
		case 2:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_3;
			break;
		case 3:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_4;
			break;
		case 4:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_5;
			break;
		case 5:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_6;
			break;
		case 6:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_7;
			break;
		case 7:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_8;
			break;
		case 8:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_9;
			break;
		case 9:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_10;
			break;
		case 10:
			ControllerStates[DeviceIndex].Hand = EControllerHand::Special_11;
			break;
		default:
			// initial mapping verification above should catch any erroneous NumTrackersMapped
			check(false);
			break;
		}

		++NumTrackersMapped;
		UE_LOG(LogSteamVRController, Log, TEXT("Tracker device %i is being assigned unreal hand: Special %i, for player %i"), DeviceIndex, NumTrackersMapped, GENERIC_TRACKER_PLAYER_NUM);

		SetUnrealControllerIdToControllerIndex(DeviceToControllerMap[DeviceIndex], ControllerStates[DeviceIndex].Hand, DeviceIndex);

		return true;
	}

	void UnregisterController(uint32 DeviceIndex)
	{
		UnrealControllerHandUsageCount[(int32)ControllerStates[DeviceIndex].Hand] -= 1;
		UnregisterDevice(DeviceIndex);
		NumControllersMapped--;
	}

	void UnregisterTracker(uint32 DeviceIndex)
	{
		UnregisterDevice(DeviceIndex);
		NumTrackersMapped--;
	}

	void UnregisterDevice(uint32 DeviceIndex)
	{
		// undo the mappings
		SetUnrealControllerIdToControllerIndex(DeviceToControllerMap[DeviceIndex], ControllerStates[DeviceIndex].Hand, INDEX_NONE);
		DeviceToControllerMap[DeviceIndex] = INDEX_NONE;

		// re-zero out the controller state
		FMemory::Memzero(&ControllerStates[DeviceIndex], sizeof(FControllerState));
	}

	// Hack to prefer emitting MotionController keys for action events
	static bool MatchKeyNamePrefix(const FKey& Key, const TCHAR* Prefix)
	{
		return Key.GetFName().ToString().StartsWith(Prefix);
	};

	static bool MatchKeyNameSuffix(const FKey& Key, const TCHAR* Suffix)
	{
		return Key.GetFName().ToString().EndsWith(Suffix);
	};

	// Finds an axis key mapping from a list of mapping with the following preferences:
	// 1. Tries to find a FloatAxis key that starts with "MotionController" and ends with "X"
	// 2. Tries to find a FloatAxis key that starts with "MotionController" and ends with "Y"
	// 3. Any to find a FloatAxis key that starts with "MotionController"
	// 4. Any FloatAxis that ends with "X"
	// 5. Any FloatAxis that ends with "Y"
	// 6. Any FloatAxis
	// 7. Any Valid key.
	// If case 1 or 3 is matched bOutIsXAxis will be set to true. 
	static FName FindAxisKeyMapping(TArray<FInputAxisKeyMapping>& Mappings, bool& bOutIsXAxis)
	{
		FInputAxisKeyMapping* Found = nullptr;
		bOutIsXAxis = false;
		// First filter out all floatAxes, as all except the 5th case require a float axis.
		TArray<FInputAxisKeyMapping> FloatMappings = Mappings.FilterByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return Mapping.Key.IsFloatAxis();
		});

		// If there were no float axis key bindings, return the first valid mapping
		if (FloatMappings.Num() == 0)
		{
			Found = Mappings.FindByPredicate([](FInputAxisKeyMapping& Mapping)
			{
				return Mapping.Key.IsValid();
			});

			if (Found != nullptr)
			{
				return Found->Key.GetFName();
			}
			else
			{
				return FName();
			}
		}

		// Then get all mappings with keys starting with "MotionController"
		TArray<FInputAxisKeyMapping> MotionControllerMappings = FloatMappings.FilterByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return MatchKeyNamePrefix(Mapping.Key, TEXT("MotionController"));
		});

		// If there are no MotionController keys, search through all FloatAxes:
		TArray<FInputAxisKeyMapping>& MappingsSubset = MotionControllerMappings.Num() == 0 ? FloatMappings : MotionControllerMappings;

		Found = FloatMappings.FindByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return MatchKeyNameSuffix(Mapping.Key, TEXT("X"));
		});
		if (Found != nullptr)
		{
			bOutIsXAxis = true;
			return Found->Key.GetFName();
		}

		Found = FloatMappings.FindByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return MatchKeyNameSuffix(Mapping.Key, TEXT("Y"));
		});
		if (Found != nullptr)
		{
			return Found->Key.GetFName();
		}


		Found = FloatMappings.FindByPredicate([](const FInputAxisKeyMapping& Mapping)
		{
			return Mapping.Key.IsValid();
		});
		if (Found != nullptr)
		{
			return Found->Key.GetFName();
		}
		else
		{
			return FName();
		}
	}

	/** Returns the concatenation of two strings, skipping all characters at the beginning of string B that match the beginning of string A and
	    all characters at the end of string A that match the end of string B.
		Example: passing in "MoveUpAction" and "MoveRightAction" should result in "MoveUpRightAction"
		If the strings have no common suffix or prefix, the result will simply be the concatenation of both strings.
		If the strings are identical, returns the first string.

		The algorithm treats separator characters ' ', '_' and '/' differently. If either the suffix begins with one or the prefix ends with one,
		the function will keep one of them in the resulting string.
		Example "move_up_action" and "move_right_action" will result in "move_up_right_action" and not "move_upright_action"
	   */
	static FString MergeActionNames(const FString& A, const FString& B)
	{
		if (A.Equals(B, ESearchCase::CaseSensitive))
		{
			return A;
		}
		const int LastA = A.Len() - 1;
		const int LastB = B.Len() - 1;
		const int MinLen = (LastA < LastB) ? A.Len() : B.Len();

		int CommonPrefix = 0;
		int CommonSuffix = 0;
		for (;CommonPrefix < MinLen && A[CommonPrefix] == B[CommonPrefix]; CommonPrefix++)
		{
			/* intentionally blank */
		}

		for (;CommonSuffix < MinLen && A[LastA - CommonSuffix] == B[LastB - CommonSuffix]; CommonSuffix++)
		{
			/* intentionally blank */
		}

		// If either the common prefix ends with or the common suffix begins with a space, an underscore or a dash, keep one of them.
		if (CommonPrefix > 0 && (A[CommonPrefix - 1] == TEXT(' ') || A[CommonPrefix - 1] == TEXT('_') || A[CommonPrefix - 1] == TEXT('-')))
		{
			CommonPrefix--;
		}
		else if (CommonSuffix > 0 && (A[LastA - CommonSuffix + 1] == TEXT(' ') || A[LastA - CommonSuffix + 1] == TEXT('_') || A[LastA - CommonSuffix + 1] == TEXT('-')))
		{
			CommonSuffix--;
		}

		return A.LeftChop(CommonSuffix) + B.RightChop(CommonPrefix);
	}

#if WITH_EDITOR
	void BuildDefaultActionBindings(const FString& BindingsDir, TArray<TSharedPtr<FJsonValue>>& InOutDefaultBindings)
	{
		IFileManager& FileManager = FFileManagerGeneric::Get();

		TSet<FString> ExistingBindings;
		for (const TSharedPtr<FJsonValue>& Value : InOutDefaultBindings)
		{
			const TSharedPtr<FJsonObject>* Object;
			FString ControllerType;
			if (Value.IsValid() && Value->TryGetObject(Object) && (*Object)->TryGetStringField(TEXT("controller_type"), ControllerType) && !ControllerType.IsEmpty())
			{
				ExistingBindings.Emplace(ControllerType);
			}
		}

		// Create the directory if it doesn't exist.
		if (!FileManager.DirectoryExists(*BindingsDir))
		{
			FileManager.MakeDirectory(*BindingsDir);
		}
		
		static TTuple<const TCHAR*, FText> CommonControllerTypes[] =
		{
			MakeTuple(TEXT("vive"), NSLOCTEXT("SteamVR", "CTypeVive", "Vive") ),
			MakeTuple(TEXT("vive_controller"), NSLOCTEXT("SteamVR", "CTypeViveController", "Vive Controllers") ),
			MakeTuple(TEXT("oculus_touch"), NSLOCTEXT("SteamVR", "CTypeOculusTouch", "Oculus Touch Controllers") ),
			MakeTuple(TEXT("holographic_controller"), NSLOCTEXT("SteamVR", "CTypeHolographicController", "Holographic Controllers") ),
			MakeTuple(TEXT("gamepad"), NSLOCTEXT("SteamVR", "CTypeGamepad", "Game Pads") )
		};

		for (auto& Item : CommonControllerTypes)
		{
			
			// Skip if the controller type has already be defined
			if (ExistingBindings.Contains(Item.Key))
			{
				continue;
			}

			// Create a unique file path for the generated file.
			FString BindingsPath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*FString::Printf(TEXT("%s/%s.json"), *BindingsDir, Item.Key));
			int count = 0;
			while (FileManager.FileExists(*BindingsPath) && FileManager.FileSize(*BindingsPath) > 0)
			{
				BindingsPath = FileManager.ConvertToAbsolutePathForExternalAppForRead(*FString::Printf(TEXT("%s/%s_%d.json"), *BindingsDir , Item.Key, ++count));
			}


			// Creating a minimal bindings file without any bindings will allow editing it in the SteamVR bindings tool
			TSharedRef<FJsonObject> BindingsStub = MakeShareable(new FJsonObject());
			TSharedRef<FJsonObject> EmptyJsonObject = MakeShareable(new FJsonObject());
			BindingsStub->SetStringField(TEXT("name"), *FText::Format(NSLOCTEXT("SteamVR", "DefaultBindingsFor", "Default bindings for {0}"), Item.Value).ToString());
			BindingsStub->SetStringField(TEXT("controller_type"), Item.Key);

			// These two fields are required for SteamVR to accept the bindings file and allow editing it.
			BindingsStub->SetObjectField(TEXT("bindings"), EmptyJsonObject);
			BindingsStub->SetStringField(TEXT("description"), TEXT(""));

			// Print the stub bindings to a JSON string and save it to a file
			FString OutputJsonString;
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputJsonString);
			FJsonSerializer::Serialize(BindingsStub, JsonWriter);
			FFileHelper::SaveStringToFile(OutputJsonString, *BindingsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

			// Add the path of the generated file the action manifest
			TSharedRef<FJsonObject> BindingObject = MakeShareable(new FJsonObject());
			BindingObject->SetStringField(TEXT("controller_type"), Item.Key);
			BindingObject->SetStringField(TEXT("binding_url"), *BindingsPath);
			InOutDefaultBindings.Add(MakeShareable(new FJsonValueObject(BindingObject)));
		}
	}
#endif

	void BuildActionManifest()
	{
		vr::IVRInput* VRInput;

		if (bEnableVRInput && (VRInput = vr::VRInput()) != nullptr)
		{
			Actions.Empty();
			auto InputSettings = GetDefault<UInputSettings>();
			if (InputSettings != nullptr)
			{
				TArray<FName> ActionNames;
				InputSettings->GetActionNames(ActionNames);
				for (const auto& ActionName : ActionNames)
				{
					TArray<FInputActionKeyMapping> Mappings;
					InputSettings->GetActionMappingByName(ActionName, Mappings);

					FInputActionKeyMapping* KeyMapping = Mappings.FindByPredicate([](FInputActionKeyMapping& Mapping) 
					{ 
						return MatchKeyNamePrefix(Mapping.Key, TEXT("MotionController")); 
					});

					if (KeyMapping == nullptr)
					{
						KeyMapping = Mappings.FindByPredicate([](FInputActionKeyMapping& Mapping) { return Mapping.Key.IsValid(); });
					}

					if (KeyMapping != nullptr)
					{
						FString ActionPath = FString("/actions/main/in") / ActionName.ToString();
						Actions.Add(FSteamVRAction( ActionPath, ActionName, KeyMapping->Key.GetFName(), false ));
					}
				}

				TArray<FName> AxisNames;
				InputSettings->GetAxisNames(AxisNames);
				for (const auto& AxisName : AxisNames)
				{
					bool bIsXAxis = false;
					TArray<FInputAxisKeyMapping> Mappings;
					InputSettings->GetAxisMappingByName(AxisName, Mappings);

					FName KeyName = FindAxisKeyMapping(Mappings, bIsXAxis);

					if (!KeyName.IsNone())
					{
						FString ActionPath = FString("/actions/main/in") / AxisName.ToString() + TEXT("_axis");
						Actions.Add(FSteamVRAction( ActionPath, AxisName, KeyName, 0.0f ));

						// If the current axis is bound to an X axis, find the corresponding Y axis binding and create
						// a combined vector2 action from them (and if there were Z axes, create vector3 actions.)
						if (bIsXAxis)
						{
							FName YKeyName = FName(*(KeyName.ToString().LeftChop(1) + TEXT('Y')));
							FName ZKeyName = FName(*(KeyName.ToString().LeftChop(1) + TEXT('Z')));
							FName YAxisName, ZAxisName;

							for (const auto& InnerAxisName : AxisNames)
							{
								TArray<FInputAxisKeyMapping> InnerMappings;
								InputSettings->GetAxisMappingByName(InnerAxisName, InnerMappings);

								if (YAxisName.IsNone() && 
									InnerMappings.ContainsByPredicate([&YKeyName](auto& Mapping)
									{
										return Mapping.Key.GetFName() == YKeyName;
									})
								)
								{
									YAxisName = InnerAxisName;
								}

								if (ZAxisName.IsNone() && 
									InnerMappings.ContainsByPredicate([&ZKeyName](auto& Mapping)
									{
										return Mapping.Key.GetFName() == ZKeyName;
									})
								)
								{
									ZAxisName = InnerAxisName;
								}

								if (!YAxisName.IsNone() && !ZAxisName.IsNone())
								{
									break;
								}

							}

							if (!YAxisName.IsNone())
							{
								FString CombinedAxisName = MergeActionNames(AxisName.ToString(), YAxisName.ToString());
								if (!ZAxisName.IsNone())
								{
									CombinedAxisName = MergeActionNames(CombinedAxisName, ZAxisName.ToString());
									FString CombinedActionPath = FString("/actions/main/in") / CombinedAxisName + TEXT("_axis3d");
									Actions.Add(FSteamVRAction(CombinedActionPath, FName(*CombinedAxisName), KeyName, YKeyName, ZKeyName, FVector::ZeroVector));
								}
								else
								{
									FString CombinedActionPath = FString("/actions/main/in") / CombinedAxisName + TEXT("_axis2d");
									Actions.Add(FSteamVRAction(CombinedActionPath, FName(*CombinedAxisName), KeyName, YKeyName, FVector2D::ZeroVector));
								}
							}
									
						}
					}
				}

				// Open console
				{
					const FKey* ConsoleKey = InputSettings->ConsoleKeys.FindByPredicate([](FKey& Key) { return Key.IsValid(); });
					if (ConsoleKey != nullptr)
					{
						Actions.Add(FSteamVRAction( FString("/actions/main/in/open_console"), FName(TEXT("Open Console")), ConsoleKey->GetFName(), false ));
					}
				}
			}

			if (Actions.Num() > 0)
			{
				// The steamvr_actions.json file is generated from internal data, so it goes to GeneratedConfig directory.
				const FString ManifestPath = FPaths::GeneratedConfigDir() / TEXT("steamvr_actions.json");
				// The default bindings files need to be generated by the developer, so they are stored in the project config directory.
				const FString BindingsDir = FPaths::ProjectConfigDir() / TEXT("SteamVRBindings");

				TSharedPtr<FJsonObject> DescriptionsObject = MakeShareable(new FJsonObject);

				TArray<TSharedPtr<FJsonValue>> ActionsArray;
				for (auto Action : Actions)
				{
					TSharedRef<FJsonObject> ActionObject = MakeShareable(new FJsonObject());
					ActionObject->SetStringField(TEXT("name"), Action.Path);
					ActionObject->SetStringField(TEXT("type"), Action.TypeAsString());
					ActionObject->SetStringField(TEXT("requirement"), TEXT("optional"));
					ActionsArray.Add(MakeShareable(new FJsonValueObject(ActionObject)));

					DescriptionsObject->SetStringField(Action.Path, Action.Name.ToString());
				}

				TArray<TSharedPtr<FJsonValue>> DefaultBindings;
				{
					IFileManager& FileManager = FFileManagerGeneric::Get();

					// Find any default bindings stored in the project bindings dir.
					// They must be saved as <PROJECT_CONFIG_DIR>/SteamVRBindings/<CONTROLLER_TYPE>.json in order to be included in the manifest.
					TArray<FString> FoundFiles;
					FileManager.FindFiles(FoundFiles, *BindingsDir, TEXT("*.json"));
					UE_LOG(LogSteamVRController, Log, TEXT("Searching for bindings files in %s"), *BindingsDir);
					for (FString& File : FoundFiles)
					{
						FString ControllerType;
						FString JsonStr;
						FString FilePath = BindingsDir / File;
						FFileHelper::LoadFileToString(JsonStr, *FilePath);
						TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonStr);
						TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
						if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
						{
							UE_LOG(LogSteamVRController, Warning, TEXT("Invalid controller binding file %s: Invalid JSON."), *FilePath);
							continue;
						}

						if (!JsonObject->TryGetStringField(TEXT("controller_type"), ControllerType) || ControllerType.IsEmpty())
						{
							UE_LOG(LogSteamVRController, Warning, TEXT("Invalid controller binding file %s: Missing or empty controller_type field."), *FilePath);
							continue;
						}

						TSharedRef<FJsonObject> BindingObject = MakeShareable(new FJsonObject());
						BindingObject->SetStringField(TEXT("controller_type"), *ControllerType);
						BindingObject->SetStringField(TEXT("binding_url"), *FileManager.ConvertToAbsolutePathForExternalAppForRead(*FilePath));
						DefaultBindings.Add(MakeShareable(new FJsonValueObject(BindingObject)));
					}

#if WITH_EDITOR
					BuildDefaultActionBindings(BindingsDir, DefaultBindings);
					check(DefaultBindings.Num());
#else
					if (DefaultBindings.Num() == 0)
					{
						UE_LOG(LogSteamVRController, Error, TEXT("No default Steam VR Input bindings found in %s."), *BindingsDir);
					}
#endif
				}

				TArray<TSharedPtr<FJsonValue>> ActionSets;
				{
					TSharedRef<FJsonObject> ActionSetObject = MakeShareable(new FJsonObject());
					ActionSetObject->SetStringField(TEXT("name"), TEXT("/actions/main"));
					ActionSetObject->SetStringField(TEXT("usage"), TEXT("leftright"));
					ActionSets.Add(MakeShareable(new FJsonValueObject(ActionSetObject)));

					DescriptionsObject->SetStringField(TEXT("/actions/main"), TEXT("Main Game Actions"));
				}

				DescriptionsObject->SetStringField(TEXT("language_tag"), TEXT("en"));
				TArray<TSharedPtr<FJsonValue>> Localization;
				{
					Localization.Add(MakeShareable(new FJsonValueObject(DescriptionsObject)));
				}

				TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject());
				RootObject->SetArrayField(TEXT("default_bindings"), DefaultBindings);
				RootObject->SetArrayField(TEXT("actions"), ActionsArray);
				RootObject->SetArrayField(TEXT("action_sets"), ActionSets);
				RootObject->SetArrayField(TEXT("localization"), Localization);

				// Print the JSON data to a string
				FString OutputJsonString;
				TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&OutputJsonString);
				FJsonSerializer::Serialize(RootObject, JsonWriter);

				// Save the JSON string (force UTF-8 for JSON files.)
				if (!FFileHelper::SaveStringToFile(OutputJsonString, *ManifestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					UE_LOG(LogSteamVRController, Error, TEXT("Failed to save action manifest '%s'."), *ManifestPath);
					return;
				}

				vr::EVRInputError Err = VRInput->SetActionManifestPath(TCHAR_TO_ANSI(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ManifestPath)));

				if (Err != vr::VRInputError_None)
				{
					UE_LOG(LogSteamVRController, Error, TEXT("Failed to pass action manifest, %s, to SteamVR. Error: %d"), *ManifestPath, (int32)Err);
				}

				// Get the action set handle for our main action set
				Err = VRInput->GetActionSetHandle("/actions/main", &MainActionSet);
				if (Err != vr::VRInputError_None)
				{
					UE_LOG(LogSteamVRController, Error, TEXT("Couldn't get main action set handle. Error: %d"), (int32)Err);
				}

				// Fill in Action handles for each registered action
				for (auto& Action : Actions)
				{
					vr::VRActionHandle_t Handle;
					Err = VRInput->GetActionHandle(TCHAR_TO_ANSI(*Action.Path), &Handle);
					Action.Handle = Handle;
					if (Err != vr::VRInputError_None || !Action.Handle)
					{
						UE_LOG(LogSteamVRController, Warning, TEXT("Couldn't get main action handle for %s. Error: %d"), *Action.Path, (int32)Err);
					}
				}
#if WITH_EDITOR
				if (!ActionMappingsChangedHandle.IsValid())
				{
					ActionMappingsChangedHandle = FEditorDelegates::OnActionAxisMappingsChanged.AddLambda([this]()
					{
						UE_LOG(LogSteamVRController, Warning, TEXT("You will need to quit and restart both SteamVR and the Editor in order to use the modified input actions or axes."));
					});
				}
#endif
			}
		}
	}

	struct FControllerState
	{
		/** Which hand this controller is representing */
		EControllerHand Hand;

		/** If packet num matches that on your prior call, then the controller state hasn't been changed since 
		  * your last call and there is no need to process it. */
		uint32 PacketNum;

		/** touchpad analog values */
		float TouchPadXAnalog;
		float TouchPadYAnalog;

		/** trigger analog value */
		float TriggerAnalog;

		/** Knuckles Controller Axes */
		float HandGripAnalog;
		float IndexGripAnalog;
		float MiddleGripAnalog;
		float RingGripAnalog;
		float PinkyGripAnalog;

		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[ ESteamVRControllerButton::TotalButtonCount ];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[ ESteamVRControllerButton::TotalButtonCount ];

		/** Value for force feedback on this controller hand */
		float ForceFeedbackValue;
	};

	struct FSteamVRAction
	{
		enum EActionType
		{
			Boolean,
			Vector1,
			Vector2,
			Vector3,
			Vibration,
			Pose,
			Skeleton,
			Invalid
		};

		FString		Path;
		EActionType	Type;
		FName		Name;
		FName		ActionKey_X;
		FName		ActionKey_Y;
		FName		ActionKey_Z;
		union {
			bool	bState;
			FVector Value;
		};

		vr::VRActionHandle_t Handle;
		vr::EVRInputError LastError;

		FString TypeAsString()
		{
			static FString TypeStrings[] = {
				TEXT("boolean"),
				TEXT("vector1"),
				TEXT("vector2"),
				TEXT("vector3"),
				TEXT("vibration"),
				TEXT("pose"),
				TEXT("skeleton"),
				TEXT("")
			};

			return TypeStrings[(int)Type];
		}

		FSteamVRAction(const FString& inPath, const FName& inName, const FName& inActionKey, bool inState)
			: Path(inPath)
			, Type(Boolean)
			, Name(inName)
			, ActionKey_X(inActionKey)
			, ActionKey_Y()
			, ActionKey_Z()
			, Value()
			, Handle()
			, LastError(vr::VRInputError_None)
		{
			bState = inState;
		}

		FSteamVRAction(const FString& inPath, const FName& inName, const FName& inActionKey, float inValue1D)
			: Path(inPath)
			, Type(Vector1)
			, Name(inName)
			, ActionKey_X(inActionKey)
			, ActionKey_Y()
			, ActionKey_Z()
			, Value(inValue1D, 0, 0)
			, Handle()
			, LastError(vr::VRInputError_None)
		{}

		FSteamVRAction(const FString& inPath, const FName& inName, const FName& inActionKey_X, const FName& inActionKey_Y, const FVector2D& inValue2D)
			: Path(inPath)
			, Type(Vector2)
			, Name(inName)
			, ActionKey_X(inActionKey_X)
			, ActionKey_Y(inActionKey_Y)
			, ActionKey_Z()
			, Value(inValue2D.X, inValue2D.Y, 0)
			, Handle()
			, LastError(vr::VRInputError_None)
		{}

		FSteamVRAction(const FString& inPath, const FName& inName, const FName& inActionKey_X, const FName& inActionKey_Y, const FName& inActionKey_Z, const FVector& inValue3D)
			: Path(inPath)
			, Type(Vector2)
			, Name(inName)
			, ActionKey_X(inActionKey_X)
			, ActionKey_Y(inActionKey_Y)
			, ActionKey_Z(inActionKey_Z)
			, Value(inValue3D)
			, Handle()
			, LastError(vr::VRInputError_None)
		{}

	};

	/** Mappings between tracked devices and 0 indexed controllers */
	int32 NumControllersMapped;
	int32 NumTrackersMapped;
	int32 DeviceToControllerMap[ vr::k_unMaxTrackedDeviceCount ];
	int32 UnrealControllerIdAndHandToDeviceIdMap[ MaxUnrealControllers ][ vr::k_unMaxTrackedDeviceCount ];
	int32 UnrealControllerHandUsageCount[CONTROLLERS_PER_PLAYER];

	/** Controller states */
	FControllerState ControllerStates[ MaxControllers ];

	TArray<FSteamVRAction> Actions;
	vr::VRActionSetHandle_t MainActionSet;

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** Mapping of controller buttons */
	FGamepadKeyNames::Type Buttons[ vr::k_unMaxTrackedDeviceCount ][ ESteamVRControllerButton::TotalButtonCount ];

	/** weak pointer to the IVRSystem owned by the HMD module */
	TWeakPtr<vr::IVRSystem> HMDVRSystem;

#if WITH_EDITOR
	FDelegateHandle ActionMappingsChangedHandle;
#endif

#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS

	/** Whether the VRInput API is enabled or not */
	bool bEnableVRInput;

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** the SteamVR plugin module */
	mutable ISteamVRPlugin* SteamVRPlugin;
};

FName FSteamVRController::DeviceTypeName(TEXT("SteamVRController"));
/// @cond DOXYGEN_WARNINGS
#if STEAMVR_SUPPORTED_PLATFORMS
ESteamVRTouchDPadMapping FSteamVRController::DefaultDPadMapping = ESteamVRTouchDPadMapping::FaceButtons;
#endif // STEAMVR_SUPPORTED_PLATFORMS
/// @endcond

// defined here in this .cpp file so we have access to FSteamVRController
void USteamVRControllerLibrary::SetTouchDPadMapping(ESteamVRTouchDPadMapping NewMapping)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	// modify the default mapping in case we haven't instantiated a FSteamVRController yet
	FSteamVRController::DefaultDPadMapping = NewMapping;
#endif // STEAMVR_SUPPORTED_PLATFORMS

	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (IMotionController* MotionController : MotionControllers)
	{
		if (MotionController != nullptr && MotionController->GetMotionControllerDeviceTypeName() == FSteamVRController::DeviceTypeName)
		{
			static_cast<FSteamVRController*>(MotionController)->SetTouchDPadMapping(NewMapping);
		}
	}
}

class FSteamVRControllerPlugin : public ISteamVRControllerPlugin
{
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
		return TSharedPtr< class IInputDevice >(new FSteamVRController(InMessageHandler));
	}
};

#undef LOCTEXT_NAMESPACE //"SteamVRController"

IMPLEMENT_MODULE( FSteamVRControllerPlugin, SteamVRController)
