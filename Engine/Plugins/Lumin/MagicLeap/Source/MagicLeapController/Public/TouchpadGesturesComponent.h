// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	/** Hand on which the gesture was performed. */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap", meta = (DeprecatedProperty, DeprecationMessage = "Hand is deprecated. Please use MotionSource instead."))
	EControllerHand Hand;

	/** Motion source on which the gesture was performed. */
	UPROPERTY(BlueprintReadOnly, Category = "TouchpadGesture|MagicLeap")
	FName MotionSource;

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
