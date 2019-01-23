// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterInputModule.h"
#include "InputCoreTypes.h"


struct FTrackerState
{
public:
	FTrackerState() :
		Orientation(0, 0, 0),
		Position(0, 0, 0),
		OrientationNext(0, 0, 0),
		PositionNext(0, 0, 0)
	{ }

	// Clear all binds to UE4
	void Reset()
	{
		Trackers.Empty();
	}

	// Get binded trackers count
	int32 GetTrackersNum() const
	{
		return Trackers.Num();
	}

	// Find binded tracker index, return INDEX_NONE id not binded
	int32 FindTracker(const EControllerHand NewTrackerKey) const
	{
		return Trackers.Find(NewTrackerKey);
	}

	// Add new UE4 target to tracker array
	bool BindTarget(const EControllerHand NewTrackerKey)
	{
		if (Trackers.Contains(NewTrackerKey))
		{
			return false;
		}

		Trackers.AddUnique(NewTrackerKey);

		return true;
	}

	// Set next state for this tracker
	void SetData(const FRotator& NewOrientation, const FVector& NewPosition)
	{
		OrientationNext = NewOrientation;
		PositionNext = NewPosition;
	}

	// Return current tracker state
	void GetCurrentData(FRotator& CurrentOrientation, FVector& CurrentPosition) const
	{
		CurrentOrientation = Orientation;
		CurrentPosition = Position;
	}

	// Apply tracker state changes
	void ApplyChanges()
	{
		Orientation = OrientationNext;
		Position = PositionNext;
	}

private:
	// List of all UE4 trackers binded to this vrpn channel
	TArray<EControllerHand> Trackers;

	// Current tracker state
	FRotator Orientation;
	FVector  Position;
	// Next tracker state
	FRotator OrientationNext;
	FVector  PositionNext;
};
