#if WITH_MLSDK
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

FMagicLeapTouchpadGesture MLToUnrealTouchpadGesture(EControllerHand hand, FName MotionSource, const MLInputControllerTouchpadGesture& touchpad_gesture)
{
	FMagicLeapTouchpadGesture gesture;
	gesture.Hand = hand;
	gesture.MotionSource = MotionSource;
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

const FName& MLToUnrealButton(EControllerHand Hand, MLInputControllerButton ml_button)
{
	static const FName empty;

	switch (ml_button)
	{
	case MLInputControllerButton_Move:
		if (Hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::Left_MoveButton_Name;
		}
		return FMagicLeapControllerKeyNames::Right_MoveButton_Name;
	case MLInputControllerButton_App:
		if (Hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::Left_AppButton_Name;
		}
		return FMagicLeapControllerKeyNames::Right_AppButton_Name;
	case MLInputControllerButton_Bumper:
		if (Hand == EControllerHand::Left)
		{
			return FGamepadKeyNames::MotionController_Left_Shoulder;
		}
		return FGamepadKeyNames::MotionController_Right_Shoulder;
	case MLInputControllerButton_HomeTap:
		if (Hand == EControllerHand::Left)
		{
			return FMagicLeapControllerKeyNames::Left_HomeButton_Name;
		}
		return FMagicLeapControllerKeyNames::Right_HomeButton_Name;
	default:
		break;
	}
	return empty;
}

const FName& MLToUnrealButton(FName MotionSource, MLInputControllerButton ml_button)
{
	static const FName empty;
	return empty;
}
#endif //WITH_MLSDK


const FName& MLTouchToUnrealThumbstickAxis(EControllerHand Hand, uint32 TouchIndex)
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

const FName& MLTouchToUnrealThumbstickButton(EControllerHand Hand)
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

const FName& MLTriggerToUnrealTriggerAxis(EControllerHand Hand)
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

const FName& MLTriggerToUnrealTriggerKey(EControllerHand Hand)
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

MLInputControllerFeedbackPatternLED UnrealToMLPatternLED(EMLControllerLEDPattern LEDPattern)
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

MLInputControllerFeedbackEffectLED UnrealToMLEffectLED(EMLControllerLEDEffect LEDEffect)
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
MLInputControllerFeedbackColorLED UnrealToMLColorLED(EMLControllerLEDColor LEDColor)
{
	switch (LEDColor)
	{
		LED_COLOR_CASE(BrightMissionRed)
			LED_COLOR_CASE(PastelMissionRed)
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

MLInputControllerFeedbackEffectSpeedLED UnrealToMLSpeedLED(EMLControllerLEDSpeed LEDSpeed)
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

MLInputControllerFeedbackPatternVibe UnrealToMLPatternVibe(EMLControllerHapticPattern HapticPattern)
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

MLInputControllerFeedbackIntensity UnrealToMLHapticIntensity(EMLControllerHapticIntensity HapticIntensity)
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

