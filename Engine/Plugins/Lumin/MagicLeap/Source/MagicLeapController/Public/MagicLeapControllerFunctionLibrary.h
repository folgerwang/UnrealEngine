// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "MagicLeapControllerKeys.h"
#include "MagicLeapControllerFunctionLibrary.generated.h"

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPCONTROLLER_API UMagicLeapControllerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	Light up the LED on the Magic Leap Controller in the given pattern for the specified duration.
	@param MotionSource Controller to play the LED pattern on.
	@param LEDPattern Pattern to play on the controller.
	@param LEDColor Color of the LED.
	@param DurationInSec Duration (in seconds) to play the LED pattern.
	@return True if the command to play the LED pattern was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static bool PlayLEDPattern(FName MotionSource, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);

	/**
	Starts a LED feedback effect using the specified pattern on the specified controller.
	@param MotionSource Controller to play the LED pattern on.
	@param LEDEffect Effect to play on the controller.
	@param LEDSpeed Effect speed.
	@param LEDPattern Pattern to play on the controller.
	@param LEDColor Color of the LED.
	@param DurationInSec Duration (in seconds) to play the LED pattern.
	@return True if the command to play the LED effect was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static bool PlayLEDEffect(FName MotionSource, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);

	/**
	Play haptic feedback on the controller.
	@param MotionSource Controller to play the haptic feedback on.
	@param HapticPattern Pattern to play on the controller.
	@param Intensity Intensity to play on the controller.
	@return True if the command to play the haptic feedback was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static bool PlayHapticPattern(FName MotionSource, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);

	/**
	Set controller tracking mode.
	@param TrackingMode Controller tracking mode. Note that this is global (it affects all control devices).
	@return True if the command to set the tracking mode was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static bool SetControllerTrackingMode(EMLControllerTrackingMode TrackingMode);

	/**
	Get controller tracking mode.
	@return Controller tracking mode.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static EMLControllerTrackingMode GetControllerTrackingMode();

	/**
	Get motion source for hand.
	@return Motion source to which hand is mapped.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static FName GetMotionSourceForHand(EControllerHand Hand);

	/**
	Get motion source for hand.
	@return Hand to which motion source is mapped, ControllerHand_Count if not mapped.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static EControllerHand GetHandForMotionSource(FName MotionSource);

	/**
	Set motion source for hand.
	@return True if successful, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
		static bool SetMotionSourceForHand(EControllerHand Hand, FName MotionSource);

	/**
	Type of ML device bound to the specified motion source
	@param MotionSource Motion source to query.
	@return Type of ML device which maps to given Unreal controller hand.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MotionController|MagicLeap")
		static EMLControllerType GetControllerType(FName MotionSource);

	/////////////////////////////////////////////////////////////////////////////////////////////
	// DEPRECATED FUNCTIONS
	/////////////////////////////////////////////////////////////////////////////////////////////
	/**
	  Gets the maximum number of Magic Leap controllers supported at a time.
	  @return the maximum number of Magic Leap controllers supported at a time.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction))
	static int32 MaxSupportedMagicLeapControllers();

	/**
	  Returns the hand to which given controller index has been mapped to in the device backend.

	  The native api does not have a concept of left vs right controller. They deal with indices. The first connected
	  controller is index 0 and so on. By default, index 0 is mapped to the right hand in Unreal.
	  You can invert these mappings by calling InvertControllerMapping() function.
	  @param ControllerIndex Zero based controller index to get the hand mapping for. Should be less than MaxSupportedMagicLeapControllers().
	  @param Hand Output parameter which is the hand the given index maps to. Valid only if the function returns true.
	  @return true of the controller index maps to a valid hand, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Use GetHandForMotionSource instead"))
	static bool GetControllerMapping(int32 ControllerIndex, EControllerHand& Hand);

	/**
	Type of ML device being tracking the given hand.
	@param Hand Controller hand to query.
	@return Type of ML device which maps to given Unreal controller hand.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Use GetMotionSourceForHand instead"))
		static EMLControllerType GetMLControllerType(EControllerHand Hand);

	/**
	  Inverts the controller mapping i.e. keys mapped to left hand controller will now be treated as right hand and vice-versa.
	  @see GetControllerMapping()
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Use SetMotionSourceForHand instead"))
	static void InvertControllerMapping();

	/**
	  Light up the LED on the Magic Leap Controller in the given pattern for the specified duration.
	  @param Hand Controller to play the LED pattern on.
	  @param LEDPattern Pattern to play on the controller.
	  @param LEDColor Color of the LED.
	  @param DurationInSec Duration (in seconds) to play the LED pattern.
	  @return True if the command to play the LED pattern was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Use PlayLEDPattern instead"))
	static bool PlayControllerLED(EControllerHand Hand, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);

	/**
	  Starts a LED feedback effect using the specified pattern on the specified controller.
	  @param Hand Controller to play the LED pattern on.
	  @param LEDEffect Effect to play on the controller.
	  @param LEDSpeed Effect speed.
	  @param LEDPattern Pattern to play on the controller.
	  @param LEDColor Color of the LED.
	  @param DurationInSec Duration (in seconds) to play the LED pattern.
	  @return True if the command to play the LED effect was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Use PlayLEDEffect instead"))
	static bool PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);

	/**
	  Play haptic feedback on the controller.
	  @param Hand Controller to play the haptic feedback on.
	  @param HapticPattern Pattern to play on the controller.
	  @param Intensity Intensity to play on the controller.
	  @return True if the command to play the haptic feedback was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Use PlayHapticPattern instead"))
	static bool PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);
};
