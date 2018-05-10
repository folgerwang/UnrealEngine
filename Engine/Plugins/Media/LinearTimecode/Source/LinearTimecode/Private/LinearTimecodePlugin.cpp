// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LinearTimecodePlugin.h"

#include "LinearTimecodeMediaCustomTimeStep.h"

#include "FrameRate.h"
#include "Engine/Engine.h"
#include "ITimeManagementModule.h"
#include "MediaSource.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY(LogLinearTimecode);

class FLinearTimecodeModule : public IModuleInterface, public FSelfRegisteringExec
{
	TStrongObjectPtr<ULinearTimecodeMediaCustomTimeStep> CustomTimeStep;

	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
#if !UE_BUILD_SHIPPING
		if (FParse::Command(&Cmd, TEXT("LinearTimecode")))
		{
			if (FParse::Command(&Cmd, TEXT("MediaCustomTimeStep")))
			{
				if (FParse::Command(&Cmd, TEXT("Start")))
				{
					FString MediaSourcePath;
					FParse::Value(Cmd, TEXT("MediaSource="), MediaSourcePath);

					FSoftObjectPath ObjectPath = MediaSourcePath;
					UObject* Object = ObjectPath.TryLoad();
					if (Object == nullptr)
					{
						UE_LOG(LogLinearTimecode, Warning, TEXT("Can't load the MediaSource."));
						return true;
					}

					UMediaSource* MediaSource = Cast<UMediaSource>(Object);
					if (MediaSource == nullptr)
					{
						UE_LOG(LogLinearTimecode, Warning, TEXT("Can't cast to a MediaSource."));
						return true;
					}

					CustomTimeStep.Reset(NewObject<ULinearTimecodeMediaCustomTimeStep>());
					CustomTimeStep->MediaSource = MediaSource;

					uint32 Numerator = 30;
					uint32 Denominator = 1;
					FParse::Value(Cmd, TEXT("Numerator="), Numerator);
					FParse::Value(Cmd, TEXT("Denominator="), Denominator);
					CustomTimeStep->FrameRate = FFrameRate(Numerator, Denominator);

					GEngine->SetCustomTimeStep(CustomTimeStep.Get());
					ITimeManagementModule::Get().SetTimecodeProvider(CustomTimeStep.Get());
				}
				else if (FParse::Command(&Cmd, TEXT("Stop")))
				{
					if (CustomTimeStep.IsValid())
					{
						GEngine->SetCustomTimeStep(nullptr);
						if (ITimeManagementModule::Get().GetTimecodeProvider() == CustomTimeStep.Get())
						{
							ITimeManagementModule::Get().SetTimecodeProvider(nullptr);
						}
						CustomTimeStep.Reset();
					}
				}

				return true;
			}
		}
#endif
		return false;
	}
};

IMPLEMENT_MODULE(FLinearTimecodeModule, LinearTimecode)
