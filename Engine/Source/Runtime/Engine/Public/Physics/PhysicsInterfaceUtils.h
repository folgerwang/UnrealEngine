// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysXPublic.h"
#include "Containers/Union.h"
#include "Physics/PhysicsInterfaceTypes.h"

class FPhysScene_PhysX;

#if WITH_PHYSX

ENGINE_API PxShapeFlags BuildPhysXShapeFlags(FBodyCollisionFlags BodyCollisionFlags, bool bPhysicsStatic, bool bIsSync, bool bIsTriangleMesh);
ENGINE_API PxFilterData U2PFilterData(const FCollisionFilterData& FilterData);
FCollisionFilterData P2UFilterData(const PxFilterData& PFilterData);
ENGINE_API PxGeometryType::Enum U2PCollisionShapeType(ECollisionShapeType InUType);
ENGINE_API ECollisionShapeType P2UCollisionShapeType(PxGeometryType::Enum InPType);

template<typename AGGREGATE_FLAG_TYPE, typename FLAG_TYPE>
inline void ModifyFlag_Default(AGGREGATE_FLAG_TYPE& Flags, const FLAG_TYPE FlagToSet, const bool bValue)
{
	if (bValue)
	{
		Flags |= FlagToSet;
	}
	else
	{
		Flags.clear(FlagToSet);
	}
}

template<const PxActorFlag::Enum FlagToSet>
inline void ModifyActorFlag(physx::PxActorFlags& Flags, const bool bValue)
{
	ModifyFlag_Default(Flags, FlagToSet, bValue);
}

template<const PxShapeFlag::Enum FlagToSet>
inline void ModifyShapeFlag(physx::PxShapeFlags& Flags, const bool bValue)
{
	ModifyFlag_Default(Flags, FlagToSet, bValue);
}

template<const PxRigidBodyFlag::Enum FlagToSet>
inline void ModifyRigidBodyFlag(physx::PxRigidBodyFlags& Flags, const bool bValue)
{
	ModifyFlag_Default(Flags, FlagToSet, bValue);
}

template<>
inline void ModifyRigidBodyFlag<PxRigidBodyFlag::eKINEMATIC>(physx::PxRigidBodyFlags& Flags, const bool bValue)
{
	// Objects can't be CCD and Kinematic at the same time.
	// If enabling Kinematic and CCD is on, disable it and turn on Speculative CCD instead.
	if (bValue && Flags.isSet(PxRigidBodyFlag::eENABLE_CCD))
	{
		Flags |= PxRigidBodyFlag::eKINEMATIC;
		Flags |= PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD;
		Flags.clear(PxRigidBodyFlag::eENABLE_CCD);
	}

	// If disabling Kinematic and Speculative CCD is on, disable it and turn on CCD instead.
	else if (!bValue && Flags.isSet(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD))
	{
		Flags |= PxRigidBodyFlag::eENABLE_CCD;
		Flags.clear(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD);
		Flags.clear(PxRigidBodyFlag::eKINEMATIC);
	}

	// No sanitization is needed.
	else
	{
		ModifyFlag_Default(Flags, PxRigidBodyFlag::eKINEMATIC, bValue);
	}
}

template<>
inline void ModifyRigidBodyFlag<PxRigidBodyFlag::eENABLE_CCD>(physx::PxRigidBodyFlags& Flags, const bool bValue)
{
	// Objects can't be CCD and Kinematic at the same time.
	// If disabling CCD and Speculative CCD is on, disable it too.
	if (!bValue && Flags.isSet(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD))
	{
		// CCD shouldn't be enabled at this point, but force disable just in case.
		Flags.clear(PxRigidBodyFlag::eENABLE_CCD);
		Flags.clear(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD);
	}

	// If enabling CCD but Kinematic is on, enable Speculative CCD instead.
	else if (bValue && Flags.isSet(PxRigidBodyFlag::eKINEMATIC))
	{
		Flags |= PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD;
	}

	// No sanitization is needed.
	else
	{
		ModifyFlag_Default(Flags, PxRigidBodyFlag::eENABLE_CCD, bValue);
	}
}

template<const PxActorFlag::Enum FlagToSet>
inline void ModifyActorFlag_Isolated(PxActor* PActor, const bool bValue)
{
	PxActorFlags ActorFlags = PActor->getActorFlags();
	ModifyActorFlag<FlagToSet>(ActorFlags, bValue);
	PActor->setActorFlags(ActorFlags);
}

template<const PxRigidBodyFlag::Enum FlagToSet>
inline void ModifyRigidBodyFlag_Isolated(PxRigidBody* PRigidBody, const bool bValue)
{
	PxRigidBodyFlags RigidBodyFlags = PRigidBody->getRigidBodyFlags();
	ModifyRigidBodyFlag<FlagToSet>(RigidBodyFlags, bValue);
	PRigidBody->setRigidBodyFlags(RigidBodyFlags);
}

template<const PxShapeFlag::Enum FlagToSet>
inline void ModifyShapeFlag_Isolated(PxShape* PShape, const bool bValue)
{
	PxShapeFlags ShapeFlags = PShape->getFlags();
	ModifyShapeFlag<FlagToSet>(ShapeFlags, bValue);
	PShape->setFlags(ShapeFlags);
}

// MISC

/** Convert from unreal to physx capsule rotation */
PxQuat ConvertToPhysXCapsuleRot(const FQuat& GeomRot);
/** Convert from physx to unreal capsule rotation */
FQuat ConvertToUECapsuleRot(const PxQuat & GeomRot);
FQuat ConvertToUECapsuleRot(const FQuat & GeomRot);
/** Convert from unreal to physx capsule pose */
PxTransform ConvertToPhysXCapsulePose(const FTransform& GeomPose);

// FILTER DATA

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
PxFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const struct FCollisionQueryParams& QueryParam, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace);



//Find the face index for a given hit. This gives us a chance to modify face index based on things like most opposing normal
PxU32 FindFaceIndex(const PxSweepHit& PHit, const PxVec3& UnitDirection);

// Adapts a FCollisionShape to a PxGeometry type, used for various queries
struct FPhysXShapeAdaptor
{
public:
	FPhysXShapeAdaptor(const FQuat& Rot, const FCollisionShape& CollisionShape);

	PxGeometry& GetGeometry() const
	{
		return *PtrToUnionData;
	}

public:
	PxTransform GetGeomPose(const FVector& Pos) const
	{
		return PxTransform(U2PVector(Pos), Rotation);
	}

	PxQuat GetGeomOrientation() const
	{
		return Rotation;
	}

private:
	TUnion<PxSphereGeometry, PxBoxGeometry, PxCapsuleGeometry> UnionData;

	PxGeometry* PtrToUnionData;
	PxQuat Rotation;
};

struct FConstraintBrokenDelegateData
{
	FConstraintBrokenDelegateData(FConstraintInstance* ConstraintInstance);

	void DispatchOnBroken()
	{
		OnConstraintBrokenDelegate.ExecuteIfBound(ConstraintIndex);
	}

	FOnConstraintBroken OnConstraintBrokenDelegate;
	int32 ConstraintIndex;
};

class FPhysicsReplication;

/** Interface for the creation of customized physics replication.*/
class IPhysicsReplicationFactory
{
public:
	virtual FPhysicsReplication* Create(FPhysScene* OwningPhysScene) = 0;
	virtual void Destroy(FPhysicsReplication* PhysicsReplication) = 0;
};

#endif // WITH_PHYX
