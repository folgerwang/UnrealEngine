// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if WITH_CHAOS || PHYSICS_INTERFACE_LLIMMEDIATE
#define STARTQUERYTIMER()
#endif

#include "Engine/World.h"
#include "Collision.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodySetup.h"
#include "CollisionDebugDrawingPublic.h"
#include "Templates/EnableIf.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "Collision/CollisionConversions.h"
#include "PhysicsEngine/ScopedSQHitchRepeater.h"

#if PHYSICS_INTERFACE_PHYSX
#include "PhysXInterfaceWrapper.h"
#elif PHYSICS_INTERFACE_LLIMMEDIATE
#include "Physics/Experimental/LLImmediateInterfaceWrapper.h"
#endif

#include "Collision/CollisionDebugDrawing.h"

float DebugLineLifetime = 2.f;

#if !WITH_CHAOS

#include "PhysicsEngine/PhysXSupport.h"
#include "PhysicsEngine/CollisionAnalyzerCapture.h"



#if WITH_PHYSX
#include "PhysicsEngine/PxQueryFilterCallback.h"
#endif

enum class ESingleMultiOrTest : uint8
{
	Single,
	Multi,
	Test
};

enum class ESweepOrRay : uint8
{
	Raycast,
	Sweep,
};

struct FGeomSQAdditionalInputs
{
	FGeomSQAdditionalInputs(const FCollisionShape& InCollisionShape, const FQuat& InGeomRot)
#if WITH_PHYSX
		: ShapeAdaptor(InGeomRot, InCollisionShape)
		, CollisionShape(InCollisionShape)
	{
	}

	const PxGeometry* GetGeometry() const
	{
		return &ShapeAdaptor.GetGeometry();
	}

	const FQuat* GetGeometryOrientation() const
	{
		return &ShapeAdaptor.GetGeomOrientation();
	}

	FPhysXShapeAdaptor ShapeAdaptor;
#else
	{}

	const FPhysicsGeometry* GetGeometry() const
	{
		return nullptr;
	}

	const FQuat* GetGeometryOrientation() const
	{
		return nullptr;
	}

#endif

	const FCollisionShape* GetCollisionShape() const
	{
		return &CollisionShape;
	}

	const FCollisionShape& CollisionShape;
};

struct FGeomCollectionSQAdditionalInputs
{
	FGeomCollectionSQAdditionalInputs(const FPhysicsGeometryCollection& InCollection, const FQuat& InGeomRot)
	: Collection(InCollection)
	, GeomRot(InGeomRot)
	{
	}

	const FPhysicsGeometry* GetGeometry() const
	{
		return &Collection.GetGeometry();
	}

	const FQuat* GetGeometryOrientation() const
	{
		return &GeomRot;
	}

	const FPhysicsGeometryCollection* GetCollisionShape() const
	{
		return &Collection;
	}

	const FPhysicsGeometryCollection& Collection;
	const FQuat& GeomRot;
};

struct FRaycastSQAdditionalInputs
{
	const FPhysicsGeometry* GetGeometry() const
	{
		return nullptr;
	}

	const FQuat* GetGeometryOrientation() const
	{
		return nullptr;
	}

	const FCollisionShape* GetCollisionShape() const
	{
		return nullptr;
	}
};

void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback);
void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback);
void LowLevelOverlap(FPhysScene& Scene, const FPhysicsGeometry& Geom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback);

template <typename InHitType, ESweepOrRay InGeometryQuery, ESingleMultiOrTest InSingleMultiOrTest>
struct TSQTraits
{
	static const ESingleMultiOrTest SingleMultiOrTest = InSingleMultiOrTest;
	static const ESweepOrRay GeometryQuery = InGeometryQuery;
	using THitType = InHitType;
	using TOutHits = typename TChooseClass<InSingleMultiOrTest == ESingleMultiOrTest::Multi, TArray<FHitResult>, FHitResult>::Result;
	using THitBuffer = typename TChooseClass<InSingleMultiOrTest == ESingleMultiOrTest::Multi, FDynamicHitBuffer<InHitType>, typename TChooseClass<InGeometryQuery == ESweepOrRay::Sweep, FPhysicsSweepBuffer, FPhysicsRaycastBuffer>::Result >::Result;

	// GetNumHits - multi
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T == ESingleMultiOrTest::Multi, int32>::Type GetNumHits(const THitBuffer& HitBuffer)
	{
		return HitBuffer.GetNumHits();
	}

	// GetNumHits - single/test
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T != ESingleMultiOrTest::Multi, int32>::Type GetNumHits(const THitBuffer& HitBuffer)
	{
		return GetHasBlock(HitBuffer) ? 1 : 0;
	}

	//GetHits - multi
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T == ESingleMultiOrTest::Multi, THitType*>::Type GetHits(THitBuffer& HitBuffer)
	{
		return HitBuffer.GetHits();
	}

	//GetHits - single/test
	template <ESingleMultiOrTest T = SingleMultiOrTest>
	static typename TEnableIf<T != ESingleMultiOrTest::Multi, THitType*>::Type GetHits(THitBuffer& HitBuffer)
	{
		return GetBlock(HitBuffer);
	}

	//SceneTrace - ray
	template <typename TGeomInputs, ESweepOrRay T = GeometryQuery>
	static typename TEnableIf<T == ESweepOrRay::Raycast, void>::Type SceneTrace(FPhysScene& Scene, const TGeomInputs& GeomInputs, const FVector& Dir, float DeltaMag, const FTransform& StartTM, THitBuffer& HitBuffer, EHitFlags OutputFlags, EQueryFlags QueryFlags, const FCollisionFilterData& FilterData, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback)
	{
		LowLevelRaycast(Scene, StartTM.GetLocation(), Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, FilterData, Params, QueryCallback);	//todo(ocohen): namespace?
	}

	//SceneTrace - sweep
	template <typename TGeomInputs, ESweepOrRay T = GeometryQuery>
	static typename TEnableIf<T == ESweepOrRay::Sweep, void>::Type SceneTrace(FPhysScene& Scene, const TGeomInputs& GeomInputs, const FVector& Dir, float DeltaMag, const FTransform& StartTM, THitBuffer& HitBuffer, EHitFlags OutputFlags, EQueryFlags QueryFlags, const FCollisionFilterData& FilterData, const FCollisionQueryParams& Params, FPxQueryFilterCallback* QueryCallback)
	{
		LowLevelSweep(Scene, *GeomInputs.GetGeometry(), StartTM, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, FilterData, Params, QueryCallback);	//todo(ocohen): namespace?
	}

	static void ResetOutHits(TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End)
	{
		OutHits.Reset();
	}

	static void ResetOutHits(FHitResult& OutHit, const FVector& Start, const FVector& End)
	{
		OutHit = FHitResult();
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
	}

	static void DrawTraces(const UWorld* World, const FVector& Start, const FVector& End, const FPhysicsGeometry* PGeom, const FQuat* PGeomRot, const TArray<FHitResult>& Hits)
	{
		if (IsRay())
		{
			DrawLineTraces(World, Start, End, Hits, DebugLineLifetime);
		}
		else
		{
			DrawGeomSweeps(World, Start, End, *PGeom, U2PQuat(*PGeomRot), Hits, DebugLineLifetime);
		}
	}

	static void DrawTraces(const UWorld* World, const FVector& Start, const FVector& End, const FPhysicsGeometry* PGeom, const FQuat* GeomRotation, const FHitResult& Hit)
	{
		TArray<FHitResult> Hits;
		Hits.Add(Hit);

		DrawTraces(World, Start, End, PGeom, GeomRotation, Hits);
	}

	template <typename TGeomInputs>
	static void CaptureTraces(const UWorld* World, const FVector& Start, const FVector& End, const TGeomInputs& GeomInputs, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams, const TArray<FHitResult>& Hits, bool bHaveBlockingHit, double StartTime)
	{
#if ENABLE_COLLISION_ANALYZER
		ECAQueryMode::Type QueryMode = IsMulti() ? ECAQueryMode::Multi : (IsSingle() ? ECAQueryMode::Single : ECAQueryMode::Test);
		if (IsRay())
		{
			CAPTURERAYCAST(World, Start, End, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
		}
		else
		{
			CAPTUREGEOMSWEEP(World, Start, End, *GeomInputs.GetGeometryOrientation(), QueryMode, *GeomInputs.GetCollisionShape(), TraceChannel, Params, ResponseParams, ObjectParams, Hits);
		}
#endif
	}

	template <typename TGeomInputs>
	static void CaptureTraces(const UWorld* World, const FVector& Start, const FVector& End, const TGeomInputs& GeomInputs, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams, const FHitResult& Hit, bool bHaveBlockingHit, double StartTime)
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(Hit);
		}
		CaptureTraces(World, Start, End, GeomInputs, TraceChannel, Params, ResponseParams, ObjectParams, Hits, bHaveBlockingHit, StartTime);
	}

	static EHitFlags GetHitFlags()
	{
		if (IsTest())
		{
			return EHitFlags::None;
		}
		else
		{
			if (IsRay())
			{
				return EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance | EHitFlags::MTD | EHitFlags::FaceIndex;
			}
			else
			{
				if (IsSingle())
				{
					return EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance | EHitFlags::MTD;
				}
				else
				{
					return EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance | EHitFlags::MTD | EHitFlags::FaceIndex;
				}
			}
		}
	}

	static EQueryFlags GetQueryFlags()
	{
		if (IsRay())
		{
			return (IsTest() ? (EQueryFlags::PreFilter | EQueryFlags::AnyHit) : EQueryFlags::PreFilter);
		}
		else
		{
			if (IsTest())
			{
				return (EQueryFlags::PreFilter | EQueryFlags::PostFilter | EQueryFlags::AnyHit);
			}
			else if (IsSingle())
			{
				return EQueryFlags::PreFilter;
			}
			else
			{
				return (EQueryFlags::PreFilter | EQueryFlags::PostFilter);
			}
		}
	}

	CA_SUPPRESS(6326);
	constexpr static bool IsSingle() { return SingleMultiOrTest == ESingleMultiOrTest::Single;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsTest() { return SingleMultiOrTest == ESingleMultiOrTest::Test;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsMulti() { return SingleMultiOrTest == ESingleMultiOrTest::Multi;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsRay() { return GeometryQuery == ESweepOrRay::Raycast;  }

	CA_SUPPRESS(6326);
	constexpr static bool IsSweep() { return GeometryQuery == ESweepOrRay::Sweep;  }
};

template <typename Traits, typename TGeomInputs>
bool TSceneCastCommon(const UWorld* World, typename Traits::TOutHits& OutHits, const TGeomInputs& GeomInputs, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	if (!Traits::IsTest())
	{
		Traits::ResetOutHits(OutHits, Start, End);
	}

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	float MinBlockingDistance = DeltaMag;
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		// Create filter data used to filter collisions 
		CA_SUPPRESS(6326);
		FCollisionFilterData Filter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, Traits::SingleMultiOrTest == ESingleMultiOrTest::Multi);
		
		CA_SUPPRESS(6326);
		FPhysicsQueryFilterCallback QueryCallback(Params, Traits::GeometryQuery == ESweepOrRay::Sweep);

		CA_SUPPRESS(6326);
		if (Traits::SingleMultiOrTest != ESingleMultiOrTest::Multi)
		{
			QueryCallback.bIgnoreTouches = true;
		}

		typename Traits::THitBuffer HitBufferSync(DeltaMag);

		bool bBlockingHit = false;
		const FVector Dir = Delta / DeltaMag;
		const FTransform StartTM = Traits::IsRay() ? FTransform(Start) : FTransform(*GeomInputs.GetGeometryOrientation(), Start);

		// Enable scene locks, in case they are required
		FPhysScene& PhysScene = *World->GetPhysicsScene();
		FScopedSceneReadLock SceneLocks(PhysScene);
		{
			FScopedSQHitchRepeater<decltype(HitBufferSync)> HitchRepeater(HitBufferSync, QueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				Traits::SceneTrace(PhysScene, GeomInputs, Dir, DeltaMag, StartTM, HitchRepeater.GetBuffer(), Traits::GetHitFlags(), Traits::GetQueryFlags(), Filter, Params, &QueryCallback);
			} while (HitchRepeater.RepeatOnHitch());
		}

		const int32 NumHits = Traits::GetNumHits(HitBufferSync);

		if(NumHits > 0 && GetHasBlock(HitBufferSync))
		{
			bBlockingHit = true;
			MinBlockingDistance = GetDistance(Traits::GetHits(HitBufferSync)[NumHits - 1]);
		}

		if (NumHits > 0 && !Traits::IsTest())
		{
			bool bSuccess = ConvertTraceResults(bBlockingHit, World, NumHits, Traits::GetHits(HitBufferSync), DeltaMag, Filter, OutHits, Start, End, *GeomInputs.GetGeometry(), StartTM, MinBlockingDistance, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Valid;

			if (!bSuccess)
			{
				// We don't need to change bBlockingHit, that's done by ConvertTraceResults if it removed the blocking hit.
				UE_LOG(LogCollision, Error, TEXT("%s%s resulted in a NaN/INF in PHit!"), Traits::IsRay() ? TEXT("Raycast") : TEXT("Sweep"), Traits::IsMulti() ? TEXT("Multi") : (Traits::IsSingle() ? TEXT("Single") : TEXT("Test")));
#if ENABLE_NAN_DIAGNOSTIC
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				if (Traits::IsSweep())
				{
					UE_LOG(LogCollision, Error, TEXT("--------GeomRotation : %s"), *(GeomInputs.GetGeometryOrientation()->ToString()));
				}
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}

		bHaveBlockingHit = bBlockingHit;

	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (World->DebugDrawSceneQueries(Params.TraceTag))
	{
		Traits::DrawTraces(World, Start, End, GeomInputs.GetGeometry(), GeomInputs.GetGeometryOrientation(), OutHits);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	Traits::CaptureTraces(World, Start, End, GeomInputs, TraceChannel, Params, ResponseParams, ObjectParams, OutHits, bHaveBlockingHit, StartTime);
#endif

	return bHaveBlockingHit;
}

//////////////////////////////////////////////////////////////////////////
// RAYCAST

bool FGenericPhysicsInterface::RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastAny);

	using TCastTraits = TSQTraits<FHitRaycast, ESweepOrRay::Raycast, ESingleMultiOrTest::Test>;
	FHitResult DummyHit(NoInit);
	return TSceneCastCommon<TCastTraits>(World, DummyHit, FRaycastSQAdditionalInputs(), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool FGenericPhysicsInterface::RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastSingle);

	using TCastTraits = TSQTraits<FHitRaycast, ESweepOrRay::Raycast, ESingleMultiOrTest::Single>;
	return TSceneCastCommon<TCastTraits>(World, OutHit, FRaycastSQAdditionalInputs(), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}



bool FGenericPhysicsInterface::RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastMultiple);

	using TCastTraits = TSQTraits<FHitRaycast, ESweepOrRay::Raycast, ESingleMultiOrTest::Multi>;
	return TSceneCastCommon<TCastTraits> (World, OutHits, FRaycastSQAdditionalInputs(), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

//////////////////////////////////////////////////////////////////////////
// GEOM SWEEP

bool FGenericPhysicsInterface::GeomSweepTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepAny);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Test>;
	FHitResult DummyHit(NoInit);
	return TSceneCastCommon<TCastTraits>(World, DummyHit, FGeomSQAdditionalInputs(CollisionShape, Rot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

bool FGenericPhysicsInterface::GeomSweepSingle(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepSingle);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Single>;
	return TSceneCastCommon<TCastTraits>(World, OutHit, FGeomSQAdditionalInputs(CollisionShape, Rot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

template<>
bool FGenericPhysicsInterface::GeomSweepMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Multi>;
	return TSceneCastCommon<TCastTraits>(World, OutHits, FGeomCollectionSQAdditionalInputs(InGeom, InGeomRot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}

template<>
bool FGenericPhysicsInterface::GeomSweepMulti(const UWorld* World, const FCollisionShape& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);

	using TCastTraits = TSQTraits<FHitSweep, ESweepOrRay::Sweep, ESingleMultiOrTest::Multi>;
	return TSceneCastCommon<TCastTraits>(World, OutHits, FGeomSQAdditionalInputs(InGeom, InGeomRot), Start, End, TraceChannel, Params, ResponseParams, ObjectParams);
}
#endif

//////////////////////////////////////////////////////////////////////////
// GEOM OVERLAP

namespace EQueryInfo
{
	//This is used for templatizing code based on the info we're trying to get out.
	enum Type
	{
		GatherAll,		//get all data and actually return it
		IsBlocking,		//is any of the data blocking? only return a bool so don't bother collecting
		IsAnything		//is any of the data blocking or touching? only return a bool so don't bother collecting
	};
}

template <EQueryInfo::Type InfoType, typename TCollisionAnalyzerType>
bool GeomOverlapMultiImp(const UWorld* World, const FPhysicsGeometry& Geom, const TCollisionAnalyzerType& CollisionAnalyzerType, const FTransform& GeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	FScopeCycleCounter Counter(Params.StatId);

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	STARTQUERYTIMER();

	bool bHaveBlockingHit = false;

	// overlapMultiple only supports sphere/capsule/box
	const ECollisionShapeType GeomType = GetType(Geom);
	if (GeomType == ECollisionShapeType::Sphere || GeomType == ECollisionShapeType::Capsule || GeomType == ECollisionShapeType::Box || GeomType == ECollisionShapeType::Convex)
	{
		// Create filter data used to filter collisions
		FCollisionFilterData Filter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, InfoType != EQueryInfo::IsAnything);
		FPhysicsQueryFilterCallback QueryCallback(Params, false);
		QueryCallback.bIgnoreTouches |= (InfoType == EQueryInfo::IsBlocking); // pre-filter to ignore touches and only get blocking hits, if that's what we're after.
		QueryCallback.bIsOverlapQuery = true;

		const EQueryFlags QueryFlags = InfoType == EQueryInfo::GatherAll ? EQueryFlags::PreFilter : (EQueryFlags::PreFilter | EQueryFlags::AnyHit);
		
		FDynamicHitBuffer<FHitOverlap> OverlapBuffer(0.f);

		// Enable scene locks, in case they are required
		FPhysScene& PhysScene = *World->GetPhysicsScene();

		FPhysicsCommand::ExecuteRead(&PhysScene, [&]()
		{
			{
				FScopedSQHitchRepeater<TRemoveReference<decltype(OverlapBuffer)>::Type> HitchRepeater(OverlapBuffer, QueryCallback, FHitchDetectionInfo(GeomPose, TraceChannel, Params));
				do
				{
					LowLevelOverlap(PhysScene, Geom, GeomPose, HitchRepeater.GetBuffer(), QueryFlags, Filter, Params, &QueryCallback);
				} while(HitchRepeater.RepeatOnHitch());

				if(GetHasBlock(OverlapBuffer) && InfoType != EQueryInfo::GatherAll)	//just want true or false so don't bother gathering info
				{
					bHaveBlockingHit = true;
				}
			}

			if(InfoType == EQueryInfo::GatherAll)	//if we are gathering all we need to actually convert to UE format
			{
				const int32 NumHits = OverlapBuffer.GetNumHits();

				if(NumHits > 0)
				{
					bHaveBlockingHit = ConvertOverlapResults(NumHits, OverlapBuffer.GetHits(), Filter, OutOverlaps);
				}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if(World->DebugDrawSceneQueries(Params.TraceTag))
				{
					DrawGeomOverlaps(World, Geom, U2PTransform(GeomPose), OutOverlaps, DebugLineLifetime);
				}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			}

		});
	}
	else
	{
		UE_LOG(LogCollision, Log, TEXT("GeomOverlapMulti : unsupported shape - only supports sphere, capsule, box"));
	}

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		// Determine query mode ('single' doesn't really exist for overlaps)
		ECAQueryMode::Type QueryMode = (InfoType == EQueryInfo::GatherAll) ? ECAQueryMode::Multi : ECAQueryMode::Test;

		CAPTUREGEOMOVERLAP(World, CollisionAnalyzerType, GeomPose, QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, OutOverlaps);
	}
#endif // ENABLE_COLLISION_ANALYZER

	return bHaveBlockingHit;
}

bool FGenericPhysicsInterface::GeomOverlapBlockingTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapBlocking);

	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	FTransform GeomTransform(Rot, Pos);
#if WITH_PHYSX
	FPhysXShapeAdaptor Adaptor(GeomTransform.GetRotation(), CollisionShape);
	return GeomOverlapMultiImp<EQueryInfo::IsBlocking>(World, Adaptor.GetGeometry(), CollisionShape, Adaptor.GetGeomPose(GeomTransform.GetTranslation()), Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
#else
	return false;
#endif
}

bool FGenericPhysicsInterface::GeomOverlapAnyTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapAny);

	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	FTransform GeomTransform(Rot, Pos);
#if WITH_PHYSX
	FPhysXShapeAdaptor Adaptor(GeomTransform.GetRotation(), CollisionShape);
	return GeomOverlapMultiImp<EQueryInfo::GatherAll>(World, Adaptor.GetGeometry(), CollisionShape, Adaptor.GetGeomPose(GeomTransform.GetTranslation()), Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
#else
	return false;
#endif
}

template<>
bool FGenericPhysicsInterface::GeomOverlapMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);

#if WITH_PHYSX
	FTransform GeomTransform(InRotation, InPosition);
	return GeomOverlapMultiImp<EQueryInfo::GatherAll>(World, InGeom.GetGeometry(), InGeom, GeomTransform, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
#else
	return false;
#endif
}

template<>
bool FGenericPhysicsInterface::GeomOverlapMulti(const UWorld* World, const FCollisionShape& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);

	FTransform GeomTransform(InRotation, InPosition);
#if WITH_PHYSX
	FPhysXShapeAdaptor Adaptor(GeomTransform.GetRotation(), InGeom);
	return GeomOverlapMultiImp<EQueryInfo::GatherAll>(World, Adaptor.GetGeometry(), InGeom, Adaptor.GetGeomPose(GeomTransform.GetTranslation()), OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
#else
	return false;
#endif
}