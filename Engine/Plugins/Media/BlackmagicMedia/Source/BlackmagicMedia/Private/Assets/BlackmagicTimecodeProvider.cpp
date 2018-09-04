// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicTimecodeProvider.h"
#include "BlackmagicMediaPrivate.h"
#include "Blackmagic.h"


//~ BlackMagicDevice::IPortCallbackInterface implementation
//--------------------------------------------------------------------
// Those are called from the Blackmagic thread. There's a lock inside the Blackmagic layer
// to prevent this object from dying while in this thread.

struct UBlackmagicTimecodeProvider::FCallbackHandler : public BlackmagicDevice::IPortCallback
{
	FCallbackHandler(UBlackmagicTimecodeProvider* InOwner)
		: Owner(InOwner)
	{}

	//~ BlackmagicDevice::IPortCallback interface
	virtual void OnInitializationCompleted(bool bSucceed) override
	{
		Owner->State = bSucceed ? ETimecodeProviderSynchronizationState::Synchronized : ETimecodeProviderSynchronizationState::Error;
		if (!bSucceed)
		{
			UE_LOG(LogBlackmagicMedia, Error, TEXT("The initialization of '%s' failed. The TimecodeProvider won't be synchronized."), *Owner->GetName());
		}
	}
	virtual bool OnFrameArrived(BlackmagicDevice::FFrame InFrame)
	{
		return false;
	}

protected:
	UBlackmagicTimecodeProvider* Owner;
};


//~ UBlackmagicTimecodeProvider implementation
//--------------------------------------------------------------------
UBlackmagicTimecodeProvider::UBlackmagicTimecodeProvider(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AudioChannels(EBlackmagicMediaAudioChannel::Stereo2)
	, Device(nullptr)
	, Port(nullptr)
	, CallbackHandler(nullptr)
	, bIsRunning(false)
	, State(ETimecodeProviderSynchronizationState::Closed)
{
}

FTimecode UBlackmagicTimecodeProvider::GetTimecode() const
{
	if (Port)
	{
		BlackmagicDevice::FTimecode Timecode;
		if (Port->GetTimecode(Timecode))
		{
			return FTimecode(Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames, Timecode.bIsDropFrame);
		}
		else if (State == ETimecodeProviderSynchronizationState::Synchronized)
		{
			const_cast<UBlackmagicTimecodeProvider*>(this)->State = ETimecodeProviderSynchronizationState::Error;
		}
	}
	return FTimecode();
}

bool UBlackmagicTimecodeProvider::Initialize(class UEngine* InEngine)
{
	State = ETimecodeProviderSynchronizationState::Closed;

	if (!MediaPort.IsValid())
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The Source of '%s' is not valid."), *GetName());
		State = ETimecodeProviderSynchronizationState::Error;
		return false;
	}

	// create the device
	Device = BlackmagicDevice::VideoIOCreateDevice(MediaPort.DeviceIndex);

	if (Device == nullptr)
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("Can't aquire the Blackmagic device."));
		return false;
	}

	BlackmagicDevice::FPortOptions Options;
	FMemory::Memset(&Options, 0, sizeof(Options));
	// to enable the OnInitializationCompleted callback
	Options.bUseSync = true;
	Options.bUseTimecode = true;

	// TODO: configure audio
	if (AudioChannels == EBlackmagicMediaAudioChannel::Surround8)
	{
		Options.AudioChannels = 8;
	}
	else
	{
		Options.AudioChannels = 2;
	}

	BlackmagicDevice::FFrameDesc FrameDesc;

	// Blackmagic requires YUV for input
	FrameDesc.PixelFormat = BlackmagicDevice::EPixelFormat::PF_UYVY;
	Port = BlackmagicDevice::VideoIODeviceOpenSharedPort(Device, MediaPort.PortIndex, FrameDesc, Options);

	if (Port == nullptr)
	{
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("Can't aquire the Blackmagic port."));
		ReleaseResources();
		return false;
	}

	bIsRunning = true;

	check(CallbackHandler == nullptr);
	CallbackHandler = new FCallbackHandler(this);
	Port->SetCallback(CallbackHandler);

	return true;
}

void UBlackmagicTimecodeProvider::Shutdown(class UEngine* InEngine)
{
	State = ETimecodeProviderSynchronizationState::Closed;
	ReleaseResources();
}

void UBlackmagicTimecodeProvider::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
}

void UBlackmagicTimecodeProvider::ReleaseResources()
{
	// Stop if we are running
	if (bIsRunning && Port)
	{
		bIsRunning = false;
	}
	// cleanup the callback handler
	if (CallbackHandler && Port)
	{
		if (Port->SetCallback(nullptr))
		{
			delete CallbackHandler;
			CallbackHandler = nullptr;
		}
	}
	// close the port
	if (Port)
	{
		Port->Release();
		delete Port;
		Port = nullptr;
	}
	// close the device
	if (Device)
	{
		BlackmagicDevice::VideoIOReleaseDevice(Device);
		Device = nullptr;
	}
}
