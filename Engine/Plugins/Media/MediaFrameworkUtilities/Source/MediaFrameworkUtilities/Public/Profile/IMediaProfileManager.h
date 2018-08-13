// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMediaProfile;

class MEDIAFRAMEWORKUTILITIES_API IMediaProfileManager
{
public:
	static IMediaProfileManager& Get();

	/** IMediaProfileManager structors */
	virtual ~IMediaProfileManager() {}
	
	/** Get the current profile used by the manager. Can be null. */
	virtual UMediaProfile* GetCurrentMediaProfile() const = 0;
	
	/** Set the current profile used by the manager. */
	virtual void SetCurrentMediaProfile(UMediaProfile* InMediaProfile) = 0;

	/** Delegate type for media profile changed event */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMediaProfileChanged, UMediaProfile* /*Preivous*/, UMediaProfile* /*New*/);

	/** Delegate for media profile changed event */
	virtual FOnMediaProfileChanged& OnMediaProfileChanged() = 0;
};
