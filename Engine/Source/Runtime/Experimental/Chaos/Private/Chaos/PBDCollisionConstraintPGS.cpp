// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDCollisionConstraintPGS.h"

#include "Chaos/BoundingVolume.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/Defines.h"
#include "Chaos/Pair.h"
#include "Chaos/PBDContactGraph.h"
#include "Chaos/Sphere.h"
#include "Chaos/Transform.h"

#include "ProfilingDebugging/ScopedTimers.h"

#define USE_SHOCK_PROPOGATION 1

using namespace Chaos;

template<class T_PARTICLES, class T, int d>
TVector<T, d> GetTranslationPGS(const T_PARTICLES& InParticles)
{
	check(false);
	return TRigidTransform<T, d>();
}

template<class T, int d>
TVector<T, d> GetTranslationPGS(const TRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.X(Index);
}

template<class T, int d>
TVector<T, d> GetTranslationPGS(const TPBDRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.P(Index);
}

template<class T_PARTICLES, class T, int d>
TRotation<T, d> GetRotationPGS(const T_PARTICLES& InParticles)
{
	check(false);
	return TRotation<T, d>();
}

template<class T, int d>
TRotation<T, d> GetRotationPGS(const TRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.R(Index);
}

template<class T, int d>
TRotation<T, d> GetRotationPGS(const TPBDRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.Q(Index);
}

template<class T_PARTICLES, class T, int d>
TRigidTransform<T, d> GetTransformPGS(const T_PARTICLES& InParticles)
{
	check(false);
	return TRigidTransform<T, d>();
}

template<class T, int d>
TRigidTransform<T, d> GetTransformPGS(const TRigidParticles<T, d>& InParticles, const int32 Index)
{
	return TRigidTransform<T, d>(InParticles.X(Index), InParticles.R(Index));
}

template<class T, int d>
TRigidTransform<T, d> GetTransformPGS(const TPBDRigidParticles<T, d>& InParticles, const int32 Index)
{
	return TRigidTransform<T, d>(InParticles.P(Index), InParticles.Q(Index));
}

template<class T, int d>
TPBDCollisionConstraintPGS<T, d>::TPBDCollisionConstraintPGS(TPBDRigidParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const int32 PushOutIterations /*= 1*/, const int32 PushOutPairIterations /*= 1*/, const T Thickness /*= (T)0*/, const T Restitution /*= (T)0*/, const T Friction /*= (T)0*/)
    : MCollided(Collided), MContactGraph(InParticles), MNumIterations(PushOutIterations), MPairIterations(PushOutPairIterations), MThickness(Thickness), MRestitution(Restitution), MFriction(Friction), Tolerance(KINDA_SMALL_NUMBER), MaxIterations(10), bUseCCD(false)
{
	MContactGraph.Initialize(InParticles.Size());
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::ComputeConstraints(const TPBDRigidParticles<T, d>& InParticles, const T Dt)
{
	double Time = 0;
	FDurationTimer Timer(Time);
	// Broad phase
	//TBoundingVolumeHierarchy<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles);
	TBoundingVolume<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles, true, Dt);
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDCollisionConstraint Construct Hierarchy %f"), Time);
	// Narrow phase
	FCriticalSection CriticalSection;
	Time = 0;
	Timer.Start();
	PhysicsParallelFor(InParticles.Size(), [&](int32 Body1Index) {
		if (InParticles.Disabled(Body1Index))
			return;
		TArray<int32> PotentialIntersections;
		TBox<T, d> Box1 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body1Index);
		if (InParticles.Geometry(Body1Index)->HasBoundingBox())
		{
			PotentialIntersections = Hierarchy.FindAllIntersections(Box1);
		}
		else
		{
			PotentialIntersections = Hierarchy.GlobalObjects();
		}
		for (int32 i = 0; i < PotentialIntersections.Num(); ++i)
		{
			int32 Body2Index = PotentialIntersections[i];
			if (InParticles.InvM(Body1Index) < FLT_MIN && InParticles.InvM(Body2Index) < FLT_MIN)
			{
				continue;
			}
			if (Body1Index == Body2Index || ((InParticles.Geometry(Body1Index)->HasBoundingBox() == InParticles.Geometry(Body2Index)->HasBoundingBox()) && Body2Index > Body1Index))
			{
				continue;
			}
			const auto& Box2 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body2Index);
			if (InParticles.Geometry(Body1Index)->HasBoundingBox() && InParticles.Geometry(Body2Index)->HasBoundingBox() && !Box1.Intersects(Box2))
			{
				continue;
			}
			auto Constraint = ComputeConstraint(InParticles, Body1Index, Body2Index, MThickness);
			CriticalSection.Lock();
			MConstraints.Add(Constraint);
			CriticalSection.Unlock();
		}
	});
	MContactGraph.ComputeGraph(InParticles, MConstraints);
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDCollisionConstraint Construct %d Constraints with Potential Collisions %f"), MConstraints.Num(), Time);
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::RemoveConstraints(const TSet<uint32>& RemovedParticles)
{
	for (int32 i = 0; i < MConstraints.Num(); ++i)
	{
		const auto& Constraint = MConstraints[i];
		if (RemovedParticles.Contains(Constraint.ParticleIndex) || RemovedParticles.Contains(Constraint.LevelsetIndex))
		{
			MConstraints.RemoveAtSwap(i);
			i--;
		}
	}
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::UpdateConstraints(const TPBDRigidParticles<T, d>& InParticles, T Dt, const TSet<uint32>& AddedParticles, const TArray<uint32>& ActiveParticles)
{
	double Time = 0;
	FDurationTimer Timer(Time);

	//
	// Broad phase
	//

	// @todo(mlentine): We only need to construct the hierarchy for the islands we care about
	TBoundingVolume<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles, ActiveParticles, true, Dt);
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDCollisionConstraint Construct Hierarchy %f"), Time);

	//
	// Narrow phase
	//

	FCriticalSection CriticalSection;
	Time = 0;
	Timer.Start();
	TArray<uint32> AddedParticlesArray = AddedParticles.Array();
	PhysicsParallelFor(AddedParticlesArray.Num(), [&](int32 Index) {
		int32 Body1Index = AddedParticlesArray[Index];
		if (InParticles.Disabled(Body1Index))
			return;
		TArray<int32> PotentialIntersections;
		TBox<T, d> Box1 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body1Index);
		if (InParticles.Geometry(Body1Index)->HasBoundingBox())
		{
			PotentialIntersections = Hierarchy.FindAllIntersections(Box1);
		}
		else
		{
			PotentialIntersections = Hierarchy.GlobalObjects();
		}
		for (int32 i = 0; i < PotentialIntersections.Num(); ++i)
		{
			int32 Body2Index = PotentialIntersections[i];

			if (Body1Index == Body2Index || ((InParticles.Geometry(Body1Index)->HasBoundingBox() == InParticles.Geometry(Body2Index)->HasBoundingBox()) && AddedParticles.Contains(Body2Index) && AddedParticles.Contains(Body1Index) && Body2Index > Body1Index))
			{
				continue;
			}
			const auto& Box2 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body2Index);
			if (InParticles.Geometry(Body1Index)->HasBoundingBox() && InParticles.Geometry(Body2Index)->HasBoundingBox() && !Box1.Intersects(Box2))
			{
				continue;
			}
			
			if (InParticles.InvM(Body1Index) && InParticles.InvM(Body2Index) && (InParticles.Island(Body1Index) != InParticles.Island(Body2Index)))	//todo(ocohen): this is a hack - we should not even consider dynamics from other islands
			{
				continue;
			}
			auto Constraint = ComputeConstraint(InParticles, Body1Index, Body2Index, MThickness);
			CriticalSection.Lock();
			MConstraints.Add(Constraint);
			CriticalSection.Unlock();
		}
	});
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDCollisionConstraint Update %d Constraints with Potential Collisions %f"), MConstraints.Num(), Time);
}

template<class T_PARTICLES, class T, int d>
void ComputePGSPropeties(const T_PARTICLES& InParticles, const TRigidBodyContactConstraintPGS<T, d>& Constraint, const int32 PointIndex, const int32 FlattenedIndex,
	const PMatrix<T, d, d>& WorldSpaceInvI1, const PMatrix<T, d, d>& WorldSpaceInvI2, const TVector<T, d> Direction,
	TArray<TVector<TVector<T, d>, 2>>& Angulars, TArray<TVector<TVector<T, d>, 2>>& MassWeightedAngulars, TArray<T>& Multipliers)
{
	TVector<T, d> VectorToPoint1 = Constraint.Location[PointIndex] - GetTranslationPGS(InParticles, Constraint.ParticleIndex);
	TVector<T, d> VectorToPoint2 = Constraint.Location[PointIndex] - GetTranslationPGS(InParticles, Constraint.LevelsetIndex);
	Angulars[FlattenedIndex][0] = -TVector<T, d>::CrossProduct(VectorToPoint1, Direction);
	Angulars[FlattenedIndex][1] = TVector<T, d>::CrossProduct(VectorToPoint2, Direction);
	MassWeightedAngulars[FlattenedIndex][0] = WorldSpaceInvI1 * Angulars[FlattenedIndex][0];
	MassWeightedAngulars[FlattenedIndex][1] = WorldSpaceInvI2 * Angulars[FlattenedIndex][1];
	if (InParticles.InvM(Constraint.ParticleIndex))
	{
		Multipliers[FlattenedIndex] += InParticles.InvM(Constraint.ParticleIndex) + TVector<T, d>::DotProduct(Angulars[FlattenedIndex][0], MassWeightedAngulars[FlattenedIndex][0]);
	}
	if (InParticles.InvM(Constraint.LevelsetIndex))
	{
		Multipliers[FlattenedIndex] += InParticles.InvM(Constraint.LevelsetIndex) + TVector<T, d>::DotProduct(Angulars[FlattenedIndex][1], MassWeightedAngulars[FlattenedIndex][1]);
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::Solve(T_PARTICLES& InParticles, const T Dt, const int32 Island)
{
	TArray<T> Normals;
	TArray<TVector<T, d - 1>> Tangents;
	TArray<T> Multipliers;
	TArray<TVector<TVector<T, d>, 2>> Angulars, MassWeightedAngulars;
	TArray<TVector<TVector<T, d>, d - 1>> ConstraintTangents;
	TVector<TArray<T>, d - 1> TangentMultipliers;
	TVector<TArray<TVector<TVector<T, d>, 2>>, d - 1> TangentAngulars, TangentMassWeightedAngulars;
	const TArray<int32> IslandConstraints = MContactGraph.GetIslandConstraints(Island).Array();

	int32 NumConstraints = 0;
	for (int32 ConstraintIndex = 0; ConstraintIndex < IslandConstraints.Num(); ++ConstraintIndex)
	{
		NumConstraints += MConstraints[IslandConstraints[ConstraintIndex]].Phi.Num();
	}

	Normals.SetNumZeroed(NumConstraints);
	Tangents.SetNumZeroed(NumConstraints);
	Multipliers.SetNumZeroed(NumConstraints);
	Angulars.SetNum(NumConstraints);
	MassWeightedAngulars.SetNum(NumConstraints);
	ConstraintTangents.SetNum(NumConstraints);
	for (int32 Dimension = 0; Dimension < (d - 1); ++Dimension)
	{
		TangentMultipliers[Dimension].SetNumZeroed(NumConstraints);
		TangentAngulars[Dimension].SetNum(NumConstraints);
		TangentMassWeightedAngulars[Dimension].SetNum(NumConstraints);
	}

	int32 FlattenedIndex = 0;
	for (int32 ConstraintIndex = 0; ConstraintIndex < IslandConstraints.Num(); ++ConstraintIndex)
	{
		FRigidBodyContactConstraint& Constraint = MConstraints[IslandConstraints[ConstraintIndex]];
		PMatrix<T, d, d> WorldSpaceInvI1 = (GetRotationPGS(InParticles, Constraint.ParticleIndex) * FMatrix::Identity).GetTransposed() * InParticles.InvI(Constraint.ParticleIndex) * (GetRotationPGS(InParticles, Constraint.ParticleIndex) * FMatrix::Identity);
		PMatrix<T, d, d> WorldSpaceInvI2 = (GetRotationPGS(InParticles, Constraint.LevelsetIndex) * FMatrix::Identity).GetTransposed() * InParticles.InvI(Constraint.LevelsetIndex) * (GetRotationPGS(InParticles, Constraint.LevelsetIndex) * FMatrix::Identity);
		for (int32 PointIndex = 0; PointIndex < Constraint.Phi.Num(); ++PointIndex)
		{
			ComputePGSPropeties(InParticles, Constraint, PointIndex, FlattenedIndex, WorldSpaceInvI1, WorldSpaceInvI2, -Constraint.Normal[PointIndex], Angulars, MassWeightedAngulars, Multipliers);
			// Tangents
			{
				T AbsX = FMath::Abs(Constraint.Normal[PointIndex][0]);
				T AbsY = FMath::Abs(Constraint.Normal[PointIndex][1]);
				T AbsZ = FMath::Abs(Constraint.Normal[PointIndex][2]);
				if (AbsX < AbsY)
				{
					if (AbsX < AbsZ)
					{
						ConstraintTangents[FlattenedIndex][0] = TVector<T, d>(0, Constraint.Normal[PointIndex][2], -Constraint.Normal[PointIndex][1]);
					}
					else
					{
						ConstraintTangents[FlattenedIndex][0] = TVector<T, d>(Constraint.Normal[PointIndex][1], -Constraint.Normal[PointIndex][0], 0);
					}
				}
				else
				{
					if (AbsY < AbsZ)
					{
						ConstraintTangents[FlattenedIndex][0] = TVector<T, d>(-Constraint.Normal[PointIndex][2], 0, Constraint.Normal[PointIndex][0]);
					}
					else
					{
						ConstraintTangents[FlattenedIndex][0] = TVector<T, d>(Constraint.Normal[PointIndex][1], -Constraint.Normal[PointIndex][0], 0);
					}
				}
			}
			ConstraintTangents[FlattenedIndex][0] = ConstraintTangents[FlattenedIndex][0].GetSafeNormal();
			ConstraintTangents[FlattenedIndex][1] = TVector<T, d>::CrossProduct(-ConstraintTangents[FlattenedIndex][0], Constraint.Normal[PointIndex]);
			ComputePGSPropeties(InParticles, Constraint, PointIndex, FlattenedIndex, WorldSpaceInvI1, WorldSpaceInvI2, -ConstraintTangents[FlattenedIndex][0], TangentAngulars[0], TangentMassWeightedAngulars[0], TangentMultipliers[0]);
			ComputePGSPropeties(InParticles, Constraint, PointIndex, FlattenedIndex, WorldSpaceInvI1, WorldSpaceInvI2, -ConstraintTangents[FlattenedIndex][1], TangentAngulars[1], TangentMassWeightedAngulars[1], TangentMultipliers[1]);
			FlattenedIndex++;
		}
	}

	T Residual = 0;
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		Residual = 0;
		FlattenedIndex = 0;
		for (int32 ConstraintIndex = 0; ConstraintIndex < IslandConstraints.Num(); ++ConstraintIndex)
		{
			FRigidBodyContactConstraint& Constraint = MConstraints[IslandConstraints[ConstraintIndex]];
			for (int32 PointIndex = 0; PointIndex < Constraint.Phi.Num(); ++PointIndex)
			{
				T Body1NormalVelocity = TVector<T, d>::DotProduct(InParticles.V(Constraint.ParticleIndex), Constraint.Normal[PointIndex]) +
					TVector<T, d>::DotProduct(InParticles.W(Constraint.ParticleIndex), Angulars[FlattenedIndex][0]);
				T Body2NormalVelocity = TVector<T, d>::DotProduct(InParticles.V(Constraint.LevelsetIndex), -Constraint.Normal[PointIndex]) +
					TVector<T, d>::DotProduct(InParticles.W(Constraint.LevelsetIndex), Angulars[FlattenedIndex][1]);
				T RelativeNormalVelocity = Body1NormalVelocity + Body2NormalVelocity + Constraint.Phi[PointIndex] / Dt;
				T NewResidual = FMath::Max(-RelativeNormalVelocity, RelativeNormalVelocity * Normals[FlattenedIndex]);
				if (NewResidual > Residual)
				{
					Residual = NewResidual;
				}
				T NormalDelta = -RelativeNormalVelocity / Multipliers[FlattenedIndex];
				// Update Normals
				T NewNormal = Normals[FlattenedIndex] + NormalDelta;
				if (NewNormal < 0)
				{
					NewNormal = 0;
					NormalDelta = -Normals[FlattenedIndex];
				}
				check(RelativeNormalVelocity < 0 || NormalDelta == 0 || Iteration > 0);
				// Velocity update
				InParticles.V(Constraint.ParticleIndex) += NormalDelta * InParticles.InvM(Constraint.ParticleIndex) * Constraint.Normal[PointIndex];
				InParticles.V(Constraint.LevelsetIndex) += NormalDelta * InParticles.InvM(Constraint.LevelsetIndex) * -Constraint.Normal[PointIndex];
				InParticles.W(Constraint.ParticleIndex) += NormalDelta * MassWeightedAngulars[FlattenedIndex][0];
				InParticles.W(Constraint.LevelsetIndex) += NormalDelta * MassWeightedAngulars[FlattenedIndex][1];
				// Normal update
				Normals[FlattenedIndex] = NewNormal;
				if (MFriction)
				{
					for (int32 Dimension = 0; Dimension < (d - 1); Dimension++)
					{
						T Body1TangentVelocity = TVector<T, d>::DotProduct(InParticles.V(Constraint.ParticleIndex), ConstraintTangents[PointIndex][Dimension]) +
							TVector<T, d>::DotProduct(InParticles.W(Constraint.ParticleIndex), TangentAngulars[Dimension][FlattenedIndex][0]);
						T Body2TangentVelocity = TVector<T, d>::DotProduct(InParticles.V(Constraint.LevelsetIndex), -ConstraintTangents[PointIndex][Dimension]) +
							TVector<T, d>::DotProduct(InParticles.W(Constraint.LevelsetIndex), TangentAngulars[Dimension][FlattenedIndex][1]);
						T RelativeTangentVelocity = Body1TangentVelocity + Body2TangentVelocity;
						T TangentDelta = -RelativeTangentVelocity / TangentMultipliers[Dimension][FlattenedIndex];
						T NewTangent = Tangents[FlattenedIndex][Dimension] + TangentDelta;
						if (FMath::Abs(NewTangent) > MFriction * NewNormal)
						{
							NewTangent = MFriction * NewNormal;
							if (NewTangent < 0)
							{
								NewTangent *= -1;
							}
						}
						// Velocity update
						InParticles.V(Constraint.ParticleIndex) += TangentDelta * InParticles.InvM(Constraint.ParticleIndex) * ConstraintTangents[PointIndex][Dimension];
						InParticles.V(Constraint.LevelsetIndex) += TangentDelta * InParticles.InvM(Constraint.LevelsetIndex) * -ConstraintTangents[PointIndex][Dimension];
						InParticles.W(Constraint.ParticleIndex) += TangentDelta * TangentMassWeightedAngulars[Dimension][FlattenedIndex][0];
						InParticles.W(Constraint.LevelsetIndex) += TangentDelta * TangentMassWeightedAngulars[Dimension][FlattenedIndex][1];
						Tangents[FlattenedIndex][Dimension] = NewTangent;
					}
				}
				FlattenedIndex++;
			}
		}
		UE_LOG(LogChaos, Verbose, TEXT("\tPBDCollisionConstraint Solve with Residual %f"), Residual);
		if (Residual < Tolerance)
		{
			break;
		}
	}
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::PrintParticles(const TPBDRigidParticles<T, d>& InParticles, const int32 Island)
{
	const TArray<int32> IslandConstraints = MContactGraph.GetIslandConstraints(Island).Array();
	TSet<int32> ConstraintParticles;
	for (int32 i = 0; i < IslandConstraints.Num(); ++i)
	{
		FRigidBodyContactConstraint& Constraint = MConstraints[IslandConstraints[i]];
		if (!ConstraintParticles.Contains(Constraint.ParticleIndex))
		{
			ConstraintParticles.Add(Constraint.ParticleIndex);
		}
		if (!ConstraintParticles.Contains(Constraint.LevelsetIndex))
		{
			ConstraintParticles.Add(Constraint.LevelsetIndex);
		}
	}
	for (const auto& i : ConstraintParticles)
	{
		UE_LOG(LogChaos, Verbose, TEXT("Particle %d has X=(%f, %f, %f) and V=(%f, %f, %f)"), i, InParticles.X(i)[0], InParticles.X(i)[1], InParticles.X(i)[2], InParticles.V(i)[0], InParticles.V(i)[1], InParticles.V(i)[2]);
	}
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::PrintConstraints(const TPBDRigidParticles<T, d>& InParticles, const int32 Island)
{
	const TArray<int32> IslandConstraints = MContactGraph.GetIslandConstraints(Island).Array();
	for (int32 i = 0; i < IslandConstraints.Num(); ++i)
	{
		FRigidBodyContactConstraint& Constraint = MConstraints[IslandConstraints[i]];
		UE_LOG(LogChaos, Verbose, TEXT("Constraint between %d and %d has %d contacts"), Constraint.ParticleIndex, Constraint.LevelsetIndex, Constraint.Phi.Num());
		for (int32 j = 0; j < Constraint.Phi.Num(); ++j)
		{
			UE_LOG(LogChaos, Verbose, TEXT("Constraint has loction (%f, %f, %f) and phi %f"), Constraint.Location[j][0], Constraint.Location[j][1], Constraint.Location[j][2], Constraint.Phi[j]);
		}
	}
}

template<class T, int d>
void FindPointsOnHull(TRigidBodyContactConstraintPGS<T, d>& Constraint,
	const TVector<T, d>& X0, const TVector<T, d>& X1, const TVector<T, d>& X2,
	const TPlane<T, d>& SplitPlane, const TArray<int32>& Indices, TSet<int32>& HullPoints)
{
	int32 MaxD = 0; //This doesn't need to be initialized but we need to avoid the compiler warning
	T MaxDistance = 0;
	for (int32 i = 0; i < Indices.Num(); ++i)
	{
		T Distance = SplitPlane.SignedDistance(Constraint.Location[Indices[i]]);
		check(Distance >= 0);
		if (Distance > MaxDistance)
		{
			MaxDistance = Distance;
			MaxD = Indices[i];
		}
	}
	if (MaxDistance > 0)
	{
		if (!HullPoints.Contains(MaxD))
		{
			HullPoints.Add(MaxD);
		}
		const TVector<T, d>& NewX = Constraint.Location[MaxD];
		const TVector<T, d> V1 = (NewX - X0).GetSafeNormal();
		const TVector<T, d> V2 = (NewX - X1).GetSafeNormal();
		const TVector<T, d> V3 = (NewX - X2).GetSafeNormal();
		TVector<T, d> Normal1 = TVector<T, d>::CrossProduct(V1, V2).GetSafeNormal();
		if (TVector<T, 3>::DotProduct(Normal1, X2 - X0) > 0)
		{
			Normal1 *= -1;
		}
		TVector<T, d> Normal2 = TVector<T, d>::CrossProduct(V1, V3).GetSafeNormal();
		if (TVector<T, 3>::DotProduct(Normal2, X1 - X0) > 0)
		{
			Normal2 *= -1;
		}
		TVector<T, d> Normal3 = TVector<T, d>::CrossProduct(V2, V3).GetSafeNormal();
		if (TVector<T, 3>::DotProduct(Normal3, X0 - X1) > 0)
		{
			Normal3 *= -1;
		}
		TPlane<T, d> NewPlane1(NewX, Normal1);
		TPlane<T, d> NewPlane2(NewX, Normal2);
		TPlane<T, d> NewPlane3(NewX, Normal3);
		TArray<int32> NewIndices1;
		TArray<int32> NewIndices2;
		TArray<int32> NewIndices3;
		for (int32 i = 0; i < Indices.Num(); ++i)
		{
			if (MaxD == Indices[i])
			{
				continue;
			}
			if (NewPlane1.SignedDistance(Constraint.Location[Indices[i]]) > 0)
			{
				NewIndices1.Add(Indices[i]);
			}
			if (NewPlane2.SignedDistance(Constraint.Location[Indices[i]]) > 0)
			{
				NewIndices2.Add(Indices[i]);
			}
			if (NewPlane3.SignedDistance(Constraint.Location[Indices[i]]) > 0)
			{
				NewIndices3.Add(Indices[i]);
			}
		}
		FindPointsOnHull(Constraint, X0, X1, NewX, NewPlane1, NewIndices1, HullPoints);
		FindPointsOnHull(Constraint, X0, X2, NewX, NewPlane2, NewIndices2, HullPoints);
		FindPointsOnHull(Constraint, X1, X2, NewX, NewPlane3, NewIndices3, HullPoints);
	}
}

template<class T, int d>
void RemovePointsInsideHull(TRigidBodyContactConstraintPGS<T, d>& Constraint)
{
	if (Constraint.Location.Num() <= 2)
	{
		return;
	}
	// Find max and min x points
	int32 MinX = 0;
	int32 MaxX = 0;
	int32 MinY = 0;
	int32 MaxY = 0;
	int32 Index1 = 0;
	int32 Index2 = 0;
	for (int32 i = 1; i < Constraint.Location.Num(); ++i)
	{
		if (Constraint.Location[i][0] > Constraint.Location[MaxX][0])
		{
			MaxX = i;
		}
		if (Constraint.Location[i][0] < Constraint.Location[MinX][0])
		{
			MinX = i;
		}
		if (Constraint.Location[i][1] > Constraint.Location[MaxY][1])
		{
			MaxY = i;
		}
		if (Constraint.Location[i][1] < Constraint.Location[MinY][1])
		{
			MinY = i;
		}
	}
	if (MaxX == MinX && MinY == MaxY && MinX == MinY)
	{
		// Points are colinear so need to sort but for now do nothing
		return;
	}
	// Find max distance
	T DistanceY = (Constraint.Location[MaxY] - Constraint.Location[MinY]).Size();
	T DistanceX = (Constraint.Location[MaxX] - Constraint.Location[MinX]).Size();
	if (DistanceX > DistanceY)
	{
		Index1 = MaxX;
		Index2 = MinX;
	}
	else
	{
		Index1 = MaxY;
		Index2 = MinY;
	}
	TSet<int32> HullPoints;
	HullPoints.Add(Index1);
	HullPoints.Add(Index2);
	const TVector<T, d>& X1 = Constraint.Location[Index1];
	const TVector<T, d>& X2 = Constraint.Location[Index2];
	T MaxDist = 0;
	int32 MaxD = -1;
	for (int32 i = 0; i < Constraint.Location.Num(); ++i)
	{
		if (i == Index1 || i == Index2)
		{
			continue;
		}
		const TVector<T, d>& X0 = Constraint.Location[i];
		T Distance = TVector<T, d>::CrossProduct(X0 - X1, X0 - X2).Size() / (X2 - X1).Size();
		if (Distance > MaxDist)
		{
			MaxDist = Distance;
			MaxD = i;
		}
	}
	if (MaxD != -1)
	{
		HullPoints.Add(MaxD);
		const TVector<T, d>& X0 = Constraint.Location[MaxD];
		TVector<T, d> Normal = TVector<T, d>::CrossProduct((X0 - X1).GetSafeNormal(), (X0 - X2).GetSafeNormal());
		TPlane<T, d> SplitPlane(X0, Normal);
		TPlane<T, d> SplitPlaneNeg(X0, -Normal);
		TArray<int32> Left;
		TArray<int32> Right;
		for (int32 i = 0; i < Constraint.Location.Num(); ++i)
		{
			if (i == Index1 || i == Index2 || i == MaxD)
			{
				continue;
			}
			if (SplitPlane.SignedDistance(Constraint.Location[i]) >= 0)
			{
				Left.Add(i);
			}
			else
			{
				Right.Add(i);
			}
		}
		FindPointsOnHull(Constraint, X0, X1, X2, SplitPlane, Left, HullPoints);
		FindPointsOnHull(Constraint, X0, X1, X2, SplitPlaneNeg, Right, HullPoints);
	}
	TArray<TVector<T, d>> Locations;
	TArray<TVector<T, d>> Normals;
	TArray<T> Distances;
	for (const auto& Index : HullPoints)
	{
		Locations.Add(Constraint.Location[Index]);
		Normals.Add(Constraint.Normal[Index]);
		Distances.Add(Constraint.Phi[Index]);
	}
	Constraint.Location = Locations;
	Constraint.Normal = Normals;
	Constraint.Phi = Distances;
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island)
{
	const TArray<int32> IslandConstraints = MContactGraph.GetIslandConstraints(Island).Array();
	PhysicsParallelFor(IslandConstraints.Num(), [&](int32 ConstraintIndex) {
		FRigidBodyContactConstraint& Constraint = MConstraints[IslandConstraints[ConstraintIndex]];
		if (InParticles.Sleeping(Constraint.ParticleIndex))
		{
			check(InParticles.Sleeping(Constraint.LevelsetIndex) || InParticles.InvM(Constraint.LevelsetIndex) == 0);
			return;
		}
		// @todo(mlentine): This is a really coarse approximation. Prune points that we know are not relevant.
		const T Threshold = (InParticles.V(Constraint.ParticleIndex).Size() - InParticles.V(Constraint.LevelsetIndex).Size()) * Dt;
		// Guessing Max is a decent approximation as with restitution 0 the difference is X between predicted and actual is Vdt
		const T Thickness = MThickness + FMath::Max(InParticles.V(Constraint.ParticleIndex).Size(), InParticles.V(Constraint.LevelsetIndex).Size()) * Dt;
		const_cast<TPBDCollisionConstraintPGS<T, d>*>(this)->UpdateConstraint(static_cast<TRigidParticles<T, d>&>(InParticles), Thickness + Threshold, Constraint);
		// @todo(mlentine): Prune contact points based on convex hull
		RemovePointsInsideHull(Constraint);
	});
	PrintParticles(InParticles, Island);
	PrintConstraints(InParticles, Island);
	Solve(static_cast<TRigidParticles<T, d>&>(InParticles), Dt, Island);
	PrintParticles(InParticles, Island);
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::ApplyPushOut(TPBDRigidParticles<T, d>& InParticles, const T Dt, const TArray<int32>& ActiveIndices, const int32 Island)
{
	const TArray<int32> IslandConstraints = MContactGraph.GetIslandConstraints(Island).Array();
	PhysicsParallelFor(IslandConstraints.Num(), [&](int32 ConstraintIndex) {
		FRigidBodyContactConstraint& Constraint = MConstraints[IslandConstraints[ConstraintIndex]];
		if (InParticles.Sleeping(Constraint.ParticleIndex))
		{
			check(InParticles.Sleeping(Constraint.LevelsetIndex) || InParticles.InvM(Constraint.LevelsetIndex) == 0);
			return;
		}
		const_cast<TPBDCollisionConstraintPGS<T, d>*>(this)->UpdateConstraint(InParticles, MThickness, Constraint);
		// @todo(mlentine): Prune contact points based on convex hull
	});
	TArray<TVector<T, d>> SavedV, SavedW;
	SavedV.SetNum(InParticles.Size());
	SavedW.SetNum(InParticles.Size());
	PhysicsParallelFor(ActiveIndices.Num(), [&](int32 Index) {
		int32 ParticleIndex = ActiveIndices[Index];
		SavedV[ParticleIndex] = InParticles.V(ParticleIndex);
		SavedW[ParticleIndex] = InParticles.W(ParticleIndex);
		InParticles.V(ParticleIndex) = TVector<T, d>(0);
		InParticles.W(ParticleIndex) = TVector<T, d>(0);
	});
	PrintParticles(InParticles, Island);
	PrintConstraints(InParticles, Island);
	Solve(InParticles, Dt, Island);
	PrintParticles(InParticles, Island);
	PhysicsParallelFor(ActiveIndices.Num(), [&](int32 Index) {
		int32 ParticleIndex = ActiveIndices[Index];
		if (InParticles.InvM(ParticleIndex))
		{
			InParticles.P(ParticleIndex) += InParticles.V(ParticleIndex) * Dt;
			InParticles.Q(ParticleIndex) += TRotation<T, d>(InParticles.W(ParticleIndex), 0.f) * InParticles.Q(ParticleIndex) * Dt * T(0.5);
			InParticles.Q(ParticleIndex).Normalize();
		}
		InParticles.V(ParticleIndex) = SavedV[ParticleIndex];
		InParticles.W(ParticleIndex) = SavedW[ParticleIndex];
	});
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::CopyOutConstraints(int32 Island)
{
}

template<class T, int d>
bool TPBDCollisionConstraintPGS<T, d>::NearestPoint(TArray<Pair<TVector<T, d>, TVector<T, d>>>& Points, TVector<T, d>& Direction)
{
	check(Points.Num() > 1 && Points.Num() <= 4);
	if (Points.Num() == 2)
	{
		TPlane<T, d> LocalPlane(Points[1].First, Points[0].First - Points[1].First);
		TVector<T, d> Normal;
		const auto& Phi = LocalPlane.PhiWithNormal(TVector<T, d>(0), Normal);
		if ((TVector<T, d>::DotProduct(-Points[1].First, Normal.GetSafeNormal()) - Points[1].First.Size()) < SMALL_NUMBER)
		{
			T Alpha = Points[0].First.Size() / (Points[1].First - Points[0].First).Size();
			return true;
		}
		if (Phi > 0)
		{
			check(Points.Num() == 2);
			Direction = TVector<T, d>::CrossProduct(TVector<T, d>::CrossProduct(Normal, -Points[1].First), Normal);
		}
		else
		{
			Direction = -Points[1].First;
			Points.RemoveAtSwap(0);
			check(Points.Num() == 1);
		}
		check(Points.Num() > 1 && Points.Num() < 4);
		return false;
	}
	if (Points.Num() == 3)
	{
		TVector<T, d> TriangleNormal = TVector<T, d>::CrossProduct(Points[0].First - Points[2].First, Points[0].First - Points[1].First);
		TPlane<T, d> LocalPlane1(Points[2].First, TVector<T, d>::CrossProduct(Points[0].First - Points[2].First, TriangleNormal));
		TPlane<T, d> LocalPlane2(Points[2].First, TVector<T, d>::CrossProduct(Points[1].First - Points[2].First, TriangleNormal));
		TVector<T, d> Normal;
		T Phi = LocalPlane1.PhiWithNormal(TVector<T, d>(0), Normal);
		if (Phi > 0)
		{
			TVector<T, d> Delta = Points[0].First - Points[2].First;
			if (TVector<T, d>::DotProduct(-Points[2].First, Delta) > 0)
			{
				Direction = TVector<T, d>::CrossProduct(TVector<T, d>::CrossProduct(Delta, -Points[2].First), Delta);
				Points.RemoveAtSwap(1);
				check(Points.Num() == 2);
			}
			else
			{
				Delta = Points[1].First - Points[2].First;
				if (TVector<T, d>::DotProduct(-Points[2].First, Delta) > 0)
				{
					Direction = TVector<T, d>::CrossProduct(TVector<T, d>::CrossProduct(Delta, -Points[2].First), Delta);
					Points.RemoveAtSwap(0);
					check(Points.Num() == 2);
				}
				else
				{
					Direction = -Points[2].First;
					Points = {Points[2]};
					check(Points.Num() == 1);
				}
			}
		}
		else
		{
			Phi = LocalPlane2.PhiWithNormal(TVector<T, d>(0), Normal);
			if (Phi > 0)
			{
				TVector<T, d> Delta = Points[1].First - Points[2].First;
				if (TVector<T, d>::DotProduct(-Points[2].First, Delta) > 0)
				{
					Direction = TVector<T, d>::CrossProduct(TVector<T, d>::CrossProduct(Delta, -Points[2].First), Delta);
					Points.RemoveAtSwap(0);
					check(Points.Num() == 2);
				}
				else
				{
					Direction = -Points[2].First;
					Points = {Points[2]};
					check(Points.Num() == 1);
				}
			}
			else
			{
				const auto DotResult = TVector<T, d>::DotProduct(TriangleNormal, -Points[2].First);
				// We are inside the triangle
				if (DotResult < SMALL_NUMBER)
				{
					TVector<T, 3> Bary;
					TVector<T, d> P10 = Points[1].First - Points[0].First;
					TVector<T, d> P20 = Points[2].First - Points[0].First;
					TVector<T, d> PP0 = -Points[0].First;
					T Size10 = P10.SizeSquared();
					T Size20 = P20.SizeSquared();
					T ProjSides = TVector<T, d>::DotProduct(P10, P20);
					T ProjP1 = TVector<T, d>::DotProduct(PP0, P10);
					T ProjP2 = TVector<T, d>::DotProduct(PP0, P20);
					T Denom = Size10 * Size20 - ProjSides * ProjSides;
					Bary.Y = (Size20 * ProjP1 - ProjSides * ProjP2) / Denom;
					Bary.Z = (Size10 * ProjP2 - ProjSides * ProjP1) / Denom;
					Bary.X = 1.0f - Bary.Z - Bary.Y;
					return true;
				}
				if (DotResult > 0)
				{
					Direction = TriangleNormal;
				}
				else
				{
					Direction = -TriangleNormal;
					Points.Swap(0, 1);
					check(Points.Num() == 3);
				}
			}
		}
		check(Points.Num() > 0 && Points.Num() < 4);
		return false;
	}
	if (Points.Num() == 4)
	{
		TVector<T, d> TriangleNormal = TVector<T, d>::CrossProduct(Points[1].First - Points[3].First, Points[1].First - Points[2].First);
		if (TVector<T, d>::DotProduct(TriangleNormal, Points[0].First - Points[3].First) > 0)
		{
			TriangleNormal *= -1;
		}
		T DotResult = TVector<T, d>::DotProduct(TriangleNormal, -Points[3].First);
		if (DotResult > 0)
		{
			Points = {Points[1], Points[2], Points[3]};
			check(Points.Num() == 3);
			return NearestPoint(Points, Direction);
		}
		TriangleNormal = TVector<T, d>::CrossProduct(Points[2].First - Points[0].First, Points[2].First - Points[3].First);
		if (TVector<T, d>::DotProduct(TriangleNormal, Points[1].First - Points[3].First) > 0)
		{
			TriangleNormal *= -1;
		}
		DotResult = TVector<T, d>::DotProduct(TriangleNormal, -Points[3].First);
		if (DotResult > 0)
		{
			Points = {Points[0], Points[2], Points[3]};
			check(Points.Num() == 3);
			return NearestPoint(Points, Direction);
		}
		TriangleNormal = TVector<T, d>::CrossProduct(Points[3].First - Points[1].First, Points[3].First - Points[0].First);
		if (TVector<T, d>::DotProduct(TriangleNormal, Points[2].First - Points[3].First) > 0)
		{
			TriangleNormal *= -1;
		}
		DotResult = TVector<T, d>::DotProduct(TriangleNormal, -Points[3].First);
		if (DotResult > 0)
		{
			Points = {Points[0], Points[1], Points[3]};
			check(Points.Num() == 3);
			return NearestPoint(Points, Direction);
		}
		TVector<T, 4> Bary;
		TVector<T, d> PP0 = -Points[0].First;
		TVector<T, d> PP1 = -Points[1].First;
		TVector<T, d> P10 = Points[1].First - Points[0].First;
		TVector<T, d> P20 = Points[2].First - Points[0].First;
		TVector<T, d> P30 = Points[3].First - Points[0].First;
		TVector<T, d> P21 = Points[2].First - Points[1].First;
		TVector<T, d> P31 = Points[3].First - Points[1].First;
		Bary[0] = TVector<T, d>::DotProduct(PP1, TVector<T, d>::CrossProduct(P31, P21));
		Bary[1] = TVector<T, d>::DotProduct(PP0, TVector<T, d>::CrossProduct(P20, P30));
		Bary[2] = TVector<T, d>::DotProduct(PP0, TVector<T, d>::CrossProduct(P30, P10));
		Bary[3] = TVector<T, d>::DotProduct(PP0, TVector<T, d>::CrossProduct(P10, P20));
		T Denom = TVector<T, d>::DotProduct(P10, TVector<T, d>::CrossProduct(P20, P30));
		return true;
	}
	check(Points.Num() > 1 && Points.Num() < 4);
	return false;
}

template<class T, int d>
void UpdateLevelsetConstraintHelperCCD(const TRigidParticles<T, d>& InParticles, const int32 j, const TRigidTransform<T, d>& LocalToWorld1, const TRigidTransform<T, d>& LocalToWorld2, const T Thickness, TRigidBodyContactConstraintPGS<T, d>& Constraint)
{
	if(InParticles.CollisionParticles(Constraint.ParticleIndex))
	{
		const TRigidTransform<T, d> PreviousLocalToWorld1 = GetTransformPGS(InParticles, Constraint.ParticleIndex);
		TVector<T, d> WorldSpacePointStart = PreviousLocalToWorld1.TransformPosition(InParticles.CollisionParticles(Constraint.ParticleIndex)->X(j));
		TVector<T, d> WorldSpacePointEnd = LocalToWorld1.TransformPosition(InParticles.CollisionParticles(Constraint.ParticleIndex)->X(j));
		TVector<T, d> Body2SpacePointStart = LocalToWorld2.InverseTransformPosition(WorldSpacePointStart);
		TVector<T, d> Body2SpacePointEnd = LocalToWorld2.InverseTransformPosition(WorldSpacePointEnd);
		Pair<TVector<T, d>, bool> PointPair = InParticles.Geometry(Constraint.LevelsetIndex)->FindClosestIntersection(Body2SpacePointStart, Body2SpacePointEnd, Thickness);
		if (PointPair.Second)
		{
			const TVector<T, d> WorldSpaceDelta = WorldSpacePointEnd - TVector<T, d>(LocalToWorld2.TransformPosition(PointPair.First));
			Constraint.Phi.Add(-WorldSpaceDelta.Size());
			Constraint.Normal.Add(LocalToWorld2.TransformVector(InParticles.Geometry(Constraint.LevelsetIndex)->Normal(PointPair.First)));
			// @todo(mlentine): Should we be using the actual collision point or that point evolved to the current time step?
			Constraint.Location.Add(WorldSpacePointEnd);
		}
	}
}

template<class T, int d>
void UpdateLevelsetConstraintHelper(const TRigidParticles<T, d>& InParticles, const int32 j, const TRigidTransform<T, d>& LocalToWorld1, const TRigidTransform<T, d>& LocalToWorld2, const T Thickness, TRigidBodyContactConstraintPGS<T, d>& Constraint)
{
	if(InParticles.CollisionParticles(Constraint.ParticleIndex))
	{
		TVector<T, d> WorldSpacePoint = LocalToWorld1.TransformPosition(InParticles.CollisionParticles(Constraint.ParticleIndex)->X(j));
		TVector<T, d> Body2SpacePoint = LocalToWorld2.InverseTransformPosition(WorldSpacePoint);
		TVector<T, d> LocalNormal;
		T LocalPhi = InParticles.Geometry(Constraint.LevelsetIndex)->PhiWithNormal(Body2SpacePoint, LocalNormal);
		if (LocalPhi < Thickness)
		{
			Constraint.Phi.Add(LocalPhi);
			Constraint.Normal.Add(LocalToWorld2.TransformVector(LocalNormal));
			Constraint.Location.Add(WorldSpacePoint);
		}
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateLevelsetConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> LocalToWorld1 = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> LocalToWorld2 = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	if (InParticles.Geometry(Constraint.LevelsetIndex)->HasBoundingBox())
	{
		TBox<T, d> ImplicitBox = InParticles.Geometry(Constraint.LevelsetIndex)->BoundingBox().TransformedBox(LocalToWorld2 * LocalToWorld1.Inverse());
		if (InParticles.CollisionParticles(Constraint.ParticleIndex))
		{
			TArray<int32> PotentialParticles = InParticles.CollisionParticles(Constraint.ParticleIndex)->FindAllIntersections(ImplicitBox);
			for (int32 j = 0; j < PotentialParticles.Num(); ++j)
			{
				if (bUseCCD)
				{
					UpdateLevelsetConstraintHelperCCD(InParticles, PotentialParticles[j], LocalToWorld1, LocalToWorld2, Thickness, Constraint);
				}
				else
				{
					UpdateLevelsetConstraintHelper(InParticles, PotentialParticles[j], LocalToWorld1, LocalToWorld2, Thickness, Constraint);
				}
			}
		}
	}
	else
	{
		if (InParticles.CollisionParticles(Constraint.ParticleIndex))
		{
			for (uint32 j = 0; j < InParticles.CollisionParticles(Constraint.ParticleIndex)->Size(); ++j)
			{
				UpdateLevelsetConstraintHelper(InParticles, j, LocalToWorld1, LocalToWorld2, Thickness, Constraint);
			}
		}
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateLevelsetConstraintGJK(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	static int32 MaxIterationsGJK = 100;
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> LocalToWorld1 = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> LocalToWorld2 = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	TVector<T, d> Direction = LocalToWorld1.GetTranslation() - LocalToWorld2.GetTranslation();
	TVector<T, d> SupportA = LocalToWorld1.TransformPosition(InParticles.Geometry(Constraint.ParticleIndex)->Support(LocalToWorld1.InverseTransformVector(-Direction), Thickness));
	TVector<T, d> SupportB = LocalToWorld2.TransformPosition(InParticles.Geometry(Constraint.LevelsetIndex)->Support(LocalToWorld2.InverseTransformVector(Direction), Thickness));
	TVector<T, d> Point = SupportB - SupportA;
	TArray<Pair<TVector<T, d>, TVector<T, d>>> Points = {MakePair(Point, SupportA)};
	Direction = -Point;
	for (int32 i = 0; i < MaxIterationsGJK; ++i)
	{
		SupportA = LocalToWorld1.TransformPosition(InParticles.Geometry(Constraint.ParticleIndex)->Support(LocalToWorld1.InverseTransformVector(-Direction), Thickness));
		SupportB = LocalToWorld2.TransformPosition(InParticles.Geometry(Constraint.LevelsetIndex)->Support(LocalToWorld2.InverseTransformVector(Direction), Thickness));
		Point = SupportB - SupportA;
		if (TVector<T, d>::DotProduct(Point, Direction) < 0)
		{
			break;
		}
		Points.Add(MakePair(Point, SupportA));
		TVector<T, d> ClosestPoint;
		if (NearestPoint(Points, Direction))
		{
			for (const auto& SinglePoint : Points)
			{
				TVector<T, d> Body1Location = LocalToWorld1.InverseTransformPosition(SinglePoint.Second);
				TVector<T, d> Normal;
				T Phi = InParticles.Geometry(Constraint.ParticleIndex)->PhiWithNormal(Body1Location, Normal);
				Normal = LocalToWorld1.TransformVector(Normal);
				TVector<T, d> SurfacePoint = SinglePoint.Second - Phi * Normal;
				Constraint.Location.Add(SurfacePoint);
				TVector<T, d> Body2Location = LocalToWorld2.InverseTransformPosition(SurfacePoint);
				Constraint.Phi.Add(InParticles.Geometry(Constraint.LevelsetIndex)->PhiWithNormal(Body2Location, Normal));
				Constraint.Normal.Add(LocalToWorld2.TransformVector(Normal));
			}
			break;
		}
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateBoxConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> Box1Transform = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> Box2Transform = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	const auto& Box1 = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TBox<T, d>>();
	const auto& Box2 = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TBox<T, d>>();
	auto Box2SpaceBox1 = Box1.TransformedBox(Box1Transform * Box2Transform.Inverse());
	auto Box1SpaceBox2 = Box2.TransformedBox(Box2Transform * Box1Transform.Inverse());
	Box2SpaceBox1.Thicken(Thickness);
	Box1SpaceBox2.Thicken(Thickness);
	if (Box1SpaceBox2.Intersects(Box1) && Box2SpaceBox1.Intersects(Box2))
	{
		const TVector<T, d> Box1Center = (Box1Transform * Box2Transform.Inverse()).TransformPosition(Box1.Center());
		if (Box2.SignedDistance(Box1Center) < 0)
		{
			TSphere<T, d> Sphere1(Box1Transform.TransformPosition(Box1.Center()), Box1.Extents().Min() / 2);
			TSphere<T, d> Sphere2(Box2Transform.TransformPosition(Box2.Center()), Box2.Extents().Min() / 2);
			const TVector<T, d> Direction = Sphere1.Center() - Sphere2.Center();
			T Size = Direction.Size();
			if (Size < (Sphere1.Radius() + Sphere2.Radius()))
			{
				TVector<T, d> Normal = Size > SMALL_NUMBER ? Direction / Size : TVector<T, d>(0, 0, 1);
				Constraint.Normal.Add(Normal);
				Constraint.Phi.Add(Size - (Sphere1.Radius() + Sphere2.Radius()));
				Constraint.Location.Add(Sphere1.Center() - Sphere1.Radius() * Normal);
			}
		}
		if (!Constraint.Phi.Num())
		{
			//UpdateLevelsetConstraintGJK(InParticles, Thickness, Constraint);
			//check(Constraint.Phi < MThickness);
			// For now revert to doing all points vs lsv check until we can figure out a good way to get the deepest point without needing this
			UpdateLevelsetConstraint(InParticles, Thickness, Constraint);
		}
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateBoxPlaneConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> BoxTransform = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> PlaneTransform = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	const auto& ObjectBox = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TBox<T, d>>();
	const auto& ObjectPlane = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TPlane<T, d>>();
	const TRigidTransform<T, d> BoxToPlaneTransform(BoxTransform * PlaneTransform.Inverse());
	const TVector<T, d> Extents = ObjectBox.Extents();
	TArray<TVector<T, d>> Corners;
	Corners.Add(BoxToPlaneTransform.TransformPosition(ObjectBox.Max()));
	Corners.Add(BoxToPlaneTransform.TransformPosition(ObjectBox.Min()));
	for (int32 j = 0; j < d; ++j)
	{
		Corners.Add(BoxToPlaneTransform.TransformPosition(ObjectBox.Min() + TVector<T, d>::AxisVector(j) * Extents));
		Corners.Add(BoxToPlaneTransform.TransformPosition(ObjectBox.Max() - TVector<T, d>::AxisVector(j) * Extents));
	}
	TArray<TVector<T, d>> PotentialConstraints;
	for (int32 i = 0; i < Corners.Num(); ++i)
	{
		TVector<T, d> Normal;
		const T NewPhi = ObjectPlane.PhiWithNormal(Corners[i], Normal);
		if (NewPhi < Thickness)
		{
			Constraint.Phi.Add(NewPhi);
			Constraint.Normal.Add(PlaneTransform.TransformVector(Normal));
			Constraint.Location.Add(PlaneTransform.TransformPosition(Corners[i]));
		}
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateSphereConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> Sphere1Transform = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> Sphere2Transform = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	const auto& Sphere1 = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TSphere<T, d>>();
	const auto& Sphere2 = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TSphere<T, d>>();
	const TVector<T, d> Center1 = Sphere1Transform.TransformPosition(Sphere1.Center());
	const TVector<T, d> Center2 = Sphere2Transform.TransformPosition(Sphere2.Center());
	const TVector<T, d> Direction = Center1 - Center2;
	const T Size = Direction.Size();
	if (Size < (Sphere1.Radius() + Sphere2.Radius() + Thickness))
	{
		TVector<T, d> Normal = Size > SMALL_NUMBER ? Direction / Size : TVector<T, d>(0, 0, 1);
		Constraint.Normal.Add(Normal);
		Constraint.Phi.Add(Size - (Sphere1.Radius() + Sphere2.Radius()));
		Constraint.Location.Add(Center1 - Sphere1.Radius() * Normal);
	}
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateSpherePlaneConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> SphereTransform = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> PlaneTransform = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	const auto& ObjectSphere = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TSphere<T, d>>();
	const auto& ObjectPlane = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TPlane<T, d>>();
	const TRigidTransform<T, d> SphereToPlaneTransform(PlaneTransform.Inverse() * SphereTransform);
	const TVector<T, d> SphereCenter = SphereToPlaneTransform.TransformPosition(ObjectSphere.Center());
	Constraint.Normal.SetNum(1);
	Constraint.Phi.Add(ObjectPlane.PhiWithNormal(SphereCenter, Constraint.Normal[0]));
	Constraint.Phi[0] -= ObjectSphere.Radius();
	Constraint.Location.Add(SphereCenter - Constraint.Normal[0] * ObjectSphere.Radius());
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateSphereBoxConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Normal.Reset();
	Constraint.Phi.Reset();
	Constraint.Location.Reset();
	const TRigidTransform<T, d> SphereTransform = GetTransformPGS(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> BoxTransform = GetTransformPGS(InParticles, Constraint.LevelsetIndex);
	const auto& ObjectSphere = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TSphere<T, d>>();
	const auto& ObjectBox = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TBox<T, d>>();
	const TRigidTransform<T, d> SphereToBoxTransform(SphereTransform * BoxTransform.Inverse());
	const TVector<T, d> SphereCenter = SphereToBoxTransform.TransformPosition(ObjectSphere.Center());
	Constraint.Normal.SetNum(1);
	Constraint.Phi.Add(ObjectBox.PhiWithNormal(SphereCenter, Constraint.Normal[0]));
	Constraint.Phi[0] -= ObjectSphere.Radius();
	Constraint.Location.Add(SphereCenter - Constraint.Normal[0] * ObjectSphere.Radius());
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex, const T Thickness)
{
	if (!InParticles.CollisionParticlesSize(ParticleIndex))
	{
		int32 TmpIndex = ParticleIndex;
		ParticleIndex = LevelsetIndex;
		LevelsetIndex = TmpIndex;
	}
	// Find Deepest Point
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = ParticleIndex;
	Constraint.LevelsetIndex = LevelsetIndex;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex, const T Thickness)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = ParticleIndex;
	Constraint.LevelsetIndex = LevelsetIndex;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Box1Index, int32 Box2Index, const T Thickness)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = Box1Index;
	Constraint.LevelsetIndex = Box2Index;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 BoxIndex, int32 PlaneIndex, const T Thickness)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = BoxIndex;
	Constraint.LevelsetIndex = PlaneIndex;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Sphere1Index, int32 Sphere2Index, const T Thickness)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = Sphere1Index;
	Constraint.LevelsetIndex = Sphere2Index;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 PlaneIndex, const T Thickness)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = SphereIndex;
	Constraint.LevelsetIndex = PlaneIndex;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 BoxIndex, const T Thickness)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = SphereIndex;
	Constraint.LevelsetIndex = BoxIndex;
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraintPGS<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraintPGS<T, d>::ComputeConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Body1Index, int32 Body2Index, const T Thickness)
{
	if (InParticles.Geometry(Body1Index)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TBox<T, d>::GetType())
	{
		return ComputeBoxConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TSphere<T, d>::GetType())
	{
		return ComputeSphereConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType())
	{
		return ComputeBoxPlaneConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
	else if (InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Body1Index)->GetType() == TBox<T, d>::GetType())
	{
		return ComputeBoxPlaneConstraint(InParticles, Body2Index, Body1Index, Thickness);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType())
	{
		return ComputeSpherePlaneConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
	else if (InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType())
	{
		return ComputeSpherePlaneConstraint(InParticles, Body2Index, Body1Index, Thickness);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TBox<T, d>::GetType())
	{
		return ComputeSphereBoxConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
	else if (InParticles.Geometry(Body2Index)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType())
	{
		return ComputeSphereBoxConstraint(InParticles, Body2Index, Body1Index, Thickness);
	}
	else if (InParticles.Geometry(Body1Index)->IsConvex() && InParticles.Geometry(Body2Index)->IsConvex())
	{
		return ComputeLevelsetConstraintGJK(InParticles, Body1Index, Body2Index, Thickness);
	}
	return ComputeLevelsetConstraint(InParticles, Body1Index, Body2Index, Thickness);
}

template<class T, int d>
template<class T_PARTICLES>
void TPBDCollisionConstraintPGS<T, d>::UpdateConstraint(const T_PARTICLES& InParticles, const T Thickness, FRigidBodyContactConstraint& Constraint)
{
	if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TBox<T, d>::GetType())
	{
		UpdateBoxConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TSphere<T, d>::GetType())
	{
		UpdateSphereConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TPlane<T, d>::GetType())
	{
		UpdateBoxPlaneConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TPlane<T, d>::GetType())
	{
		UpdateSpherePlaneConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TBox<T, d>::GetType())
	{
		UpdateSphereBoxConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TBox<T, d>::GetType())
	{
		int32 Tmp = Constraint.ParticleIndex;
		Constraint.ParticleIndex = Constraint.LevelsetIndex;
		Constraint.LevelsetIndex = Tmp;
		UpdateBoxPlaneConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TSphere<T, d>::GetType())
	{
		int32 Tmp = Constraint.ParticleIndex;
		Constraint.ParticleIndex = Constraint.LevelsetIndex;
		Constraint.LevelsetIndex = Tmp;
		UpdateSpherePlaneConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TSphere<T, d>::GetType())
	{
		int32 Tmp = Constraint.ParticleIndex;
		Constraint.ParticleIndex = Constraint.LevelsetIndex;
		Constraint.LevelsetIndex = Tmp;
		UpdateSphereBoxConstraint(InParticles, Thickness, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->IsConvex() && InParticles.Geometry(Constraint.LevelsetIndex)->IsConvex())
	{
		UpdateLevelsetConstraintGJK(InParticles, Thickness, Constraint);
	}
	else
	{
		UpdateLevelsetConstraint(InParticles, Thickness, Constraint);
	}
}

template<class T, int d>
bool TPBDCollisionConstraintPGS<T, d>::SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, int32& IslandSleepCount, const int32 Island, const T LinearSleepThreshold, const T AngularSleepThreshold) const
{
	return MContactGraph.SleepInactive(InParticles, ActiveIndices, IslandSleepCount, Island, LinearSleepThreshold, AngularSleepThreshold);
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TArray<int32>& IslandSleepCounts, TSet<int32>& ActiveIndices)
{
	MContactGraph.UpdateIslandsFromConstraints(InParticles, IslandParticles, IslandSleepCounts, ActiveIndices, MConstraints);
}

template<class T, int d>
void TPBDCollisionConstraintPGS<T, d>::UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island)
{
	// @todo(mlentine): Do we need to do anything here?
}

template class Chaos::TPBDCollisionConstraintPGS<float, 3>;
