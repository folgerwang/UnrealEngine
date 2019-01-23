// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "IMotionController.h"
#include "InputCoreTypes.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Misc/DisplayClusterInputLog.h"


struct FButtonState
{
public:
	// Default constructor that just sets sensible defaults
	FButtonState() :
		bIsPressedState( false ),
		bIsPressedNextState( false ),
		NextRepeatTime( 0.0 )
	{ }

	// Clear all binds to UE4
	void Reset()
	{
		BindKeys.Empty();
	}

	// Add new UE4 target to binds array
	bool BindTarget(const FName& NewKey)
	{
		if (BindKeys.Contains(NewKey))
		{
			return false;
		}

		BindKeys.AddUnique(NewKey);
		return true;
	}

	void UnBindTarget(const FName& NewKey)
	{
		BindKeys.Remove(NewKey);
	}

	// Set new state for button
	void SetData(bool bIsPressed) 
	{
		bIsPressedNextState  = bIsPressed;
	}

	// Return true, if button state is changed
	bool IsChanged() 
	{
		return bIsPressedState != bIsPressedNextState;
	}

	// Apply button state changes to current state
	void ApplyChanges() 
	{
		bIsPressedState  = bIsPressedNextState;
	}

	// Send all changes to UE4 core
	void UpdateEvents(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, double CurrentTime);

private:
	// Send button events to UE4 core
	bool OnButtonPressed(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, bool bIsRepeat);
	bool OnButtonReleased(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, bool bIsRepeat);

	// Button autorepeat settings:
	static float InitialButtonRepeatDelay;
	static float ButtonRepeatDelay;

private:
	// UE4 bind targets for this channel
	TArray<FName> BindKeys;
	// Whether we're pressed or not.  While pressed, we will generate repeat presses on a timer
	bool bIsPressedState;
	// Cached next button value
	bool bIsPressedNextState;
	// Next time a repeat event should be generated for each button
	double NextRepeatTime;
};

struct FButtonKey
{
	static constexpr uint32 TotalCount = 20;

	static const FKey Button_1;
	static const FKey Button_2;
	static const FKey Button_3;
	static const FKey Button_4;
	static const FKey Button_5;
	static const FKey Button_6;
	static const FKey Button_7;
	static const FKey Button_8;
	static const FKey Button_9;
	static const FKey Button_10;

	static const FKey Button_11;
	static const FKey Button_12;
	static const FKey Button_13;
	static const FKey Button_14;
	static const FKey Button_15;
	static const FKey Button_16;
	static const FKey Button_17;
	static const FKey Button_18;
	static const FKey Button_19;
	static const FKey Button_20;

	static const FKey* ButtonKeys[TotalCount];
};
