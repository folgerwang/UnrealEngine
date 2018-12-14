// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Field/FieldSystemSimulationCorePlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FFieldSystemSimulationCorePlugin : public IFieldSystemSimulationCorePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FFieldSystemSimulationCorePlugin, FieldSystemSimulationCore )



void FFieldSystemSimulationCorePlugin::StartupModule()
{
	
}


void FFieldSystemSimulationCorePlugin::ShutdownModule()
{
	
}



