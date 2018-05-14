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

#pragma once

#include "UObject/NameTypes.h"

/** List of input key names for all keys specific to Magic Leap Controller and Mobile Companion App. */
struct FMagicLeapControllerKeyNames
{
	static const FName MotionController_Left_Thumbstick_Z_Name;
	static const FName Left_MoveButton_Name;
	static const FName Left_AppButton_Name;
	static const FName Left_HomeButton_Name;

	static const FName MotionController_Right_Thumbstick_Z_Name;
	static const FName Right_MoveButton_Name;
	static const FName Right_AppButton_Name;
	static const FName Right_HomeButton_Name;
};

/** Defines the Magic Leap controller type. */
UENUM(BlueprintType)
enum class EMLControllerType : uint8
{
	None,
	Device,
	MobileApp
};

/** LED patterns supported on the controller. */
UENUM(BlueprintType)
enum class EMLControllerLEDPattern : uint8
{
	Clock01,
	Clock02,
	Clock03,
	Clock04,
	Clock05,
	Clock06,
	Clock07,
	Clock08,
	Clock09,
	Clock10,
	Clock11,
	Clock12,
	Clock01_07,
	Clock02_08,
	Clock03_09,
	Clock04_10,
	Clock05_11,
	Clock06_12
};

/** LED effects supported on the controller. */
UENUM(BlueprintType)
enum class EMLControllerLEDEffect : uint8
{
	RotateCW,
	RotateCCW,
	Pulse,
	PaintCW,
	PaintCCW,
	Blink
};

/** LED colors supported on the controller. */
UENUM(BlueprintType)
enum class EMLControllerLEDColor : uint8
{
	BrightRed,
	PastelRed,
	BrightFloridaOrange,
	PastelFloridaOrange,
	BrightLunaYellow,
	PastelLunaYellow,
	BrightNebulaPink,
	PastelNebulaPink,
	BrightCosmicPurple,
	PastelCosmicPurple,
	BrightMysticBlue,
	PastelMysticBlue,
	BrightCelestialBlue,
	PastelCelestialBlue,
	BrightShaggleGreen,
	PastelShaggleGreen
};

/** LED speeds supported on the controller. */
UENUM(BlueprintType)
enum class EMLControllerLEDSpeed : uint8
{
	Slow,
	Medium,
	Fast
};

/** Haptic patterns supported on the controller. */
UENUM(BlueprintType)
enum class EMLControllerHapticPattern : uint8
{
	Click,
	Bump,
	DoubleClick,
	Buzz,
	Tick,
	ForceDown,
	ForceUp,
	ForceDwell,
	SecondForceDown
};

/** Haptic intesities supported on the controller. */
UENUM(BlueprintType)
enum class EMLControllerHapticIntensity : uint8
{
	Low,
	Medium,
	High
};
