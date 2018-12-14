// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "GeometryCollection/GeometryCollectionPhysicsProxy.h"

#include "CoreMinimal.h"
#include "Chaos/PBDCollisionConstraintUtil.h"
#include "Async/ParallelFor.h"
#include "PBDRigidsSolver.h"
#include "GeometryCollection/GeometryCollectionSolverCallbacks.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "GeometryCollection/RecordedTransformTrack.h"

FGeometryCollectionResults::FGeometryCollectionResults()
{
	Transforms = MakeShared<TManagedArray<FTransform>>();
	RigidBodyIds = MakeShared<TManagedArray<int32>>();
	BoneHierarchy = MakeShared<TManagedArray<FGeometryCollectionBoneNode>>();
}

void FGeometryCollectionPhysicsProxy::MergeRecordedTracks(const FRecordedTransformTrack& A, const FRecordedTransformTrack& B, FRecordedTransformTrack& Target)
{
	const int32 NumAKeys = A.Records.Num();
	const int32 NumBKeys = B.Records.Num();

	if(NumAKeys == 0)
	{
		Target = B;
		return;
	}

	if(NumBKeys == 0)
	{
		Target = A;
		return;
	}

	// We have to copy the tracks to a local cache here because Target could point at either A or B.
	FRecordedTransformTrack TempMergedTrack = A;

	// Expand to hold all the keys
	TempMergedTrack.Records.Reserve(NumAKeys + NumBKeys);

	// Insert B frames into the merged set
	for(int32 BKeyIndex = 0; BKeyIndex < NumBKeys; ++BKeyIndex)
	{
		const FRecordedFrame& BFrame = B.Records[BKeyIndex];
		int32 KeyBefore = TempMergedTrack.FindLastKeyBefore(BFrame.Timestamp);

		TempMergedTrack.Records.Insert(BFrame, KeyBefore + 1);
	}

	// Copy to target
	Target = TempMergedTrack;
}

FRecordedFrame& FGeometryCollectionPhysicsProxy::InsertRecordedFrame(FRecordedTransformTrack& InTrack, float InTime)
{
	// Can't just throw on the end, might need to insert
	const int32 BeforeNewIndex = InTrack.FindLastKeyBefore(InTime);

	if(BeforeNewIndex == InTrack.Records.Num() - 1)
	{
		InTrack.Records.AddDefaulted();
		return InTrack.Records.Last();
	}

	const int32 NewRecordIndex = InTrack.Records.Insert(FRecordedFrame(), BeforeNewIndex + 1);
	return InTrack.Records[NewRecordIndex];
}

FGeometryCollectionPhysicsProxy::FGeometryCollectionPhysicsProxy(FGeometryCollection* InDynamicCollection, FInitFunc InInitFunc, FCacheSyncFunc InCacheSyncFunc, FFinalSyncFunc InFinalSyncFunc)
	: SimulationCollection(nullptr)
	, GTDynamicCollection(InDynamicCollection)
	, Callbacks(nullptr)
	, InitFunc(InInitFunc)
	, CacheSyncFunc(InCacheSyncFunc)
	, FinalSyncFunc(InFinalSyncFunc)
	, LastSyncCountGT(MAX_uint32)
{
	check(IsInGameThread());
}

FSolverCallbacks* FGeometryCollectionPhysicsProxy::OnCreateCallbacks()
{
	check(IsInGameThread());

	if(Callbacks)
	{
		delete Callbacks;
		Callbacks = nullptr;
	}

	Callbacks = new FGeometryCollectionSolverCallbacks();

	SimulationCollection = new FGeometryCollection(*GTDynamicCollection);
	if(IsMultithreaded())
	{
		SimulationCollection->LocalizeAttribute("Transform", FGeometryCollection::TransformGroup);
	}

	Callbacks->SetUpdateRecordedStateFunction([this](float SolverTime, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles, const FSolverCallbacks::FCollisionConstraintsType& CollisionRule)
	{
		UpdateRecordedState(SolverTime, RigidBodyID, Hierarchy, Particles, CollisionRule);
	});

	Callbacks->SetCommitRecordedStateFunction([this](FRecordedTransformTrack& InTrack)
	{
		InTrack = FRecordedTransformTrack::ProcessRawRecordedData(RecordedTracks);
	});

	FSimulationParameters Params;
	
	// Back to engine for setup from components
	InitFunc(Params, FieldSystem);

	// Setup proxy parameters
	Params.DynamicCollection = SimulationCollection;
	Params.FieldSystem = FieldSystem.Num() > 0 ? &FieldSystem : nullptr;

	Callbacks->UpdateParameters(Params);
	Callbacks->Initialize();

	RecordedTracks.Records.Reset();

	// Setup double buffer data
	Results.Get(0).Transforms->Init(*SimulationCollection->Transform);
	Results.Get(0).RigidBodyIds->Init(Callbacks->GetRigidBodyIdArray());
	Results.Get(1).Transforms->Init(*SimulationCollection->Transform);
	Results.Get(1).RigidBodyIds->Init(Callbacks->GetRigidBodyIdArray());

	LastSyncCountGT = 0;

	return Callbacks;
}

void FGeometryCollectionPhysicsProxy::OnDestroyCallbacks(FSolverCallbacks* InCallbacks)
{
	check(InCallbacks == Callbacks);
	delete InCallbacks;
}

void FGeometryCollectionPhysicsProxy::UpdateRecordedState(float SolverTime, const TManagedArray<int32> & RigidBodyID, const TManagedArray<FGeometryCollectionBoneNode>& Hierarchy, const FSolverCallbacks::FParticlesType& Particles, const FSolverCallbacks::FCollisionConstraintsType& CollisionRule)
{
	FRecordedFrame* ExistingFrame = RecordedTracks.FindRecordedFrame(SolverTime);

	if (!ExistingFrame)
	{
		ExistingFrame = &InsertRecordedFrame(RecordedTracks, SolverTime);
	}

	ExistingFrame->Reset(RigidBodyID.Num());
	ExistingFrame->Timestamp = SolverTime;

	// Build CollisionData out of CollisionRule	
	FSimulationParameters Params = Callbacks->GetParameters();

	ExistingFrame->Collisions.Empty();
	if (Params.SaveCollisionData)
	{
		const TArray<Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint>& AllConstraintsArray = CollisionRule.GetAllConstraints();
		
		if (AllConstraintsArray.Num() > 0)
		{
			// Only process the constraints with AccumulatedImpulse != 0
			TArray<Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint> ConstraintsArray;
			FBox BoundingBox(ForceInitToZero);
			for (int32 Idx = 0; Idx < AllConstraintsArray.Num(); ++Idx)
			{
				if (!AllConstraintsArray[Idx].AccumulatedImpulse.IsZero() &&
					AllConstraintsArray[Idx].Phi < 0.f)
				{
					ensure(FMath::IsFinite(AllConstraintsArray[Idx].Location.X) &&
						   FMath::IsFinite(AllConstraintsArray[Idx].Location.Y) &&
						   FMath::IsFinite(AllConstraintsArray[Idx].Location.Z));
					ConstraintsArray.Add(AllConstraintsArray[Idx]);
					BoundingBox += AllConstraintsArray[Idx].Location;
				}
			}
		
			if (ConstraintsArray.Num() > 0 &&
				Params.SpatialHashRadius > 0.0 &&
				(BoundingBox.GetExtent().X > 0.0 || BoundingBox.GetExtent().Y > 0.0 || BoundingBox.GetExtent().Z > 0.0))

			{
				// Spatial hash the constraints
				TMultiMap<int32, int32> HashTableMap;
				Chaos::ComputeHashTable(ConstraintsArray, BoundingBox, HashTableMap, Params.SpatialHashRadius);

				TArray<int32> UsedCellsArray;
				HashTableMap.GetKeys(UsedCellsArray);

				int32 CollisionIndex = 0;
				for (int32 IdxCell = 0; IdxCell < UsedCellsArray.Num(); ++IdxCell)
				{
					if (CollisionIndex < Params.CollisionDataMaxSize)
					{
						TArray<int32> ConstraintsInCellArray;
						HashTableMap.MultiFind(UsedCellsArray[IdxCell], ConstraintsInCellArray);

						int32 NumConstraintsToGetFromCell = FMath::Min(Params.MaxCollisionPerCell, ConstraintsInCellArray.Num());

						for (int32 IdxConstraint = 0; IdxConstraint < NumConstraintsToGetFromCell; ++IdxConstraint)
						{
							const Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint& Constraint = ConstraintsArray[ConstraintsInCellArray[IdxConstraint]];
							FSolverCollisionData Collision{
								SolverTime,
								Constraint.Location,
								Constraint.AccumulatedImpulse,
								Constraint.Normal,
								Particles.V(Constraint.ParticleIndex), Particles.V(Constraint.LevelsetIndex),
								Particles.M(Constraint.ParticleIndex), Particles.M(Constraint.LevelsetIndex),
								Constraint.ParticleIndex, Constraint.LevelsetIndex
							};

							ExistingFrame->Collisions.Add(Collision);
							CollisionIndex++;
							if (CollisionIndex == Params.CollisionDataMaxSize)
							{
								break;
							}
						}
					}
					else
					{
						break;
					}
				}
			}
		}
	}

	if (Params.SaveTrailingData)
	{
		const float TrailingMinSpeedThresholdSquared = Params.TrailingMinSpeedThreshold * Params.TrailingMinSpeedThreshold;

		if (Particles.Size() > 0)
		{
			for (uint32 IdxParticle = 0; IdxParticle < Particles.Size(); ++IdxParticle)
			{
				if (ExistingFrame->Trailings.Num() >= Params.TrailingDataSizeMax)
				{
					break;
				}

				if (!Particles.Disabled(IdxParticle) &&
					!Particles.Sleeping(IdxParticle) &&
					Particles.InvM(IdxParticle) != 0.f)
				{
					if (Particles.Geometry(IdxParticle)->HasBoundingBox())
					{
						FVector Location = Particles.X(IdxParticle);
						FVector Velocity = Particles.V(IdxParticle);
						FVector AngularVelocity = Particles.W(IdxParticle);
						float Mass = Particles.M(IdxParticle);

						if (ensure(FMath::IsFinite(Location.X) &&
							FMath::IsFinite(Location.Y) &&
							FMath::IsFinite(Location.Z) &&
							FMath::IsFinite(Velocity.X) &&
							FMath::IsFinite(Velocity.Y) &&
							FMath::IsFinite(Velocity.Z) &&
							FMath::IsFinite(AngularVelocity.X) &&
							FMath::IsFinite(AngularVelocity.Y) &&
							FMath::IsFinite(AngularVelocity.Z)))
						{
							Chaos::TBox<float, 3> BoundingBox = Particles.Geometry(IdxParticle)->BoundingBox();
							FVector Extents = BoundingBox.Extents();
							float ExtentMax = Extents[BoundingBox.LargestAxis()];

							int32 SmallestAxis;
							if (Extents[0] < Extents[1] && Extents[0] < Extents[2])
							{
								SmallestAxis = 0;
							}
							else if (Extents[1] < Extents[2])
							{
								SmallestAxis = 1;
							}
							else
							{
								SmallestAxis = 2;
							}
							float ExtentMin = Extents[SmallestAxis];
							float Volume = Extents[0] * Extents[1] * Extents[2];
							float SpeedSquared = Velocity.SizeSquared();

							if (SpeedSquared > TrailingMinSpeedThresholdSquared &&
								Volume > Params.TrailingMinVolumeThreshold)
							{
								FSolverTrailingData Trailing{
									SolverTime,
									Location,
									ExtentMin,
									ExtentMax,
									Velocity,
									AngularVelocity,
									Mass,
									(int32)IdxParticle
								};
								ExistingFrame->Trailings.Add(Trailing);
							}
						}
					}
				}
			}
		}
	}

	ParallelFor(RigidBodyID.Num(), [&](int32 Index)
	{
		const int32 ExternalIndex = RigidBodyID[Index];

		if(ExternalIndex >= 0)
		{
			FTransform& NewTransform = ExistingFrame->Transforms[Index];

			NewTransform.SetTranslation(Particles.P(ExternalIndex));
			NewTransform.SetRotation(Particles.Q(ExternalIndex));
			NewTransform.SetScale3D(FVector(1.0f));
			ExistingFrame->DisabledFlags[Index] = Particles.Disabled(ExternalIndex);
		}
	});
}

void FGeometryCollectionPhysicsProxy::OnRemoveFromScene()
{
	// #BG TODO This isn't great - we currently cannot handle things being removed from the solver.
	// need to refactor how we handle this and actually remove the particles instead of just constantly
	// growing the array. Currently everything is just tracked by index though so the solver will have
	// to notify all the proxies that a chunk of data was removed - or use a sparse array (undesireable)
	Chaos::PBDRigidsSolver::FParticlesType& Particles = GetSolver()->GetRigidParticles();

	// #BG TODO Special case here because right now we reset/realloc the evolution per geom component
	// in endplay which clears this out. That needs to not happen and be based on world shutdown
	if(Particles.Size() == 0)
	{
		return;
	}

	const int32 Begin = Callbacks->GetBaseParticleIndex();
	const int32 Count = Callbacks->GetNumParticles();

	check((int32)Particles.Size() > 0 && (Begin + Count) <= (int32)Particles.Size());

	for(int32 ParticleIndex = 0; ParticleIndex < Count; ++ParticleIndex)
	{
		Particles.Disabled(Begin + ParticleIndex) = true;
	}

	// Rebuild internal particles from currently active particles to "remove" this proxy
	GetSolver()->InitializeFromParticleData();
}

void FGeometryCollectionPhysicsProxy::SyncBeforeDestroy()
{
	if(FinalSyncFunc)
	{
		FinalSyncFunc(RecordedTracks);
	}
}

void FGeometryCollectionPhysicsProxy::CacheResults()
{
	FGeometryCollectionResults& TargetResults = Results.GetPhysicsDataForWrite();

	TManagedArray<FTransform>& TransformCache = *TargetResults.Transforms;
	TManagedArray<int32>& IdCache = *TargetResults.RigidBodyIds;
	TManagedArray<FGeometryCollectionBoneNode>& BoneHierarchyCache = *TargetResults.BoneHierarchy;

	TransformCache.Init(*SimulationCollection->Transform);
	IdCache.Init(Callbacks->GetRigidBodyIdArray());
	BoneHierarchyCache.Init(*SimulationCollection->BoneHierarchy);
}

void FGeometryCollectionPhysicsProxy::FlipCache()
{
	Results.Flip();
}

void FGeometryCollectionPhysicsProxy::SyncToCache()
{
	uint32 LastSyncCountFromPhysics = Results.GetGameDataSyncCount();
	if(LastSyncCountFromPhysics != LastSyncCountGT)
	{
		LastSyncCountGT = LastSyncCountFromPhysics;

		FGeometryCollectionResults& TargetResult = Results.GetGameDataForWrite();
		TSharedPtr<TManagedArray<int32>> IdCachePtr = TargetResult.RigidBodyIds;

		TSharedPtr<TManagedArray<FTransform>> TempTransformPtr = GTDynamicCollection->Transform;
		GTDynamicCollection->Transform = TargetResult.Transforms;
		TargetResult.Transforms = TempTransformPtr;

		TSharedPtr<TManagedArray<FGeometryCollectionBoneNode>> TempBoneHierarchyPtr = GTDynamicCollection->BoneHierarchy;
		GTDynamicCollection->BoneHierarchy = TargetResult.BoneHierarchy;
		TargetResult.BoneHierarchy = TempBoneHierarchyPtr;
		
		GTDynamicCollection->MakeDirty();

		if(CacheSyncFunc)
		{
			CacheSyncFunc(*IdCachePtr);
		}
	}
}

#endif
