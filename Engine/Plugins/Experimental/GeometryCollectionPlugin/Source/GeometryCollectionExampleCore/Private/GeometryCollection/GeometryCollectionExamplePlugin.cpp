// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExamplePlugin.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"




class FGeometryCollectionExamplePlugin : public IGeometryCollectionExamplePlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FGeometryCollectionExamplePlugin, GeometryCollectionExampleCore)



void FGeometryCollectionExamplePlugin::StartupModule()
{

}


void FGeometryCollectionExamplePlugin::ShutdownModule()
{

}
