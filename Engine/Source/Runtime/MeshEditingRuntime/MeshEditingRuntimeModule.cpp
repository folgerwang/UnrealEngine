// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MeshEditingRuntimeModule.h"
#include "IMeshEditingRuntimeModule.h"
#include "StaticMeshEditableMeshFormat.h"

DEFINE_LOG_CATEGORY( LogMeshEditingRuntime );


class FMeshEditingRuntimeModule : public IMeshEditingRuntimeModule
{
public:

	FMeshEditingRuntimeModule()
	{
	}

	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:

	/** Static mesh editing */
	FStaticMeshEditableMeshFormat StaticMeshEditableMeshFormat;

};


void FMeshEditingRuntimeModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature( "EditableMeshFormat", &StaticMeshEditableMeshFormat );
}


void FMeshEditingRuntimeModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature( "EditableMeshFormat", &StaticMeshEditableMeshFormat );
}



IMPLEMENT_MODULE( FMeshEditingRuntimeModule, MeshEditingRuntime )
