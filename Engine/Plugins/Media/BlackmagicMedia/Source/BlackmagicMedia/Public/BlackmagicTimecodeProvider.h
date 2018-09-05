// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TimecodeProvider.h"

#include "BlackmagicMediaFinder.h"
#include "BlackmagicMediaSource.h"

#include "BlackmagicTimecodeProvider.generated.h"

namespace BlackmagicDevice
{
	struct IPortShared;
	typedef void* FDevice;
	typedef IPortShared* FPort;
}

/**
 * Class to fetch a timecode via an AJA card
 */
UCLASS(editinlinenew, meta=(DisplayName="Blackmagic SDI Input"))
class BLACKMAGICMEDIA_API UBlackmagicTimecodeProvider : public UTimecodeProvider
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

	void ReleaseResources();

public:
	/** The AJA source from where the Timecode signal will be coming from. */
	UPROPERTY(EditAnywhere, Category="Timecode options", AssetRegistrySearchable, meta=(DisplayName="Source"))
	FBlackmagicMediaPort MediaPort;

	/** Frame rate expected from the SDI stream.  */
	UPROPERTY(EditAnywhere, Category="Timecode options")
	FFrameRate FrameRate;

	/** Desired number of audio channel to capture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Audio)
	EBlackmagicMediaAudioChannel AudioChannels;

private:

	/** Hardware device handle */
	BlackmagicDevice::FDevice Device;

	/** Hardware port handle */
	BlackmagicDevice::FPort Port;

	//* get notifications for InitializationCompleted */
	struct FCallbackHandler;
	friend FCallbackHandler;
	FCallbackHandler* CallbackHandler;

	/** Input is running */
	bool bIsRunning;

	/** The current SynchronizationState of the TimecodeProvider*/
	ETimecodeProviderSynchronizationState State;
};
