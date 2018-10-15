// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Profile/IMediaProfileManager.h"
#include "UObject/GCObject.h"

class UMediaProfile;
class UProxyMediaSource;
class UProxyMediaOutput;

class FMediaProfileManager : public IMediaProfileManager
{
public:
	FMediaProfileManager();

	virtual UMediaProfile* GetCurrentMediaProfile() const override;
	virtual void SetCurrentMediaProfile(UMediaProfile* InMediaProfile) override;
	virtual FOnMediaProfileChanged& OnMediaProfileChanged() override;

private:
	class FInternalReferenceCollector : public FGCObject
	{
	public:
		FInternalReferenceCollector(FMediaProfileManager* InOwner);
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	private:
		FMediaProfileManager* Owner;
	};
	friend FInternalReferenceCollector;
	FInternalReferenceCollector Collector;

	UMediaProfile* CurrentMediaProfile;
	TArray<UProxyMediaSource*> CurrentProxyMediaSources;
	TArray<UProxyMediaOutput*> CurrentProxyMediaOutputs;

	FOnMediaProfileChanged MediaProfileChangedDelegate;
};
