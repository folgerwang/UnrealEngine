// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Physics/ImmediatePhysics/ResourceManager.h"

ImmediatePhysics::FSharedResourceManager ImmediatePhysics::FSharedResourceManager::Instance;

ImmediatePhysics::FResourceHandle ImmediatePhysics::FSharedResourceManager::CreateMaterial()
{
	int32 Index = Materials.Add(TResourceWithId<ImmediatePhysics::FMaterial>());
	int32 Id = Materials[Index].Id;

	return FResourceHandle(EResourceType::Material, Index, Id);
}

void ImmediatePhysics::FSharedResourceManager::ReleaseMaterial(int32 InIndex)
{
	Materials.RemoveAt(InIndex);
}

int32 ImmediatePhysics::FSharedResourceManager::GetMaterialId(int32 InIndex)
{
	if(InIndex != INDEX_NONE && Materials.IsAllocated(InIndex))
	{
		return Materials[InIndex].Id;
	}

	return INDEX_NONE;
}

ImmediatePhysics::FMaterial* ImmediatePhysics::FSharedResourceManager::GetMaterial(int32 InIndex)
{
	if(InIndex != INDEX_NONE && Materials.IsAllocated(InIndex))
	{
		return &Materials[InIndex].Resource;
	}

	return nullptr;
}

FRWLock& ImmediatePhysics::FSharedResourceManager::GetLockObject()
{
	return ResourceLock;
}
