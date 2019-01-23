// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "IMotionController.h"
#include "InputCoreTypes.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"


// Axis State, in range 0.0f..1.0f
struct FAnalogState
{
public:
	FAnalogState() :
		AnalogState(0.f),
		AnalogNextState(0.f)
	{
	}

	// Unbind everything
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

	// Set new state for axis
	void SetData(float NewAnalogState)
	{
		AnalogNextState = NewAnalogState;
	}

	// Return true, if axis state is changed
	bool IsChanged()
	{
		return AnalogState != AnalogNextState;
	}

	// Apply axis state changes to current state
	void ApplyChanges()
	{
		AnalogState = AnalogNextState;
	}

	// Send all changes to UE4 core
	void UpdateEvents(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, double CurrentTime);

private:
	// Send axis event to UE4 core
	bool OnAnalogChanges(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId);

private:
	// UE4 bind targets for this channel
	TArray<FName> BindKeys;
	// Analog value [0.f,1.f]
	float AnalogState;
	// Next state (runtime value)
	float AnalogNextState;
};


struct FAnalogKey
{
	static constexpr uint32 TotalCount = 20;

	static const FKey Analog_1;
	static const FKey Analog_2;
	static const FKey Analog_3;
	static const FKey Analog_4;
	static const FKey Analog_5;
	static const FKey Analog_6;
	static const FKey Analog_7;
	static const FKey Analog_8;
	static const FKey Analog_9;
	static const FKey Analog_10;

	static const FKey Analog_11;
	static const FKey Analog_12;
	static const FKey Analog_13;
	static const FKey Analog_14;
	static const FKey Analog_15;
	static const FKey Analog_16;
	static const FKey Analog_17;
	static const FKey Analog_18;
	static const FKey Analog_19;
	static const FKey Analog_20;

	static const FKey* AnalogKeys[TotalCount];
};
