// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionPlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FGeometryCollectionPlugin : public IGeometryCollectionPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FGeometryCollectionPlugin, GeometryCollection )



void FGeometryCollectionPlugin::StartupModule()
{
	
}


void FGeometryCollectionPlugin::ShutdownModule()
{
	
}



