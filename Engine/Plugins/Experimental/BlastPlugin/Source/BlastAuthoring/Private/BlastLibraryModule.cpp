// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BlastLibraryModule.h"
#include "MeshFractureSettings.h"
#include "FractureMesh.h"

class FBlastLibraryModule : public IBlastLibraryModule
{
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FBlastLibraryModule, BlastAuthoring);