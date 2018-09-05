// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimeSynchronizationSource.h"
#include "LiveLinkClient.h"
#include "Misc/Guid.h"
#include "LiveLinkTimeSynchronizationSource.generated.h"

UCLASS(EditInlineNew)
class ULiveLinkTimeSynchronizationSource : public UTimeSynchronizationSource
{
	GENERATED_BODY()

private:

	UPROPERTY(EditAnywhere, Category="LiveLink")
	FName SubjectName;

	FLiveLinkClient* LiveLinkClient;

	enum class ESyncState
	{
		NotSynced,
		Opened,
		Synced
	};

	mutable ESyncState State = ESyncState::NotSynced;
	mutable int64 LastUpdateFrame;
	mutable FLiveLinkSubjectTimeSyncData CachedData;
	mutable FGuid LastUpdateGuid;

public:

	ULiveLinkTimeSynchronizationSource();

	//~ Begin TimeSynchronizationSource API
	virtual FFrameTime GetNewestSampleTime() const override;
	virtual FFrameTime GetOldestSampleTime() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual bool IsReady() const override;
	virtual bool Open(const FTimeSynchronizationOpenData& OpenData) override;
	virtual void Start(const FTimeSynchronizationStartData& StartData) override;
	virtual void Close() override;
	virtual FString GetDisplayName() const override;
	//~ End TimeSynchronizationSource API

private:

	void OnModularFeatureRegistered(const FName& FeatureName, class IModularFeature* Feature);
	void OnModularFeatureUnregistered(const FName& FeatureName, class IModularFeature* Feature);
	void UpdateCachedState() const;
};