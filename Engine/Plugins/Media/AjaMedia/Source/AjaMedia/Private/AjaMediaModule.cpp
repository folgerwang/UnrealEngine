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

/**
 * Implements the AJAMedia module.
 */
class FAjaMediaModule : public IAjaMediaModule
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
};

IMPLEMENT_MODULE(FAjaMediaModule, AjaMedia);

#undef LOCTEXT_NAMESPACE
