// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
