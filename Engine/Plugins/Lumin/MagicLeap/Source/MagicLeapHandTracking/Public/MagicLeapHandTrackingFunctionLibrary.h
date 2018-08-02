// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MagicLeapHandTrackingTypes.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "ILiveLinkSource.h"
#include "MagicLeapHandTrackingFunctionLibrary.generated.h"

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPHANDTRACKING_API UMagicLeapHandTrackingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	  Transform of the center of the hand.  Approximately the center of the palm.

	  Note that this returns a transform in world space.

	  @param Hand Hand to query the hand center transform for. Only Left and Right hand are supported.
	  @param HandCenter Output parameter containing the position and orientation of the given hand.
	  @return true if the output param was populated with a valid value, false means that is is either unchanged or populated with a stale value.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "GetHandCenter is deprecated. Please use GetGestureKeypointTransform instead."))
	static bool GetHandCenter(EControllerHand Hand, FTransform& HandCenter);

	/**
	  Transform of the Index Finger Tip.

	  @param Hand Hand to query the hand center transform for. Only Left and Right hand are supported.
	  @param TransformSpace Get the transform relative to the world, hand center, or tracking space.
	  @param Pointer Output parameter containing the position and orientation.
	  @return true if the output param was populated with a valid value, false means that is is either unchanged or populated with a stale value.
	  */
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "GetHandIndexFingerTip is deprecated. Please use GetGestureKeypointTransform instead."))
	static bool GetHandIndexFingerTip(EControllerHand Hand, EGestureTransformSpace TransformSpace, FTransform& Pointer);

	/**
	  Transform of Thumb Tip.

	  Note that this returns a transform in the Tracking space. To get the transform in Unreal's
	  world space, use the MotioController component as a child of the XRPawn with hand set to EControllerHand::Special_5
	  for the left hand secondary and EControllerHand::Special_6 for the right hand secondary.

	  @param Hand Hand to query the hand center transform for. Only Left and Right hand are supported.
	  @param TransformSpace Get the transform relative to the world, hand center, or tracking space.
	  @param Secondary Output parameter containing the position and orientation.
	  @return true if the output param was populated with a valid value, false means that is is either unchanged or populated with a stale value.
	  */
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "GetHandThumbTip is deprecated. Please use GetGestureKeypointTransform instead."))
	static bool GetHandThumbTip(EControllerHand Hand, EGestureTransformSpace TransformSpace, FTransform& Secondary);

	/**
	  Normalized position of the center of the given hand. This can be used to detect and warn the users that the hand is out of the gesture detection frame.

	  @param Hand Hand to query the normalized hand center position for. Only Left and Right hand are supported.
	  @param HandCenterNormalized Output paramter containing the normalized position of the given hand.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool GetHandCenterNormalized(EControllerHand Hand, FVector& HandCenterNormalized);

	/**
	  List of keypoints detected on the given hand.

	  Note that this returns a transform in the Tracking space. To get the transform in Unreal's
	  world space, use the MotioController component as a child of the XRPawn with hand set to the following.
	  Special_3 - Left Index Finger Tip
	  Special_5 - Left Thumb Tip
	  Special_4 - Right Index Finger Tip
	  Special_6 - Right Thumb Tip

	  @param Hand Hand to query the keypoints for. Only Left and Right hand are supported.
	  @param Keypoints Output parameter containing transforms of the keypoints detected on the given hand.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "GetGestureKeypoints is deprecated. Please use GetGestureKeypointTransform instead."))
	static bool GetGestureKeypoints(EControllerHand Hand, TArray<FTransform>& Keypoints);

	/**
	Get Transform for a point on the hand.

	@param Hand
	@param Keypoint the specific joint or wrist point to fetch.
	@param Transform Output parameter to write the data to.
	@param TransformSpace Get the transform relative to the world, hand center, or tracking space.
	@return true if the output param was populated with a valid value, false means that is is either unchanged or populated with a stale value.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool GetGestureKeypointTransform(EControllerHand Hand, EHandTrackingKeypoint Keypoint, EGestureTransformSpace TransformSpace, FTransform& Transform);

	/**
	  Enables and disables the gestures to be detected by the gesture recognition system.

	  @param StaticHandTrackingToActivate List of static gestures to be detected by the system.
	  @param KeypointsFilterLevel Filtering for the keypoints and hand centers.
	  @param GestureFilterLevel Filtering for the static gesture recognition.
	  @param HandSwitchingFilterLevel Filtering for if the left or right hand is present.
	  @return true if the configuration was set successfully.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool SetConfiguration(const TArray<EHandTrackingGesture>& StaticGesturesToActivate, EHandTrackingKeypointFilterLevel KeypointsFilterLevel = EHandTrackingKeypointFilterLevel::NoFilter, EHandTrackingGestureFilterLevel GestureFilterLevel = EHandTrackingGestureFilterLevel::NoFilter, EHandTrackingGestureFilterLevel HandSwitchingFilterLevel = EHandTrackingGestureFilterLevel::NoFilter, bool bTrackingEnabled = true);

	/**
	  Gets the list of static and dynamic gestures currently set to be identified by the gesture recognition system.

	  @param StaticHandTrackingToActivate Output parameter to list the static gestures that can be detected by the system.
	  @param KeypointsFilterLevel Filtering for the keypoints and hand centers.
	  @param GestureFilterLevel Filtering for the static gesture recognition.
	  @param HandSwitchingFilterLevel Filtering for if the left or right hand is present.
	  @return true if the output params were populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool GetConfiguration(TArray<EHandTrackingGesture>& ActiveStaticGestures, EHandTrackingKeypointFilterLevel& KeypointsFilterLevel, EHandTrackingGestureFilterLevel& GestureFilterLevel, EHandTrackingGestureFilterLevel& HandSwitchingFilterLevel, bool& bTrackingEnabled);

	/**
	  Sets the minimum gesture confidence to filter out the detected static gesture.

	  @param Gesture The gesture to set the confidence threshold for.
	  @param Confidence The gesture confidence threshold.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static void SetStaticGestureConfidenceThreshold(EHandTrackingGesture Gesture, float Confidence);

	/**
	  Gets the minimum gesture confidence used to filter out the detected static gesture.

	  @param Gesture The gesture to get the confidence threshold for.
	  @return The gesture confidence threshold.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static float GetStaticGestureConfidenceThreshold(EHandTrackingGesture Gesture);

	/**
	  The confidence level of the current gesture being performed by the given hand.
	  Value is between [0, 1], 0 is low, 1 is high degree of confidence. For a NoHand, the confidence is always set to 1.

	  @param Hand Hand to query the gesture confidence value for. Only Left and Right hand are supported.
	  @param Confidence Output parameter containing the confidence value for the given hand's gesture.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool GetCurrentGestureConfidence(EControllerHand Hand, float& Confidence);
	
	/**
	  The current gesture being performed by the given hand.

	  @param Hand Hand to query the gesture for. Only Left and Right hand are supported.
	  @param Gesture Output parameter containing the given hand's gesture, or NoHand if there isn't one or the system isnt working now.
	  @return true if the output param was populated with a valid value, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool GetCurrentGesture(EControllerHand Hand, EHandTrackingGesture& Gesture);


	/**
	Get a LiveLinkSourceHandle for magic leap hand tracking.

	@param SourceHandle Output parameter SourceHandle that will be filled in.
	@return true if a LiveLink Source was assigned.
	*/
	UFUNCTION(BlueprintCallable, Category = "HandTracking|MagicLeap")
	static bool GetMagicLeapHandTrackingLiveLinkSource(struct FLiveLinkSourceHandle& SourceHandle);

};
