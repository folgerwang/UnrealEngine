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

class UEngine;

/**
 * Control the Engine TimeStep via the AJA card.
 * When the signal is lost in the editor (not in PIE), the CustomTimeStep will try to re-synchronize every second.
 */
UCLASS(Blueprintable, editinlinenew, meta=(DisplayName="AJA SDI Input"))
class AJAMEDIA_API UAjaCustomTimeStep : public UFixedFrameRateCustomTimeStep
{
	GENERATED_UCLASS_BODY()

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(UEngine* InEngine) override;
	virtual void Shutdown(UEngine* InEngine) override;
	virtual bool UpdateTimeStep(UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override;
	virtual FFrameRate GetFixedFrameRate() const override;

	//~ UObject interface
	virtual void BeginDestroy() override;
	
public:
	FAjaMediaMode GetMediaMode() const;
	void OverrideMediaMode(const FAjaMediaMode& InMediaMode);
	void DisableMediaModeOverride() { bIsDefaultModeOverriden = false; }

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

private:
	/** Override project setting's media mode. */
	UPROPERTY()
	bool bIsDefaultModeOverriden;

	/** The expected input signal format from the MediaPort. Uses project settings by default.*/
	UPROPERTY(EditAnywhere, Category="Genlock options", meta=(EditCondition="bIsDefaultModeOverriden", MediaPort="MediaPort"))
	FAjaMediaMode MediaMode;

public:
	/**
	 * If true, the Engine will wait for a signal coming in from the Reference In pin.
	 * It will also configure the card Genlock mode and configure the selected Media Port as an output.
	 */
	UPROPERTY(EditAnywhere, Category = "Genlock options")
	bool bUseReferenceIn;

	/**
	 * If true, the Engine will wait for the frame to be read.
	 * This will introduce random latency (the time it takes to read a frame).
     * Use this option when you want to synchronize the engine with the incoming frame and discard the buffered frames.
     * @note If false, there is no guarantee that the incoming frame will be ready since it takes some time to read a frame.
     * @note This will not work as intended with interlaced transport because both fields are processed at the same time.
	 */
	UPROPERTY(EditAnywhere, Category = "Genlock options", meta=(EditCondition="!bUseReferenceIn"))
	bool bWaitForFrameToBeReady;

	/** The type of Timecode to read from SDI stream. */
	UPROPERTY(EditAnywhere, Category="Genlock options", meta=(EditCondition="!bUseReferenceIn"))
	EAjaMediaTimecodeFormat TimecodeFormat;

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

#if WITH_EDITORONLY_DATA
	/** Engine used to initialize the CustomTimeStep */
	UPROPERTY(Transient)
	UEngine* InitializedEngine;

	/** When Auto synchronize is enabled, the time the last attempt was triggered. */
	double LastAutoSynchronizeInEditorAppTime;
#endif

	/** The current SynchronizationState of the CustomTimeStep */
	ECustomTimeStepSynchronizationState State;
	bool bDidAValidUpdateTimeStep;

	bool bWarnedAboutVSync;
};
