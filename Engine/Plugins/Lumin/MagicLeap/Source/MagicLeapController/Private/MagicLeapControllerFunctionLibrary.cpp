// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapControllerFunctionLibrary.h"
#include "IMagicLeapControllerPlugin.h"
#include "MagicLeapController.h"
#include "IMagicLeapPlugin.h"

bool UMagicLeapControllerFunctionLibrary::PlayLEDPattern(FName MotionSource, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->PlayLEDPattern(MotionSource, LEDPattern, LEDColor, DurationInSec);
	}
	return false;
}

bool UMagicLeapControllerFunctionLibrary::PlayLEDEffect(FName MotionSource, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->PlayLEDEffect(MotionSource, LEDEffect, LEDSpeed, LEDPattern, LEDColor, DurationInSec);
	}
	return false;
}

bool UMagicLeapControllerFunctionLibrary::PlayHapticPattern(FName MotionSource, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->PlayHapticPattern(MotionSource, HapticPattern, Intensity);
	}
	return false;
}

bool UMagicLeapControllerFunctionLibrary::SetControllerTrackingMode(EMLControllerTrackingMode TrackingMode)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->SetControllerTrackingMode(TrackingMode);
	}
	return false;
}

EMLControllerTrackingMode UMagicLeapControllerFunctionLibrary::GetControllerTrackingMode()
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->GetControllerTrackingMode();
	}
	return EMLControllerTrackingMode::InputService;
}

FName UMagicLeapControllerFunctionLibrary::GetMotionSourceForHand(EControllerHand Hand)
{
#if WITH_MLSDK
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->ControllerMapper.GetMotionSourceForHand(Hand);
	}
#endif //WITH_MLSDK
	return FMagicLeapMotionSourceNames::Unknown;
}

EControllerHand UMagicLeapControllerFunctionLibrary::GetHandForMotionSource(FName MotionSource)
{
#if WITH_MLSDK
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->ControllerMapper.GetHandForMotionSource(MotionSource);
	}
#endif //WITH_MLSDK
	return EControllerHand::ControllerHand_Count;
}

bool UMagicLeapControllerFunctionLibrary::SetMotionSourceForHand(EControllerHand Hand, FName MotionSource)
{
#if WITH_MLSDK
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		controller->ControllerMapper.MapHandToMotionSource(Hand, MotionSource);
		return true;
	}
#endif //WITH_MLSDK
	return false;
}

EMLControllerType UMagicLeapControllerFunctionLibrary::GetControllerType(FName MotionSource)
{
#if WITH_MLSDK
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->ControllerMapper.MotionSourceToControllerType(MotionSource);
	}
#endif //WITH_MLSDK
	return EMLControllerType::None;
}













/////////////////////////////////////////////////////////////////////////////////////////////
// DEPRECATED FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////
int32 UMagicLeapControllerFunctionLibrary::MaxSupportedMagicLeapControllers()
{
#if WITH_MLSDK
	return MLInput_MaxControllers;
#else
	return 0;
#endif //WITH_MLSDK
}

bool UMagicLeapControllerFunctionLibrary::GetControllerMapping(int32 ControllerIndex, EControllerHand& Hand)
{
	return false;
}

void UMagicLeapControllerFunctionLibrary::InvertControllerMapping()
{
#if WITH_MLSDK
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->ControllerMapper.SwapHands();
	}
#endif //WITH_MLSDK
}

EMLControllerType UMagicLeapControllerFunctionLibrary::GetMLControllerType(EControllerHand Hand)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->GetMLControllerType(Hand);
	}
	return EMLControllerType::None;
}

bool UMagicLeapControllerFunctionLibrary::PlayControllerLED(EControllerHand Hand, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->PlayControllerLED(Hand, LEDPattern, LEDColor, DurationInSec);
	}
	return false;
}

bool UMagicLeapControllerFunctionLibrary::PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec)
{
	TSharedPtr<FMagicLeapController> Controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (Controller.IsValid())
	{
		return Controller->PlayControllerLEDEffect(Hand, LEDEffect, LEDSpeed, LEDPattern, LEDColor, DurationInSec);
	}
	return false;
}

bool UMagicLeapControllerFunctionLibrary::PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity)
{
	TSharedPtr<FMagicLeapController> Controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (Controller.IsValid())
	{
		return Controller->PlayControllerHapticFeedback(Hand, HapticPattern, Intensity);
	}
	return false;
}
