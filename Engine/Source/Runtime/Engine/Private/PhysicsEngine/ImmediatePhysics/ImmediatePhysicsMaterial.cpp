// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Physics/ImmediatePhysics/ImmediatePhysicsMaterial.h"

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

namespace ImmediatePhysics
{
#if WITH_PHYSX
	FMaterial::FMaterial(physx::PxMaterial* InPxMaterial)
		: StaticFriction(InPxMaterial->getStaticFriction())
		, DynamicFriction(InPxMaterial->getDynamicFriction())
		, Restitution(InPxMaterial->getRestitution())
		, FrictionCombineMode((EFrictionCombineMode::Type)InPxMaterial->getFrictionCombineMode())
		, RestitutionCombineMode((EFrictionCombineMode::Type)InPxMaterial->getRestitutionCombineMode())
	{
	}
#endif

	/** Default shape material. Created from the CDO of UPhysicalMaterial */
	FMaterial FMaterial::Default;
}