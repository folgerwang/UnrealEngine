// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealitySpatialInput.h"

#include "WindowsMixedRealityStatics.h"

#define LOCTEXT_NAMESPACE "WindowsMixedRealitySpatialInput"
#define MotionControllerDeviceTypeName "WindowsMixedRealitySpatialInput"

#define WindowsMixedRealityCategory "WindowsMixedRealitySubCategory"
#define WindowsMixedRealityCategoryName "WindowsMixedReality"
#define WindowsMixedRealityCategoryFriendlyName "Windows Mixed Reality"

namespace WindowsMixedReality
{
	FWindowsMixedRealitySpatialInput::FWindowsMixedRealitySpatialInput(
		const TSharedRef< FGenericApplicationMessageHandler > & InMessageHandler)
		: MessageHandler(InMessageHandler)
	{
		RegisterKeys();

		InitializeSpatialInput();
	}

	FWindowsMixedRealitySpatialInput::~FWindowsMixedRealitySpatialInput()
	{
		UninitializeSpatialInput();
	}

	void FWindowsMixedRealitySpatialInput::Tick(float DeltaTime)
	{
		if (!FWindowsMixedRealityStatics::SupportsSpatialInput())
		{
			return;
		}

		if (!IsInitialized)
		{
			// We failed to initialize in the constructor. Try again.
			InitializeSpatialInput();
			return;
		}
	}

	void FWindowsMixedRealitySpatialInput::SendControllerEvents()
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (!FWindowsMixedRealityStatics::PollInput())
		{
			return;
		}

		const uint32 sourceId = 0;
		SendButtonEvents(sourceId);
		SendAxisEvents(sourceId);
#endif
	}

#if WITH_WINDOWS_MIXED_REALITY
	void SendControllerButtonEvent(
		TSharedPtr< FGenericApplicationMessageHandler > messageHandler,
		uint32 controllerId,
		FKey button,
		MixedRealityInterop::HMDInputPressState pressState) noexcept
	{
		FName buttonName = button.GetFName();

		if (pressState == MixedRealityInterop::HMDInputPressState::NotApplicable)
		{
			// No event should be sent.
			return;
		}

		if (pressState == MixedRealityInterop::HMDInputPressState::Pressed)
		{
			// Send the press event.
			messageHandler->OnControllerButtonPressed(
				buttonName,
				static_cast<int32>(controllerId),
				false);
		}
		else
		{
			// Send the release event
			messageHandler->OnControllerButtonReleased(
				buttonName,
				static_cast<int32>(controllerId),
				false);
		}
	}
#endif

	void SendControllerAxisEvent(
		TSharedPtr< FGenericApplicationMessageHandler > messageHandler,
		uint32 controllerId,
		FKey axis,
		double axisPosition) noexcept
	{
		FName axisName = axis.GetFName();

		messageHandler->OnControllerAnalog(
			axisName,
			static_cast<int32>(controllerId),
			static_cast<float>(axisPosition));
	}

#if WITH_WINDOWS_MIXED_REALITY
	void FWindowsMixedRealitySpatialInput::SendAxisEvents(uint32 source)
	{
		FKey key;
		float position;

		for (int i = 0; i < 2; i++)
		{
			MixedRealityInterop::HMDHand hand = (MixedRealityInterop::HMDHand)i;

			// Trigger
			position = FWindowsMixedRealityStatics::GetAxisPosition(hand, MixedRealityInterop::HMDInputControllerAxes::SelectValue);
			key = (hand == MixedRealityInterop::HMDHand::Left) ?
				EKeys::MotionController_Left_TriggerAxis :
				EKeys::MotionController_Right_TriggerAxis;

			SendControllerAxisEvent(MessageHandler, source, key, position);

			// Thumbstick X
			position = FWindowsMixedRealityStatics::GetAxisPosition(hand, MixedRealityInterop::HMDInputControllerAxes::ThumbstickX);
			key = (hand == MixedRealityInterop::HMDHand::Left) ?
				EKeys::MotionController_Left_Thumbstick_X :
				EKeys::MotionController_Right_Thumbstick_X;

			SendControllerAxisEvent(MessageHandler, source, key, position);

			// Thumbstick Y
			position = FWindowsMixedRealityStatics::GetAxisPosition(hand, MixedRealityInterop::HMDInputControllerAxes::ThumbstickY);
			key = (hand == MixedRealityInterop::HMDHand::Left) ?
				EKeys::MotionController_Left_Thumbstick_Y :
				EKeys::MotionController_Right_Thumbstick_Y;

			SendControllerAxisEvent(MessageHandler, source, key, position);

			// Touchpad X
			position = FWindowsMixedRealityStatics::GetAxisPosition(hand, MixedRealityInterop::HMDInputControllerAxes::TouchpadX);
			key = (hand == MixedRealityInterop::HMDHand::Left) ?
				FSpatialInputKeys::LeftTouchpadX :
				FSpatialInputKeys::RightTouchpadX;

			if ((key == FSpatialInputKeys::LeftTouchpadX && !isLeftTouchpadTouched) ||
				(key == FSpatialInputKeys::RightTouchpadX && !isRightTouchpadTouched))
			{
				position = 0.0f;
			}

			SendControllerAxisEvent(MessageHandler, source, key, position);

			// Touchpad Y
			position = FWindowsMixedRealityStatics::GetAxisPosition(hand, MixedRealityInterop::HMDInputControllerAxes::TouchpadY);
			key = (hand == MixedRealityInterop::HMDHand::Left) ?
				FSpatialInputKeys::LeftTouchpadY :
				FSpatialInputKeys::RightTouchpadY;

			if ((key == FSpatialInputKeys::LeftTouchpadY && !isLeftTouchpadTouched) ||
				(key == FSpatialInputKeys::RightTouchpadY && !isRightTouchpadTouched))
			{
				position = 0.0f;
			}

			SendControllerAxisEvent(MessageHandler, source, key, position);
		}
	}

	void FWindowsMixedRealitySpatialInput::SendButtonEvents(uint32 source)
	{
		MixedRealityInterop::HMDInputPressState pressState;
		FKey key;

		for (int i = 0; i < 2; i++)
		{
			MixedRealityInterop::HMDHand hand = (MixedRealityInterop::HMDHand)i;

			// Select
			pressState = FWindowsMixedRealityStatics::GetPressState(hand, MixedRealityInterop::HMDInputControllerButtons::Select);
			if (pressState != MixedRealityInterop::HMDInputPressState::NotApplicable)
			{
				key = (hand == MixedRealityInterop::HMDHand::Left) ?
					EKeys::MotionController_Left_Trigger :
					EKeys::MotionController_Right_Trigger;

				SendControllerButtonEvent(MessageHandler, source, key, pressState);
			}

			// Grasp
			pressState = FWindowsMixedRealityStatics::GetPressState(hand, MixedRealityInterop::HMDInputControllerButtons::Grasp);
			if (pressState != MixedRealityInterop::HMDInputPressState::NotApplicable)
			{
				key = (hand == MixedRealityInterop::HMDHand::Left) ?
					EKeys::MotionController_Left_Grip1 :
					EKeys::MotionController_Right_Grip1;

				SendControllerButtonEvent(MessageHandler, source, key, pressState);
			}

			// Menu
			pressState = FWindowsMixedRealityStatics::GetPressState(hand, MixedRealityInterop::HMDInputControllerButtons::Menu);
			if (pressState != MixedRealityInterop::HMDInputPressState::NotApplicable)
			{
				key = (hand == MixedRealityInterop::HMDHand::Left) ?
					FSpatialInputKeys::LeftMenu :
					FSpatialInputKeys::RightMenu;

				SendControllerButtonEvent(MessageHandler, source, key, pressState);
			}

			// Thumbstick press
			pressState = FWindowsMixedRealityStatics::GetPressState(hand, MixedRealityInterop::HMDInputControllerButtons::Thumbstick);
			if (pressState != MixedRealityInterop::HMDInputPressState::NotApplicable)
			{
				key = (hand == MixedRealityInterop::HMDHand::Left) ?
					EKeys::MotionController_Left_Thumbstick :
					EKeys::MotionController_Right_Thumbstick;

				SendControllerButtonEvent(MessageHandler, source, key, pressState);
			}

			// Touchpad press
			pressState = FWindowsMixedRealityStatics::GetPressState(hand, MixedRealityInterop::HMDInputControllerButtons::Touchpad);
			if (pressState != MixedRealityInterop::HMDInputPressState::NotApplicable)
			{
				key = (hand == MixedRealityInterop::HMDHand::Left) ?
					FSpatialInputKeys::LeftTouchpadPress :
					FSpatialInputKeys::RightTouchpadPress;

				SendControllerButtonEvent(MessageHandler, source, key, pressState);
			}

			// Touchpad touch
			pressState = FWindowsMixedRealityStatics::GetPressState(hand, MixedRealityInterop::HMDInputControllerButtons::TouchpadIsTouched);
			if (pressState != MixedRealityInterop::HMDInputPressState::NotApplicable)
			{
				key = (hand == MixedRealityInterop::HMDHand::Left) ?
					FSpatialInputKeys::LeftTouchpadIsTouched :
					FSpatialInputKeys::RightTouchpadIsTouched;

				if (key == FSpatialInputKeys::LeftTouchpadIsTouched)
				{
					isLeftTouchpadTouched = pressState == MixedRealityInterop::HMDInputPressState::Pressed;
				}
				else if (key == FSpatialInputKeys::RightTouchpadIsTouched)
				{
					isRightTouchpadTouched = pressState == MixedRealityInterop::HMDInputPressState::Pressed;
				}

				SendControllerButtonEvent(MessageHandler, source, key, pressState);
			}
		}
	}
#endif

	void FWindowsMixedRealitySpatialInput::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	{
		MessageHandler = InMessageHandler;
	}

	bool FWindowsMixedRealitySpatialInput::Exec(UWorld * InWorld, const TCHAR * Cmd, FOutputDevice & Ar)
	{
		return false;
	}

	void FWindowsMixedRealitySpatialInput::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
	{
		// Large channel type maps to amplitude. We are interested in amplitude.
		if ((ChannelType == FForceFeedbackChannelType::LEFT_LARGE) ||
			(ChannelType == FForceFeedbackChannelType::RIGHT_LARGE))
		{
			// SpatialInteractionController supports SimpleHapticsController. Amplitude is the value
			// we need to send. Set Frequency to 1.0f so that the amplitude is properly sent to the
			// controller.
			FHapticFeedbackValues hapticValues = FHapticFeedbackValues(1.0f, Value);
			EControllerHand controllerHand = (ChannelType == FForceFeedbackChannelType::LEFT_LARGE) ?
				EControllerHand::Left : EControllerHand::Right;

			SetHapticFeedbackValues(
				ControllerId,
				(int32)controllerHand,
				hapticValues);
		}
	}

	void FWindowsMixedRealitySpatialInput::SetChannelValues(int32 ControllerId, const FForceFeedbackValues & values)
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

	void FWindowsMixedRealitySpatialInput::SetHapticFeedbackValues(int32 ControllerId, int32 DeviceHand, const FHapticFeedbackValues & Values)
	{
		EControllerHand controllerHand = (EControllerHand)DeviceHand;
		if ((controllerHand != EControllerHand::Left) &&
			(controllerHand != EControllerHand::Right))
		{
			return;
		}

#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop::HMDHand hand = (MixedRealityInterop::HMDHand)DeviceHand;
		FWindowsMixedRealityStatics::SubmitHapticValue(hand, (Values.Frequency > 0.0f) ? Values.Amplitude : 0.0f);
#endif
	}

	void FWindowsMixedRealitySpatialInput::GetHapticFrequencyRange(float & MinFrequency, float & MaxFrequency) const
	{
		MinFrequency = 0.0f;
		MaxFrequency = 1.0f;
	}

	float FWindowsMixedRealitySpatialInput::GetHapticAmplitudeScale() const
	{
		return 1.0f;
	}

	FName FWindowsMixedRealitySpatialInput::GetMotionControllerDeviceTypeName() const
	{
		const static FName DeviceTypeName(TEXT(MotionControllerDeviceTypeName));
		return DeviceTypeName;
	}

	bool FWindowsMixedRealitySpatialInput::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator & OutOrientation, FVector & OutPosition, float WorldToMetersScale) const
	{
#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop::HMDHand hand = (MixedRealityInterop::HMDHand)((int)DeviceHand);

		bool success = FWindowsMixedRealityStatics::GetControllerOrientationAndPosition(hand, OutOrientation, OutPosition);
		OutPosition *= WorldToMetersScale;

		return success;
#else
		return false;
#endif
	}

	ETrackingStatus FWindowsMixedRealitySpatialInput::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
	{
#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop::HMDHand hand = (MixedRealityInterop::HMDHand)((int)DeviceHand);
		MixedRealityInterop::HMDTrackingStatus trackingStatus = FWindowsMixedRealityStatics::GetControllerTrackingStatus(hand);

		return (ETrackingStatus)((int)trackingStatus);
#else
		return ETrackingStatus::NotTracked;
#endif
	}

	void FWindowsMixedRealitySpatialInput::RegisterKeys() noexcept
	{
		EKeys::AddMenuCategoryDisplayInfo(
			WindowsMixedRealityCategoryName,
			LOCTEXT(WindowsMixedRealityCategory, WindowsMixedRealityCategoryFriendlyName),
			TEXT("GraphEditor.PadEvent_16x"));

		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::LeftMenu,
			LOCTEXT(LeftMenuName, LeftMenuFriendlyName),
			FKeyDetails::GamepadKey,
			WindowsMixedRealityCategoryName));
		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::RightMenu,
			LOCTEXT(RightMenuName, RightMenuFriendlyName),
			FKeyDetails::GamepadKey,
			WindowsMixedRealityCategoryName));

		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::LeftTouchpadPress,
			LOCTEXT(LeftTouchpadPressName, LeftTouchpadPressFriendlyName),
			FKeyDetails::GamepadKey,
			WindowsMixedRealityCategoryName));
		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::RightTouchpadPress,
			LOCTEXT(RightTouchpadPressName, RightTouchpadPressFriendlyName),
			FKeyDetails::GamepadKey,
			WindowsMixedRealityCategoryName));

		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::LeftTouchpadIsTouched,
			LOCTEXT(LeftTouchpadIsTouchedName, LeftTouchpadIsTouchedFriendlyName),
			FKeyDetails::GamepadKey,
			WindowsMixedRealityCategoryName));
		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::RightTouchpadIsTouched,
			LOCTEXT(RightTouchpadIsTouchedName, RightTouchpadIsTouchedFriendlyName),
			FKeyDetails::GamepadKey,
			WindowsMixedRealityCategoryName));

		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::LeftTouchpadX,
			LOCTEXT(LeftTouchpadXName, LeftTouchpadXFriendlyName),
			FKeyDetails::GamepadKey | FKeyDetails::FloatAxis,
			WindowsMixedRealityCategoryName));
		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::RightTouchpadX,
			LOCTEXT(RightTouchpadXName, RightTouchpadXFriendlyName),
			FKeyDetails::GamepadKey | FKeyDetails::FloatAxis,
			WindowsMixedRealityCategoryName));

		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::LeftTouchpadY,
			LOCTEXT(LeftTouchpadYName, LeftTouchpadYFriendlyName),
			FKeyDetails::GamepadKey | FKeyDetails::FloatAxis,
			WindowsMixedRealityCategoryName));
		EKeys::AddKey(FKeyDetails(
			FSpatialInputKeys::RightTouchpadY,
			LOCTEXT(RightTouchpadYName, RightTouchpadYFriendlyName),
			FKeyDetails::GamepadKey | FKeyDetails::FloatAxis,
			WindowsMixedRealityCategoryName));
	}

	void FWindowsMixedRealitySpatialInput::InitializeSpatialInput() noexcept
	{
		if (IsInitialized ||
			!FWindowsMixedRealityStatics::SupportsSpatialInput())
		{
			return;
		}

		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

		IsInitialized = true;
	}

	void FWindowsMixedRealitySpatialInput::UninitializeSpatialInput() noexcept
	{
		if (!IsInitialized)
		{
			return;
		}

		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}
}

#undef LOCTEXT_NAMESPACE // "WindowsMixedRealitySpatialInput"