// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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
