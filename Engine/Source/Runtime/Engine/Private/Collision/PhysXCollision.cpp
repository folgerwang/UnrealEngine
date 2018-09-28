// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if !WITH_APEIRON && !PHYSICS_INTERFACE_LLIMMEDIATE

#include "Engine/World.h"
#include "Collision.h"
#include "CollisionDebugDrawingPublic.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysXSupport.h"

float DebugLineLifetime = 2.f;

/** Temporary result buffer size */
#define HIT_BUFFER_SIZE							512		// Hit buffer size for traces and sweeps. This is the total size allowed for sync + async tests.
static_assert(HIT_BUFFER_SIZE > 0, "Invalid PhysX hit buffer size.");

#if WITH_PHYSX


#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "Collision/CollisionDebugDrawing.h"
#include "Collision/CollisionConversions.h"
#include "PhysicsEngine/PxQueryFilterCallback.h"
#include "PhysicsEngine/ScopedSQHitchRepeater.h"
#include "PhysicsEngine/CollisionAnalyzerCapture.h"

/**
 * Helper to lock/unlock multiple scenes that also makes sure to unlock everything when it goes out of scope.
 * Multiple locks on the same scene are NOT SAFE. You can't call LockRead() if already locked.
 * Multiple unlocks on the same scene are safe (repeated unlocks do nothing after the first successful unlock).
 */
struct FScopedMultiSceneReadLock
{
	FScopedMultiSceneReadLock()
	{
		for (int32 i=0; i < ARRAY_COUNT(SceneLocks); i++)
		{
			SceneLocks[i] = nullptr;
		}
	}

	~FScopedMultiSceneReadLock()
	{
		UnlockAll();
	}

	inline void LockRead(const UWorld* World, PxScene* Scene, EPhysicsSceneType SceneType)
	{
		checkSlow(SceneLocks[SceneType] == nullptr); // no nested locks allowed.
		SCENE_LOCK_READ(Scene);
		SceneLocks[SceneType] = Scene;
	}

	inline void UnlockRead(PxScene* Scene, EPhysicsSceneType SceneType)
	{
		checkSlow(SceneLocks[SceneType] == Scene || SceneLocks[SceneType] == nullptr);
		SCENE_UNLOCK_READ(Scene);
		SceneLocks[SceneType] = nullptr;
	}

	inline void UnlockAll()
	{
		for (int32 i=0; i < ARRAY_COUNT(SceneLocks); i++)
		{
			SCENE_UNLOCK_READ(SceneLocks[i]);
			SceneLocks[i] = nullptr;
		}
	}

	PxScene* SceneLocks[PST_MAX];
};


//////////////////////////////////////////////////////////////////////////


PxQueryFlags StaticDynamicQueryFlags(const FCollisionQueryParams& Params)
{
	switch(Params.MobilityType)
	{
		case EQueryMobilityType::Any: return  PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC;
		case EQueryMobilityType::Static: return  PxQueryFlag::eSTATIC;
		case EQueryMobilityType::Dynamic: return  PxQueryFlag::eDYNAMIC;
		default: check(0);
	}

	check(0);
	return PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC;
}

#endif // WITH_PHYSX 


//////////////////////////////////////////////////////////////////////////
// RAYCAST

bool FPhysicsInterface::RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastAny);
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	bool bHaveBlockingHit = false; // Track if we get any 'blocking' hits

#if WITH_PHYSX
	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		{
			const PxVec3 PDir = U2PVector(Delta / DeltaMag);
			PxRaycastBuffer PRaycastBuffer;

			// Create filter data used to filter collisions
			PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
			PxSceneQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER | PxQueryFlag::eANY_HIT);
			PxHitFlags POutputFlags = PxHitFlags();
			FPxQueryFilterCallback PQueryCallback(Params, false);
			PQueryCallback.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

			FPhysScene* PhysScene = World->GetPhysicsScene();
			{
				// Enable scene locks, in case they are required
				PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);
				SCOPED_SCENE_READ_LOCK(SyncScene);
				FScopedSQHitchRepeater<decltype(PRaycastBuffer)> HitchRepeater(PRaycastBuffer, PQueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
				do
				{
					SyncScene->raycast(U2PVector(Start), PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());
				bHaveBlockingHit = PRaycastBuffer.hasBlock;
			}

			// Test async scene if we have no blocking hit, and async tests are requested
			if (!bHaveBlockingHit && Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
			{
				PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
				SCOPED_SCENE_READ_LOCK(AsyncScene);
				FScopedSQHitchRepeater<decltype(PRaycastBuffer)> HitchRepeater(PRaycastBuffer, PQueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
				do
				{
					AsyncScene->raycast(U2PVector(Start), PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());
				bHaveBlockingHit = PRaycastBuffer.hasBlock;
			}
		}
	}

	TArray<FHitResult> Hits;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(World->DebugDrawSceneQueries(Params.TraceTag))
	{
		DrawLineTraces(World, Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	CAPTURERAYCAST(World, Start, End, ECAQueryMode::Test, TraceChannel, Params, ResponseParams, ObjectParams, Hits);

#endif // WITH_PHYSX 
	return bHaveBlockingHit;
}

bool FPhysicsInterface::RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastSingle);
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	OutHit = FHitResult();
	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	bool bHaveBlockingHit = false; // Track if we get any 'blocking' hits
#if WITH_PHYSX

	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		{
			FScopedMultiSceneReadLock SceneLocks;

			// Create filter data used to filter collisions
			PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
			PxQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER);
			PxHitFlags POutputFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE | PxHitFlag::eMTD | PxHitFlag::eFACE_INDEX;
			FPxQueryFilterCallback PQueryCallback(Params, false);
			PQueryCallback.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

			PxVec3 PStart = U2PVector(Start);
			PxVec3 PDir = U2PVector(Delta / DeltaMag);

			FPhysScene* PhysScene = World->GetPhysicsScene();
			PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);

			// Enable scene locks, in case they are required
			SceneLocks.LockRead(World, SyncScene, PST_Sync);

			PxRaycastBuffer PRaycastBuffer;
			{
				FScopedSQHitchRepeater<decltype(PRaycastBuffer)> HitchRepeater(PRaycastBuffer, PQueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
				do
				{
					SyncScene->raycast(U2PVector(Start), PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());
			}
			bHaveBlockingHit = PRaycastBuffer.hasBlock;
			if (!bHaveBlockingHit)
			{
				// Not going to use anything from this scene, so unlock it now.
				SceneLocks.UnlockRead(SyncScene, PST_Sync);
			}

			// Test async scene if async tests are requested
			if (Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
			{
				PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
				SceneLocks.LockRead(World, AsyncScene, PST_Async);
				PxRaycastBuffer PRaycastBufferAsync;
				FScopedSQHitchRepeater<decltype(PRaycastBufferAsync)> HitchRepeater(PRaycastBufferAsync, PQueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
				do
				{
					AsyncScene->raycast(U2PVector(Start), PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());
				const bool bHaveBlockingHitAsync = PRaycastBufferAsync.hasBlock;

				// If we have a blocking hit in the async scene and there was no sync blocking hit, or if the async blocking hit came first,
				// then this becomes the blocking hit.  We can test the PxSceneQueryImpactHit::distance since the DeltaMag is the same for both queries.
				if (bHaveBlockingHitAsync && (!bHaveBlockingHit || PRaycastBufferAsync.block.distance < PRaycastBuffer.block.distance))
				{
					PRaycastBuffer = PRaycastBufferAsync;
					bHaveBlockingHit = true;
				}
				else
				{
					// Not going to use anything from this scene, so unlock it now.
					SceneLocks.UnlockRead(AsyncScene, PST_Async);
				}
			}

			if (bHaveBlockingHit) // If we got a hit
			{
				PxTransform PStartTM(U2PVector(Start));
				if (ConvertQueryImpactHit(World, PRaycastBuffer.block, OutHit, DeltaMag, PFilter, Start, End, NULL, PStartTM, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
				{
					bHaveBlockingHit = false;
					UE_LOG(LogCollision, Error, TEXT("RaycastSingle resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC
					UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
					UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
					UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
					UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
				}
			}
		}

	}


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (World->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		DrawLineTraces(World, Start, End, Hits, DebugLineLifetime);	
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording && IsInGameThread())
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		CAPTURERAYCAST(World, Start, End, ECAQueryMode::Single, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
	}
#endif
#endif // WITH_PHYSX

	return bHaveBlockingHit;
}

#if WITH_PHYSX
template<typename HitType>
class FDynamicHitBuffer : public PxHitCallback<HitType>
{
private:
	/** Hit buffer used to provide hits via processTouches */
	TTypeCompatibleBytes<HitType> HitBuffer[HIT_BUFFER_SIZE];

	/** Hits encountered. Can be larger than HIT_BUFFER_SIZE */
	TArray<TTypeCompatibleBytes<HitType>, TInlineAllocator<HIT_BUFFER_SIZE>> Hits;

public:
	FDynamicHitBuffer()
		: PxHitCallback<HitType>((HitType*)HitBuffer, HIT_BUFFER_SIZE)
	{}

	virtual PxAgain processTouches(const HitType* buffer, PxU32 nbHits) override
	{
		Hits.Append((TTypeCompatibleBytes<HitType>*)buffer, nbHits);
		return true;
	}

	virtual void finalizeQuery() override
	{
		if (this->hasBlock)
		{
			// copy blocking hit to hits
			processTouches(&this->block, 1);
		}
	}

	FORCEINLINE int32 GetNumHits() const
	{
		return Hits.Num();
	}

	FORCEINLINE HitType* GetHits()
	{
		return (HitType*)Hits.GetData();
	}
};
#endif // WITH_PHYSX

bool FPhysicsInterface::RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastMultiple);
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	OutHits.Reset();

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

#if WITH_PHYSX
	FVector Delta = End - Start;
	float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, true);
		PxQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER);
		PxHitFlags POutputFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE | PxHitFlag::eMTD | PxHitFlag::eFACE_INDEX;
		FPxQueryFilterCallback PQueryCallback(Params, false);
		FDynamicHitBuffer<PxRaycastHit> PRaycastBuffer;

		bool bBlockingHit = false;
		const PxVec3 PDir = U2PVector(Delta/DeltaMag);

		// Enable scene locks, in case they are required
		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);

		FScopedMultiSceneReadLock SceneLocks;
		SceneLocks.LockRead(World, SyncScene, PST_Sync);
		{
			FScopedSQHitchRepeater<decltype(PRaycastBuffer)> HitchRepeater(PRaycastBuffer, PQueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				SyncScene->raycast(U2PVector(Start), PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallback);
			} while (HitchRepeater.RepeatOnHitch());
		}

		PxI32 NumHits = PRaycastBuffer.GetNumHits();

		if (NumHits == 0)
		{
			// Not going to use anything from this scene, so unlock it now.
			SceneLocks.UnlockRead(SyncScene, PST_Sync);
		}

		// Test async scene if async tests are requested and there was no overflow
		if( Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
			SceneLocks.LockRead(World, AsyncScene, PST_Async);

			// Write into the same PHits buffer
			bool bBlockingHitAsync = false;

			// If we have a blocking hit from the sync scene, there is no need to raycast past that hit
			const float RayLength = bBlockingHit ? PRaycastBuffer.GetHits()[NumHits-1].distance : DeltaMag;

			PxI32 NumAsyncHits = 0;
			if(RayLength > SMALL_NUMBER) // don't bother doing the trace if the sync scene trace gave a hit time of zero
			{
				FScopedSQHitchRepeater<decltype(PRaycastBuffer)> HitchRepeater(PRaycastBuffer, PQueryCallback, FHitchDetectionInfo(Start, End, TraceChannel, Params));
				do 
				{
					AsyncScene->raycast(U2PVector(Start), PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());

				NumAsyncHits = PRaycastBuffer.GetNumHits() - NumHits;
			}

			if (NumAsyncHits == 0)
			{
				// Not going to use anything from this scene, so unlock it now.
				SceneLocks.UnlockRead(AsyncScene, PST_Async);
			}

			PxI32 TotalNumHits = NumHits + NumAsyncHits;

			// If there is a blocking hit in the sync scene, and it is closer than the blocking hit in the async scene (or there is no blocking hit in the async scene),
			// then move it to the end of the array to get it out of the way.
			if (bBlockingHit)
			{
				if (!bBlockingHitAsync || PRaycastBuffer.GetHits()[NumHits-1].distance < PRaycastBuffer.GetHits()[TotalNumHits-1].distance)
				{
					PRaycastBuffer.GetHits()[TotalNumHits-1] = PRaycastBuffer.GetHits()[NumHits-1];
				}
			}

			// Merge results
			NumHits = TotalNumHits;

			bBlockingHit = bBlockingHit || bBlockingHitAsync;

			// Now eliminate hits which are farther than the nearest blocking hit, or even those that are the exact same distance as the blocking hit,
			// to ensure the blocking hit is the last in the array after sorting in ConvertRaycastResults (below).
			if (bBlockingHit)
			{
				const PxF32 MaxDistance = PRaycastBuffer.GetHits()[NumHits-1].distance;
				PxI32 TestHitCount = NumHits-1;
				for (PxI32 HitNum = TestHitCount; HitNum-- > 0;)
				{
					if (PRaycastBuffer.GetHits()[HitNum].distance >= MaxDistance)
					{
						PRaycastBuffer.GetHits()[HitNum] = PRaycastBuffer.GetHits()[--TestHitCount];
					}
				}
				if (TestHitCount < NumHits-1)
				{
					PRaycastBuffer.GetHits()[TestHitCount] = PRaycastBuffer.GetHits()[NumHits-1];
					NumHits = TestHitCount + 1;
				}
			}
		}

		if (NumHits > 0)
		{
			if (ConvertRaycastResults(bBlockingHit, World, NumHits, PRaycastBuffer.GetHits(), DeltaMag, PFilter, OutHits, Start, End, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
			{
				// We don't need to change bBlockingHit, that's done by ConvertRaycastResults if it removed the blocking hit.
				UE_LOG(LogCollision, Error, TEXT("RaycastMulti resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}

		bHaveBlockingHit = bBlockingHit;

	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(World->DebugDrawSceneQueries(Params.TraceTag))
	{
		DrawLineTraces(World, Start, End, OutHits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	CAPTURERAYCAST(World, Start, End, ECAQueryMode::Multi, TraceChannel, Params, ResponseParams, ObjectParams, OutHits);

#endif // WITH_PHYSX
	return bHaveBlockingHit;
}

//////////////////////////////////////////////////////////////////////////
// GEOM SWEEP

bool FPhysicsInterface::GeomSweepTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepAny);
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	bool bHaveBlockingHit = false; // Track if we get any 'blocking' hits

#if WITH_PHYSX
	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxQuat& PGeomRot = ShapeAdaptor.GetGeomOrientation();

	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
		PxQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER | PxQueryFlag::ePOSTFILTER | PxQueryFlag::eANY_HIT);
		PxHitFlags POutputFlags; 

		FPxQueryFilterCallback PQueryCallbackSweep(Params, true);
		PQueryCallbackSweep.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

		PxTransform PStartTM(U2PVector(Start), PGeomRot);
		PxVec3 PDir = U2PVector(Delta/DeltaMag);

		FPhysScene* PhysScene = World->GetPhysicsScene();
		{
			// Enable scene locks, in case they are required
			PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);
			SCOPED_SCENE_READ_LOCK(SyncScene);
			PxSweepBuffer PSweepBuffer;
			FScopedSQHitchRepeater<decltype(PSweepBuffer)> HitchRepeater(PSweepBuffer, PQueryCallbackSweep, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				SyncScene->sweep(PGeom, PStartTM, PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallbackSweep);
			} while (HitchRepeater.RepeatOnHitch());

			bHaveBlockingHit = PSweepBuffer.hasBlock;
		}

		// Test async scene if async tests are requested and there was no blocking hit was found in the sync scene (since no hit info other than a boolean yes/no is recorded)
		if( !bHaveBlockingHit && Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
			SCOPED_SCENE_READ_LOCK(AsyncScene);
			PxSweepBuffer PSweepBuffer;
			FScopedSQHitchRepeater<decltype(PSweepBuffer)> HitchRepeater(PSweepBuffer, PQueryCallbackSweep, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				AsyncScene->sweep(PGeom, PStartTM, PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallbackSweep);
			} while (HitchRepeater.RepeatOnHitch());
			bHaveBlockingHit = PSweepBuffer.hasBlock;
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(World->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		DrawGeomSweeps(World, Start, End, PGeom, PGeomRot, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> Hits;
		CAPTUREGEOMSWEEP(World, Start, End, Rot, ECAQueryMode::Test, CollisionShape, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
	}
#endif // ENABLE_COLLISION_ANALYZER

#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GeomSweepTest

	return bHaveBlockingHit;
}

bool FPhysicsInterface::GeomSweepSingle(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepSingle);
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();

	OutHit = FHitResult();
	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;

	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

#if WITH_PHYSX
	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxQuat& PGeomRot = ShapeAdaptor.GetGeomOrientation();

	const FVector Delta = End - Start;
	const float DeltaMagSize = Delta.Size();
	const float DeltaMag = FMath::IsNearlyZero(DeltaMagSize) ? 0.f : DeltaMagSize;
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, false);
		//UE_LOG(LogCollision, Log, TEXT("PFilter: %x %x %x %x"), PFilter.word0, PFilter.word1, PFilter.word2, PFilter.word3);
		PxQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER);
		PxHitFlags POutputFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE | PxHitFlag::eMTD;
		FPxQueryFilterCallback PQueryCallbackSweep(Params, true);
		PQueryCallbackSweep.bIgnoreTouches = true; // pre-filter to ignore touches and only get blocking hits.

		PxTransform PStartTM(U2PVector(Start), PGeomRot);
		PxVec3 PDir = DeltaMag == 0.f ? PxVec3(1.f, 0.f, 0.f) : U2PVector(Delta/DeltaMag);	//If DeltaMag is 0 (equality of float is fine because we sanitized to 0) then just use any normalized direction

		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);

		// Enable scene locks, in case they are required
		FScopedMultiSceneReadLock SceneLocks;
		SceneLocks.LockRead(World, SyncScene, PST_Sync);

		PxSweepBuffer PSweepBuffer;
		{
			FScopedSQHitchRepeater<decltype(PSweepBuffer)> HitchRepeater(PSweepBuffer, PQueryCallbackSweep, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				SyncScene->sweep(PGeom, PStartTM, PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallbackSweep);

			} while (HitchRepeater.RepeatOnHitch());
		}
		bHaveBlockingHit = PSweepBuffer.hasBlock;
		PxSweepHit PHit = PSweepBuffer.block;

		if (!bHaveBlockingHit)
		{
			// Not using anything from this scene, so unlock it.
			SceneLocks.UnlockRead(SyncScene, PST_Sync);
		}

		// Test async scene if async tests are requested
		if( Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
			SceneLocks.LockRead(World, AsyncScene, PST_Async);

			bool bHaveBlockingHitAsync;
			PxSweepBuffer PSweepBufferAsync;
			FScopedSQHitchRepeater<decltype(PSweepBuffer)> HitchRepeater(PSweepBuffer, PQueryCallbackSweep, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				AsyncScene->sweep(PGeom, PStartTM, PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallbackSweep);

			} while (HitchRepeater.RepeatOnHitch());
			bHaveBlockingHitAsync = PSweepBufferAsync.hasBlock;
			PxSweepHit PHitAsync = PSweepBufferAsync.block;

			// If we have a blocking hit in the async scene and there was no sync blocking hit, or if the async blocking hit came first,
			// then this becomes the blocking hit.  We can test the PxSceneQueryImpactHit::distance since the DeltaMag is the same for both queries.
			if (bHaveBlockingHitAsync && (!bHaveBlockingHit || PHitAsync.distance < PHit.distance))
			{
				PHit = PHitAsync;
				bHaveBlockingHit = true;
			}
			else
			{
				// Not using anything from this scene, so unlock it.
				SceneLocks.UnlockRead(AsyncScene, PST_Async);
			}
		}

		if(bHaveBlockingHit) // If we got a hit, convert it to unreal type
		{
			PHit.faceIndex = FindFaceIndex(PHit, PDir);
			if (ConvertQueryImpactHit(World, PHit, OutHit, DeltaMag, PFilter, Start, End, &PGeom, PStartTM, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
			{
				bHaveBlockingHit = false;
				UE_LOG(LogCollision, Error, TEXT("GeomSweepSingle resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (World->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		DrawGeomSweeps(World, Start, End, PGeom, PGeomRot, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_COLLISION_ANALYZER
	if (GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> Hits;
		if (bHaveBlockingHit)
		{
			Hits.Add(OutHit);
		}
		CAPTUREGEOMSWEEP(World, Start, End, Rot, ECAQueryMode::Single, CollisionShape, TraceChannel, Params, ResponseParams, ObjectParams, Hits);
	}
#endif

#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GeomSweepSingle

	return bHaveBlockingHit;
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
static bool FirstNaNCheckPhysXCollision = true;

void PrintSceneActors(PxScene* Scene)
{
	TArray<PxActor*> Actors;
	PxU32 Size;
	Size = Scene->getNbActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC | physx::PxActorTypeFlag::eRIGID_STATIC);
	Actors.SetNum(Size);
	Scene->getActors(physx::PxActorTypeFlag::eRIGID_DYNAMIC | physx::PxActorTypeFlag::eRIGID_STATIC, Actors.GetData(), Size);
	for (uint32 i = 0; i < Size; ++i)
	{
		PxRigidActor* RigidActor = Actors[i]->is<PxRigidActor>();
		if (RigidActor)
		{
			UE_LOG(LogCollision, Warning, TEXT("Actor is %s with pointer %p"), RigidActor->getName() ? ANSI_TO_TCHAR(RigidActor->getName()) : TEXT(""), (void*)RigidActor);
			PxU32 NumShapes = RigidActor->getNbShapes();
			TArray<PxShape*> ShapeBuffer;
			ShapeBuffer.SetNum(NumShapes);
			RigidActor->getShapes(ShapeBuffer.GetData(), NumShapes);
			for (uint32 j = 0; j < NumShapes; ++j)
			{
				if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::eBOX)
				{
					PxBoxGeometry Geometry;
					check(ShapeBuffer[j]->getBoxGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is Box with Extents %f %f %f"), Geometry.halfExtents.x, Geometry.halfExtents.y, Geometry.halfExtents.z);
				}
				else if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::eCAPSULE)
				{
					PxCapsuleGeometry Geometry;
					check(ShapeBuffer[j]->getCapsuleGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is Capsule with Height %f, Radius %f"), Geometry.halfHeight, Geometry.radius);
				}
				else if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::eCONVEXMESH)
				{
					PxConvexMeshGeometry Geometry;
					check(ShapeBuffer[j]->getConvexMeshGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is Convex"));
				}
				else if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::eHEIGHTFIELD)
				{
					PxHeightFieldGeometry Geometry;
					check(ShapeBuffer[j]->getHeightFieldGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is Height Field"));
				}
				else if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::ePLANE)
				{
					PxPlaneGeometry Geometry;
					check(ShapeBuffer[j]->getPlaneGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is a Plane"));
				}
				else if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::eSPHERE)
				{
					PxSphereGeometry Geometry;
					check(ShapeBuffer[j]->getSphereGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is a Sphere with radius %f"), Geometry.radius);
				}
				else if (ShapeBuffer[j]->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
				{
					PxTriangleMeshGeometry Geometry;
					check(ShapeBuffer[j]->getTriangleMeshGeometry(Geometry));
					UE_LOG(LogCollision, Warning, TEXT("Shape is a Triangle Mesh"));
				}
				UE_LOG(LogCollision, Warning, TEXT("Collision Shape %d for Actor %d Translation %f %f %f"), j, i, P2UTransform(ShapeBuffer[j]->getLocalPose()).GetTranslation().X, P2UTransform(ShapeBuffer[j]->getLocalPose()).GetTranslation().Y, P2UTransform(ShapeBuffer[j]->getLocalPose()).GetTranslation().Z);
			}
		}
		UE_LOG(LogCollision, Warning, TEXT("Actor %d Center %f %f %f"), i, Actors[i]->getWorldBounds().getCenter().x, Actors[i]->getWorldBounds().getCenter().y, Actors[i]->getWorldBounds().getCenter().z);
		UE_LOG(LogCollision, Warning, TEXT("Actor %d Extents %f %f %f"), i, Actors[i]->getWorldBounds().getExtents(0), Actors[i]->getWorldBounds().getExtents(1), Actors[i]->getWorldBounds().getExtents(2));
	}
}

#define PRINT_QUERY_INPUTS() \
	UE_LOG(LogCollision, Warning, TEXT("Geometry Type is %d"), PGeom.getType()); \
	UE_LOG(LogCollision, Warning, TEXT("Rotation is %f, %f, %f"), P2UQuat(PGeomRot).Euler().X, P2UQuat(PGeomRot).Euler().Y, P2UQuat(PGeomRot).Euler().Z); \
	UE_LOG(LogCollision, Warning, TEXT("Start is %f, %f, %f"), Start.X, Start.Y, Start.Z); \
	UE_LOG(LogCollision, Warning, TEXT("End is %f, %f, %f"), End.X, End.Y, End.Z); \
	UE_LOG(LogCollision, Warning, TEXT("Trace Channel is %d"), TraceChannel); \
	UE_LOG(LogCollision, Warning, TEXT("Collision Query Params %s"), *Params.ToString()); \
	for (int32 ii = 0; ii < 32; ++ii) \
	{ \
		UE_LOG(LogCollision, Warning, TEXT("Collision Response Params %d %d"), ii, ResponseParams.CollisionResponse.GetResponse((ECollisionChannel)ii)); \
	} \
	UE_LOG(LogCollision, Warning, TEXT("Collision Object Query Params %d"), ObjectParams.ObjectTypesToQuery);

#define CHECK_NAN(Val) \
	if (FPlatformMath::IsNaN(Val) && FirstNaNCheckPhysXCollision) \
	{ \
        FirstNaNCheckPhysXCollision = false; \
		PRINT_QUERY_INPUTS() \
		logOrEnsureNanError(TEXT("Failed!")); \
	}

#define CHECK_NAN_SYNC(Val) \
	if (FPlatformMath::IsNaN(Val) && FirstNaNCheckPhysXCollision) \
	{ \
        FirstNaNCheckPhysXCollision = false; \
		PRINT_QUERY_INPUTS() \
		PrintSceneActors(SyncScene); \
		logOrEnsureNanError(TEXT("Failed!")); \
	}

#define CHECK_NAN_ASYNC(Val) \
	if (FPlatformMath::IsNaN(Val) && FirstNaNCheckPhysXCollision) \
	{ \
        FirstNaNCheckPhysXCollision = false; \
		PRINT_QUERY_INPUTS() \
		PrintSceneActors(AsyncScene); \
		logOrEnsureNanError(TEXT("Failed!")); \
	}
#endif

#if WITH_PHYSX
bool /*FPhysicsInterface::*/GeomSweepMulti_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxQuat& PGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomSweepMultiple);
	FScopeCycleCounter Counter(Params.StatId);
	STARTQUERYTIMER();
	bool bBlockingHit = false;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	CHECK_NAN(Start.X);
	CHECK_NAN(Start.Y);
	CHECK_NAN(Start.Z);
	CHECK_NAN(End.X);
	CHECK_NAN(End.Y);
	CHECK_NAN(End.Z);
#endif

	const int32 InitialHitCount = OutHits.Num();

	// Create filter data used to filter collisions
	PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, true);
	PxQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER | PxQueryFlag::ePOSTFILTER);
	PxHitFlags POutputFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE | PxHitFlag::eMTD | PxHitFlag::eFACE_INDEX;
	FPxQueryFilterCallback PQueryCallbackSweep(Params, true);

	const FVector Delta = End - Start;
	const float DeltaMagSize = Delta.Size();
	const float DeltaMag = FMath::IsNearlyZero(DeltaMagSize) ? 0.f : DeltaMagSize;
	{
		FPhysScene* PhysScene = World->GetPhysicsScene();
		PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);

		// Lock scene
		FScopedMultiSceneReadLock SceneLocks;
		SceneLocks.LockRead(World, SyncScene, PST_Sync);

		const PxTransform PStartTM(U2PVector(Start), PGeomRot);
		PxVec3 PDir = DeltaMag == 0.f ? PxVec3(1.f, 0.f, 0.f) : U2PVector(Delta/DeltaMag);	//If DeltaMag is 0 (equality of float is fine because we sanitized to 0) then just use any normalized direction

		// Keep track of closest blocking hit distance.
		float MinBlockDistance = DeltaMag;

		FDynamicHitBuffer<PxSweepHit> PSweepBuffer;
		{
			FScopedSQHitchRepeater<decltype(PSweepBuffer)> HitchRepeater(PSweepBuffer, PQueryCallbackSweep, FHitchDetectionInfo(Start, End, TraceChannel, Params));
			do
			{
				SyncScene->sweep(PGeom, PStartTM, PDir, DeltaMag, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallbackSweep);
			} while (HitchRepeater.RepeatOnHitch());
		}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		for (int32 i = 0; i < PSweepBuffer.GetNumHits(); ++i)
		{
			PxSweepHit& PHit = PSweepBuffer.GetHits()[i];
			if (PHit.flags & PxHitFlag::ePOSITION)
			{
				CHECK_NAN_SYNC(PHit.position.x);
				CHECK_NAN_SYNC(PHit.position.y);
				CHECK_NAN_SYNC(PHit.position.z);
			}
		}
#endif
				
		bool bBlockingHitSync = PSweepBuffer.hasBlock;
		PxI32 NumHits = PSweepBuffer.GetNumHits();

		if (bBlockingHitSync)
		{
			MinBlockDistance = PSweepBuffer.block.distance;
			bBlockingHit = true;
		}
		else if (NumHits == 0)
		{
			// Not using anything from this scene, so unlock it.
			SceneLocks.UnlockRead(SyncScene, PST_Sync);
		}

		// Test async scene if async tests are requested and there was no overflow
		if (Params.bTraceAsyncScene && MinBlockDistance > SMALL_NUMBER && PhysScene->HasAsyncScene())
		{
			PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
			SceneLocks.LockRead(World, AsyncScene, PST_Async);

			{
				FScopedSQHitchRepeater<decltype(PSweepBuffer)> HitchRepeater(PSweepBuffer, PQueryCallbackSweep, FHitchDetectionInfo(Start, End, TraceChannel, Params));
				do
				{
					AsyncScene->sweep(PGeom, PStartTM, PDir, MinBlockDistance, HitchRepeater.GetBuffer(), POutputFlags, PQueryFilterData, &PQueryCallbackSweep);
				} while (HitchRepeater.RepeatOnHitch());
			}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
			for (int32 i = 0; i < PSweepBuffer.GetNumHits(); ++i)
			{
				PxSweepHit& PHit = PSweepBuffer.GetHits()[i];
				if (PHit.flags & PxHitFlag::ePOSITION)
				{
					CHECK_NAN_ASYNC(PHit.position.x);
					CHECK_NAN_ASYNC(PHit.position.y);
					CHECK_NAN_ASYNC(PHit.position.z);
				}
			}
#endif

			bool bBlockingHitAsync = PSweepBuffer.hasBlock;
			PxI32 NumAsyncHits = PSweepBuffer.GetNumHits() - NumHits;
			if (NumAsyncHits == 0)
			{
				// Not using anything from this scene, so unlock it.
				SceneLocks.UnlockRead(AsyncScene, PST_Async);
			}

			if (bBlockingHitAsync)
			{
				MinBlockDistance = FMath::Min<float>(PSweepBuffer.block.distance, MinBlockDistance);
				bBlockingHit = true;
			}
		}

		NumHits = PSweepBuffer.GetNumHits();

		// Convert all hits to unreal structs. This will remove any hits further than MinBlockDistance, and sort results.
		if (NumHits > 0)
		{
			if (AddSweepResults(bBlockingHit, World, NumHits, PSweepBuffer.GetHits(), DeltaMag, PFilter, OutHits, Start, End, PGeom, PStartTM, MinBlockDistance, Params.bReturnFaceIndex, Params.bReturnPhysicalMaterial) == EConvertQueryResult::Invalid)
			{
				// We don't need to change bBlockingHit, that's done by AddSweepResults if it removed the blocking hit.
				UE_LOG(LogCollision, Error, TEXT("GeomSweepMulti resulted in a NaN/INF in PHit!"));
#if ENABLE_NAN_DIAGNOSTIC				
				UE_LOG(LogCollision, Error, TEXT("--------TraceChannel : %d"), (int32)TraceChannel);
				UE_LOG(LogCollision, Error, TEXT("--------Start : %s"), *Start.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------End : %s"), *End.ToString());
				UE_LOG(LogCollision, Error, TEXT("--------%s"), *Params.ToString());
#endif
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (World->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> OnlyMyHits(OutHits);
		OnlyMyHits.RemoveAt(0, InitialHitCount, false); // Remove whatever was there initially.
		DrawGeomSweeps(World, Start, End, PGeom, PGeomRot, OnlyMyHits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return bBlockingHit;
}
#endif // WITH_PHYSX 

template<>
bool FPhysicsInterface::GeomSweepMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/)
{
	STARTQUERYTIMER();

	OutHits.Reset();

	if((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bBlockingHit = false;

#if ENABLE_COLLISION_ANALYZER
	const int32 InitialHitCount = OutHits.Num();
#endif // ENABLE_COLLISION_ANALYZER

	bBlockingHit = GeomSweepMulti_PhysX(World, InGeom.GetGeometry(), U2PQuat(InGeomRot), OutHits, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);

#if ENABLE_COLLISION_ANALYZER
	if(GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> OnlyMyHits(OutHits);
		OnlyMyHits.RemoveAt(0, InitialHitCount, false); // Remove whatever was there initially.
		CAPTUREGEOMSWEEP(World, Start, End, InGeomRot, ECAQueryMode::Multi, InGeom, TraceChannel, Params, ResponseParams, ObjectParams, OnlyMyHits);
	}
#endif // ENABLE_COLLISION_ANALYZER

	return bBlockingHit;
}

template<>
bool FPhysicsInterface::GeomSweepMulti(const UWorld* World, const FCollisionShape& InGeom, const FQuat& InGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams /*= FCollisionObjectQueryParams::DefaultObjectQueryParam*/)
{
	STARTQUERYTIMER();

	OutHits.Reset();

	if((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	// Track if we get any 'blocking' hits
	bool bBlockingHit = false;

	FPhysXShapeAdaptor ShapeAdaptor(InGeomRot, InGeom);
	const PxGeometry& PGeom = ShapeAdaptor.GetGeometry();
	const PxQuat& PGeomRot = ShapeAdaptor.GetGeomOrientation();

#if ENABLE_COLLISION_ANALYZER
	const int32 InitialHitCount = OutHits.Num();
#endif // ENABLE_COLLISION_ANALYZER

	bBlockingHit = GeomSweepMulti_PhysX(World, PGeom, PGeomRot, OutHits, Start, End, TraceChannel, Params, ResponseParams, ObjectParams);

#if ENABLE_COLLISION_ANALYZER
	if(GCollisionAnalyzerIsRecording)
	{
		TArray<FHitResult> OnlyMyHits(OutHits);
		OnlyMyHits.RemoveAt(0, InitialHitCount, false); // Remove whatever was there initially.
		CAPTUREGEOMSWEEP(World, Start, End, InGeomRot, ECAQueryMode::Multi, InGeom, TraceChannel, Params, ResponseParams, ObjectParams, OnlyMyHits);
	}
#endif // ENABLE_COLLISION_ANALYZER

	return bBlockingHit;
}

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

#if WITH_PHYSX

template <EQueryInfo::Type InfoType>
bool GeomOverlapMultiImp_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxTransform& PGeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);
	FScopeCycleCounter Counter(Params.StatId);
	
	bool bHaveBlockingHit = false;

	// overlapMultiple only supports sphere/capsule/box 
	if (PGeom.getType()==PxGeometryType::eSPHERE || PGeom.getType()==PxGeometryType::eCAPSULE || PGeom.getType()==PxGeometryType::eBOX || PGeom.getType()==PxGeometryType::eCONVEXMESH )
	{
		// Create filter data used to filter collisions
		PxFilterData PFilter = CreateQueryFilterData(TraceChannel, Params.bTraceComplex, ResponseParams.CollisionResponse, Params, ObjectParams, InfoType != EQueryInfo::IsAnything);
		PxQueryFilterData PQueryFilterData(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER);
		PxQueryFilterData PQueryFilterDataAny(PFilter, StaticDynamicQueryFlags(Params) | PxQueryFlag::ePREFILTER | PxQueryFlag::eANY_HIT);
		FPxQueryFilterCallback PQueryCallback(Params, false);
		PQueryCallback.bIgnoreTouches |= (InfoType == EQueryInfo::IsBlocking); // pre-filter to ignore touches and only get blocking hits, if that's what we're after.
		PQueryCallback.bIsOverlapQuery = true;

		// Enable scene locks, in case they are required
		FScopedMultiSceneReadLock SceneLocks;
		FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr;
		// @todo(mlentine): Should this ever happen?
		if (!PhysScene)
		{
			UE_LOG(LogCollision, Log, TEXT("GeomOverlapMulti : cannot detect collisions with an empty world"));
			return false;
		}
		PxScene* SyncScene = PhysScene->GetPxScene(PST_Sync);

		// we can't use scoped because we later do a conversion which depends on these results and it should all be atomic
		SceneLocks.LockRead(World, SyncScene, PST_Sync);

		FDynamicHitBuffer<PxOverlapHit> POverlapBuffer;
		PxI32 NumHits = 0;
		
		if ((InfoType == EQueryInfo::IsAnything) || (InfoType == EQueryInfo::IsBlocking))
		{
			FScopedSQHitchRepeater<decltype(POverlapBuffer)> HitchRepeater(POverlapBuffer, PQueryCallback, FHitchDetectionInfo(PGeomPose, TraceChannel, Params));
			do
			{
				SyncScene->overlap(PGeom, PGeomPose, HitchRepeater.GetBuffer(), PQueryFilterDataAny, &PQueryCallback);
			} while (HitchRepeater.RepeatOnHitch());

			if (POverlapBuffer.hasBlock)
			{
				return true;
			}
		}
		else
		{
			checkSlow(InfoType == EQueryInfo::GatherAll);
			
			FScopedSQHitchRepeater<decltype(POverlapBuffer)> HitchRepeater(POverlapBuffer, PQueryCallback, FHitchDetectionInfo(PGeomPose, TraceChannel, Params));
			do
			{
				SyncScene->overlap(PGeom, PGeomPose, HitchRepeater.GetBuffer(), PQueryFilterData, &PQueryCallback);
			} while (HitchRepeater.RepeatOnHitch());

			NumHits = POverlapBuffer.GetNumHits();
			if (NumHits == 0)
			{
				// Not using anything from this scene, so unlock it.
				SceneLocks.UnlockRead(SyncScene, PST_Sync);
			}
		}

		// Test async scene if async tests are requested and there was no overflow
		if (Params.bTraceAsyncScene && PhysScene->HasAsyncScene())
		{		
			PxScene* AsyncScene = PhysScene->GetPxScene(PST_Async);
			
			// we can't use scoped because we later do a conversion which depends on these results and it should all be atomic
			SceneLocks.LockRead(World, AsyncScene, PST_Async);
		
			if ((InfoType == EQueryInfo::IsAnything) || (InfoType == EQueryInfo::IsBlocking))
			{
				FScopedSQHitchRepeater<decltype(POverlapBuffer)> HitchRepeater(POverlapBuffer, PQueryCallback, FHitchDetectionInfo(PGeomPose, TraceChannel, Params));
				do
				{
					AsyncScene->overlap(PGeom, PGeomPose, HitchRepeater.GetBuffer(), PQueryFilterDataAny, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());
				
				if (POverlapBuffer.hasBlock)
				{
					return true;
				}
			}
			else
			{
				checkSlow(InfoType == EQueryInfo::GatherAll);

				FScopedSQHitchRepeater<decltype(POverlapBuffer)> HitchRepeater(POverlapBuffer, PQueryCallback, FHitchDetectionInfo(PGeomPose, TraceChannel, Params));
				do
				{
					AsyncScene->overlap(PGeom, PGeomPose, HitchRepeater.GetBuffer(), PQueryFilterData, &PQueryCallback);
				} while (HitchRepeater.RepeatOnHitch());

				PxI32 NumAsyncHits = POverlapBuffer.GetNumHits() - NumHits;
				if (NumAsyncHits == 0)
				{
					// Not using anything from this scene, so unlock it.
					SceneLocks.UnlockRead(AsyncScene, PST_Async);
				}
			}
		}

		NumHits = POverlapBuffer.GetNumHits();

		if (InfoType == EQueryInfo::GatherAll)	//if we are gathering all we need to actually convert to UE format
		{
			if (NumHits > 0)
			{
				bHaveBlockingHit = ConvertOverlapResults(NumHits, POverlapBuffer.GetHits(), PFilter, OutOverlaps);
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (World->DebugDrawSceneQueries(Params.TraceTag))
			{
				DrawGeomOverlaps(World, PGeom, PGeomPose, OutOverlaps, DebugLineLifetime);
			}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
	}
	else
	{
		UE_LOG(LogCollision, Log, TEXT("GeomOverlapMulti : unsupported shape - only supports sphere, capsule, box"));
	}

	return bHaveBlockingHit;
}

bool GeomOverlapMulti_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxTransform& PGeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	return GeomOverlapMultiImp_PhysX<EQueryInfo::GatherAll>(World, PGeom, PGeomPose, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
}

#endif

template <EQueryInfo::Type InfoType>
bool GeomOverlapMultiImp(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
	if ((World == NULL) || (World->GetPhysicsScene() == NULL))
	{
		return false;
	}

	STARTQUERYTIMER();

	// Track if we get any 'blocking' hits
	bool bHaveBlockingHit = false;

	FPhysXShapeAdaptor ShapeAdaptor(Rot, CollisionShape);
	const PxGeometry& InGeom = ShapeAdaptor.GetGeometry();
	const PxTransform& PGeomPose = ShapeAdaptor.GetGeomPose(Pos);
	bHaveBlockingHit = GeomOverlapMultiImp_PhysX<InfoType>(World, InGeom, PGeomPose, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);

#if ENABLE_COLLISION_ANALYZER
	if(GCollisionAnalyzerIsRecording)
	{
		// Determine query mode ('single' doesn't really exist for overlaps)
		ECAQueryMode::Type QueryMode = (InfoType == EQueryInfo::GatherAll) ? ECAQueryMode::Multi : ECAQueryMode::Test;

		CAPTUREGEOMOVERLAP(World, CollisionShape, FTransform(Rot, Pos), QueryMode, TraceChannel, Params, ResponseParams, ObjectParams, OutOverlaps);
	}
#endif // ENABLE_COLLISION_ANALYZER

	return bHaveBlockingHit;
}

//bool FPhysicsInterface::GeomOverlapMulti(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
//{
//#if WITH_PHYSX
//	OutOverlaps.Reset();
//	return GeomOverlapMultiImp<EQueryInfo::GatherAll>(World, CollisionShape, Pos, Rot, OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);
//#else
//	return false;
//#endif // WITH_PHYSX
//}

bool FPhysicsInterface::GeomOverlapBlockingTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
#if WITH_PHYSX
	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	return GeomOverlapMultiImp<EQueryInfo::IsBlocking>(World, CollisionShape, Pos, Rot, Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
#else
	return false;
#endif // WITH_PHYSX
}

bool FPhysicsInterface::GeomOverlapAnyTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams)
{
#if WITH_PHYSX
	TArray<FOverlapResult> Overlaps;	//needed only for template shared code
	return GeomOverlapMultiImp<EQueryInfo::IsAnything>(World, CollisionShape, Pos, Rot, Overlaps, TraceChannel, Params, ResponseParams, ObjectParams);
#else
	return false;
#endif // WITH_PHYSX
}

template<>
bool FPhysicsInterface::GeomOverlapMulti(const UWorld* World, const FPhysicsGeometryCollection& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
{
	STARTQUERYTIMER();

	FTransform GeomTransform(InRotation, InPosition);
	bool bBlockingHit = GeomOverlapMultiImp_PhysX<EQueryInfo::GatherAll>(World, InGeom.GetGeometry(), U2PTransform(GeomTransform), OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);

	CAPTUREGEOMOVERLAP(World, InGeom, GeomTransform, ECAQueryMode::Multi, TraceChannel, Params, ResponseParams, ObjectParams, OutOverlaps);

	return bBlockingHit;
}

template<>
bool FPhysicsInterface::GeomOverlapMulti(const UWorld* World, const FCollisionShape& InGeom, const FVector& InPosition, const FQuat& InRotation, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectParams)
{
	STARTQUERYTIMER();

	FTransform GeomTransform(InRotation, InPosition);
	FPhysXShapeAdaptor Adaptor(GeomTransform.GetRotation(), InGeom);

	bool bBlockingHit = GeomOverlapMultiImp_PhysX<EQueryInfo::GatherAll>(World, Adaptor.GetGeometry(), Adaptor.GetGeomPose(GeomTransform.GetTranslation()), OutOverlaps, TraceChannel, Params, ResponseParams, ObjectParams);

	CAPTUREGEOMOVERLAP(World, InGeom, GeomTransform, ECAQueryMode::Multi, TraceChannel, Params, ResponseParams, ObjectParams, OutOverlaps);

	return bBlockingHit;
}

#endif