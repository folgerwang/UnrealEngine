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

#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "TouchpadGesturesComponent.generated.h"

/** Recognized touchpad gesture types. */
UENUM(BlueprintType)
enum class EMagicLeapTouchpadGestureType : uint8
{
	None,
	Tap,
	ForceTapDown,
	ForceTapUp,
	ForceDwell,
	SecondForceDown,
	LongHold,
	RadialScroll,
	Swipe,
	Scroll,
	Pinch
};

/** Direction of touchpad gesture. */
UENUM(BlueprintType)
enum class EMagicLeapTouchpadGestureDirection : uint8
{
	None,
	Up,
	Down,
	Left,
	Right,
	In,
	Out,
	Clockwise,
	CounterClockwise
};

/** Information about a recognized touchpad gesture. */
USTRUCT(BlueprintType)
struct FMagicLeapTouchpadGesture
{
	GENERATED_BODY()

public:
	/** Controller on which the gesture was performed. */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	EControllerHand Hand;

	/** Type of gesture. */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	EMagicLeapTouchpadGestureType Type;

	/** Direction of gesture */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	EMagicLeapTouchpadGestureDirection Direction;

	/** 
	  Gesture position (x,y) and force (z).
	  Position is in the [-1.0,1.0] range and force is in the [0.0,1.0] range.
	*/
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	FVector PositionAndForce;

	/**
	  Speed of gesture. Note that this takes on different meanings depending
      on the gesture type being performed:
      - For radial gestures, this will be the angular speed around the axis.
      - For pinch gestures, this will be the speed at which the distance
        between fingers is changing. The touchpad is defined as having extents
        of [-1.0,1.0] so touchpad distance has a range of [0.0,2.0]; this value
        will be in touchpad distance per second.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	float Speed;

	/**
	  For radial gestures, this is the absolute value of the angle. For scroll
      and pinch gestures, this is the absolute distance traveled in touchpad
      distance. The touchpad is defined as having extents of [-1.0,1.0] so
      this distance has a range of [0.0,2.0].
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	float Distance;

	/**
	  Distance between the two fingers performing the gestures in touchpad
      distance. The touchpad is defined as having extents of [-1.0,1.0] so
      this distance has a range of [0.0,2.0].
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	float FingerGap;

	/**
	  For radial gestures, this is the radius of the gesture. The touchpad
      is defined as having extents of [-1.0,1.0] so this radius has a range
      of [0.0,2.0].
	 */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	float Radius;

	/** Angle from the center of the touchpad to the finger. */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	float Angle;
};

class MAGICLEAPCONTROLLER_API IMagicLeapTouchpadGestures
{
public:
	virtual void OnTouchpadGestureStartCallback(const FMagicLeapTouchpadGesture& GestureData) = 0;
	virtual void OnTouchpadGestureContinueCallback(const FMagicLeapTouchpadGesture& GestureData) = 0;
	virtual void OnTouchpadGestureEndCallback(const FMagicLeapTouchpadGesture& GestureData) = 0;
};

/** Delegates touchpad gesture events for the Magic Leap Controller & MLMA */
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAPCONTROLLER_API UTouchpadGesturesComponent : public UActorComponent, public IMagicLeapTouchpadGestures
{
	GENERATED_BODY()

public:
	UTouchpadGesturesComponent();
	virtual ~UTouchpadGesturesComponent();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTouchpadGestureEvent, const FMagicLeapTouchpadGesture&, GestureData);

	/** Event called when a touchpad gesture starts. Provides all the meta data about the given gestures. */
	UPROPERTY(BlueprintAssignable)
	FTouchpadGestureEvent OnTouchpadGestureStart;

	/** Event called when a touchpad gesture continues. Provides all the meta data about the given gestures. */
	UPROPERTY(BlueprintAssignable)
	FTouchpadGestureEvent OnTouchpadGestureContinue;

	/** Event called when a touchpad gesture ends. Provides all the meta data about the given gestures. */
	UPROPERTY(BlueprintAssignable)
	FTouchpadGestureEvent OnTouchpadGestureEnd;

	/** IMagicLeapTouchpadGestures interface */
	virtual void OnTouchpadGestureStartCallback(const FMagicLeapTouchpadGesture& GestureData) override;
	virtual void OnTouchpadGestureContinueCallback(const FMagicLeapTouchpadGesture& GestureData) override;
	virtual void OnTouchpadGestureEndCallback(const FMagicLeapTouchpadGesture& GestureData) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

private:
	FCriticalSection CriticalSection;
	TArray<FMagicLeapTouchpadGesture> PendingTouchpadGestureStart;
	TArray<FMagicLeapTouchpadGesture> PendingTouchpadGestureContinue;
	TArray<FMagicLeapTouchpadGesture> PendingTouchpadGestureEnd;
};
