// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineCustomTimeStep.h"
	
#include "BlackmagicLib.h"
#include "BlackmagicMediaFinder.h"
#include "BlackmagicMediaSource.h"

#include "HAL/RunnableThread.h"
#include "MediaIOCoreWaitVSyncThread.h"

#include "BlackmagicCustomTimeStep.generated.h"

/**
 * Class to control the Engine TimeStep via the Blackmagic card
 */
UCLASS(editinlinenew, meta=(DisplayName="Blackmagic SDI Input"))
class BLACKMAGICMEDIA_API UBlackmagicCustomTimeStep : public UEngineCustomTimeStep
{
	GENERATED_UCLASS_BODY()

public:
	
	//~ UEngineCustomTimeStep interface
	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;
	virtual bool UpdateTimeStep(class UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override;

private:

	void WaitForVSync() const;

public:
	/**
	* The Blackmagic source from where the Genlock signal will be coming from.
	*/
	UPROPERTY(EditAnywhere, Category="Genlock options", AssetRegistrySearchable, meta=(DisplayName="Source"))
	FBlackmagicMediaPort MediaPort;

	/** Fixed tick rate */
	UPROPERTY(EditAnywhere, Category="Genlock options", Meta= (ClampMin = 1))
	float FixedFPS;

	/** Enable mechanism to detect Engine loop overrunning the source */
	UPROPERTY(EditAnywhere, Category="Genlock options", meta=(DisplayName="Display Dropped Frames Warning"))
	bool bEnableOverrunDetection;

	/** Desired number of audio channel to capture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	EBlackmagicMediaAudioChannel AudioChannels;

private:
	/** Blackmagic Device to capture the Sync */
	BlackmagicDevice::FDevice Device;

	/** Blackmagic Port to capture the Sync */
	BlackmagicDevice::FPort Port;
	
	/** WaitForVSync task Runnable */
	TUniquePtr<FMediaIOCoreWaitVSyncThread> VSyncThread;

	/** WaitForVSync thread */
	TUniquePtr<FRunnableThread> VSyncRunnableThread;

	/** The current SynchronizationState of the CustomTimeStep */
	ECustomTimeStepSynchronizationState State;
};
