// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSynchronizationSource.h"

#include "MediaBundleTimeSynchronizationSource.generated.h"


class UMediaBundle;

/**
 * Synchronization Source using the Media Bundle
 */
UCLASS(EditInlineNew)
class MEDIAFRAMEWORKUTILITIES_API UMediaBundleTimeSynchronizationSource : public UTimeSynchronizationSource
{
	GENERATED_BODY()

public:
	/* Media bundle asset of this input*/
	UPROPERTY(EditAnywhere, Category=Player)
	UMediaBundle* MediaBundle;

public:

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin UTimeSynchronizationSource Interface
	virtual FFrameTime GetNextSampleTime() const override;
	virtual int32 GetAvailableSampleCount() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual bool IsReady() const override;
	virtual bool Open() override;
	virtual void Start() override;
	virtual void Close() override;
	virtual FString GetDisplayName() const override;
	//~ End UTimeSynchronizationSource Interface
};
