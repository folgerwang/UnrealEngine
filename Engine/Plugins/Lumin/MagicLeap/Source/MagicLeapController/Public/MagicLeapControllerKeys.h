// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "UObject/NameTypes.h"
#include "InputCoreTypes.h"

/** Magic Leap motion sources */
struct FMagicLeapMotionSourceNames
{
	static const FName Control0;
	static const FName Control1;
	static const FName MobileApp;
	static const FName Unknown;
};

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

struct FMagicLeapKeys
{
	static const FKey MotionController_Left_Thumbstick_Z;
	static const FKey Left_MoveButton;
	static const FKey Left_AppButton;
	static const FKey Left_HomeButton;

	static const FKey MotionController_Right_Thumbstick_Z;
	static const FKey Right_MoveButton;
	static const FKey Right_AppButton;
	static const FKey Right_HomeButton;
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
	None,
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
	BrightMissionRed,
	PastelMissionRed,
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
	None,
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

/** Tracking modes provided by Magic Leap. */
UENUM(BlueprintType)
enum class EMLControllerTrackingMode : uint8
{
	InputService,
	CoordinateFrameUID,
};
