// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TimecodeProvider.h"

#include "AjaMediaFinder.h"
#include "AjaMediaSource.h"

#include "AjaTimecodeProvider.generated.h"

namespace AJA
{
	class AJASyncChannel;
}

/**
 * Class to fetch a timecode via an AJA card
 */
UCLASS(editinlinenew, meta=(DisplayName="AJA SDI Input"))
class AJAMEDIA_API UAjaTimecodeProvider : public UTimecodeProvider
{
	GENERATED_UCLASS_BODY()

public:
	//~ UTimecodeProvider interface
	virtual FTimecode GetTimecode() const override;
	virtual FFrameRate GetFrameRate() const override { return FrameRate; }
	virtual ETimecodeProviderSynchronizationState GetSynchronizationState() const override { return State; }
	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;

	//~ UObject interface
	virtual void BeginDestroy() override;

private:
	struct FAJACallback;
	friend FAJACallback;

	void ReleaseResources();

public:
	/** The AJA source from where the Timecode signal will be coming from. */
	UPROPERTY(EditAnywhere, Category="Timecode options", AssetRegistrySearchable, meta=(DisplayName="Source"))
	FAjaMediaPort MediaPort;

	/** The type of Timecode to read from SDI stream. */
	UPROPERTY(EditAnywhere, Category="Timecode options")
	EAjaMediaTimecodeFormat TimecodeFormat;

	/** Frame rate expected from the SDI stream.  */
	UPROPERTY(EditAnywhere, Category="Timecode options")
	FFrameRate FrameRate;


private:
	/** AJA Port to capture the Sync */
	AJA::AJASyncChannel* SyncChannel;
	FAJACallback* SyncCallback;

	/** The current SynchronizationState of the TimecodeProvider*/
	ETimecodeProviderSynchronizationState State;
};
