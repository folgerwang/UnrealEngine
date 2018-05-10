// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IAjaMediaOutputModule.h"

#include "AjaMediaFrameGrabberProtocol.h"

#include "IMovieSceneCaptureProtocol.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneCaptureModule.h"
#include "MovieSceneCaptureProtocolRegistry.h"

#define LOCTEXT_NAMESPACE "AjaMediaOutput"

DEFINE_LOG_CATEGORY(LogAjaMediaOutput);

class FAjaMediaOutputModule : public IAjaMediaOutputModule
{
	static const TCHAR* AjaMediaOutputProtocol_Str;

	virtual void StartupModule() override
	{
		FMovieSceneCaptureProtocolInfo Info;
		Info.DisplayName = LOCTEXT("AjaCaptureProtocol", "AJA");
		Info.SettingsClassType = UAjaFrameGrabberProtocolSettings::StaticClass();
		Info.Factory = []() -> TSharedRef<IMovieSceneCaptureProtocol> { return MakeShareable(new FAjaFrameGrabberProtocol()); };

		IMovieSceneCaptureModule::Get().GetProtocolRegistry().RegisterProtocol(AjaMediaOutputProtocol_Str, Info);
	}

	virtual void ShutdownModule() override
	{
		IMovieSceneCaptureModule::Get().GetProtocolRegistry().UnRegisterProtocol(AjaMediaOutputProtocol_Str);
	}
};

const TCHAR* FAjaMediaOutputModule::AjaMediaOutputProtocol_Str = TEXT("AJA Output");

IMPLEMENT_MODULE(FAjaMediaOutputModule, AjaMediaOutput )

#undef LOCTEXT_NAMESPACE
