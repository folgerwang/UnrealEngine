// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosSolverEnginePlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FChaosSolverEnginePlugin : public IChaosSolverEnginePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FChaosSolverEnginePlugin, ChaosSolverEngine )

void FChaosSolverEnginePlugin::StartupModule()
{
	
}

void FChaosSolverEnginePlugin::ShutdownModule()
{
	
}



