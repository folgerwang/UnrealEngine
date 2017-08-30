// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IMeshBuilderModule.h"
#include "Modules/ModuleManager.h"

#include "StaticMeshBuilder.h"
#include "Engine/StaticMesh.h"

class FMeshBuilderModule : public IMeshBuilderModule
{
public:

	FMeshBuilderModule()
	{
	}

	virtual void StartupModule() override
	{
		// Register any modular features here
	}

	virtual void ShutdownModule() override
	{
		// Unregister any modular features here
	}

	virtual bool BuildMesh(class UObject* Mesh) override;

private:

};

IMPLEMENT_MODULE(FMeshBuilderModule, MeshBuilder );

bool FMeshBuilderModule::BuildMesh(class UObject* Mesh)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh);
	if (StaticMesh != nullptr)
	{
		//Call the static mesh builder
		return FStaticMeshBuilder::Get().Build(StaticMesh);
	}
	return false;
}
