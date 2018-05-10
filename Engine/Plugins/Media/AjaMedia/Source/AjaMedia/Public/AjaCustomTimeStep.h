// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FixedFrameRateCustomTimeStep.h"

#include "AJALib.h"
#include "AjaMediaFinder.h"
#include "AjaMediaSource.h"

#include "MediaIOCoreWaitVsyncThread.h"
#include "AjaHardwareSync.h"

#include "ITimecodeProvider.h"
#include "FrameRate.h"
#include "Timecode.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

#include "AjaCustomTimeStep.generated.h"


/**
 * Class to control the Engine TimeStep via the Aja card
 */
UCLASS(editinlinenew, meta=(DisplayName="AJA SDI Input"))
class AJAMEDIA_API UAjaCustomTimeStep : public UFixedFrameRateCustomTimeStep
									, public ITimecodeProvider
{
	GENERATED_UCLASS_BODY()

public:

	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;
	virtual bool UpdateTimeStep(class UEngine* InEngine) override;

	//~ ITimecodeProvider interface
	virtual FTimecode GetCurrentTimecode() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual bool IsSynchronizing() const override;
	virtual bool IsSynchronized() const override;

private:
	struct FAJACallback;
	friend FAJACallback;

	void WaitForSync();
	void ReleaseResources();

public:
	/**
	 * The AJA source from where the Genlock and Timecode signal will be coming from.
	 */
	UPROPERTY(EditAnywhere, Category="Genlock and Timecode options", AssetRegistrySearchable, meta=(DisplayName="Source"))
	FAjaMediaPort MediaPort;

	/** The type of Timecode to read from SDI stream. */
	UPROPERTY(EditAnywhere, Category="Genlock and Timecode options")
	EAjaMediaTimecodeFormat TimecodeFormat;

	/** Enable mechanism to detect Engine loop overrunning the source */
	UPROPERTY(EditAnywhere, Category="Genlock and Timecode options", meta=(DisplayName="Display Dropped Frames Warning"))
	bool bEnableOverrunDetection;

private:
	/** Aja Port to capture the Sync */
	AJA::AJASyncChannel* SyncChannel;
	FAJACallback* SyncCallback;

	/** WaitForVSync task Runnable */
	TUniquePtr<FMediaIOCoreWaitVSyncThread> VSyncThread;

	/** WaitForVSync thread */
	TUniquePtr<FRunnableThread> VSyncRunnableThread;

	/**
	 * Synchronized timecode. Only valid when the Synchronizer is synced
	 */
	FTimecode CurrentTimecode;
	bool bWaitIsValid;
	bool bInitialized;
	bool bInitializationSucceed;
};
