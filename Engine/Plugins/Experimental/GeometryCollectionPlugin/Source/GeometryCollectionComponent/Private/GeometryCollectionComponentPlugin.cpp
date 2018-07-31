// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollectionComponentPlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FGeometryCollectionComponentPlugin : public IGeometryCollectionComponentPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FGeometryCollectionComponentPlugin, GeometryCollectionComponent )



void FGeometryCollectionComponentPlugin::StartupModule()
{
	
}


void FGeometryCollectionComponentPlugin::ShutdownModule()
{
	
}



