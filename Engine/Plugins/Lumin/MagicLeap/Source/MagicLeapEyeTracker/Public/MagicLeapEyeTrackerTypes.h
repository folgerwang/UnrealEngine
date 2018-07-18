// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
