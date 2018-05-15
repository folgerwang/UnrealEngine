// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FixedFrameRateCustomTimeStep.h"

#include "AJALib.h"
#include "AjaMediaFinder.h"
#include "AjaMediaSource.h"

#include "MediaIOCoreWaitVsyncThread.h"
#include "AjaHardwareSync.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/Timecode.h"

#include "AjaCustomTimeStep.generated.h"


/**
 * Control the Engine TimeStep via the AJA card
 */
UCLASS(editinlinenew, meta=(DisplayName="AJA SDI Input"))
class AJAMEDIA_API UAjaCustomTimeStep : public UFixedFrameRateCustomTimeStep
{
	GENERATED_UCLASS_BODY()

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;
	virtual bool UpdateTimeStep(class UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override;

	//~ UObject interface
	virtual void BeginDestroy() override;

private:
	struct FAJACallback;
	friend FAJACallback;

	void WaitForSync();
	void ReleaseResources();

public:
	/**
	 * The AJA source from where the Genlock signal will be coming from.
	 */
	UPROPERTY(EditAnywhere, Category="Genlock options", AssetRegistrySearchable, meta=(DisplayName="Source"))
	FAjaMediaPort MediaPort;

	/** Enable mechanism to detect Engine loop overrunning the source */
	UPROPERTY(EditAnywhere, Category="Genlock options", meta=(DisplayName="Display Dropped Frames Warning"))
	bool bEnableOverrunDetection;

private:
	/** AJA Port to capture the Sync */
	AJA::AJASyncChannel* SyncChannel;
	FAJACallback* SyncCallback;

	/** WaitForVSync task Runnable */
	TUniquePtr<FMediaIOCoreWaitVSyncThread> VSyncThread;

	/** WaitForVSync thread */
	TUniquePtr<FRunnableThread> VSyncRunnableThread;

	/** The current SynchronizationState of the CustomTimeStep */
	ECustomTimeStepSynchronizationState State;
	bool bDidAValidUpdateTimeStep;
};
