// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureTimecodeProviderModule.h"

#include "AudioCaptureTimecodeProvider.h"

#include "Engine/Engine.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY(LogAudioCaptureTimecodeProvider);

class FAudioCaptureTimecodeProviderModule : public IModuleInterface, public FSelfRegisteringExec
{
	TStrongObjectPtr<UAudioCaptureTimecodeProvider> TimecodeProvider;
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("AudioCapture")))
		{
			if (FParse::Command(&Cmd, TEXT("TimecodeProvider")))
			{
				if (FParse::Command(&Cmd, TEXT("Start")))
				{
					TimecodeProvider.Reset(NewObject<UAudioCaptureTimecodeProvider>());

					FParse::Bool(Cmd, TEXT("DetectFrameRate="), TimecodeProvider->bDetectFrameRate);
					FParse::Bool(Cmd, TEXT("AssumeDropFrameFormat="), TimecodeProvider->bAssumeDropFrameFormat);
					FParse::Value(Cmd, TEXT("Numerator="), TimecodeProvider->FrameRate.Numerator);
					FParse::Value(Cmd, TEXT("Denominator="), TimecodeProvider->FrameRate.Denominator);
					FParse::Value(Cmd, TEXT("AudioChannel="), TimecodeProvider->AudioChannel);

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

IMPLEMENT_MODULE(FAudioCaptureTimecodeProviderModule, AudioCaptureTimecodeProvider)
