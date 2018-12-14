// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

#include "ImmediatePhysicsMaterial.h"

namespace ImmediatePhysics
{
	struct FMaterialHandle;
}

namespace ImmediatePhysics
{
/** Holds shape data*/
struct FShape
{
#if WITH_PHYSX
	PxTransform LocalTM;
	FMaterial* Material;
	PxGeometry* Geometry;
	PxVec3 BoundsOffset;
	float BoundsMagnitude;
	void* UserData;

	FShape()
		: LocalTM(PxTransform(PxIDENTITY::PxIdentity))
		, Material(nullptr)
		, Geometry(nullptr)
		, BoundsOffset(PxVec3(PxIDENTITY::PxIdentity))
		, BoundsMagnitude(0.0f)
		, UserData(nullptr)
	{}

	FShape(const PxTransform& InLocalTM, const PxVec3& InBoundsOffset, const float InBoundsMagnitude, PxGeometry* InGeometry, FMaterial* InMaterial = nullptr)
		: LocalTM(InLocalTM)
		, Material(InMaterial)
		, Geometry(InGeometry)
		, BoundsOffset(InBoundsOffset)
		, BoundsMagnitude(InBoundsMagnitude)
		, UserData(nullptr)
	{
	}

#endif
};

}