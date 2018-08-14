// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Profile/IMediaProfileManager.h"

#include "Profile/MediaProfile.h"
#include "UObject/StrongObjectPtr.h"

class FMediaProfileManager : public IMediaProfileManager
{
public:

	virtual UMediaProfile* GetCurrentMediaProfile() const override;
	virtual void SetCurrentMediaProfile(UMediaProfile* InMediaProfile) override;
	virtual FOnMediaProfileChanged& OnMediaProfileChanged() override;

private:

	TStrongObjectPtr<UMediaProfile> CurrentMediaProfile;
	FOnMediaProfileChanged MediaProfileChangedDelegate;
};
