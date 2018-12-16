// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditableMeshModule.h"
#include "IEditableMeshModule.h"
#include "StaticMeshEditableMeshFormat.h"
#include "GeometryCollectionEditableMeshFormat.h"


class FEditableMeshModule : public IEditableMeshModule
{
public:

	FEditableMeshModule()
	{
	}

	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:

	/** Static mesh editing */
	FStaticMeshEditableMeshFormat StaticMeshEditableMeshFormat;

	/** Geometry Collection editing */
	FGeometryCollectionEditableMeshFormat GeometryCollectionEditableMeshFormat;

};


void FEditableMeshModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature("EditableMeshFormat", &StaticMeshEditableMeshFormat);
	IModularFeatures::Get().RegisterModularFeature("EditableMeshFormat", &GeometryCollectionEditableMeshFormat);
}


void FEditableMeshModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature("EditableMeshFormat", &StaticMeshEditableMeshFormat);
	IModularFeatures::Get().UnregisterModularFeature("EditableMeshFormat", &GeometryCollectionEditableMeshFormat);
}



IMPLEMENT_MODULE( FEditableMeshModule, EditableMesh )
