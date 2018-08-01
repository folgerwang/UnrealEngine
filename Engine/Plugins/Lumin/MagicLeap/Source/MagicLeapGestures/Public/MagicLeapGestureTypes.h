// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MagicLeapGestureTypes.generated.h"

/** Static gesture types which are available when both hands are separated. */
UENUM(BlueprintType)
enum class EStaticGestures : uint8
{
	/** One finger. */
	Finger,
	/** A closed fist. */
	Fist,
	/** A pinch. */
	Pinch,
	/** A closed fist with the thumb pointed up. */
	Thumb,
	/** An L shape. */
	L,
	/** An open hand with the back of the hand facing the user. */
	OpenHandBack,
	/** A pinch with all fingers, except the index finger and the thumb, extended out. */
	Ok,
	/** A rounded 'C' alphabet shape. */
	C,
	/** No hand was present. */
	NoHand
};

/** Filtering for the keypoints and hand centers. */
UENUM(BlueprintType)
enum class EGestureKeypointsFilterLevel : uint8
{
	/** No filtering is done, the points are raw. */
	NoFilter,
	/** Some smoothing at the cost of latency. */
	SimpleSmoothing,
	/** Predictive smoothing, at higher cost of latency. */
	PredictiveSmoothing
};

/** Filtering for the gesture recognition and hand switching. */
UENUM(BlueprintType)
enum class EGestureRecognitionFilterLevel : uint8
{
	/** No filtering is done, the gestures are raw. */
	NoFilter,
	/** Some robustness to flicker at some cost of latency. */
	SlightRobustnessToFlicker,
	/** More robust to flicker at higher latency cost. */
	MoreRobustnessToFlicker
};

/** Gesture key point transform spaces. */
UENUM(BlueprintType)
enum class EGestureTransformSpace : uint8
{
	/** Gesture key point transforms are reported in device Tracking space. */
	Tracking,
	/** Gesture key point transforms are reported in Unreal world space. This is more costly on CPU.*/
	World,
	/** Gesture key point transforms are reported in gesture hand center space.*/
	Hand
};

/** List of input key names for all left and right hand gestures. */
struct FMagicLeapGestureKeyNames
{
	static const FName Left_Finger_Name;
	static const FName Left_Fist_Name;
	static const FName Left_Pinch_Name;
	static const FName Left_Thumb_Name;
	static const FName Left_L_Name;
	static const FName Left_OpenHandBack_Name;
	static const FName Left_Ok_Name;
	static const FName Left_C_Name;
	static const FName Left_NoHand_Name;

	static const FName Right_Finger_Name;
	static const FName Right_Fist_Name;
	static const FName Right_Pinch_Name;
	static const FName Right_Thumb_Name;
	static const FName Right_L_Name;
	static const FName Right_OpenHandBack_Name;
	static const FName Right_Ok_Name;
	static const FName Right_C_Name;
	static const FName Right_NoHand_Name;
};
