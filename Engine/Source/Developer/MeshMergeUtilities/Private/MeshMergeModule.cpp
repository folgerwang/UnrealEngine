// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeModule.h"
#include "MeshMergeUtilities.h"
#include "Modules/ModuleManager.h"

class FMeshMergeModule : public IMeshMergeModule
{
public:
	virtual const IMeshMergeUtilities& GetUtilities() const override
	{
		return *dynamic_cast<const IMeshMergeUtilities*>(&Utilities);
	}

	virtual IMeshMergeUtilities& GetUtilities() override
	{
		return Utilities;
	}
protected:
	FMeshMergeUtilities Utilities;
};


IMPLEMENT_MODULE(FMeshMergeModule, MeshMergeUtilities);