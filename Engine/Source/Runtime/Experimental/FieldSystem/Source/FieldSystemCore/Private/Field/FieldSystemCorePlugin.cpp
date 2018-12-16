// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Field/FieldSystemCorePlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FFieldSystemCorePlugin : public IFieldSystemCorePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FFieldSystemCorePlugin, FieldSystemCore )



void FFieldSystemCorePlugin::StartupModule()
{
	
}


void FFieldSystemCorePlugin::ShutdownModule()
{
	
}



