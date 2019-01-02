// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDRigidClustering.h"

#include "ChaosStats.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Levelset.h"
#include "Chaos/MassProperties.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "Chaos/PBDRigidsEvolutionPGS.h"
#include "Chaos/Sphere.h"
#include "Chaos/UniformGrid.h"
#include "ProfilingDebugging/ScopedTimers.h"

using namespace Chaos;

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::TPBDRigidClustering(FPBDRigidsEvolution& InEvolution, TPBDRigidParticles<T, d>& InParticles)
    : MEvolution(InEvolution), MParticles(InParticles)
{
	MParticles.AddArray(&MClusterIds);
	MParticles.AddArray(&MInternalCluster);
	MParticles.AddArray(&MStrains);
	MParticles.AddArray(&MConnectivityEdges);
	MParticles.AddArray(&MChildToParent);
}

template<class T, int d>
T CalculatePseudoMomentum(const TPBDRigidParticles<T, d>& InParticles, const uint32 Index)
{
	TVector<T, d> LinearPseudoMomentum = (InParticles.X(Index) - InParticles.P(Index)) * InParticles.M(Index);
	TRotation<T, d> Delta = InParticles.R(Index) * InParticles.Q(Index).Inverse();
	TVector<T, d> Axis;
	T Angle;
	Delta.ToAxisAndAngle(Axis, Angle);
	TVector<T, d> AngularPseudoMomentum = InParticles.I(Index) * (Axis * Angle);
	return LinearPseudoMomentum.Size() + AngularPseudoMomentum.Size();
}

int32 RewindOnDecluster = 1;
FAutoConsoleVariableRef CVarRewindOnDecluster(TEXT("p.RewindOnDecluster"), RewindOnDecluster, TEXT("Whether to rewind collision when decluster occurs"));

int32 RewindOnDeclusterSolve = 1;
FAutoConsoleVariableRef CVarRewindOnDeclusterSolve(TEXT("p.RewindOnDeclusterSolve"), RewindOnDeclusterSolve, TEXT("Whether to rewind collision when decluster occurs"));

template<class T, int d>
void RewindAndEvolve(TPBDRigidsEvolutionGBF<T, d>& Evolution, TPBDRigidParticles<T, d>& InParticles,
	const TMap<uint32, TSet<uint32>>& ModifiedParents, const TSet<int32>& IslandsToRecollide, const TSet<uint32>& AllActivatedChildren, const T Dt, TPBDCollisionConstraint<T, d>& CollisionRule)
{
	// Rewind active particles
	const TArray<TSet<int32>>& MIslandParticles = Evolution.IslandParticles();
	const TArray<int32> IslandsToRecollideArray = IslandsToRecollide.Array();
	PhysicsParallelFor(IslandsToRecollideArray.Num(), [&](int32 Idx) {
		int32 Island = IslandsToRecollideArray[Idx];
		TArray<int32> ParticleIndices = MIslandParticles[Island].Array();
		for (int32 ArrayIdx = ParticleIndices.Num()-1; ArrayIdx >= 0; --ArrayIdx)
		{
			int32 Index = ParticleIndices[ArrayIdx];
			if (InParticles.Sleeping(Index) || InParticles.Disabled(Index))
			{
				ParticleIndices.RemoveAtSwap(ArrayIdx);
			}
			else
			{
				InParticles.P(Index) = InParticles.X(Index);
				InParticles.Q(Index) = InParticles.R(Index);
				InParticles.V(Index) = InParticles.PreV(Index);
				InParticles.W(Index) = InParticles.PreW(Index);
			}
		}
		Evolution.Integrate(ParticleIndices, Dt);
	});

	if (RewindOnDeclusterSolve)
	{
		// Update collision constraints based on newly activate children
		TArray<uint32> ModifiedParentsArray;
		ModifiedParents.GetKeys(ModifiedParentsArray);
		CollisionRule.RemoveConstraints(TSet<uint32>(ModifiedParentsArray));

		TSet<uint32> AllIslandParticles;
		for (const TSet<int32>& Island : MIslandParticles)
		{
			TArray<int32> ParticleIndices = Island.Array();
			for (const int32 Index : ParticleIndices)
			{
				bool bDisabled = InParticles.Disabled(Index);
				
				// #TODO - Have to repeat checking out whether the particle is disabled matching the PFor above.
				// Move these into shared array so we only process it once
				if (!AllIslandParticles.Contains(Index) && !bDisabled)
				{
					AllIslandParticles.Add(Index);
				}
			}
		}
		// @todo(mlentine): We can precompute internal constraints which can filter some from the narrow phase tests but may not help much
		CollisionRule.UpdateConstraints(InParticles, Dt, AllActivatedChildren, AllIslandParticles.Array());
		// Resolve collisions
		PhysicsParallelFor(IslandsToRecollide.Num(), [&](int32 Island) {
			TArray<int32> ActiveIndices = MIslandParticles[Island].Array();
			// @todo(mlentine): This is heavy handed and probably can be simplified as we know only a little bit changed.
			CollisionRule.UpdateAccelerationStructures(InParticles, ActiveIndices, Island);
			CollisionRule.Apply(InParticles, Dt, Island);
			CollisionRule.ApplyPushOut(InParticles, Dt, ActiveIndices, Island);
			//CollisionRule.SleepInactive(InParticles, ActiveIndices, MActiveIndices, Island);	//todo(ocohen): need to actually run this on evolution side probably
		});
	}
}
 
template<class T, int d>
void RewindAndEvolve(TPBDRigidsEvolutionPGS<T, d>& Evolution, TPBDRigidParticles<T, d>& InParticles,
	const TMap<uint32, TSet<uint32>>& ModifiedParents, const TSet<int32>& IslandsToRecollide, const TSet<uint32>& AllActivatedChildren, const T Dt, TPBDCollisionConstraintPGS<T, d>& CollisionRule)
{
	// Rewind active particles
	TArray<TSet<int32>>& MIslandParticles = Evolution.IslandParticles();
	PhysicsParallelFor(IslandsToRecollide.Num(), [&](int32 Island) {
		TArray<int32> ParticleIndices = MIslandParticles[Island].Array();
		for (int32 ArrayIdx = ParticleIndices.Num()-1; ArrayIdx >= 0; --ArrayIdx)
		{
			int32 Index = ParticleIndices[ArrayIdx];
			if (InParticles.Sleeping(Index) || InParticles.Disabled(Index))
			{
				ParticleIndices.RemoveAtSwap(ArrayIdx);
			}
			else
			{
				InParticles.P(Index) = InParticles.X(Index);
				InParticles.Q(Index) = InParticles.R(Index);
				InParticles.V(Index) = InParticles.PreV(Index);
				InParticles.W(Index) = InParticles.PreW(Index);
			}
		}
		Evolution.IntegrateV(ParticleIndices, Dt);
	});

	// Update collision constraints based on newly activate children
	TArray<uint32> ModifiedParentsArray;
	ModifiedParents.GetKeys(ModifiedParentsArray);
	CollisionRule.RemoveConstraints(TSet<uint32>(ModifiedParentsArray));
		
	TSet<uint32> AllIslandParticles;
	for (const auto& Island : IslandsToRecollide)
	{
		for (const auto& Index : MIslandParticles[Island])
		{
			if (InParticles.Disabled(Index) == false)	//HACK: cluster code is incorrectly adding disabled children
			{
				if (!AllIslandParticles.Contains(Index))
				{
					AllIslandParticles.Add(Index);
				}
			}
			else
			{
				//FPlatformMisc::DebugBreak();
			}
		}
	}
	// @todo(mlentine): We can precompute internal constraints which can filter some from the narrow phase tests but may not help much
	CollisionRule.UpdateConstraints(InParticles, Dt, AllActivatedChildren, AllIslandParticles.Array());
	PhysicsParallelFor(MIslandParticles.Num(), [&](int32 Island) {
		CollisionRule.Apply(InParticles, Dt, Island);
	});
	PhysicsParallelFor(MIslandParticles.Num(), [&](int32 Island) {
		TArray<int32> ParticleIndices = MIslandParticles[Island].Array();
		Evolution.IntegrateX(ParticleIndices, Dt);
	});

	// @todo(mlentine): Need to enforce constraints
	PhysicsParallelFor(MIslandParticles.Num(), [&](int32 Island) {
		TArray<int32> ActiveIndices = MIslandParticles[Island].Array();
		CollisionRule.ApplyPushOut(InParticles, Dt, ActiveIndices, Island);
	});
}

DECLARE_CYCLE_STAT(TEXT("AdvanceClustering"), STAT_AdvanceClustering, STATGROUP_Chaos);
template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
void TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::AdvanceClustering(const T Dt, FPBDCollisionConstraint& CollisionRule)
{
	SCOPE_CYCLE_COUNTER(STAT_AdvanceClustering);
	UE_LOG(LogChaos, Verbose, TEXT("START FRAME with Dt %f"), Dt);

	double FrameTime = 0, Time = 0;
	FDurationTimer Timer(Time);

	// Break clusters as a result of collisions
	Time = 0;
	Timer.Start();
	TMap<uint32, TSet<uint32> > ModifiedParents;
	TSet<uint32> AllActivatedChildren;
	TSet<int32> IslandsToRecollide;
	TArray<uint32> ClusterParentArray;
	MParentToChildren.GenerateKeyArray(ClusterParentArray);

	TMap<uint32, T> TotalStrainMap = ComputeStrainFromCollision(CollisionRule);

	// Find all clusters to be broken and islands to be rewound
	for (const auto& ParentIndex : ClusterParentArray)
	{
		// #BG TODO - should we advance clustering for kinematics?
		if(MParticles.Sleeping(ParentIndex) || MParticles.Disabled(ParentIndex) || MParticles.InvM(ParentIndex) == 0.0)
		{
			continue;
		}

		// global strain based releasing 
		int32 Island = MParticles.Island(ParentIndex);
		TSet<uint32> ActivatedChildren = ModifyClusterParticle(ParentIndex, TotalStrainMap);
		if (ActivatedChildren.Num())
		{
			if (!IslandsToRecollide.Contains(Island))
			{
				IslandsToRecollide.Add(Island);
			}
			ModifiedParents.Add(ParentIndex, ActivatedChildren);
			AllActivatedChildren.Append(ActivatedChildren);
		}
	}

	if (!ModifiedParents.Num())
	{
		return;
	}

	if (RewindOnDecluster)
	{
		RewindAndEvolve(MEvolution, MParticles, ModifiedParents, IslandsToRecollide, AllActivatedChildren, Dt, CollisionRule);
	}


	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("Cluster Break Update Time is %f"), Time);
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
int32 TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::CreateClusterParticle(const TArray<uint32>& Children)
{
	int32 NewIndex = MParticles.Size();
	MParticles.AddParticles(1);
	MInternalCluster[NewIndex] = false;

	//
	// Update clustering data structures.
	//

	bool bClusterIsAsleep = true;
	TSet<int32>& MActiveIndices = MEvolution.ActiveIndices();
	if (MActiveIndices.Num())
	{
		MActiveIndices.Add(NewIndex);
	}

	for (const auto Child : Children)
	{
		MActiveIndices.Remove(Child);
		bClusterIsAsleep &= MParticles.Sleeping(Child);
	}

	if (MParentToChildren.Contains(NewIndex))
	{
		MParentToChildren[NewIndex] = Children;
	}
	else
	{
		MParentToChildren.Add(NewIndex, Children);
	}

	MParticles.Disabled(NewIndex) = false;
	MActiveIndices.Add(NewIndex);
	for (const auto& Child : MParentToChildren[NewIndex])
	{
		MActiveIndices.Remove(Child);
		MParticles.Disabled(Child) = true;
	}

	UpdateMassProperties(Children, NewIndex);
	UpdateGeometry(Children, NewIndex);

	UpdateIslandParticles(NewIndex);

	UpdateConnectivityGraph(NewIndex);

	MParticles.SetSleeping(NewIndex, bClusterIsAsleep);

	return NewIndex;
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
int32 TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::CreateClusterParticleFromClusterChildren(const TArray<uint32>& Children, uint32 Island, const TRigidTransform<T,d>& ClusterWorldTM)
{
	//This cluster is made up of children that are currently in a cluster. This means we don't need to update or disable as much
	int32 NewIndex = MParticles.Size();
	MParticles.AddParticles(1);
	MInternalCluster[NewIndex] = true;

	if (MParentToChildren.Contains(NewIndex))
	{
		MParentToChildren[NewIndex] = Children;
	}
	else
	{
		MParentToChildren.Add(NewIndex, Children);
	}

	MParticles.Disabled(NewIndex) = false;
	TSet<int32>& ActiveIndices = MEvolution.ActiveIndices();
	ActiveIndices.Add(NewIndex);

	//child transforms are out of date, need to update them. @todo(ocohen): if children transforms are relative we would not need to update this, but would simply have to do a final transform on the new cluster index
	for (uint32 Child : Children)
	{
		TRigidTransform<T, d> ChildFrame = MChildToParent[Child] * ClusterWorldTM;
		MParticles.X(Child) = ChildFrame.GetTranslation();
		MParticles.R(Child) = ChildFrame.GetRotation();
	}

	UpdateMassProperties(Children, NewIndex);
	UpdateGeometry(Children, NewIndex);
	UpdateIslandParticles(NewIndex);
	

	return NewIndex;
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
void TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::UpdateMassProperties(const TArray<uint32>& Children, const uint32 NewIndex)
{
	// Kinematic Clusters
	bool HasInfiniteMass = false;
	for (const auto Child : Children)
	{
		if (MParticles.InvM(Child) == 0)
		{
			MParticles.X(NewIndex) = MParticles.X(Child);
			MParticles.R(NewIndex) = MParticles.R(Child);
			MParticles.P(NewIndex) = MParticles.P(Child);
			MParticles.Q(NewIndex) = MParticles.Q(Child);
			MParticles.V(NewIndex) = MParticles.V(Child);
			MParticles.W(NewIndex) = MParticles.W(Child);
			MParticles.M(NewIndex) = MParticles.M(Child);
			MParticles.I(NewIndex) = MParticles.I(Child);
			MParticles.InvM(NewIndex) = 0;
			MParticles.InvI(NewIndex) = PMatrix<T, d, d>(0);
			HasInfiniteMass = true;
			break;
		}
	}

	// Dynamic Clusters	
	if (!HasInfiniteMass)
	{
		MParticles.X(NewIndex) = TVector<T, d>(0);
		MParticles.R(NewIndex) = TRotation<T, d>(FQuat::MakeFromEuler(TVector<T, d>(0)));
		MParticles.V(NewIndex) = TVector<T, d>(0);
		MParticles.W(NewIndex) = TVector<T, d>(0);
		MParticles.M(NewIndex) = 0;
		MParticles.I(NewIndex) = PMatrix<T, d, d>(0);
		for (const auto Child : Children)
		{
			const auto& ChildMass = MParticles.M(Child);
			MParticles.M(NewIndex) += ChildMass;
			MParticles.I(NewIndex) += MParticles.I(Child);
			MParticles.X(NewIndex) += MParticles.X(Child) * ChildMass;
			MParticles.V(NewIndex) += MParticles.V(Child) * ChildMass;
			MParticles.W(NewIndex) += MParticles.W(Child) * ChildMass;
		}
		MParticles.X(NewIndex) /= MParticles.M(NewIndex);
		MParticles.V(NewIndex) /= MParticles.M(NewIndex);
		MParticles.PreV(NewIndex) = MParticles.V(NewIndex);
		MParticles.InvM(NewIndex) = 1 / MParticles.M(NewIndex);
		MParticles.P(NewIndex) = MParticles.X(NewIndex);
		for (const auto Child : Children)
		{
			TVector<T, d> ParentToChild = MParticles.X(Child) - MParticles.X(NewIndex);
			const auto& ChildMass = MParticles.M(Child);
			MParticles.W(NewIndex) += TVector<T, d>::CrossProduct(ParentToChild, MParticles.V(Child) * ChildMass);
			{
				const T& p0 = ParentToChild[0];
				const T& p1 = ParentToChild[1];
				const T& p2 = ParentToChild[2];
				const T& m = MParticles.M(Child);
				MParticles.I(NewIndex) += PMatrix<T, d, d>(m * (p1 * p1 + p2 * p2), -m * p1 * p0, -m * p2 * p0, m * (p2 * p2 + p0 * p0), -m * p2 * p1, m * (p1 * p1 + p0 * p0));
			}
		}
		MParticles.W(NewIndex) /= MParticles.M(NewIndex);
		MParticles.PreW(NewIndex) = MParticles.W(NewIndex);
		MParticles.R(NewIndex) = Chaos::TransformToLocalSpace<T, d>(MParticles.I(NewIndex));
		MParticles.Q(NewIndex) = MParticles.R(NewIndex);
		MParticles.InvI(NewIndex) = MParticles.I(NewIndex).Inverse();
	}
}

int32 MinLevelsetDimension = 4;
FAutoConsoleVariableRef CVarMinLevelsetDimension(TEXT("p.MinLevelsetDimension"), MinLevelsetDimension, TEXT("The minimum number of cells on a single level set axis"));

int32 MaxLevelsetDimension = 20;
FAutoConsoleVariableRef CVarMaxLevelsetDimension(TEXT("p.MaxLevelsetDimension"), MaxLevelsetDimension, TEXT("The maximum number of cells on a single level set axis"));

float MinLevelsetSize = 50.f;
FAutoConsoleVariableRef CVarLevelSetResolution(TEXT("p.MinLevelsetSize"), MinLevelsetSize, TEXT("The minimum size on the smallest axis to use a level set"));

int32 UseLevelsetCollision = 0;
FAutoConsoleVariableRef CVarUseLevelsetCollision(TEXT("p.UseLevelsetCollision"), UseLevelsetCollision, TEXT("Whether unioned objects use levelsets"));

int32 LevelsetGhostCells = 1;
FAutoConsoleVariableRef CVarLevelsetGhostCells(TEXT("p.LevelsetGhostCells"), LevelsetGhostCells, TEXT("Increase the level set grid by this many ghost cells"));

float ClusterSnapDistance = 1.f;
FAutoConsoleVariableRef CVarClusterSnapDistance(TEXT("p.ClusterSnapDistance"), ClusterSnapDistance, TEXT(""));

int32 MinCleanedPointsBeforeRemovingInternals = 10;
FAutoConsoleVariableRef CVarMinCleanedPointsBeforeRemovingInternals(TEXT("p.MinCleanedPointsBeforeRemovingInternals"), MinCleanedPointsBeforeRemovingInternals, TEXT("If we only have this many clean points, don't bother removing internal points as the object is likely very small"));

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
void TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::UpdateGeometry(const TArray<uint32>& Children, const uint32 NewIndex)
{
	TArray<TUniquePtr<TImplicitObject<T, d>>> Objects;

	TArray<TVector<T, d>> OriginalPoints;
	bool bUseParticleImplicit = false;

	for (const uint32 Child : Children)
	{
		TRigidTransform<T, d> Frame = TRigidTransform<T, d>(MParticles.X(Child), MParticles.R(Child)).GetRelativeTransform(TRigidTransform<T, d>(MParticles.X(NewIndex), MParticles.R(NewIndex)));
		Objects.Add(TUniquePtr<TImplicitObject<T, d>>(new TImplicitObjectTransformed<T, d>(MParticles.Geometry(Child), Frame)));
		MParticles.Disabled(Child) = true;
		MEvolution.ActiveIndices().Remove(Child);

		MClusterIds[Child] = ClusterId(NewIndex);

		MChildToParent[Child]=Frame;

		if (MParticles.CollisionParticles(Child))
		{
			for (uint32 i = 0; i < MParticles.CollisionParticles(Child)->Size(); ++i)
			{
				OriginalPoints.Add(Frame.TransformPosition(MParticles.CollisionParticles(Child)->X(i)));
			}
		}

		if (MParticles.Geometry(Child)->GetType() == ImplicitObjectType::Unknown)
		{
			bUseParticleImplicit = true;
		}
	}

	TArray<TVector<T, d>> CleanedPoints = CleanCollisionParticles(OriginalPoints, ClusterSnapDistance);

	if (MParticles.Geometry(NewIndex))
	{
		delete MParticles.Geometry(NewIndex);
	}

	if (UseLevelsetCollision)
	{
		TImplicitObjectUnion<T, d> UnionObject(MoveTemp(Objects));
		TBox<T, d> Bounds = UnionObject.BoundingBox();
		const TVector<T, d> BoundsExtents = Bounds.Extents();
		if (BoundsExtents.Min() >= MinLevelsetSize)	//make sure the object is not too small
		{
			TVector<int32, d> NumCells = Bounds.Extents() / MinLevelsetSize;
			for (int i = 0; i < d; ++i)
			{
				NumCells[i] = FMath::Clamp(NumCells[i], MinLevelsetDimension, MaxLevelsetDimension);
			}

			TUniformGrid<T, 3> Grid(Bounds.Min(), Bounds.Max(), NumCells, LevelsetGhostCells);
			TLevelSet<T,3>* LevelSet = new TLevelSet<T, 3>(Grid, UnionObject);

			const T MinDepthToSurface = Grid.Dx().Max();
			for (int32 Idx = CleanedPoints.Num() - 1; Idx >= 0; --Idx)
			{
				if (CleanedPoints.Num() > MinCleanedPointsBeforeRemovingInternals)	//todo(ocohen): this whole thing should really be refactored
				{
					const TVector<T, d>& CleanedCollision = CleanedPoints[Idx];
					if (LevelSet->SignedDistance(CleanedCollision) < -MinDepthToSurface)
					{
						CleanedPoints.RemoveAtSwap(Idx);
					}
				}
			}

			MParticles.Geometry(NewIndex) = LevelSet;
		}
		else
		{
			MParticles.Geometry(NewIndex) = new TSphere<T, d>(TVector<T,d>(0), BoundsExtents.Size() * 0.5f);
		}

		
	}
	else
	{
		MParticles.Geometry(NewIndex) = new TImplicitObjectUnion<T, d>(MoveTemp(Objects));
	}

	if (bUseParticleImplicit)
	{
		MParticles.Geometry(NewIndex)->IgnoreAnalyticCollisions();
	}

	MParticles.CollisionParticlesInitIfNeeded(NewIndex);
	MParticles.CollisionParticles(NewIndex)->Resize(0);
	MParticles.CollisionParticles(NewIndex)->AddParticles(CleanedPoints.Num());
	for (int32 i = 0; i < CleanedPoints.Num(); ++i)
	{
		MParticles.CollisionParticles(NewIndex)->X(i) = CleanedPoints[i];
	}

	MParticles.CollisionParticles(NewIndex)->UpdateAccelerationStructures();
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
void TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::UpdateIslandParticles(const uint32 ClusterIndex)
{
	TArray<TSet<int32>>& MIslandParticles = MEvolution.IslandParticles();

	int32 Island = MParticles.Island(MParentToChildren[ClusterIndex][0]);
	if (0 <= Island && Island < MIslandParticles.Num())
	{
		MIslandParticles[Island].Add(ClusterIndex);
		for (const auto& Child : MParentToChildren[ClusterIndex])
		{
			MIslandParticles[Island].Remove(Child);
		}
	}
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
void TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::UpdateConnectivityGraph(uint32 ClusterIndex)
{
	//for now we just do a simple bounds test between all the children in the cluster
	const TArray<uint32>& Children = MParentToChildren[ClusterIndex];
	for (int32 i = 0; i < Children.Num(); ++i)
	{
		const uint32 Child1 = Children[i];
		if (!MParticles.Geometry(Child1)->HasBoundingBox()) { continue; }
		TBox<T, d> Box1 = MParticles.Geometry(Child1)->BoundingBox();
		Box1.Thicken(1.f);

		const TRigidTransform<T, d>& TM1 = MChildToParent[Child1];

		for (int32 j = i + 1; j < Children.Num(); ++j)
		{
			const uint32 Child2 = Children[j];
			if (!MParticles.Geometry(Child2)->HasBoundingBox()) { continue; }
			const TRigidTransform<T, d>& TM2 = MChildToParent[Child2];
			const TBox<T, d> Box2Local = MParticles.Geometry(Child2)->BoundingBox().TransformedBox(TM1.GetRelativeTransformReverse(TM2));
			if(Box2Local.Intersects(Box1))
			{
				const T AvgStrain = (MStrains[Child1] + MStrains[Child2]) * (T)0.5f;
				MConnectivityEdges[Child1].Add({ Child2,AvgStrain });
				MConnectivityEdges[Child2].Add({ Child1,AvgStrain });
				break;
			}
		}
	}

#if 0
	if (Children.Num() > 1)
	{
		for (uint32 Child : Children)
		{
			//ensureMsgf(MConnectivityEdges[Child].Num() == 0, TEXT("Could not generate connectivity for particle %d"), Child);
		}
	}
#endif
}

float ClusterDistanceThreshold = 100.f;	//todo(ocohen): this computation is pretty rough and wrong
FAutoConsoleVariableRef CVarClusterDistance(TEXT("p.ClusterDistanceThreshold"), ClusterDistanceThreshold, TEXT("How close a cluster child must be to a contact to break off"));

int32 UseConnectivity = 1;
FAutoConsoleVariableRef CVarUseConnectivity(TEXT("p.UseConnectivity"), UseConnectivity, TEXT("Whether to use connectivity graph when breaking up clusters"));

int32 ChildrenInheritVelocity = 1;
FAutoConsoleVariableRef CVarChildrenInheritVelocity(TEXT("p.ChildrenInheritVelocity"), ChildrenInheritVelocity, TEXT("Whether children inherit parent velocity when declustering"));

DECLARE_CYCLE_STAT(TEXT("ModifyClusterParticle"), STAT_ModifyClusterParticle, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("ComputeStrainFromCollision"), STAT_ComputeStrainFromCollision, STATGROUP_Chaos);

template<class T, int d>
TVector<T, d> GetContactLocation(const TRigidBodyContactConstraint<T, d>& Contact)
{
	return Contact.Location;
}

template<class T, int d>
TVector<T, d> GetContactLocation(const TRigidBodyContactConstraintPGS<T, d>& Contact)
{
	// @todo(mlentine): Does the exact point matter?
	T MinPhi = FLT_MAX;
	TVector<T, d> MinLoc;
	for (int32 i = 0; i < Contact.Phi.Num(); ++i)
	{
		if (Contact.Phi[i] < MinPhi)
		{
			MinPhi = Contact.Phi[i];
			MinLoc = Contact.Location[i];
		}
	}
	return MinLoc;
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
TMap<uint32, T> TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::ComputeStrainFromCollision(const FPBDCollisionConstraint& CollisionRule) const
{
	SCOPE_CYCLE_COUNTER(STAT_ComputeStrainFromCollision);
	TMap<uint32, T> Strains;

	for (const typename FPBDCollisionConstraint::FRigidBodyContactConstraint& Contact : CollisionRule.GetAllConstraints())
	{
		if (Contact.AccumulatedImpulse.Size() < KINDA_SMALL_NUMBER) { continue; }

		auto ComputeStrainLambda = [&](uint32 ClusterIndex)
		{
			const TRigidTransform<T, d> WorldToClusterTM = TRigidTransform<T, d>(MParticles.P(ClusterIndex), MParticles.Q(ClusterIndex));
			const TVector<T, d> ContactLocationClusterLocal = WorldToClusterTM.InverseTransformPosition(GetContactLocation(Contact));
			const TArray<uint32>& Children = MParentToChildren[ClusterIndex];
			TBox<T, d> ContactBox(ContactLocationClusterLocal, ContactLocationClusterLocal);
			ContactBox.Thicken(ClusterDistanceThreshold);

			for (uint32 Child : Children)
			{
				const TBox<T, d> ChildBox = MParticles.Geometry(Child)->BoundingBox().TransformedBox(MChildToParent[Child]);
				if (ChildBox.Intersects(ContactBox))
				{
					T* Strain = Strains.Find(Child);
					if (Strain == nullptr)
					{
						Strains.Add(Child) = Contact.AccumulatedImpulse.Size();
					}
					else
					{
						*Strain += Contact.AccumulatedImpulse.Size();
					}
				}
			}
		};

		if (MParentToChildren.Contains(Contact.ParticleIndex))
		{
			ComputeStrainLambda(Contact.ParticleIndex);
		}

		if (MParentToChildren.Contains(Contact.LevelsetIndex))
		{
			ComputeStrainLambda(Contact.LevelsetIndex);
		}
	}

	return Strains;
}

template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
TSet<uint32> 
TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::DeactivateClusterParticle(const uint32 ClusterIndex)
{
	TSet<uint32> ActivatedChildren;
	if (0 <= ClusterIndex && ClusterIndex < (uint32)MClusterIds.Num())
	{
		TSet<int32>& ActiveIndices = MEvolution.ActiveIndices();
		TArray<TSet<int32>>& IslandParticles = MEvolution.IslandParticles();
		const int32 Island = MParticles.Island(ClusterIndex);

		check(!MParticles.Disabled(ClusterIndex));
		if (MParentToChildren.Contains(ClusterIndex))
		{
			int32 TotalNumParticles = MParticles.Size();
			const bool SleepState = MParticles.Sleeping(ClusterIndex);
			const TRigidTransform<T, d> PreSolveTM = TRigidTransform<T, d>(MParticles.X(ClusterIndex), MParticles.R(ClusterIndex));

			ActivatedChildren = TSet<uint32>(MParentToChildren[ClusterIndex]);
			for (uint32 Child : MParentToChildren[ClusterIndex])
			{
				check(0 <= Child && Child < (uint32)TotalNumParticles);
				MParticles.Island(Child) = Island;
				if (IslandParticles.IsValidIndex(Island))
				{
					IslandParticles[Island].Add(Child);
				}

				MParticles.Disabled(Child) = false;
				MParticles.SetSleeping(Child, SleepState);
				if ( /*!MParticles.Disabled(Child) && */ !MParticles.Sleeping(Child))
				{
					ActiveIndices.Add(Child);
				}

				MClusterIds[Child] = -1;
				TRigidTransform<T, d> ChildFrame = MChildToParent[Child] * PreSolveTM;
				MParticles.X(Child) = ChildFrame.GetTranslation();
				MParticles.R(Child) = ChildFrame.GetRotation();
				MParticles.V(Child) = MParticles.V(ClusterIndex);
				MParticles.W(Child) = MParticles.W(ClusterIndex);

				if (MParticles.InvM(Child) == T(0))
				{
					MParticles.InvM(Child) = 1.f / MParticles.M(Child);
					MParticles.InvI(Child) = Chaos::PMatrix<float, 3, 3>(
						1.f / MParticles.I(Child).M[0][0], 0.f, 0.f,
						0.f, 1.f / MParticles.I(Child).M[1][1], 0.f,
						0.f, 0.f, 1.f / MParticles.I(Child).M[2][2]);
				}

				//
				// update connectivity
				//
				for (const TConnectivityEdge& Edge : MConnectivityEdges[Child])
				{
					for (int Idx = 0; Idx < MConnectivityEdges[Edge.Sibling].Num(); ++Idx)
					{
						const TConnectivityEdge& OtherEdge = MConnectivityEdges[Edge.Sibling][Idx];
						if (OtherEdge.Sibling == Child)
						{
							MConnectivityEdges[Edge.Sibling].RemoveAtSwap(Idx);
							break;
						}
					}
				}
				MConnectivityEdges[Child].SetNum(0);


			}
		}


		//disable original cluster
		MParticles.Disabled(ClusterIndex) = true;
		ActiveIndices.Remove(ClusterIndex);
		MParentToChildren.Remove(ClusterIndex);
		MClusterIds[ClusterIndex] = -1;
		if (IslandParticles.IsValidIndex(Island))
		{
			IslandParticles[Island].Remove(ClusterIndex);
		}
		MParticles.Island(ClusterIndex) = -1;

	}
	return ActivatedChildren;
}


template<class FPBDRigidsEvolution, class FPBDCollisionConstraint, class T, int d>
TSet<uint32> TPBDRigidClustering<FPBDRigidsEvolution, FPBDCollisionConstraint, T, d>::ModifyClusterParticle(const uint32 ClusterIndex, const TMap<uint32, T>& StrainMap)
{
	SCOPE_CYCLE_COUNTER(STAT_ModifyClusterParticle);
	TSet<uint32> DeactivatedChildren;
	TArray<TSet<int32>>& IslandParticles = MEvolution.IslandParticles();
	TSet<int32>& ActiveIndices = MEvolution.ActiveIndices();
	const float ClusterDistanceThreshold2 = ClusterDistanceThreshold * ClusterDistanceThreshold;
	const int32 Island = MParticles.Island(ClusterIndex);
	const bool SleepState = MParticles.Sleeping(ClusterIndex);

	if (!ensureMsgf(MParentToChildren.Contains(ClusterIndex), TEXT("Removing Cluster that does not exist!")))
	{
		return DeactivatedChildren;
	}
	TArray<uint32>& Children = MParentToChildren[ClusterIndex];
	const TRigidTransform<T, d> ParentTransform(MParticles.P(ClusterIndex), MParticles.Q(ClusterIndex));
	bool bChildrenChanged = false;

	const TRigidTransform<T, d> PreSolveTM = RewindOnDecluster ? TRigidTransform<T, d>(MParticles.X(ClusterIndex), MParticles.R(ClusterIndex)) : TRigidTransform<T, d>(MParticles.P(ClusterIndex), MParticles.Q(ClusterIndex));

	//@todo(ocohen): iterate with all the potential parents at once?
	//find all children within some distance of contact point

	auto RemoveChildLambda = [&](uint32 Child, int32 ChildIdx)
	{
		MParticles.Island(Child) = Island;
		if (IslandParticles.IsValidIndex(Island))
		{
			IslandParticles[Island].Add(Child);
		}

		ActiveIndices.Add(Child);
		MParticles.Disabled(Child) = false;
		MParticles.SetSleeping(Child, SleepState);
		MClusterIds[Child] = -1;
		TRigidTransform<T, d> ChildFrame = MChildToParent[Child] * PreSolveTM;
		MParticles.X(Child) = ChildFrame.GetTranslation();
		MParticles.R(Child) = ChildFrame.GetRotation();

		if (!RewindOnDecluster)
		{
			MParticles.P(Child) = MParticles.X(Child);
			MParticles.Q(Child) = MParticles.R(Child);
		}

		if (ChildrenInheritVelocity)
		{
			//todo(ocohen): for now just inherit velocity at new COM. This isn't quite right for rotation
			MParticles.V(Child) = MParticles.V(ClusterIndex);
			MParticles.W(Child) = MParticles.W(ClusterIndex);

			if (RewindOnDecluster)
			{
				MParticles.PreV(Child) = MParticles.PreV(ClusterIndex);
				MParticles.PreW(Child) = MParticles.PreW(ClusterIndex);
			}
		}
		else if (RewindOnDecluster)
		{
			MParticles.PreV(Child) = TVector<T, d>(0.f);
			MParticles.PreW(Child) = TVector<T, d>(0.f);
		}

		DeactivatedChildren.Add(Child);
		Children.RemoveAtSwap(ChildIdx, 1, /*bAllowShrinking=*/ false);	//@todo(ocohen): maybe avoid this until we know all children are not going away?

		if (MParticles.InvM(Child)==T(0))
		{
			MParticles.InvM(Child) = 1.f / MParticles.M(Child);
			MParticles.InvI(Child) = Chaos::PMatrix<float, 3, 3>(
				1.f / MParticles.I(Child).M[0][0], 0.f, 0.f,
				0.f, 1.f / MParticles.I(Child).M[1][1], 0.f,
				0.f, 0.f, 1.f / MParticles.I(Child).M[2][2]);
		}

		bChildrenChanged = true;
	};

	for (int32 ChildIdx = Children.Num() - 1; ChildIdx >= 0; --ChildIdx)
	{
		int32 Child = Children[ChildIdx];
		const T* StrainPtr = StrainMap.Find(Child);
		T TotalStrain = StrainPtr ? *StrainPtr : (T)0;
		if (TotalStrain >= MStrains[Child] )
		{
			RemoveChildLambda(Child, ChildIdx);	//the piece that hits just breaks off - we may want more control by looking at the edges of this piece which would give us cleaner breaks (this approach produces more rubble)
		}
	}

	if (bChildrenChanged)
	{
		if (UseConnectivity)
		{
			//cluster may have contained forests so find the connected pieces and cluster them together
			TSet<uint32> PotentialDeactivatedChildren;
			PotentialDeactivatedChildren.Append(Children);

			//first update the connected graph of the children we already removed
			for (uint32 Child : DeactivatedChildren)
			{
				for (const TConnectivityEdge& Edge : MConnectivityEdges[Child])
				{
					//todo(ocohen):make this suck less
					for (int Idx = 0; Idx < MConnectivityEdges[Edge.Sibling].Num(); ++Idx)
					{
						const TConnectivityEdge& OtherEdge = MConnectivityEdges[Edge.Sibling][Idx];
						if (OtherEdge.Sibling == Child)
						{
							MConnectivityEdges[Edge.Sibling].RemoveAtSwap(Idx);
							break;
						}
					}
				}
				MConnectivityEdges[Child].SetNum(0);
			}

			if (PotentialDeactivatedChildren.Num())
			{
				TArray<TArray<uint32>> ConnectedPiecesArray;
				//traverse connectivity and see how many connected pieces we have
				TSet<uint32> ProcessedChildren;
				for (uint32 PotentialDeactivatedChild : PotentialDeactivatedChildren)
				{
					if (!ProcessedChildren.Contains(PotentialDeactivatedChild))
					{
						ConnectedPiecesArray.AddDefaulted();
						TArray<uint32>& ConnectedPieces = ConnectedPiecesArray.Last();

						TArray<uint32> ProcessingQueue;
						ProcessingQueue.Add(PotentialDeactivatedChild);
						while (ProcessingQueue.Num())
						{
							uint32 Child = ProcessingQueue.Pop();
							if (!ProcessedChildren.Contains(Child))
							{
								ProcessedChildren.Add(Child);
								ConnectedPieces.Add(Child);
								for (const TConnectivityEdge& Edge : MConnectivityEdges[Child])
								{
									if (!ProcessedChildren.Contains(Edge.Sibling))
									{
										ProcessingQueue.Add(Edge.Sibling);
									}
								}
							}
						}
					}
				}

				for (const TArray<uint32>& ConnectedPieces : ConnectedPiecesArray)
				{
					if (ConnectedPieces.Num() == 1)
					{
						const uint32 Child = ConnectedPieces[0];
						int32 ChildIdx = INDEX_NONE;
						Children.Find(Child, ChildIdx);	//todo(ocohen): make this suck less
						RemoveChildLambda(Child, ChildIdx);
					}
					else
					{
						int32 NewClusterIndex = CreateClusterParticleFromClusterChildren(ConnectedPieces, MParticles.Island(ClusterIndex), PreSolveTM);
						MStrains[NewClusterIndex] = MStrains[ClusterIndex];
						MParticles.SetSleeping(NewClusterIndex, SleepState);
						MClusterIds[NewClusterIndex] = -1;
						if (!RewindOnDecluster)
						{
							MParticles.P(NewClusterIndex) = MParticles.X(NewClusterIndex);
							MParticles.Q(NewClusterIndex) = MParticles.R(NewClusterIndex);
						}
						else
						{
							MParticles.PreV(NewClusterIndex) = MParticles.PreV(ClusterIndex);
							MParticles.PreW(NewClusterIndex) = MParticles.PreW(ClusterIndex);
						}
						DeactivatedChildren.Add(NewClusterIndex);
					}
				}
			}
		}

		//disable original cluster
		MParticles.Disabled(ClusterIndex) = true;
		ActiveIndices.Remove(ClusterIndex);
		MParentToChildren.Remove(ClusterIndex);
		MClusterIds[ClusterIndex] = -1;
		if (IslandParticles.IsValidIndex(Island))
		{
			IslandParticles[Island].Remove(ClusterIndex);
		}
		MParticles.Island(ClusterIndex) = -1;

	}

	return DeactivatedChildren;
}

template class Chaos::TPBDRigidClustering<TPBDRigidsEvolutionGBF<float, 3>, TPBDCollisionConstraint<float, 3>, float, 3>;
template class Chaos::TPBDRigidClustering<TPBDRigidsEvolutionPGS<float, 3>, TPBDCollisionConstraintPGS<float, 3>, float, 3>;