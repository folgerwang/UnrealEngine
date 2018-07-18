// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

struct FKeyEvent;
enum class EUINavigation : uint8;

/**
 * 
 */
struct FAnalogNavigationState
{
public:
	double LastNavigationTime;
	int32 Repeats;

	FAnalogNavigationState()
		: LastNavigationTime(0)
		, Repeats(0)
	{
	}
};

/**  */
struct FUserNavigationState
{
public:
	TMap<EUINavigation, FAnalogNavigationState> AnalogNavigationState;
};

/**
 * This class is used to control which FKeys and analog axis should move focus.
 */
class SLATE_API FNavigationConfig : public TSharedFromThis<FNavigationConfig>
{
public:
	/** ctor */
	FNavigationConfig();
	/** dtor */
	virtual ~FNavigationConfig();

	/** Gets the navigation direction from a given key event. */
	virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const;
	/** Gets the navigation direction from a given analog event. */
	virtual EUINavigation GetNavigationDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent);

	/** Called when the navigation config is registered with Slate Application */
	virtual void OnRegister();
	/** Called when the navigation config is registered with Slate Application */
	virtual void OnUnregister();
	/** Notified when users are removed from the system, good chance to clean up any user specific state. */
	virtual void OnUserRemoved(int32 UserIndex);

public:
	/** Should the Tab key perform next and previous style navigation. */
	bool bTabNavigation;
	/** Should we respect keys for navigation. */
	bool bKeyNavigation;
	/** Should we respect the analog stick for navigation. */
	bool bAnalogNavigation;

	/**  */
	float AnalogNavigationHorizontalThreshold;
	/**  */
	float AnalogNavigationVerticalThreshold;

	/** Which Axis Key controls horizontal navigation */
	FKey AnalogHorizontalKey;
	/** Which Axis Key controls vertical navigation */
	FKey AnalogVerticalKey;

	/** Digital key navigation rules. */
	TMap<FKey, EUINavigation> KeyEventRules;

protected:
	/**
	 * Gets the repeat rate of the navigation based on the current pressure being applied.  The idea being
	 * that if the user moves the stick a little, we would navigate slowly, if they move it a lot, we would
	 * repeat the navigation often.
	 */
	virtual float GetRepeatRateForPressure(float InPressure, int32 InRepeats) const;

	/**
	 * Gets the navigation direction from the analog internally.
	 */
	virtual EUINavigation GetNavigationDirectionFromAnalogInternal(const FAnalogInputEvent& InAnalogEvent);

	/** Navigation state that we store per user. */
	TMap<int, FUserNavigationState> UserNavigationState;
};


/** A navigation config that doesn't do any navigation. */
class SLATE_API FNullNavigationConfig : public FNavigationConfig
{
public:
	FNullNavigationConfig()
	{
		bTabNavigation = false;
		bKeyNavigation = false;
		bAnalogNavigation = false;
	}
};