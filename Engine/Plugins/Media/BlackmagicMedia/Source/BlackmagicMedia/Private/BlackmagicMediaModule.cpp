// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IBlackmagicMediaModule.h"

#include "Blackmagic/Blackmagic.h"
#include "BlackmagicMediaPlayer.h"
#include "BlackmagicCustomTimeStep.h"
#include "BlackmagicTimecodeProvider.h"

#include "Engine/Engine.h"
#include "ITimeManagementModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"


DEFINE_LOG_CATEGORY(LogBlackmagicMedia);

#define LOCTEXT_NAMESPACE "BlackmagicMediaModule"

/**
 * Implements the NdiMedia module.
 */
class FBlackmagicMediaModule : public IBlackmagicMediaModule, public FSelfRegisteringExec
{
public:

	//~ IBlackmagicMediaModule interface
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (!FBlackmagic::IsInitialized())
		{
			return nullptr;
		}

		return MakeShared<FBlackmagicMediaPlayer, ESPMode::ThreadSafe>(EventSink);
	}

	virtual bool IsInitialized() const override { return FBlackmagic::IsInitialized(); }

public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		// initialize
		if (!FBlackmagic::Initialize())
		{
			UE_LOG(LogBlackmagicMedia, Error, TEXT("Failed to initialize Blackmagic"));
			return;
		}
	}

	virtual void ShutdownModule() override
	{
		FBlackmagic::Shutdown();
	}

	TStrongObjectPtr<UBlackmagicCustomTimeStep> CustomTimeStep;
	TStrongObjectPtr<UBlackmagicTimecodeProvider> TimecodeProvider;
	
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("Blackmagic")))
		{
			if (FParse::Command(&Cmd, TEXT("CustomTimeStep")))
			{
				if (FParse::Command(&Cmd, TEXT("Start")))
				{
					CustomTimeStep.Reset(NewObject<UBlackmagicCustomTimeStep>());

					CustomTimeStep->MediaPort.PortIndex = 1;
					CustomTimeStep->MediaPort.DeviceIndex = 0;
					FParse::Value(Cmd, TEXT("Port="), CustomTimeStep->MediaPort.PortIndex);
					FParse::Value(Cmd, TEXT("Device="), CustomTimeStep->MediaPort.DeviceIndex);
					FParse::Bool(Cmd, TEXT("EnableOverrunDetection="), CustomTimeStep->bEnableOverrunDetection);

					int32 AudioChannels = 2;
					EBlackmagicMediaAudioChannel MediaAudioChannel = EBlackmagicMediaAudioChannel::Stereo2;
					if (FParse::Value(Cmd, TEXT("AudioChannels="), AudioChannels))
					{
						if (AudioChannels == 8)
						{
							MediaAudioChannel = EBlackmagicMediaAudioChannel::Surround8;
						}
					}
					CustomTimeStep->AudioChannels = MediaAudioChannel;

					{
						GEngine->SetCustomTimeStep(CustomTimeStep.Get());
					}
				}
				else if (FParse::Command(&Cmd, TEXT("Stop")))
				{
					if (GEngine->GetCustomTimeStep() == CustomTimeStep.Get())
					{
						GEngine->SetCustomTimeStep(nullptr);
					}
					CustomTimeStep.Reset();
				}
				return true;
			}

			if (FParse::Command(&Cmd, TEXT("TimecodeProvider")))
			{
				if (FParse::Command(&Cmd, TEXT("Start")))
				{
					TimecodeProvider.Reset(NewObject<UBlackmagicTimecodeProvider>());

					// ports are numbered from 1
					TimecodeProvider->MediaPort.PortIndex = 1;
					TimecodeProvider->MediaPort.DeviceIndex = 0;
					FParse::Value(Cmd, TEXT("Port="), TimecodeProvider->MediaPort.PortIndex);
					FParse::Value(Cmd, TEXT("Device="), TimecodeProvider->MediaPort.DeviceIndex);
					FParse::Value(Cmd, TEXT("Numerator="), TimecodeProvider->FrameRate.Numerator);
					FParse::Value(Cmd, TEXT("Denominator="), TimecodeProvider->FrameRate.Denominator);

					int32 AudioChannels = 2;
					EBlackmagicMediaAudioChannel MediaAudioChannel = EBlackmagicMediaAudioChannel::Stereo2;

					if (FParse::Value(Cmd, TEXT("AudioChannels="), AudioChannels))
					{
						if (AudioChannels == 8)
						{
							MediaAudioChannel = EBlackmagicMediaAudioChannel::Surround8;
						}
					}
					TimecodeProvider->AudioChannels = MediaAudioChannel;

					GEngine->SetTimecodeProvider(TimecodeProvider.Get());
				}
				else if (FParse::Command(&Cmd, TEXT("Stop")))
				{
					if (GEngine->GetTimecodeProvider() == TimecodeProvider.Get())
					{
						GEngine->SetTimecodeProvider(nullptr);
					}
					TimecodeProvider.Reset();
				}
				return true;
			}
		}
		return false;
	}
};

IMPLEMENT_MODULE(FBlackmagicMediaModule, BlackmagicMedia);

#undef LOCTEXT_NAMESPACE
