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

struct FMagicLeapVREyeTrackingData
{
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Average")
	FVector AverageGazeRay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Average")
	FVector AverageGazeOrigin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Average")
	FVector WorldAverageGazeConvergencePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stereo")
	FVector LeftOriginPoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stereo")
	FVector RightOriginPoint;

	/** Time when the gaze point was created. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaze Data")
	FDateTime TimeStamp;

	/** Whether this gaze point is stable or not. This being true means that the gaze point is not moving much. If you want to select an object to interact with it can be useful to only use stable points for this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaze Data")
	bool bIsStable;

	/** Condidence value of the convergence point */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaze Data")
	float Confidence;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blink Data")
	bool bLeftBlink;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blink Data")
	bool bRightBlink;
};

struct FMagicLeapVRStabilityData
{

};

UENUM(BlueprintType)
enum class EMagicLeapEyeTrackingStatus : uint8
{
	NotConnected					UMETA(DisplayName = "Not Connected"),                             //The eyetracker is not connected to UE4 for some reason. The tracker might not be plugged in, the game window is currently running on a screen without an eyetracker or is otherwise not available.
	Disabled						UMETA(DisplayName = "Disabled"),                                  //Eyetracking has been disabled by the user or developer.
	UserNotPresent					UMETA(DisplayName = "User Not Present"),                          //The eyetracker is running but has not yet detected a user.
	UserPresent						UMETA(DisplayName = "User Present"),                              //The eyetracker has detected a user and is actively tracking them. They appear not to be focusing on the game window at the moment however.
	UserPresentAndWatchingWindow	UMETA(DisplayName = "User Present And Watching The Game Window"), //The user is being tracked and is looking at the game window.
};
