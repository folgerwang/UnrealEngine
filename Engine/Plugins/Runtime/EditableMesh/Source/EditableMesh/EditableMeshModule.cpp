// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EditableMeshModule.h"
#include "IEditableMeshModule.h"
#include "StaticMeshEditableMeshFormat.h"


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

};


void FEditableMeshModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature( "EditableMeshFormat", &StaticMeshEditableMeshFormat );
}


void FEditableMeshModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature( "EditableMeshFormat", &StaticMeshEditableMeshFormat );
}



IMPLEMENT_MODULE( FEditableMeshModule, EditableMesh )
