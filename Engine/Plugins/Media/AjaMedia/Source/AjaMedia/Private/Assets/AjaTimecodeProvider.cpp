// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaTimecodeProvider.h"
#include "AjaMediaPrivate.h"
#include "AJA.h"


//~ IAJASyncChannelCallbackInterface implementation
//--------------------------------------------------------------------
// Those are called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
struct UAjaTimecodeProvider::FAJACallback : public AJA::IAJASyncChannelCallbackInterface
{
	UAjaTimecodeProvider* Owner;
	FAJACallback(UAjaTimecodeProvider* InOwner)
		: Owner(InOwner)
	{}

	//~ IAJAInputCallbackInterface interface
	virtual void OnInitializationCompleted(bool bSucceed) override
	{
		Owner->State = bSucceed ? ETimecodeProviderSynchronizationState::Synchronized : ETimecodeProviderSynchronizationState::Error;
		if (!bSucceed)
		{
			UE_LOG(LogAjaMedia, Error, TEXT("The initialization of '%s' failed. The TimecodeProvider won't be synchronized."), *Owner->GetName());
		}
	}
};


//~ UAjaTimecodeProvider implementation
//--------------------------------------------------------------------
UAjaTimecodeProvider::UAjaTimecodeProvider(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimecodeFormat(EAjaMediaTimecodeFormat::LTC)
	, SyncChannel(nullptr)
	, SyncCallback(nullptr)
	, State(ETimecodeProviderSynchronizationState::Closed)
{
}

FTimecode UAjaTimecodeProvider::GetTimecode() const
{
	if (SyncChannel)
	{
		AJA::FTimecode NewTimecode;
		if (SyncChannel->GetTimecode(NewTimecode))
		{
			return FAja::ConvertAJATimecode2Timecode(NewTimecode, FrameRate);
		}
		else if (State == ETimecodeProviderSynchronizationState::Synchronized)
		{
			const_cast<UAjaTimecodeProvider*>(this)->State = ETimecodeProviderSynchronizationState::Error;
		}
	}
	return FTimecode();
}

bool UAjaTimecodeProvider::Initialize(class UEngine* InEngine)
{
	State = ETimecodeProviderSynchronizationState::Closed;

	if (!MediaPort.IsValid())
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The Source of '%s' is not valid."), *GetName());
		State = ETimecodeProviderSynchronizationState::Error;
		return false;
	}

	check(SyncCallback == nullptr);
	SyncCallback = new FAJACallback(this);

	AJA::AJADeviceOptions DeviceOptions(MediaPort.DeviceIndex);

	AJA::AJASyncChannelOptions Options(*GetName(), MediaPort.PortIndex);
	Options.CallbackInterface = SyncCallback;
	Options.bUseTimecode = TimecodeFormat != EAjaMediaTimecodeFormat::None;
	Options.bUseLTCTimecode = TimecodeFormat == EAjaMediaTimecodeFormat::LTC;

	check(SyncChannel == nullptr);
	SyncChannel = new AJA::AJASyncChannel();
	if (!SyncChannel->Initialize(DeviceOptions, Options))
	{
		State = ETimecodeProviderSynchronizationState::Error;
		ReleaseResources();
		return false;
	}

	State = ETimecodeProviderSynchronizationState::Synchronizing;
	return true;
}

void UAjaTimecodeProvider::Shutdown(class UEngine* InEngine)
{
	State = ETimecodeProviderSynchronizationState::Closed;
	ReleaseResources();
}

void UAjaTimecodeProvider::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
}

void UAjaTimecodeProvider::ReleaseResources()
{
	if (SyncChannel)
	{
		SyncChannel->Uninitialize();
		delete SyncChannel;
		SyncChannel = nullptr;

		check(SyncCallback);
		delete SyncCallback;
		SyncCallback = nullptr;
	}
}
