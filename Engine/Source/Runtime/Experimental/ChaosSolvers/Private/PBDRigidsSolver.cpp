// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#if INCLUDE_CHAOS

#include "PBDRigidsSolver.h"

#include "Chaos/Utilities.h"
#include "Chaos/PBDCollisionConstraintUtil.h"
#include "Async/AsyncWork.h"
#include "Misc/ScopeLock.h"
#include "ChaosStats.h"


DEFINE_LOG_CATEGORY_STATIC(LogPBDRigidsSolverSolver, Log, All);

namespace Chaos
{

	int8 PBDRigidsSolver::Invalid = -1;

	class AdvanceOneTimeStepTask : public FNonAbandonableTask
	{
		friend class FAutoDeleteAsyncTask<AdvanceOneTimeStepTask>;
	public:
		AdvanceOneTimeStepTask(
			PBDRigidsSolver* Scene,
			const float DeltaTime,
			TSharedPtr<FCriticalSection> PrevFrameLock,
			TSharedPtr<FEvent> PrevFrameEvent,
			TSharedPtr<FCriticalSection> CurrentFrameLock,
			TSharedPtr<FEvent> CurrentFrameEvent)
			: MScene(Scene)
			, MDeltaTime(DeltaTime)
			, PrevLock(PrevFrameLock)
			, CurrentLock(CurrentFrameLock)
			, PrevEvent(PrevFrameEvent)
			, CurrentEvent(CurrentFrameEvent)
		{
			UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("AdvanceOneTimeStepTask::AdvanceOneTimeStepTask()"));
			CurrentFrameLock->Lock();
		}

		void DoWork()
		{
			UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("AdvanceOneTimeStepTask::DoWork()"));

			while (PrevLock.IsValid() && !PrevLock->TryLock())
			{
				PrevEvent->Wait(1);
			}
			MScene->CreateRigidBodyCallback(MScene->MEvolution->Particles());
			MScene->ParameterUpdateCallback(MScene->MEvolution->Particles(), MScene->MTime);
			MScene->DisableCollisionsCallback(MScene->MEvolution->DisabledCollisions());
			
			{
				SCOPE_CYCLE_COUNTER(STAT_BeginFrame);
				MScene->StartFrameCallback(MDeltaTime, MScene->MTime);
			}

			while (MDeltaTime > MScene->MMaxDeltaTime)
			{
				MScene->ForceUpdateCallback(MScene->MEvolution->Particles(), MScene->MTime);
				MScene->MEvolution->ReconcileIslands();
				MScene->KinematicUpdateCallback(MScene->MEvolution->Particles(), MScene->MMaxDeltaTime, MScene->MTime);
				MScene->MEvolution->AdvanceOneTimeStep(MScene->MMaxDeltaTime);
				MDeltaTime -= MScene->MMaxDeltaTime;
			}
			MScene->ForceUpdateCallback(MScene->MEvolution->Particles(), MScene->MTime);
			MScene->MEvolution->ReconcileIslands();
			MScene->KinematicUpdateCallback(MScene->MEvolution->Particles(), MDeltaTime, MScene->MTime);
			MScene->MEvolution->AdvanceOneTimeStep(MDeltaTime);
			MScene->MTime += MDeltaTime;
			MScene->CurrentFrame++;

			{
				SCOPE_CYCLE_COUNTER(STAT_EndFrame);
				MScene->EndFrameCallback(MDeltaTime);
			}

			CurrentLock->Unlock();
			CurrentEvent->Trigger();
		}

	protected:

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(AdvanceOneTimeStepTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		PBDRigidsSolver* MScene;
		float MDeltaTime;
		TSharedPtr<FCriticalSection> PrevLock, CurrentLock;
		TSharedPtr<FEvent> PrevEvent, CurrentEvent;
	};
	
	PBDRigidsSolver::PBDRigidsSolver()
		: TimeStepMultiplier(1.f)
		, bEnabled(false)
		, bHasFloor(true)
		, bIsFloorAnalytic(false)
		, FloorHeight(0.f)
		, MaxCollisionDataSize(1024)
		, CollisionDataTimeWindow(0.1f)
		, DoCollisionDataSpatialHash(true)
		, CollisionDataSpatialHashRadius(15.f)
		, MaxCollisionPerCell(1)
		, MaxBreakingDataSize(1024)
		, BreakingDataTimeWindow(0.1f)
		, DoBreakingDataSpatialHash(true)
		, BreakingDataSpatialHashRadius(15.f)
		, MaxBreakingPerCell(1)
		, MaxTrailingDataSize(1024)
		, TrailingDataTimeWindow(0.1f)
		, TrailingMinSpeedThreshold(100.f)
		, TrailingMinVolumeThreshold(1000.f)
		, MCurrentEvent(nullptr)
		, MCurrentLock(nullptr)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::PBDRigidsSolver()"));
		Reset();
	}

	void PBDRigidsSolver::Reset()
	{

		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::Reset()"));

		MTime = 0;
		MLastDt = 0.0f;
		bEnabled = false;
		CurrentFrame = 0;
		MMaxDeltaTime = 1;
		FieldForceNum = 0;

		FParticlesType TRigidParticles;
		MEvolution = TUniquePtr<FPBDRigidsEvolution>(new FPBDRigidsEvolution(MoveTemp(TRigidParticles)));
		GetRigidParticles().AddArray(&FieldForce);

		MEvolution->AddPBDConstraintFunction([&](FParticlesType& Particle, const float Time, const int32 Island)
		{
			this->AddConstraintCallback(Particle, Time, Island);
		});
		MEvolution->AddForceFunction([&](FParticlesType& Particles, const float Time, const int32 Index)
		{
			this->AddForceCallback(Particles, Time, Index);
		});
		MEvolution->AddForceFunction([&](FParticlesType& Particles, const float Time, const int32 Index)
		{
			if (Index < FieldForceNum)
			{
				Particles.F(Index) += FieldForce[Index];
			}
		});
		MEvolution->SetCollisionContactsFunction([&](FParticlesType& Particles, FCollisionConstraintsType& CollisionConstraints)
		{
			this->CollisionContactsCallback(Particles, CollisionConstraints);
		});
		MEvolution->SetBreakingFunction([&](FParticlesType& Particles)
		{
			this->BreakingCallback(Particles);
		});
		MEvolution->SetTrailingFunction([&](FParticlesType& Particles)
		{
			this->TrailingCallback(Particles);
		});

		Callbacks.Reset();
		FieldCallbacks.Reset();
		KinematicProxies.Reset();
	}

	void PBDRigidsSolver::RegisterCallbacks(FSolverCallbacks* CallbacksIn)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::RegisterCallbacks()"));
		Callbacks.Add(CallbacksIn);
		KinematicProxies.Add(FKinematicProxy());
	}

	void PBDRigidsSolver::UnregisterCallbacks(FSolverCallbacks* CallbacksIn)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::UnregisterCallbacks()"));
		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			if (Callbacks[CallbackIndex] == CallbacksIn)
			{
				Callbacks[CallbackIndex] = nullptr;
			}
		}
	}

	void PBDRigidsSolver::RegisterFieldCallbacks(FSolverFieldCallbacks* CallbacksIn)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::RegisterFieldCallbacks()"));
		FieldCallbacks.Add(CallbacksIn);
	}

	void PBDRigidsSolver::UnregisterFieldCallbacks(FSolverFieldCallbacks* CallbacksIn)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::UnregisterFieldCallbacks()"));
		for (int CallbackIndex = 0; CallbackIndex < FieldCallbacks.Num(); CallbackIndex++)
		{
			if (FieldCallbacks[CallbackIndex] == CallbacksIn)
			{
				FieldCallbacks[CallbackIndex] = nullptr;
			}
		}
	}


	void PBDRigidsSolver::AdvanceSolverBy(float DeltaTime)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::Tick(%3.5f)"), DeltaTime);
		if (bEnabled)
		{
			MLastDt = DeltaTime;

			// @todo : This is kind of strange. can we expose the solver to the
			//         callbacks in a different way?
			for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
			{
				if (Callbacks[CallbackIndex] != nullptr)
				{
					Callbacks[CallbackIndex]->SetSolver(this);
				}
			}
			for (int CallbackIndex = 0; CallbackIndex < FieldCallbacks.Num(); CallbackIndex++)
			{
				if (FieldCallbacks[CallbackIndex] != nullptr)
				{
					FieldCallbacks[CallbackIndex]->SetSolver(this);
				}
			}

			int32 NumTimeSteps = (int32)(1.f*TimeStepMultiplier);
			float dt = FMath::Min(DeltaTime, float(5 / 30.)) / (float)NumTimeSteps;
			for (int i = 0; i < NumTimeSteps; i++)
			{
				TSharedPtr<FCriticalSection> NewFrameLock(new FCriticalSection());
				TSharedPtr<FEvent> NewFrameEvent(FPlatformProcess::CreateSynchEvent());
				//(new FAutoDeleteAsyncTask<AdvanceOneTimeStepTask>(this, DeltaTime, MCurrentLock, MCurrentEvent, NewFrameLock, NewFrameEvent))->StartBackgroundTask();
				AdvanceOneTimeStepTask(this, DeltaTime, MCurrentLock, MCurrentEvent, NewFrameLock, NewFrameEvent).DoWork();
				MCurrentLock = NewFrameLock;
				MCurrentEvent = NewFrameEvent;
			}
		}

	}


	void PBDRigidsSolver::CreateRigidBodyCallback(PBDRigidsSolver::FParticlesType& Particles)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::CreateRigidBodyCallback()"));
		int32 NumParticles = Particles.Size();
		if (!Particles.Size())
		{
			UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("... creating particles"));
			if (bHasFloor)
			{
				UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("... creating floor"));
				int Index = Particles.Size();
				Particles.AddParticles(1);
				Particles.X(Index) = Chaos::TVector<float, 3>(0.f, 0.f, FloorHeight);
				Particles.V(Index) = Chaos::TVector<float, 3>(0.f, 0.f, 0.f);
				Particles.R(Index) = Chaos::TRotation<float, 3>::MakeFromEuler(Chaos::TVector<float, 3>(0.f, 0.f, 0.f));
				Particles.W(Index) = Chaos::TVector<float, 3>(0.f, 0.f, 0.f);
				Particles.P(Index) = Particles.X(0);
				Particles.Q(Index) = Particles.R(0);
				Particles.M(Index) = 1.f;
				Particles.InvM(Index) = 0.f;
				Particles.I(Index) = Chaos::PMatrix<float, 3, 3>(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);
				Particles.InvI(Index) = Chaos::PMatrix<float, 3, 3>(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
				Particles.Geometry(Index) = new Chaos::TPlane<float, 3>(Chaos::TVector<float, 3>(0.f, 0.f, FloorHeight), Chaos::TVector<float, 3>(0.f, 0.f, 1.f));
				Particles.Geometry(Index)->IgnoreAnalyticCollisions(!bIsFloorAnalytic);
			}
		}

		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("... creating bodies from callbacks"));
			if (Callbacks[CallbackIndex]!=nullptr && Callbacks[CallbackIndex]->IsSimulating())
			{
				Callbacks[CallbackIndex]->CreateRigidBodyCallback(Particles);
			}
		}

		if (NumParticles != Particles.Size())
		{
			int Size = ParticleCallbackMapping.Num();
			ParticleCallbackMapping.Resize(Particles.Size());
			for (int Index = Size; Index < ParticleCallbackMapping.Num(); Index++)
			{
				ParticleCallbackMapping[Index] = Invalid;
			}

			for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
			{
				if (Callbacks[CallbackIndex] != nullptr)
				{
					Callbacks[CallbackIndex]->BindParticleCallbackMapping(CallbackIndex, ParticleCallbackMapping);
				}
			}

			InitializeFromParticleData();
		}
	}

	bool PBDRigidsSolver::Enabled() const
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::Enabled()"));
		if (bEnabled)
		{
			for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
			{
				if (Callbacks[CallbackIndex] != nullptr && Callbacks[CallbackIndex]->IsSimulating())
					return true;
			}
		}
		return false;
	}

	void PBDRigidsSolver::ParameterUpdateCallback(PBDRigidsSolver::FParticlesType& Particles, const float Time)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::ParameterUpdateCallback()"));

		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			if (Callbacks[CallbackIndex] != nullptr && Callbacks[CallbackIndex]->IsSimulating())
			{
				Callbacks[CallbackIndex]->ParameterUpdateCallback(Particles, Time);
			}
		}
	}

	void PBDRigidsSolver::ForceUpdateCallback(PBDRigidsSolver::FParticlesType& Particles, const float Time)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::ParameterUpdateCallback()"));

		// reset the FieldForces
		FieldForceNum = FieldForce.Num();
		for (int32 i = 0; i < FieldForce.Num(); i++)
		{
			FieldForce[i] = FVector(0);
		}

		for (int CallbackIndex = 0; CallbackIndex < FieldCallbacks.Num(); CallbackIndex++)
		{
			if (FieldCallbacks[CallbackIndex] != nullptr && FieldCallbacks[CallbackIndex]->IsSimulating())
			{
				FieldCallbacks[CallbackIndex]->CommandUpdateCallback(Particles, FieldForce, Time);
			}
		}
	}


	void PBDRigidsSolver::DisableCollisionsCallback(TSet<TTuple<int32, int32>>& CollisionPairs)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::DisableCollisionsCallback()"));
		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			if (Callbacks[CallbackIndex] != nullptr && Callbacks[CallbackIndex]->IsSimulating())
			{
				Callbacks[CallbackIndex]->DisableCollisionsCallback(CollisionPairs);
			}
		}
	}

	void PBDRigidsSolver::StartFrameCallback(const float Dt, const float Time)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::StartFrameCallback()"));
		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			if (Callbacks[CallbackIndex] != nullptr)
			{
				Callbacks[CallbackIndex]->StartFrameCallback(Dt, Time);
			}
			// @todo: This data should be pushed; not pulled
			if (Callbacks[CallbackIndex] != nullptr && Callbacks[CallbackIndex]->IsSimulating())
			{
				Callbacks[CallbackIndex]->UpdateKinematicBodiesCallback(MEvolution->Particles(), Dt, Time, KinematicProxies[CallbackIndex]);
			}
		}
	}

	void PBDRigidsSolver::EndFrameCallback(const float EndFrame)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::EndFrameCallback()"));
		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			if (Callbacks[CallbackIndex] != nullptr && Callbacks[CallbackIndex]->IsSimulating())
			{
				Callbacks[CallbackIndex]->EndFrameCallback(EndFrame);
			}
		}
	}

	void PBDRigidsSolver::KinematicUpdateCallback(PBDRigidsSolver::FParticlesType& Particles, const float Dt, const float Time)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::KinematicUpdateCallback()"));
		SCOPE_CYCLE_COUNTER(STAT_KinematicUpdate);

		PhysicsParallelFor(KinematicProxies.Num(), [&](int32 i)
		{
			FKinematicProxy& KinematicProxy = KinematicProxies[i];
			for (int32 ProxyIndex = 0; ProxyIndex < KinematicProxy.Ids.Num(); ++ProxyIndex)
			{
				const int32 Index = KinematicProxy.Ids[ProxyIndex];
				if (Index < 0 || Particles.InvM(Index) != 0 || Particles.Disabled(Index))
				{
					continue;
				}
				Particles.X(Index) = KinematicProxy.Position[ProxyIndex];
				Particles.R(Index) = KinematicProxy.Rotation[ProxyIndex];
				Particles.V(Index) = (KinematicProxy.NextPosition[ProxyIndex] - KinematicProxy.Position[ProxyIndex]) / Dt;
				TRotation<float, 3> Delta = KinematicProxy.NextRotation[ProxyIndex] * KinematicProxy.Rotation[ProxyIndex].Inverse();
				TVector<float, 3> Axis;
				float Angle;
				Delta.ToAxisAndAngle(Axis, Angle);
				Particles.W(Index) = Axis * Angle / Dt;
			}
		});
	}

	void PBDRigidsSolver::AddConstraintCallback(PBDRigidsSolver::FParticlesType& Particles, const float Time, const int32 Island)
	{
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::AddConstraintCallback()"));
		for (int CallbackIndex = 0; CallbackIndex < Callbacks.Num(); CallbackIndex++)
		{
			if (Callbacks[CallbackIndex] != nullptr && Callbacks[CallbackIndex]->IsSimulating())
			{
				Callbacks[CallbackIndex]->AddConstraintCallback(Particles, Time, Island);
			}
		}
	}

	void PBDRigidsSolver::AddForceCallback(PBDRigidsSolver::FParticlesType& Particles, const float Dt, const int32 Index)
	{
		// @todo : The index based callbacks need to change. This should be based on the indices
		//         managed by the specific Callback. 
		UE_LOG(LogPBDRigidsSolverSolver, Verbose, TEXT("PBDRigidsSolver::AddForceCallback()"));
		Chaos::PerParticleGravity<float, 3>(Chaos::TVector<float, 3>(0, 0, -1), 980.0).Apply(Particles, Dt, Index);
	}

	void PBDRigidsSolver::CollisionContactsCallback(PBDRigidsSolver::FParticlesType& Particles, PBDRigidsSolver::FCollisionConstraintsType& CollisionConstraints)
	{
		float CurrentTime = MTime;

		if (CurrentTime == 0.f)
		{
			CollisionData.TimeCreated = CurrentTime;
			CollisionData.NumCollisions = 0;
			CollisionData.CollisionDataArray.SetNum(MaxCollisionDataSize);
		}
		else
		{
			if (CurrentTime - CollisionData.TimeCreated > CollisionDataTimeWindow)
			{
				CollisionData.TimeCreated = CurrentTime;
				CollisionData.NumCollisions = 0;
				CollisionData.CollisionDataArray.Empty(MaxCollisionDataSize);
				CollisionData.CollisionDataArray.SetNum(MaxCollisionDataSize);
			}
		}

		const TArray<Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint>& AllConstraintsArray = CollisionConstraints.GetAllConstraints();
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
					if (ensure(FMath::IsFinite(AllConstraintsArray[Idx].Location.X) &&
							   FMath::IsFinite(AllConstraintsArray[Idx].Location.Y) &&
							   FMath::IsFinite(AllConstraintsArray[Idx].Location.Z)))
					{
						ConstraintsArray.Add(AllConstraintsArray[Idx]);
						BoundingBox += AllConstraintsArray[Idx].Location;
					}
				}
			}

			if (ConstraintsArray.Num() > 0)
			{			
				if (DoCollisionDataSpatialHash)
				{
					if (CollisionDataSpatialHashRadius > 0.0 &&
						(BoundingBox.GetExtent().X > 0.0 || BoundingBox.GetExtent().Y > 0.0 || BoundingBox.GetExtent().Z > 0.0))
					{
						// Spatial hash the constraints
						TMultiMap<int32, int32> HashTableMap;
						ComputeHashTable(ConstraintsArray, BoundingBox, HashTableMap, CollisionDataSpatialHashRadius);

						TArray<int32> UsedCellsArray;
						HashTableMap.GetKeys(UsedCellsArray);

						for (int32 IdxCell = 0; IdxCell < UsedCellsArray.Num(); ++IdxCell)
						{
							TArray<int32> ConstraintsInCellArray;
							HashTableMap.MultiFind(UsedCellsArray[IdxCell], ConstraintsInCellArray);

							int32 NumConstraintsToGetFromCell = FMath::Min(MaxCollisionPerCell, ConstraintsInCellArray.Num());

							for (int32 IdxConstraint = 0; IdxConstraint < NumConstraintsToGetFromCell; ++IdxConstraint)
							{
								const Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint& Constraint = ConstraintsArray[ConstraintsInCellArray[IdxConstraint]];
								TCollisionData<float, 3> CollisionDataItem{
									CurrentTime,
									Constraint.Location,
									Constraint.AccumulatedImpulse,
									Constraint.Normal,
									Particles.V(Constraint.ParticleIndex), Particles.V(Constraint.LevelsetIndex),
									Particles.M(Constraint.ParticleIndex), Particles.M(Constraint.LevelsetIndex),
									Constraint.ParticleIndex, Constraint.LevelsetIndex
								};

								int32 Idx = CollisionData.NumCollisions % MaxCollisionDataSize;
								CollisionData.CollisionDataArray[Idx] = CollisionDataItem;
								CollisionData.NumCollisions++;
							}
						}
					}
				}
				else
				{
					for (int32 IdxConstraint = 0; IdxConstraint < ConstraintsArray.Num(); ++IdxConstraint)
					{
						const Chaos::TPBDCollisionConstraint<float, 3>::FRigidBodyContactConstraint& Constraint = ConstraintsArray[IdxConstraint];
						TCollisionData<float, 3> CollisionDataItem{
							CurrentTime,
							Constraint.Location,
							Constraint.AccumulatedImpulse,
							Constraint.Normal,
							Particles.V(Constraint.ParticleIndex), Particles.V(Constraint.LevelsetIndex),
							Particles.M(Constraint.ParticleIndex), Particles.M(Constraint.LevelsetIndex),
							Constraint.ParticleIndex, Constraint.LevelsetIndex
						};

						int32 Idx = CollisionData.NumCollisions % MaxCollisionDataSize;
						CollisionData.CollisionDataArray[Idx] = CollisionDataItem;
						CollisionData.NumCollisions++;
					}
				}
			}
		}
	}

	void PBDRigidsSolver::BreakingCallback(PBDRigidsSolver::FParticlesType& Particles)
	{
	}

	void PBDRigidsSolver::TrailingCallback(PBDRigidsSolver::FParticlesType& Particles)
	{
		float CurrentTime = MTime;
		if (CurrentTime == 0.f)
		{
			TrailingData.TimeLastUpdated = 0.f;
			TrailingData.TrailingDataSet.Empty(MaxTrailingDataSize);
		}
		else
		{
			if (CurrentTime - TrailingData.TimeLastUpdated > TrailingDataTimeWindow)
			{
				TrailingData.TimeLastUpdated = CurrentTime;
			}
			else
			{
				return;
			}
		}

		const float TrailingMinSpeedThresholdSquared = TrailingMinSpeedThreshold * TrailingMinSpeedThreshold;

		if (Particles.Size() > 0)
		{
			// Remove Sleeping, Disabled or too slow particles from TrailingData.TrailingDataSet
			if (TrailingData.TrailingDataSet.Num() > 0)
			{
				FTrailingDataSet ParticlesToRemoveFromTrailingSet;
				for (auto& TrailingDataItem : TrailingData.TrailingDataSet)
				{
					int32 ParticleIndex = TrailingDataItem.ParticleIndex;
					if (Particles.Sleeping(ParticleIndex) ||
						Particles.Disabled(ParticleIndex) ||
						Particles.V(ParticleIndex).SizeSquared() < TrailingMinSpeedThresholdSquared)
					{
						ParticlesToRemoveFromTrailingSet.Add(TrailingDataItem);
					}
				}
				TrailingData.TrailingDataSet = TrailingData.TrailingDataSet.Difference(TrailingData.TrailingDataSet.Intersect(ParticlesToRemoveFromTrailingSet));
			}

			for (uint32 IdxParticle = 0; IdxParticle < Particles.Size(); ++IdxParticle)
			{
				if (TrailingData.TrailingDataSet.Num() >= MaxTrailingDataSize)
				{
					break;
				}

				if (!Particles.Disabled(IdxParticle) &&
					!Particles.Sleeping(IdxParticle) &&
					Particles.InvM(IdxParticle) != 0.f)
				{
					if (Particles.Geometry(IdxParticle)->HasBoundingBox())
					{
						TVector<float, 3> Location = Particles.X(IdxParticle);
						TVector<float, 3> Velocity = Particles.V(IdxParticle);
						TVector<float, 3> AngularVelocity = Particles.W(IdxParticle);
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
							TBox<float, 3> BoundingBox = Particles.Geometry(IdxParticle)->BoundingBox();
							TVector<float, 3> Extents = BoundingBox.Extents();
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
								Volume > TrailingMinVolumeThreshold)
							{
								TTrailingData<float, 3> TrailingDataItem{
									CurrentTime,
									Location,
									ExtentMin,
									ExtentMax,
									Velocity,
									AngularVelocity,
									Mass,
									(int32)IdxParticle
								};

								if (!TrailingData.TrailingDataSet.Contains(TrailingDataItem))
								{
									TrailingData.TrailingDataSet.Add(TrailingDataItem);
								}
								else
								{
									FSetElementId Id = TrailingData.TrailingDataSet.FindId(TrailingDataItem);
									TrailingData.TrailingDataSet[Id].Location = Location;
									TrailingData.TrailingDataSet[Id].Velocity = Velocity;
									TrailingData.TrailingDataSet[Id].AngularVelocity = AngularVelocity;
								}
							}
						}
					}
				}
			}
		}
	}
}

#endif
