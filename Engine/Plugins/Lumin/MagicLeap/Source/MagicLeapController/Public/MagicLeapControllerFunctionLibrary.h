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
	  Gets the maximum number of Magic Leap controllers supported at a time.
	  @return the maximum number of Magic Leap controllers supported at a time.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
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
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
	static bool GetControllerMapping(int32 ControllerIndex, EControllerHand& Hand);

	/**
	  Inverts the controller mapping i.e. keys mapped to left hand controller will now be treated as right hand and vice-versa.
	  @see GetControllerMapping()
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
	static void InvertControllerMapping();

	/**
	  Type of ML device being tracking the given hand.
	  @param Hand Controller hand to query.
	  @return Type of ML device which maps to given Unreal controller hand.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MotionController|MagicLeap")
	static EMLControllerType GetMLControllerType(EControllerHand Hand);

	/**
	  Match the position & orientation of the physical controller with an entity in the map and call this function
	  with the position & orientation of that entity relative to the Player Pawn. This would apply the correct offsets
	  to the MotionController component's transform.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
	static void CalibrateControllerNow(EControllerHand Hand, const FVector& StartPosition, const FRotator& StartOrientation);

	/**
	  Light up the LED on the Magic Leap Controller in the given pattern for the specified duration.
	  @param Hand Controller to play the LED pattern on.
	  @param LEDPattern Pattern to play on the controller.
	  @param LEDColor Color of the LED.
	  @param DurationInSec Duration (in seconds) to play the LED pattern.
	  @return True if the command to play the LED pattern was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
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
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
	static bool PlayControllerLEDEffect(EControllerHand Hand, EMLControllerLEDEffect LEDEffect, EMLControllerLEDSpeed LEDSpeed, EMLControllerLEDPattern LEDPattern, EMLControllerLEDColor LEDColor, float DurationInSec);

	/**
	  Play haptic feedback on the controller.
	  @param Hand Controller to play the haptic feedback on.
	  @param HapticPattern Pattern to play on the controller.
	  @param Intensity Intensity to play on the controller.
	  @return True if the command to play the haptic feedback was successfully sent to the controller, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MotionController|MagicLeap")
	static bool PlayControllerHapticFeedback(EControllerHand Hand, EMLControllerHapticPattern HapticPattern, EMLControllerHapticIntensity Intensity);
};
