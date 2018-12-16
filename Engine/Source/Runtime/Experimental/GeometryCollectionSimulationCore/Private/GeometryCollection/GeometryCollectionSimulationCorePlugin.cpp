// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionSimulationCorePlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FGeometryCollectionSimulationCorePlugin : public IGeometryCollectionSimulationCorePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FGeometryCollectionSimulationCorePlugin, GeometryCollectionSimulationCore )



void FGeometryCollectionSimulationCorePlugin::StartupModule()
{
	
}


void FGeometryCollectionSimulationCorePlugin::ShutdownModule()
{
	
}



