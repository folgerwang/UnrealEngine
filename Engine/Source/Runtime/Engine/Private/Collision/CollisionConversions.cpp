// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Collision/CollisionConversions.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

#include "Collision/CollisionDebugDrawing.h"
#include "Components/LineBatchComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PxQueryFilterCallback.h"
#include "Physics/PhysicsInterfaceUtils.h"

#if PHYSICS_INTERFACE_PHYSX
#include "PhysXInterfaceWrapper.h"
#include "Collision/CollisionConversionsPhysx.h"
#elif PHYSICS_INTERFACE_LLIMMEDIATE
#include "Physics/Experimental/LLImmediateInterfaceWrapper.h"
#include "Collision/Experimental/CollisionConversionsLLImmediate.h"
#elif WITH_CHAOS
#include "Physics/Experimental/PhysInterface_Chaos.h"
#endif

// Used to place overlaps into a TMap when deduplicating them
struct FOverlapKey
{
	UPrimitiveComponent* Component;
	int32 ComponentIndex;

	FOverlapKey(UPrimitiveComponent* InComponent, int32 InComponentIndex)
		: Component(InComponent)
		, ComponentIndex(InComponentIndex)
	{
	}

	friend bool operator==(const FOverlapKey& X, const FOverlapKey& Y)
	{
		return (X.ComponentIndex == Y.ComponentIndex) && (X.Component == Y.Component);
	}
};

uint32 GetTypeHash(const FOverlapKey& Key)
{
	return GetTypeHash(Key.Component) ^ GetTypeHash(Key.ComponentIndex);
}


extern int32 CVarShowInitialOverlaps;

// Forward declare, I don't want to move the entire function right now or we lose change history.
static bool ConvertOverlappedShapeToImpactHit(const UWorld* World, const FHitLocation& Hit, const FVector& StartLoc, const FVector& EndLoc, FHitResult& OutResult, const FPhysicsGeometry& Geom, const FTransform& QueryTM, const FCollisionFilterData& QueryFilter, bool bReturnPhysMat);

DECLARE_CYCLE_STAT(TEXT("ConvertQueryHit"), STAT_ConvertQueryImpactHit, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("ConvertOverlapToHit"), STAT_CollisionConvertOverlapToHit, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("ConvertOverlap"), STAT_CollisionConvertOverlap, STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("SetHitResultFromShapeAndFaceIndex"), STAT_CollisionSetHitResultFromShapeAndFaceIndex, STATGROUP_Collision);

#define ENABLE_CHECK_HIT_NORMAL  (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))

#if ENABLE_CHECK_HIT_NORMAL
/* Validate Normal of OutResult. We're on hunt for invalid normal */
static void CheckHitResultNormal(const FHitResult& OutResult, const TCHAR* Message, const FVector& Start=FVector::ZeroVector, const FVector& End = FVector::ZeroVector, const FPhysicsGeometry* Geom=nullptr)
{
	if(!OutResult.bStartPenetrating && !OutResult.Normal.IsNormalized())
	{
		UE_LOG(LogPhysics, Warning, TEXT("(%s) Non-normalized OutResult.Normal from hit conversion: %s (Component- %s)"), Message, *OutResult.Normal.ToString(), *GetNameSafe(OutResult.Component.Get()));
		UE_LOG(LogPhysics, Warning, TEXT("Start Loc(%s), End Loc(%s), Hit Loc(%s), ImpactNormal(%s)"), *Start.ToString(), *End.ToString(), *OutResult.Location.ToString(), *OutResult.ImpactNormal.ToString() );
		if (Geom)
		{
			if (GetType(*Geom) == ECollisionShapeType::Capsule)
			{
				const FPhysicsCapsuleGeometry& Capsule = (FPhysicsCapsuleGeometry&)*Geom;
				UE_LOG(LogPhysics, Warning, TEXT("Capsule radius (%f), Capsule Halfheight (%f)"), GetRadius(Capsule), GetHalfHeight(Capsule));
			}
		}
		ensure(OutResult.Normal.IsNormalized());
	}
}
#endif // ENABLE_CHECK_HIT_NORMAL




static FVector FindSimpleOpposingNormal(const FHitLocation& Hit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	// We don't compute anything special
	return InNormal;
}


/**
 * Util to find the normal of the face that we hit. Will use faceIndex from the hit if possible.
 * @param PHit - incoming hit from PhysX
 * @param TraceDirectionDenorm - direction of sweep test (not normalized)
 * @param InNormal - default value in case no new normal is computed.
 * @return New normal we compute for geometry.
 */
static FVector FindGeomOpposingNormal(ECollisionShapeType QueryGeomType, const FHitLocation& Hit, const FVector& TraceDirectionDenorm, const FVector InNormal)
{
	// TODO: can we support other shapes here as well?
	if (QueryGeomType == ECollisionShapeType::Capsule || QueryGeomType == ECollisionShapeType::Sphere)
	{
		FPhysicsShape* Shape = GetShape(Hit);
		if (Shape)
		{
			ECollisionShapeType GeomType = GetGeometryType(*Shape);
			switch (GeomType)
			{
			case ECollisionShapeType::Sphere:
			case ECollisionShapeType::Capsule:			return FindSimpleOpposingNormal(Hit, TraceDirectionDenorm, InNormal);
			case ECollisionShapeType::Box:				return FindBoxOpposingNormal(Hit, TraceDirectionDenorm, InNormal);
			case ECollisionShapeType::Convex:		return FindConvexMeshOpposingNormal(Hit, TraceDirectionDenorm, InNormal);
			case ECollisionShapeType::Heightfield:		return FindHeightFieldOpposingNormal(Hit, TraceDirectionDenorm, InNormal);
			case ECollisionShapeType::Trimesh:	return FindTriMeshOpposingNormal(Hit, TraceDirectionDenorm, InNormal);
			default: check(false);	//unsupported geom type
			}
		}
	}

	return InNormal;
}

/** Set info in the HitResult (Actor, Component, PhysMaterial, BoneName, Item) based on the supplied shape and face index */
static void SetHitResultFromShapeAndFaceIndex(const FPhysicsShape& Shape,  const FPhysicsActor& Actor, const uint32 FaceIndex, FHitResult& OutResult, bool bReturnPhysMat)
{
	SCOPE_CYCLE_COUNTER(STAT_CollisionSetHitResultFromShapeAndFaceIndex);
	
	UPrimitiveComponent* OwningComponent = nullptr;
	if(const FBodyInstance* BodyInst = GetUserData(Actor))
	{
#if WITH_CHAOS || WITH_IMMEDIATE_PHYSX || PHYSICS_INTERFACE_LLIMMEDIATE
        ensure(false);
#else
		BodyInst = FPhysicsInterface_PhysX::ShapeToOriginalBodyInstance(BodyInst, &Shape);

		//Normal case where we hit a body
		OutResult.Item = BodyInst->InstanceBodyIndex;
		const UBodySetup* BodySetup = BodyInst->BodySetup.Get();	//this data should be immutable at runtime so ok to check from worker thread.
		if (BodySetup)
		{
			OutResult.BoneName = BodySetup->BoneName;
		}

		OwningComponent = BodyInst->OwnerComponent.Get();
#endif
	}
#if PHYSICS_INTERFACE_PHYSX
	else if(const FCustomPhysXPayload* CustomPayload = GetUserData<FCustomPhysXPayload>(Shape))	//todo(ocohen): wrap with PHYSX
	{
		//Custom payload case
		OwningComponent = CustomPayload->GetOwningComponent().Get();
		if(OwningComponent && OwningComponent->bMultiBodyOverlap)
		{
			OutResult.Item = CustomPayload->GetItemIndex();
			OutResult.BoneName = CustomPayload->GetBoneName();
		}
		else
		{
			OutResult.Item = INDEX_NONE;
			OutResult.BoneName = NAME_None;
		
		}
		
	}
#endif
	else
	{
		ensureMsgf(false, TEXT("SetHitResultFromShapeAndFaceIndex hit shape with invalid userData"));
	}

	OutResult.PhysMaterial = nullptr;

	// Grab actor/component
	if( OwningComponent )
	{
		OutResult.Actor = OwningComponent->GetOwner();
		OutResult.Component = OwningComponent;

		if (bReturnPhysMat)
		{
			// This function returns the single material in all cases other than trimesh or heightfield
			if(const FPhysicsMaterial* PhysicsMaterial = GetMaterialFromInternalFaceIndex(Shape, FaceIndex))
			{
				OutResult.PhysMaterial = GetUserData(*PhysicsMaterial);
			}
		}
	}

	OutResult.FaceIndex = INDEX_NONE;
}

#if WITH_PHYSX

EConvertQueryResult ConvertQueryImpactHit(const UWorld* World, const FHitLocation& Hit, FHitResult& OutResult, float CheckLength, const FCollisionFilterData& QueryFilter, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry* Geom, const FTransform& QueryTM, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	SCOPE_CYCLE_COUNTER(STAT_ConvertQueryImpactHit);

#if WITH_EDITOR
	if(bReturnFaceIndex && World->IsGameWorld())
	{
		if(UPhysicsSettings::Get()->bSuppressFaceRemapTable)
		{
			bReturnFaceIndex = false;	//The editor uses the remap table, so we modify this to get the same results as you would in a cooked build
		}
	}
#endif

	FHitFlags Flags = GetFlags(Hit);

	checkSlow(Flags & EHitFlags::Distance);
	const bool bInitialOverlap = HadInitialOverlap(Hit);
	if (bInitialOverlap && Geom)
	{
		ConvertOverlappedShapeToImpactHit(World, Hit, StartLoc, EndLoc, OutResult, *Geom, QueryTM, QueryFilter, bReturnPhysMat);
		return EConvertQueryResult::Valid;
	}

	const FPhysicsShape& HitShape = *GetShape(Hit);
	const FPhysicsActor& HitActor = *GetActor(Hit);
	const uint32 InternalFaceIndex = GetInternalFaceIndex(Hit);

	// See if this is a 'blocking' hit
	const FCollisionFilterData ShapeFilter = GetQueryFilterData(HitShape);
	const ECollisionQueryHitType HitType = FCollisionQueryFilterCallback::CalcQueryHitType(QueryFilter, ShapeFilter);
	OutResult.bBlockingHit = (HitType == ECollisionQueryHitType::Block);
	OutResult.bStartPenetrating = bInitialOverlap;

	// calculate the hit time
	const float HitTime = GetDistance(Hit)/CheckLength;
	OutResult.Time = HitTime;
	OutResult.Distance = GetDistance(Hit);

	// figure out where the the "safe" location for this shape is by moving from the startLoc toward the ImpactPoint
	const FVector TraceStartToEnd = EndLoc - StartLoc;
	const FVector SafeLocationToFitShape = StartLoc + (HitTime * TraceStartToEnd);
	OutResult.Location = SafeLocationToFitShape;

	const bool bUseReturnedPoint = ((Flags & EHitFlags::Position) && !bInitialOverlap);
	FVector Position = StartLoc;
	if (bUseReturnedPoint)
	{
		Position = GetPosition(Hit);
		if (Position.ContainsNaN())
		{
#if ENABLE_NAN_DIAGNOSTIC
			SetHitResultFromShapeAndFaceIndex(HitShape, HitActor, InternalFaceIndex, OutResult, bReturnPhysMat);
			UE_LOG(LogCore, Error, TEXT("ConvertQueryImpactHit() NaN details:\n>> Actor:%s (%s)\n>> Component:%s\n>> Item:%d\n>> BoneName:%s\n>> Time:%f\n>> Distance:%f\n>> Location:%s\n>> bIsBlocking:%d\n>> bStartPenetrating:%d"),
				*GetNameSafe(OutResult.GetActor()), OutResult.Actor.IsValid() ? *OutResult.GetActor()->GetPathName() : TEXT("no path"),
				*GetNameSafe(OutResult.GetComponent()), OutResult.Item, *OutResult.BoneName.ToString(),
				OutResult.Time, OutResult.Distance, *OutResult.Location.ToString(), OutResult.bBlockingHit ? 1 : 0, OutResult.bStartPenetrating ? 1 : 0);
#endif // ENABLE_NAN_DIAGNOSTIC

			OutResult.Reset();
			logOrEnsureNanError(TEXT("ConvertQueryImpactHit() received NaN/Inf for position: %s"), *Position.ToString());
			return EConvertQueryResult::Invalid;
		}
	}
	OutResult.ImpactPoint = Position;
	
	// Caution: we may still have an initial overlap, but with null Geom. This is the case for RayCast results.
	const bool bUseReturnedNormal = ((Flags & EHitFlags::Normal) && !bInitialOverlap);
	const FVector HitNormal = GetNormal(Hit);
	if (bUseReturnedNormal && HitNormal.ContainsNaN())
	{
#if ENABLE_NAN_DIAGNOSTIC
		SetHitResultFromShapeAndFaceIndex(HitShape, HitActor, InternalFaceIndex, OutResult, bReturnPhysMat);
		UE_LOG(LogCore, Error, TEXT("ConvertQueryImpactHit() NaN details:\n>> Actor:%s (%s)\n>> Component:%s\n>> Item:%d\n>> BoneName:%s\n>> Time:%f\n>> Distance:%f\n>> Location:%s\n>> bIsBlocking:%d\n>> bStartPenetrating:%d"),
			*GetNameSafe(OutResult.GetActor()), OutResult.Actor.IsValid() ? *OutResult.GetActor()->GetPathName() : TEXT("no path"),
			*GetNameSafe(OutResult.GetComponent()), OutResult.Item, *OutResult.BoneName.ToString(),
			OutResult.Time, OutResult.Distance, *OutResult.Location.ToString(), OutResult.bBlockingHit ? 1 : 0, OutResult.bStartPenetrating ? 1 : 0);
#endif // ENABLE_NAN_DIAGNOSTIC

		OutResult.Reset();
		logOrEnsureNanError(TEXT("ConvertQueryImpactHit() received NaN/Inf for normal: %s"), *HitNormal.ToString());
		return EConvertQueryResult::Invalid;
	}

	FVector Normal = bUseReturnedNormal ? HitNormal.GetSafeNormal() : -TraceStartToEnd.GetSafeNormal();
	OutResult.Normal = Normal;
	OutResult.ImpactNormal = Normal;

	OutResult.TraceStart = StartLoc;
	OutResult.TraceEnd = EndLoc;


#if ENABLE_CHECK_HIT_NORMAL
	CheckHitResultNormal(OutResult, TEXT("Invalid Normal from ConvertQueryImpactHit"), StartLoc, EndLoc, Geom);
#endif // ENABLE_CHECK_HIT_NORMAL

	if (bUseReturnedNormal && !Normal.IsNormalized())
	{
		// TraceStartToEnd should never be zero, because of the length restriction in the raycast and sweep tests.
		Normal = -TraceStartToEnd.GetSafeNormal();
		OutResult.Normal = Normal;
		OutResult.ImpactNormal = Normal;
	}

	const ECollisionShapeType SweptGeometryType = Geom ? GetType(*Geom) : ECollisionShapeType::None;
	OutResult.ImpactNormal = FindGeomOpposingNormal(SweptGeometryType, Hit, TraceStartToEnd, Normal);

	// Fill in Actor, Component, material, etc.
	SetHitResultFromShapeAndFaceIndex(HitShape, HitActor, InternalFaceIndex, OutResult, bReturnPhysMat);

	ECollisionShapeType GeomType = GetGeometryType(HitShape);

	if(GeomType == ECollisionShapeType::Heightfield)
	{
		// Lookup physical material for heightfields
		if (bReturnPhysMat && InternalFaceIndex != GetInvalidPhysicsFaceIndex())
		{
			if (const FPhysicsMaterial* Material = GetMaterialFromInternalFaceIndex(HitShape, InternalFaceIndex))
			{
				OutResult.PhysMaterial = GetUserData(*Material);
			}
		}
	}
	else if (bReturnFaceIndex && GeomType == ECollisionShapeType::Trimesh && InternalFaceIndex != GetInvalidPhysicsFaceIndex())
	{
		OutResult.FaceIndex = GetTriangleMeshExternalFaceIndex(HitShape, InternalFaceIndex);
	}

	return EConvertQueryResult::Valid;
}

template <typename HitType>
EConvertQueryResult ConvertTraceResults(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, HitType* Hits, float CheckLength, const FCollisionFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry& Geom, const FTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	OutHits.Reserve(OutHits.Num() + NumHits);
	EConvertQueryResult ConvertResult = EConvertQueryResult::Valid;
	bool bHadBlockingHit = false;
	const FVector Dir = (EndLoc - StartLoc).GetSafeNormal();

	for(int32 i=0; i<NumHits; i++)
	{
		HitType& Hit = Hits[i];
		if(GetDistance(Hit) <= MaxDistance)
		{
			if (TIsSame<HitType, FHitSweep>::Value)
			{
				SetInternalFaceIndex(Hit, FindFaceIndex(Hit, Dir));
			}

			FHitResult& NewResult = OutHits[OutHits.AddDefaulted()];
			if (ConvertQueryImpactHit(World, Hit, NewResult, CheckLength, QueryFilter, StartLoc, EndLoc, &Geom, QueryTM, bReturnFaceIndex, bReturnPhysMat) == EConvertQueryResult::Valid)
			{
				bHadBlockingHit |= NewResult.bBlockingHit;
			}
			else
			{
				// Reject invalid result (this should be rare). Remove from the results.
				OutHits.Pop(/*bAllowShrinking=*/ false);
				ConvertResult = EConvertQueryResult::Invalid;
			}
			
		}
	}

	// Sort results from first to last hit
	OutHits.Sort( FCompareFHitResultTime() );
	OutHasValidBlockingHit = bHadBlockingHit;
	return ConvertResult;
}

template <typename Hit>
EConvertQueryResult ConvertTraceResults(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, Hit* Hits, float CheckLength, const FCollisionFilterData& QueryFilter, FHitResult& OutHit, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry& Geom, const FTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	const FVector Dir = (EndLoc - StartLoc).GetSafeNormal();
	if (TIsSame<Hit, FHitSweep>::Value)
	{
		SetInternalFaceIndex(Hits[0], FindFaceIndex(Hits[0], Dir));
	}
	EConvertQueryResult Result = ConvertQueryImpactHit(World, Hits[0], OutHit, CheckLength, QueryFilter, StartLoc, EndLoc, &Geom, QueryTM, bReturnFaceIndex, bReturnPhysMat);
	OutHasValidBlockingHit = Result == EConvertQueryResult::Valid;
	return Result;
}

template EConvertQueryResult ConvertTraceResults<FHitSweep>(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, FHitSweep* Hits, float CheckLength, const FCollisionFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry& Geom, const FTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat);
template EConvertQueryResult ConvertTraceResults<FHitSweep>(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, FHitSweep* Hits, float CheckLength, const FCollisionFilterData& QueryFilter, FHitResult& OutHit, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry& Geom, const FTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat);
template EConvertQueryResult ConvertTraceResults<FHitRaycast>(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, FHitRaycast* Hits, float CheckLength, const FCollisionFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry& Geom, const FTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat);
template EConvertQueryResult ConvertTraceResults<FHitRaycast>(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, FHitRaycast* Hits, float CheckLength, const FCollisionFilterData& QueryFilter, FHitResult& OutHit, const FVector& StartLoc, const FVector& EndLoc, const FPhysicsGeometry& Geom, const FTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat);

#endif

/** Util to convert an overlapped shape into a sweep hit result, returns whether it was a blocking hit. */
static bool ConvertOverlappedShapeToImpactHit(const UWorld* World, const FHitLocation& Hit, const FVector& StartLoc, const FVector& EndLoc, FHitResult& OutResult, const FPhysicsGeometry& Geom, const FTransform& QueryTM, const FCollisionFilterData& QueryFilter, bool bReturnPhysMat)
{
	SCOPE_CYCLE_COUNTER(STAT_CollisionConvertOverlapToHit);

	const FPhysicsShape& HitShape = *GetShape(Hit);
	const FPhysicsActor& HitActor = *GetActor(Hit);

	// See if this is a 'blocking' hit
	FCollisionFilterData ShapeFilter = GetQueryFilterData(HitShape);
	const ECollisionQueryHitType HitType = FCollisionQueryFilterCallback::CalcQueryHitType(QueryFilter, ShapeFilter);
	const bool bBlockingHit = (HitType == ECollisionQueryHitType::Block);
	OutResult.bBlockingHit = bBlockingHit;

	// Time of zero because initially overlapping
	OutResult.bStartPenetrating = true;
	OutResult.Time = 0.f;
	OutResult.Distance = 0.f;

	// Return start location as 'safe location'
	OutResult.Location = QueryTM.GetLocation();

	const bool bValidPosition = (GetFlags(Hit) & EHitFlags::Position);
	if (bValidPosition)
	{
		const FVector HitPosition = GetPosition(Hit);
		const bool bFinitePosition = !HitPosition.ContainsNaN();
		if (bFinitePosition)
		{
			OutResult.ImpactPoint = HitPosition;
		}
		else
		{
			OutResult.ImpactPoint = StartLoc;
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: MTD returned NaN :( position: %s"), *HitPosition.ToString());
		}
	}
	else
	{
		OutResult.ImpactPoint = StartLoc;
	}
	OutResult.TraceStart = StartLoc;
	OutResult.TraceEnd = EndLoc;

	const FVector HitNormal = GetNormal(Hit);
	const bool bFiniteNormal = !HitNormal.ContainsNaN();
	const bool bValidNormal = (GetFlags(Hit) & EHitFlags::Normal) && bFiniteNormal;

	// Use MTD result if possible. We interpret the MTD vector as both the direction to move and the opposing normal.
	if (bValidNormal)
	{
		OutResult.ImpactNormal = HitNormal;
		OutResult.PenetrationDepth = FMath::Abs(GetDistance(Hit));
	}
	else
	{
		// Fallback normal if we can't find it with MTD or otherwise.
		OutResult.ImpactNormal = FVector::UpVector;
		OutResult.PenetrationDepth = 0.f;
		if (!bFiniteNormal)
		{
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: MTD returned NaN :( normal: %s"), *HitNormal.ToString());
		}
	}

#if DRAW_OVERLAPPING_TRIS
	if (CVarShowInitialOverlaps != 0 && World && World->IsGameWorld())
	{
		DrawOverlappingTris(World, Hit, Geom, QueryTM);
	}
#endif

	if (bBlockingHit)
	{
		// Zero-distance hits are often valid hits and we can extract the hit normal.
		// For invalid normals we can try other methods as well (get overlapping triangles).
		if (GetDistance(Hit) == 0.f || !bValidNormal)	//todo(ocohen): isn't hit distance always zero in this function? should this be if(!bValidNormal) ?
		{
			ComputeZeroDistanceImpactNormalAndPenetration(World, Hit, Geom, QueryTM, OutResult);
		}
	}
	else
	{
		// non blocking hit (overlap).
		if (!bValidNormal)
		{
			OutResult.ImpactNormal = (StartLoc - EndLoc).GetSafeNormal();
			ensure(OutResult.ImpactNormal.IsNormalized());
		}
	}

	OutResult.Normal = OutResult.ImpactNormal;
	
	SetHitResultFromShapeAndFaceIndex(HitShape, HitActor, GetInternalFaceIndex(Hit), OutResult, bReturnPhysMat);

	return bBlockingHit;
}


void ConvertQueryOverlap(const FPhysicsShape& Shape, const FPhysicsActor& Actor, FOverlapResult& OutOverlap, const FCollisionFilterData& QueryFilter)
{
	const bool bBlock = IsBlocking(Shape, QueryFilter);

	// Grab actor/component
	
	// Try body instance
	if (const FBodyInstance* BodyInst = GetUserData(Actor))
	{
#if WITH_CHAOS || WITH_IMMEDIATE_PHYSX || PHYSICS_INTERFACE_LLIMMEDIATE
        ensure(false);
#else
        BodyInst = FPhysicsInterface_PhysX::ShapeToOriginalBodyInstance(BodyInst, &Shape);
		if (const UPrimitiveComponent* OwnerComponent = BodyInst->OwnerComponent.Get())
		{
			OutOverlap.Actor = OwnerComponent->GetOwner();
			OutOverlap.Component = BodyInst->OwnerComponent; // Copying weak pointer is faster than assigning raw pointer.
			OutOverlap.ItemIndex = OwnerComponent->bMultiBodyOverlap ? BodyInst->InstanceBodyIndex : INDEX_NONE;
		}
#endif
	}
#if PHYSICS_INTERFACE_PHYSX
	else if(const FCustomPhysXPayload* CustomPayload = GetUserData<FCustomPhysXPayload>(Shape))
	{
		TWeakObjectPtr<UPrimitiveComponent> OwnerComponent = CustomPayload->GetOwningComponent();
		if (UPrimitiveComponent* OwnerComponentRaw = OwnerComponent.Get())
		{
			OutOverlap.Actor = OwnerComponentRaw->GetOwner();
			OutOverlap.Component = OwnerComponent; // Copying weak pointer is faster than assigning raw pointer.
			OutOverlap.ItemIndex = OwnerComponent->bMultiBodyOverlap ? CustomPayload->GetItemIndex() : INDEX_NONE;
		}
	}
#endif
	else
	{
		ensureMsgf(false, TEXT("ConvertQueryOverlap called with bad payload type"));
	}

	// Other info
	OutOverlap.bBlockingHit = bBlock;
}

/** Util to add NewOverlap to OutOverlaps if it is not already there */
static void AddUniqueOverlap(TArray<FOverlapResult>& OutOverlaps, const FOverlapResult& NewOverlap)
{
	// Look to see if we already have this overlap (based on component)
	for(int32 TestIdx=0; TestIdx<OutOverlaps.Num(); TestIdx++)
	{
		FOverlapResult& Overlap = OutOverlaps[TestIdx];

		if (Overlap.ItemIndex == NewOverlap.ItemIndex && Overlap.Component == NewOverlap.Component)
		{
			// These should be the same if the component matches!
			checkSlow(Overlap.Actor == NewOverlap.Actor);

			// If we had a non-blocking overlap with this component, but now we have a blocking one, use that one instead!
			if(!Overlap.bBlockingHit && NewOverlap.bBlockingHit)
			{
				Overlap = NewOverlap;
			}

			return;
		}
	}

	// Not found, so add it 
	OutOverlaps.Add(NewOverlap);
}

bool IsBlocking(const FPhysicsShape& Shape, const FCollisionFilterData& QueryFilter)
{
	// See if this is a 'blocking' hit
	const FCollisionFilterData ShapeFilter = GetQueryFilterData(Shape);
	const ECollisionQueryHitType HitType = FCollisionQueryFilterCallback::CalcQueryHitType(QueryFilter, ShapeFilter);
	const bool bBlock = (HitType == ECollisionQueryHitType::Block);
	return bBlock;
}

/** Min number of overlaps required to start using a TMap for deduplication */
int32 GNumOverlapsRequiredForTMap = 3;

static FAutoConsoleVariableRef GTestOverlapSpeed(
	TEXT("Engine.MinNumOverlapsToUseTMap"),
	GNumOverlapsRequiredForTMap,
	TEXT("Min number of overlaps required before using a TMap for deduplication")
	);

bool ConvertOverlapResults(int32 NumOverlaps, FHitOverlap* OverlapResults, const FCollisionFilterData& QueryFilter, TArray<FOverlapResult>& OutOverlaps)
{
	SCOPE_CYCLE_COUNTER(STAT_CollisionConvertOverlap);

	const int32 ExpectedSize = OutOverlaps.Num() + NumOverlaps;
	OutOverlaps.Reserve(ExpectedSize);
	bool bBlockingFound = false;

	if (ExpectedSize >= GNumOverlapsRequiredForTMap)
	{
		// Map from an overlap to the position in the result array (the index has one added to it so 0 can be a sentinel)
		TMap<FOverlapKey, int32, TInlineSetAllocator<64>> OverlapMap;
		OverlapMap.Reserve(ExpectedSize);

		// Fill in the map with existing hits
		for (int32 ExistingIndex = 0; ExistingIndex < OutOverlaps.Num(); ++ExistingIndex)
		{
			const FOverlapResult& ExistingOverlap = OutOverlaps[ExistingIndex];
			OverlapMap.Add(FOverlapKey(ExistingOverlap.Component.Get(), ExistingOverlap.ItemIndex), ExistingIndex + 1);
		}

		for (int32 PResultIndex = 0; PResultIndex < NumOverlaps; ++PResultIndex)
		{
			FOverlapResult NewOverlap;
			ConvertQueryOverlap(*GetShape(OverlapResults[PResultIndex]), *GetActor(OverlapResults[PResultIndex]), NewOverlap, QueryFilter);

			if (NewOverlap.bBlockingHit)
			{
				bBlockingFound = true;
			}

			// Look for it in the map, newly added elements will start with 0, so we know we need to add it to the results array then (the index is stored as +1)
			int32& DestinationIndex = OverlapMap.FindOrAdd(FOverlapKey(NewOverlap.Component.Get(), NewOverlap.ItemIndex));
			if (DestinationIndex == 0)
			{
				DestinationIndex = OutOverlaps.Add(NewOverlap) + 1;
			}
			else
			{
				FOverlapResult& ExistingOverlap = OutOverlaps[DestinationIndex - 1];

				// If we had a non-blocking overlap with this component, but now we have a blocking one, use that one instead!
				if (!ExistingOverlap.bBlockingHit && NewOverlap.bBlockingHit)
				{
					ExistingOverlap = NewOverlap;
				}
			}
		}
	}
	else
	{
		// N^2 approach, no maps
		for (int32 i = 0; i < NumOverlaps; i++)
		{
			FOverlapResult NewOverlap;
			ConvertQueryOverlap(*GetShape(OverlapResults[i]), *GetActor(OverlapResults[i]), NewOverlap, QueryFilter);

			if (NewOverlap.bBlockingHit)
			{
				bBlockingFound = true;
			}

			AddUniqueOverlap(OutOverlaps, NewOverlap);
		}
	}

	return bBlockingFound;
}
