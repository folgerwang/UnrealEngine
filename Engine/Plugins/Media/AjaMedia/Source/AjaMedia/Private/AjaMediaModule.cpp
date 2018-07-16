// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IAjaMediaModule.h"

#include "Aja/Aja.h"
#include "AJALib.h"
#include "AjaCustomTimeStep.h"
#include "AjaMediaPlayer.h"
#include "AjaTimecodeProvider.h"

#include "Engine/Engine.h"
#include "ITimeManagementModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY(LogAjaMedia);

#define LOCTEXT_NAMESPACE "AjaMediaModule"

namespace AJAHelpers
{
	FAjaMediaMode FromVideoFormatDescriptor(int32 InDeviceIndex, const AJA::AJAVideoFormats::VideoFormatDescriptor& InDescriptor)
	{
		FAjaMediaMode Mode;
		Mode.DeviceIndex = InDeviceIndex;
		Mode.ModeName = InDescriptor.FormatedText;
		Mode.VideoFormatIndex = InDescriptor.VideoFormatIndex;
		Mode.FrameRate = FFrameRate(InDescriptor.FrameRateNumerator, InDescriptor.FrameRateDenominator);
		Mode.TargetSize = FIntPoint(InDescriptor.Width, InDescriptor.Height);
		return Mode;
	}
}

/**
 * Implements the AJAMedia module.
 */
class FAjaMediaModule : public IAjaMediaModule, public FSelfRegisteringExec
{
public:

	//~ IAjaMediaModule interface
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (!FAja::IsInitialized())
		{
			return nullptr;
		}

		return MakeShared<FAjaMediaPlayer, ESPMode::ThreadSafe>(EventSink);
	}

	virtual bool IsInitialized() const override { return FAja::IsInitialized(); }

	virtual bool CanBeUsed() const override { return FAja::CanUseAJACard(); }

public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		// initialize AJA
		if (!FAja::Initialize())
		{
			UE_LOG(LogAjaMedia, Error, TEXT("Failed to initialize AJA"));
			return;
		}
	}

	virtual void ShutdownModule() override
	{
		FAja::Shutdown();
	}

	TStrongObjectPtr<UAjaCustomTimeStep> CustomTimeStep;
	TStrongObjectPtr<UAjaTimecodeProvider> TimecodeProvider;
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("AJA")))
		{
			if (FParse::Command(&Cmd, TEXT("CustomTimeStep")))
			{
				if (FParse::Command(&Cmd, TEXT("Start")))
				{
					CustomTimeStep.Reset(NewObject<UAjaCustomTimeStep>());

					CustomTimeStep->MediaPort.PortIndex = 0;
					CustomTimeStep->MediaPort.DeviceIndex = 0;
					FParse::Value(Cmd, TEXT("Port="), CustomTimeStep->MediaPort.PortIndex);
					FParse::Value(Cmd, TEXT("Device="), CustomTimeStep->MediaPort.DeviceIndex);
					FParse::Bool(Cmd, TEXT("EnableOverrunDetection="), CustomTimeStep->bEnableOverrunDetection);

					bool bOverrideProjectSetting = false;
					FParse::Bool(Cmd, TEXT("OverrideProjectSetting="), bOverrideProjectSetting);

					if (bOverrideProjectSetting)
					{
						AJA::FAJAVideoFormat VideoFormatIndex = AjaMediaOption::DefaultVideoFormat;
						FParse::Value(Cmd, TEXT("VideoFormat="), VideoFormatIndex);

						AJA::AJAVideoFormats::VideoFormatDescriptor Descriptor = AJA::AJAVideoFormats::GetVideoFormat(VideoFormatIndex);
						if (Descriptor.bValid)
						{
							const FAjaMediaMode MediaMode = AJAHelpers::FromVideoFormatDescriptor(CustomTimeStep->MediaPort.DeviceIndex, Descriptor);
							CustomTimeStep->OverrideMediaMode(MediaMode);
						}
					}

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
					TimecodeProvider.Reset(NewObject<UAjaTimecodeProvider>());

					TimecodeProvider->MediaPort.PortIndex = 0;
					TimecodeProvider->MediaPort.DeviceIndex = 0;
					FParse::Value(Cmd, TEXT("Port="), TimecodeProvider->MediaPort.PortIndex);
					FParse::Value(Cmd, TEXT("Device="), TimecodeProvider->MediaPort.DeviceIndex);
					int32 TimecodeFormatInt = 1;
					if (FParse::Value(Cmd, TEXT("TimecodeFormat="), TimecodeFormatInt))
					{
						TimecodeFormatInt = FMath::Clamp(TimecodeFormatInt, 0, (int32)EAjaMediaTimecodeFormat::VITC);
						TimecodeProvider->TimecodeFormat = (EAjaMediaTimecodeFormat)TimecodeFormatInt;
					}

					bool bOverrideProjectSetting = false;
					FParse::Bool(Cmd, TEXT("OverrideProjectSetting="), bOverrideProjectSetting);

					if (bOverrideProjectSetting)
					{
						AJA::FAJAVideoFormat VideoFormatIndex = AjaMediaOption::DefaultVideoFormat;
						FParse::Value(Cmd, TEXT("VideoFormat="), VideoFormatIndex);

						AJA::AJAVideoFormats::VideoFormatDescriptor Descriptor = AJA::AJAVideoFormats::GetVideoFormat(VideoFormatIndex);
						if (Descriptor.bValid)
						{
							const FAjaMediaMode MediaMode = AJAHelpers::FromVideoFormatDescriptor(TimecodeProvider->MediaPort.DeviceIndex, Descriptor);
							TimecodeProvider->OverrideMediaMode(MediaMode);
						}
					}

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

IMPLEMENT_MODULE(FAjaMediaModule, AjaMedia);

#undef LOCTEXT_NAMESPACE
