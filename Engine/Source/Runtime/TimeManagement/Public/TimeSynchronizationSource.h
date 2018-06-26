// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"

#include "TimeSynchronizationSource.generated.h"

/**
* Base class for sources to be used for time synchronization
*/
UCLASS(Abstract)
class TIMEMANAGEMENT_API UTimeSynchronizationSource : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** If the source has a time base that can be used to synchronize other sources. */
	UPROPERTY(EditAnywhere, Category = Synchronization)
	bool bUseForSynchronization;

	/** Extra frame to buffered before allowing the Manager to by in synchronized mode. */
	UPROPERTY(EditAnywhere, Category = Synchronization, Meta = (ClampMin = 0, ClampMax = 60, EditCondition = "bUseForSynchronization"))
	int32 NumberOfExtraBufferedFrame;

	/** Fixed delay in seconds to align with other sources. */
	UPROPERTY(EditAnywhere, Category = Synchronization, Meta = (EditCondition = "!bUseForSynchronization"))
	float TimeDelay;

public:
	/** Get next available sample Time based on the source FrameRate */
	virtual FFrameTime GetNextSampleTime() const PURE_VIRTUAL(UTimeSynchronizationSource::GetNextSampleTime, return FFrameTime();)

	/** Get number of available samples buffered in the source */
	virtual int32 GetAvailableSampleCount() const PURE_VIRTUAL(UTimeSynchronizationSource::GetAvailableSampleCount, return 0;)

	/** Get the source actual FrameRate */
	virtual FFrameRate GetFrameRate() const PURE_VIRTUAL(UTimeSynchronizationSource::GetFrameRate, return FFrameRate();)

	/** Used to know if the source is ready to be used for synchronization. */
	virtual bool IsReady() const PURE_VIRTUAL(UTimeSynchronizationSource::IsReady, return false;)

	/** Open the source to initiate frame acquisition */
	virtual bool Open() PURE_VIRTUAL(UTimeSynchronizationSource::Open, return false;)

	/** Start Rolling/Playing the source */
	virtual void Start() PURE_VIRTUAL(UTimeSynchronizationSource::Start, return;)

	/** Stop the source from rolling. The source may clear all his buffered frame. */
	virtual void Close() PURE_VIRTUAL(UTimeSynchronizationSource::Close, return;)

	/** Name to used when displaying an error message or to used in UI. */
	virtual FString GetDisplayName() const PURE_VIRTUAL(UTimeSynchronizationSource::GetDisplayName, return FString(););
};
