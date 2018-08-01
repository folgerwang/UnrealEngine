// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "PhysicsInterfaceDeclares.h"
#include "PhysicsEngine/BodySetupEnums.h"


// Defines for enabling hitch repeating (see ScopedSQHitchRepeater.h)
#if !UE_BUILD_SHIPPING
#define DETECT_SQ_HITCHES 1
#endif

#ifndef DETECT_SQ_HITCHES
#define DETECT_SQ_HITCHES 0
#endif

struct FKAggregateGeom;

namespace physx
{
	class PxShape;
	class PxTriangleMesh;
}

struct FCollisionFilterData
{
	uint32 Word0;
	uint32 Word1;
	uint32 Word2;
	uint32 Word3;

	FORCEINLINE FCollisionFilterData()
	{
		Word0 = Word1 = Word2 = Word3 = 0;
	}
};

/**
* Type of query for object type or trace type
* Trace queries correspond to trace functions with TravelChannel/ResponseParams
* Object queries correspond to trace functions with Object types
*/
enum class ECollisionQuery : uint8
{
	ObjectQuery = 0,
	TraceQuery = 1
};

enum class ECollisionShapeType : uint8
{
	Sphere,
	Box,
	Capsule,
	Convex,
	Trimesh,
	Heightfield,
	None
};

/** Helper struct holding physics body filter data during initialisation */
struct FBodyCollisionFilterData
{
	FCollisionFilterData SimFilter;
	FCollisionFilterData QuerySimpleFilter;
	FCollisionFilterData QueryComplexFilter;
};

struct FBodyCollisionFlags
{
	FBodyCollisionFlags()
		: bEnableSimCollisionSimple(false)
		, bEnableSimCollisionComplex(false)
		, bEnableQueryCollision(false)
	{
	}

	bool bEnableSimCollisionSimple;
	bool bEnableSimCollisionComplex;
	bool bEnableQueryCollision;
};


/** Helper object to hold initialisation data for shapes */
struct FBodyCollisionData
{
	FBodyCollisionFilterData CollisionFilterData;
	FBodyCollisionFlags CollisionFlags;
};

struct FActorCreationParams
{
	FActorCreationParams()
		: Scene(nullptr)
		, InitialTM(FTransform::Identity)
		, bStatic(false)
		, bQueryOnly(false)
		, bUseAsyncScene(false)
		, bEnableGravity(false)
		, DebugName(nullptr)
	{}

	FPhysScene* Scene;
	FTransform InitialTM;
	bool bStatic;
	bool bQueryOnly;
	bool bUseAsyncScene;
	bool bEnableGravity;
	char* DebugName;
};

struct FGeometryAddParams
{
	EPhysicsSceneType SceneType;
	bool bSharedShapes;
	bool bDoubleSided;
	FBodyCollisionData CollisionData;
	ECollisionTraceFlag CollisionTraceType;
	FVector Scale;
	UPhysicalMaterial* SimpleMaterial;
	TArrayView<UPhysicalMaterial*> ComplexMaterials;
	FTransform LocalTransform;
	FKAggregateGeom* Geometry;
	// FPhysicsInterfaceTriMesh - Per implementation
#if WITH_PHYSX
	TArrayView<physx::PxTriangleMesh*> TriMeshes;
#endif
};

namespace PhysicsInterfaceTypes
{
	enum class ELimitAxis : uint8
	{
		X,
		Y,
		Z,
		Twist,
		Swing1,
		Swing2
	};

	enum class EDriveType : uint8
	{
		X,
		Y,
		Z,
		Swing,
		Twist,
		Slerp
	};

	/**
	* Default number of inlined elements used in FInlineShapeArray.
	* Increase if for instance character meshes use more than this number of physics bodies and are involved in many queries.
	*/
	enum { NumInlinedPxShapeElements = 32 };

	/** Array that is intended for use when fetching shapes from a rigid body. */
	typedef TArray<FPhysicsShapeHandle, TInlineAllocator<NumInlinedPxShapeElements>> FInlineShapeArray;
}

static void SetupNonUniformHelper(FVector InScale3D, float& OutMinScale, float& OutMinScaleAbs, FVector& OutScale3DAbs)
{
	// if almost zero, set min scale
	// @todo fixme
	if(InScale3D.IsNearlyZero())
	{
		// set min scale
		InScale3D = FVector(0.1f);
	}

	OutScale3DAbs = InScale3D.GetAbs();
	OutMinScaleAbs = OutScale3DAbs.GetMin();

	OutMinScale = FMath::Max3(InScale3D.X, InScale3D.Y, InScale3D.Z) < 0.f ? -OutMinScaleAbs : OutMinScaleAbs;	//if all three values are negative make minScale negative

	if(FMath::IsNearlyZero(OutMinScale))
	{
		// only one of them can be 0, we make sure they have mini set up correctly
		OutMinScale = 0.1f;
		OutMinScaleAbs = 0.1f;
	}
}

/** Util to determine whether to use NegX version of mesh, and what transform (rotation) to apply. */
static bool CalcMeshNegScaleCompensation(const FVector& InScale3D, FTransform& OutTransform)
{
	OutTransform = FTransform::Identity;

	if(InScale3D.Y > 0.f)
	{
		if(InScale3D.Z > 0.f)
		{
			// no rotation needed
		}
		else
		{
			// y pos, z neg
			OutTransform.SetRotation(FQuat(FVector(0.0f, 1.0f, 0.0f), PI));
			//OutTransform.q = PxQuat(PxPi, PxVec3(0,1,0));
		}
	}
	else
	{
		if(InScale3D.Z > 0.f)
		{
			// y neg, z pos
			//OutTransform.q = PxQuat(PxPi, PxVec3(0,0,1));
			OutTransform.SetRotation(FQuat(FVector(0.0f, 0.0f, 1.0f), PI));
		}
		else
		{
			// y neg, z neg
			//OutTransform.q = PxQuat(PxPi, PxVec3(1,0,0));
			OutTransform.SetRotation(FQuat(FVector(1.0f, 0.0f, 0.0f), PI));
		}
	}

	// Use inverted mesh if determinant is negative
	return (InScale3D.X * InScale3D.Y * InScale3D.Z) < 0.f;
}