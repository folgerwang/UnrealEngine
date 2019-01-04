// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicalMaterial.cpp
=============================================================================*/ 

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UObjectIterator.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "PhysicsPublic.h"
#include "PhysicalMaterials/PhysicalMaterialPropertyBase.h"
#include "PhysicsEngine/PhysicsSettings.h"

#if WITH_PHYSX
	#include "PhysicsEngine/PhysXSupport.h"
#endif // WITH_PHYSX

UDEPRECATED_PhysicalMaterialPropertyBase::UDEPRECATED_PhysicalMaterialPropertyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPhysicalMaterial::UPhysicalMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Friction = 0.7f;
	Restitution = 0.3f;
	RaiseMassToPower = 0.75f;
	Density = 1.0f;
	DestructibleDamageThresholdScale = 1.0f;
	TireFrictionScale = 1.0f;
	bOverrideFrictionCombineMode = false;
#if WITH_PHYSX
	PhysxUserData = FPhysxUserData(this);
#endif
}

#if WITH_EDITOR
void UPhysicalMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Update PhysX material last so we have a valid Parent
	FPhysicsInterface::UpdateMaterial(MaterialHandle, this);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UPhysicalMaterial::RebuildPhysicalMaterials()
{
	for (FObjectIterator Iter(UPhysicalMaterial::StaticClass()); Iter; ++Iter)
	{
		if (UPhysicalMaterial * PhysicalMaterial = Cast<UPhysicalMaterial>(*Iter))
		{
			FPhysicsInterface::UpdateMaterial(PhysicalMaterial->MaterialHandle, PhysicalMaterial);
		}
	}
}

#endif // WITH_EDITOR

void UPhysicalMaterial::PostLoad()
{
	Super::PostLoad();

	// we're removing physical material property, so convert to Material type
	if (GetLinkerUE4Version() < VER_UE4_REMOVE_PHYSICALMATERIALPROPERTY)
	{
		if (PhysicalMaterialProperty)
		{
			SurfaceType = PhysicalMaterialProperty->ConvertToSurfaceType();
		}
	}
}

void UPhysicalMaterial::FinishDestroy()
{
	FPhysicsInterface::ReleaseMaterial(MaterialHandle);
	Super::FinishDestroy();
}

FPhysicsMaterialHandle& UPhysicalMaterial::GetPhysicsMaterial()
{
#if WITH_CHAOS
    check(false);
    MaterialHandle = nullptr;
    return MaterialHandle;
#else
	if(!MaterialHandle.IsValid())
	{
		MaterialHandle = FPhysicsInterface::CreateMaterial(this);
		check(MaterialHandle.IsValid());

#if WITH_PHYSX
		FPhysicsInterface::SetUserData(MaterialHandle, &PhysxUserData);
#endif
		FPhysicsInterface::UpdateMaterial(MaterialHandle, this);
	}

	return MaterialHandle;
#endif
}

EPhysicalSurface UPhysicalMaterial::DetermineSurfaceType(UPhysicalMaterial const* PhysicalMaterial)
{
	if (PhysicalMaterial == NULL)
	{
		PhysicalMaterial = GEngine->DefaultPhysMaterial;
	}
	
	return PhysicalMaterial->SurfaceType;
}

