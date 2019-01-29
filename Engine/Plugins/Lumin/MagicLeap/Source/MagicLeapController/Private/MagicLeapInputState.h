// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

//-------------------------------------------------------------------------------------------------
// FMagicLeapControllerState - Input state for the magic leap motion controller
//-------------------------------------------------------------------------------------------------

struct FMagicLeapControllerState
{
	static constexpr int kMaxTouches = 2;

	bool bIsConnected = false;
	bool bTriggerKeyPressing = false;
	bool bTouchActive[kMaxTouches] = { };
	float TriggerAnalog = 0.0f;
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;
	FTransform Transform;
	FVector TouchPosAndForce[kMaxTouches] = { };
};


