// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseMediaSource.h"

#include "TimeSynchronizableMediaSource.generated.h"


namespace TimeSynchronizableMedia
{
	/** Name of bUseTimeSynchronization media option. */
	static FName UseTimeSynchronizatioOption("UseTimeSynchronization");
}

/**
 * Base class for media sources that can be synchronized with the engine's timecode.
 */
UCLASS(Abstract)
class MEDIAASSETS_API UTimeSynchronizableMediaSource : public UBaseMediaSource
{
	GENERATED_BODY()
	
public:
	/** Default constructor. */
	UTimeSynchronizableMediaSource();

public:

	/** Synchronize the media with the engine's timecode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Synchronization, meta=(DisplayName="Synchronize with Engine's Timecode"))
	bool bUseTimeSynchronization;

public:
	//~ IMediaOptions interface
	using Super::GetMediaOption;
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;
};
