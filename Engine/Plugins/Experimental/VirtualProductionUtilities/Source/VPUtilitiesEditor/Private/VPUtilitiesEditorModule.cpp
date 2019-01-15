// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VPUtilitiesEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "VPCustomUIHandler.h"
#include "UObject/StrongObjectPtr.h"


DEFINE_LOG_CATEGORY(LogVPUtilitiesEditor);


class FBVPUEditorModule : public IModuleInterface
{
public:
	TStrongObjectPtr<UVPCustomUIHandler> CustomUIHandler;

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		CustomUIHandler.Reset(NewObject<UVPCustomUIHandler>());
		CustomUIHandler->Init();
	}

	virtual void ShutdownModule() override
	{
		CustomUIHandler->Uninit();
		CustomUIHandler.Reset();
	}
};


IMPLEMENT_MODULE(FBVPUEditorModule, VPUtilitiesEditor)
