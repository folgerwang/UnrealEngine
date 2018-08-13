// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "STimecodeProviderTab.h"

/**
 * Timecode Provider Editor module
 */
class FTimecodeProviderEditorModule : public IModuleInterface
{
public:
	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		STimecodeProviderTab::RegisterNomadTabSpawner();
	}

	virtual void ShutdownModule() override
	{
		STimecodeProviderTab::UnregisterNomadTabSpawner();
	}
};

IMPLEMENT_MODULE(FTimecodeProviderEditorModule, TimecodeProviderEditorModule);