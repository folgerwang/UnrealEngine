// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapControllerFunctionLibrary.h"
#include "IMagicLeapControllerPlugin.h"
#include "MagicLeapController.h"
#include "IMagicLeapPlugin.h"

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
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->GetControllerMapping(ControllerIndex, Hand);
	}
	return false;
}

void UMagicLeapControllerFunctionLibrary::InvertControllerMapping()
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		controller->InvertControllerMapping();
	}
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

void UMagicLeapControllerFunctionLibrary::CalibrateControllerNow(EControllerHand Hand, const FVector& StartPosition, const FRotator& StartOrientation)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		controller->CalibrateControllerNow(Hand, StartPosition, StartOrientation);
	}
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
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->PlayControllerLEDEffect(Hand, LEDEffect, LEDSpeed, LEDPattern, LEDColor, DurationInSec);
	}
	return false;
}

bool UMagicLeapControllerFunctionLibrary::PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity)
{
	TSharedPtr<FMagicLeapController> controller = StaticCastSharedPtr<FMagicLeapController>(IMagicLeapControllerPlugin::Get().GetInputDevice());
	if (controller.IsValid())
	{
		return controller->PlayControllerHapticFeedback(Hand, HapticPattern, Intensity);
	}
	return false;
}
