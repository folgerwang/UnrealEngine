// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TimecodeProvider.h"

#include "AjaMediaFinder.h"
#include "AjaMediaSource.h"
#include "Tickable.h"

#include "AjaTimecodeProvider.generated.h"

namespace AJA
{
	class AJASyncChannel;
}

class UEngine;

/**
 * Class to fetch a timecode via an AJA card.
 * When the signal is lost in the editor (not in PIE), the TimecodeProvider will try to re-synchronize every second.
 */
UCLASS(Blueprintable, editinlinenew, meta=(DisplayName="AJA SDI Input"))
class AJAMEDIA_API UAjaTimecodeProvider : public UTimecodeProvider, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:
	//~ UTimecodeProvider interface
	virtual FTimecode GetTimecode() const override;
	virtual FFrameRate GetFrameRate() const override { return GetMediaMode().FrameRate; }
	virtual ETimecodeProviderSynchronizationState GetSynchronizationState() const override { return State; }
	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;

	//~ FTickableGameObject interface
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAjaTimecodeProvider, STATGROUP_Tickables); }

	//~ UObject interface
	virtual void BeginDestroy() override;

public:
	FAjaMediaMode GetMediaMode() const;
	void OverrideMediaMode(const FAjaMediaMode& InMediaMode);
	void DisableMediaModeOverride() { bIsDefaultModeOverriden = false; }

private:
	struct FAJACallback;
	friend FAJACallback;

	void ReleaseResources();

public:
	/** The AJA source from where the Timecode signal will be coming from. */
	UPROPERTY(EditAnywhere, Category="Timecode options", AssetRegistrySearchable, meta=(DisplayName="Source"))
	FAjaMediaPort MediaPort;

private:
	/** Override project setting's media mode. */
	UPROPERTY()
	bool bIsDefaultModeOverriden;

	/** The expected input signal format from the MediaPort. Uses project settings by default. */
	UPROPERTY(EditAnywhere, Category="Timecode options", meta=(EditCondition="bIsDefaultModeOverriden", MediaPort="MediaPort"))
	FAjaMediaMode MediaMode;

public:

	/** The type of Timecode to read from SDI stream. */
	UPROPERTY(EditAnywhere, Category="Timecode options")
	EAjaMediaTimecodeFormat TimecodeFormat;

private:
	/** AJA Port to capture the Sync */
	AJA::AJASyncChannel* SyncChannel;
	FAJACallback* SyncCallback;

#if WITH_EDITORONLY_DATA
	/** Engine used to initialize the Provider */
	UPROPERTY(Transient)
	UEngine* InitializedEngine;

	/** The time the last attempt to auto synchronize was triggered. */
	double LastAutoSynchronizeInEditorAppTime;
#endif

	/** The current SynchronizationState of the TimecodeProvider*/
	ETimecodeProviderSynchronizationState State;
};
