// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if WITH_PHYSX

#include "PhysXPublic.h"
#include "Physics/PhysicsInterfaceDeclares.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/SQAccelerator.h"
#include "PhysicsEngine/CollisionQueryFilterCallback.h"
#include "PhysicsEngine/PxQueryFilterCallback.h"

#if PHYSICS_INTERFACE_PHYSX
#include "PhysXInterfaceWrapper.h"
#include "SceneQueryPhysXImp.h"
#elif PHYSICS_INTERFACE_LLIMMEDIATE
#include "Physics/Experimental//LLImmediateInterfaceWrapper.h"
#include "Experimental/SceneQueryLLImmediateImp.h"
#endif


PxQueryFlags StaticDynamicQueryFlags(const FCollisionQueryParams& Params)
{
	switch (Params.MobilityType)
	{
	case EQueryMobilityType::Any: return  PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC;
	case EQueryMobilityType::Static: return  PxQueryFlag::eSTATIC;
	case EQueryMobilityType::Dynamic: return  PxQueryFlag::eDYNAMIC;
	default: check(0);
	}

	check(0);
	return PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC;
}

int32 ForceStandardSQ = 0;
FAutoConsoleVariableRef CVarForceStandardSQ(TEXT("p.ForceStandardSQ"), ForceStandardSQ, TEXT("If enabled, we force the standard scene query even if custom SQ structure is enabled"));

void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback)
{
#if WITH_CUSTOM_SQ_STRUCTURE
	if (ForceStandardSQ == 0)
	{
		ISQAccelerator* SQAccelerator = Scene.GetSQAccelerator();
		SQAccelerator->Raycast(Start, Dir, HitBuffer, OutputFlags, QueryFlags, Filter, *QueryCallback);
		FinalizeQuery(HitBuffer);
	}
	else
#endif
	{
#if PHYSICS_INTERFACE_PHYSX
		PxQueryFilterData QueryFilterData(U2PFilterData(Filter), U2PQueryFlags(QueryFlags) | StaticDynamicQueryFlags(Params));	//todo: is this needed for custom sq?
		Scene.GetPxScene()->raycast(U2PVector(Start), U2PVector(Dir), DeltaMag, HitBuffer, U2PHitFlags(OutputFlags), QueryFilterData, QueryCallback);
#endif
	}
}

void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback)
{
#if WITH_CUSTOM_SQ_STRUCTURE
	if (ForceStandardSQ == 0)
	{
		ISQAccelerator* SQAccelerator = Scene.GetSQAccelerator();
		SQAccelerator->Sweep(QueryGeom, StartTM, Dir, HitBuffer, OutputFlags, QueryFlags, Filter, *QueryCallback);
		FinalizeQuery(HitBuffer);
	}
	else
#endif
	{
#if PHYSICS_INTERFACE_PHYSX
		PxQueryFilterData QueryFilterData(U2PFilterData(Filter), U2PQueryFlags(QueryFlags) | StaticDynamicQueryFlags(Params));
		Scene.GetPxScene()->sweep(QueryGeom, U2PTransform(StartTM), U2PVector(Dir), DeltaMag, HitBuffer, U2PHitFlags(OutputFlags), QueryFilterData, QueryCallback);
#endif
	}
}

void LowLevelOverlap(FPhysScene& Scene, const PxGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback)
{
#if WITH_CUSTOM_SQ_STRUCTURE
	if (ForceStandardSQ == 0)
	{
		ISQAccelerator* SQAccelerator = Scene.GetSQAccelerator();
		SQAccelerator->Overlap(QueryGeom, GeomPose, HitBuffer, QueryFlags, Filter, *QueryCallback);
		FinalizeQuery(HitBuffer);
	}
	else
#endif
	{
#if PHYSICS_INTERFACE_PHYSX
		PxQueryFilterData QueryFilterData(U2PFilterData(Filter), StaticDynamicQueryFlags(Params) | U2PQueryFlags(QueryFlags));
		Scene.GetPxScene()->overlap(QueryGeom, U2PTransform(GeomPose), HitBuffer, QueryFilterData, QueryCallback);
#endif
	}
}


#endif // WITH_PHYSX 