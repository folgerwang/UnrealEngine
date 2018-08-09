// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Apeiron/PBDCollisionConstraint.h"

#include "Apeiron/BoundingVolume.h"
#include "Apeiron/BoundingVolumeHierarchy.h"
#include "Apeiron/Defines.h"
#include "Apeiron/PBDContactGraph.h"
#include "Apeiron/Sphere.h"
#include "Apeiron/Transform.h"

#include "Async/ParallelFor.h"
#include "ProfilingDebugging/ScopedTimers.h"

#define USE_SHOCK_PROPOGATION 0

using namespace Apeiron;

template<class T, int d>
TPBDCollisionConstraint<T, d>::TPBDCollisionConstraint(TPBDRigidParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const int32 PushOutIterations /*= 1*/, const int32 PushOutPairIterations /*= 1*/, const T Thickness /*= (T)0*/, const T Restitution /*= (T)0*/, const T Friction /*= (T)0*/)
    : MCollided(Collided), MContactGraph(InParticles), MNumIterations(PushOutIterations), MPairIterations(PushOutPairIterations), MThickness(Thickness), MRestitution(Restitution), MFriction(Friction)
{
	MContactGraph.Initialize(InParticles.Size());
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::ComputeConstraints(const TPBDRigidParticles<T, d>& InParticles)
{
	double Time = 0;
	FDurationTimer Timer(Time);
	// Broad phase
	//TBoundingVolumeHierarchy<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles);
	TBoundingVolume<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDCollisionConstraint Construct Hierarchy %f"), Time);
	// Narrow phase
	FCriticalSection CriticalSection;
	Time = 0;
	Timer.Start();
	ParallelFor(InParticles.Size(), [&](int32 Body1Index) {
		if (InParticles.Disabled(Body1Index))
			return;
		TArray<int32> PotentialIntersections;
		TBox<T, d> Box1 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body1Index);
		// Thicken to pick up resting contact
		Box1.Thicken(MThickness + KINDA_SMALL_NUMBER);
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
			auto Constraint = ComputeConstraint(InParticles, Body1Index, Body2Index);
			if (Constraint.Phi < (MThickness + KINDA_SMALL_NUMBER))
			{
				CriticalSection.Lock();
				MConstraints.Add(Constraint);
				CriticalSection.Unlock();
			}
		}
	});
	MContactGraph.ComputeGraph(InParticles, MConstraints);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDCollisionConstraint Construct %d Constraints with Potential Collisions %f"), MConstraints.Num(), Time);
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::RemoveConstraints(const TSet<uint32>& RemovedParticles)
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
void TPBDCollisionConstraint<T, d>::UpdateConstraints(const TPBDRigidParticles<T, d>& InParticles, const TSet<uint32>& AddedParticles, const TArray<uint32>& ActiveParticles)
{
	double Time = 0;
	FDurationTimer Timer(Time);

	//
	// Broad phase
	//

	// @todo(mlentine): We only need to construct the hierarchy for the islands we care about
	TBoundingVolume<TGeometryParticles<T, d>, T, d> Hierarchy(static_cast<const TGeometryParticles<T, d>&>(InParticles), ActiveParticles);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDCollisionConstraint Construct Hierarchy %f"), Time);

	//
	// Narrow phase
	//

	FCriticalSection CriticalSection;
	Time = 0;
	Timer.Start();
	TArray<uint32> AddedParticlesArray = AddedParticles.Array();
	ParallelFor(AddedParticlesArray.Num(), [&](int32 Index) {
		int32 Body1Index = AddedParticlesArray[Index];
		if (InParticles.Disabled(Body1Index))
			return;
		TArray<int32> PotentialIntersections;
		const auto& Box1 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body1Index);
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
			auto Constraint = ComputeConstraint(InParticles, Body1Index, Body2Index);
			if (Constraint.Phi < (MThickness + KINDA_SMALL_NUMBER))
			{
				CriticalSection.Lock();
				MConstraints.Add(Constraint);
				CriticalSection.Unlock();
			}
		}
	});
	MContactGraph.Reset(InParticles, MConstraints);
	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("\tPBDCollisionConstraint Update %d Constraints with Potential Collisions %f"), MConstraints.Num(), Time);
}

template<class T>
PMatrix<T, 3, 3> ComputeFactorMatrix(const TVector<T, 3>& V, const PMatrix<T, 3, 3>& M, const T& Im)
{
	// Rigid objects rotational contribution to the impulse.
	// Vx*M*VxT+Im
	check(Im > FLT_MIN)
	    T yy = V[1] * V[1],
	      zz = V[2] * V[2], bx = M.M[1][1] * V[0], cx = M.M[2][2] * V[0];
	return PMatrix<T, 3, 3>(
	    M.M[1][1] * zz + M.M[2][2] * yy + Im, -cx * V[1], -bx * V[2], M.M[0][0] * zz + cx * V[0] + Im,
	    -V[1] * V[2] * M.M[0][0], M.M[0][0] * yy + bx * V[0] + Im);
}

template<class T, int d>
TVector<T, d> GetEnergyClampedImpulse(const TPBDRigidParticles<T, d>& InParticles, const TRigidBodyContactConstraint<T, d>& Constraint, const TVector<T, d>& Impulse, const TVector<T, d>& VectorToPoint1, const TVector<T, d>& VectorToPoint2, const TVector<T, d>& Velocity1, const TVector<T, d>& Velocity2)
{
	TVector<T, d> Jr0, Jr1, IInvJr0, IInvJr1;
	T ImpulseRatioNumerator0 = 0, ImpulseRatioNumerator1 = 0, ImpulseRatioDenom0 = 0, ImpulseRatioDenom1 = 0;
	T ImpulseSize = Impulse.SizeSquared();
	TVector<T, d> KinematicVelocity = !InParticles.InvM(Constraint.ParticleIndex) ? Velocity1 : !InParticles.InvM(Constraint.LevelsetIndex) ? Velocity2 : TVector<T, d>(0);
	if (InParticles.InvM(Constraint.ParticleIndex))
	{
		Jr0 = TVector<T, d>::CrossProduct(VectorToPoint1, Impulse);
		IInvJr0 = InParticles.R(Constraint.ParticleIndex).RotateVector(InParticles.InvI(Constraint.ParticleIndex) * InParticles.R(Constraint.ParticleIndex).UnrotateVector(Jr0));
		ImpulseRatioNumerator0 = TVector<T, d>::DotProduct(Impulse, InParticles.V(Constraint.ParticleIndex) - KinematicVelocity) + TVector<T, d>::DotProduct(IInvJr0, InParticles.I(Constraint.ParticleIndex) * InParticles.W(Constraint.ParticleIndex));
		ImpulseRatioDenom0 = ImpulseSize / InParticles.M(Constraint.ParticleIndex) + TVector<T, d>::DotProduct(Jr0, IInvJr0);
	}
	if (InParticles.InvM(Constraint.LevelsetIndex))
	{
		Jr1 = TVector<T, d>::CrossProduct(VectorToPoint2, Impulse);
		IInvJr1 = InParticles.R(Constraint.LevelsetIndex).RotateVector(InParticles.InvI(Constraint.LevelsetIndex) * InParticles.R(Constraint.LevelsetIndex).UnrotateVector(Jr1));
		ImpulseRatioNumerator1 = TVector<T, d>::DotProduct(Impulse, InParticles.V(Constraint.LevelsetIndex) - KinematicVelocity) + TVector<T, d>::DotProduct(IInvJr1, InParticles.I(Constraint.LevelsetIndex) * InParticles.W(Constraint.LevelsetIndex));
		ImpulseRatioDenom1 = ImpulseSize / InParticles.M(Constraint.LevelsetIndex) + TVector<T, d>::DotProduct(Jr1, IInvJr1);
	}
	T Numerator = -2 * (ImpulseRatioNumerator0 - ImpulseRatioNumerator1);
	if (Numerator < 0)
	{
		return TVector<T, d>(0);
	}
	check(Numerator >= 0);
	T Denominator = ImpulseRatioDenom0 + ImpulseRatioDenom1;
	return Numerator < Denominator ? (Impulse * Numerator / Denominator) : Impulse;
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island) const
{
	// @todo : The current implementation supports just the no-level approach.

	const TArray<typename TPBDContactGraph<T, d>::ContactMap>& LevelToColorToContactMap = MContactGraph.GetContactMapAt(Island);
	int32 MaxColor = MContactGraph.GetMaxColorAt(Island);
	int32 MaxLevel = MContactGraph.GetMaxLevelAt(Island);
	for (int32 Level = 0; Level <= MaxLevel; ++Level)
	{
		for (int32 i = 0; i <= MaxColor; ++i)
		{
			if (LevelToColorToContactMap[Level].Contains(i))
			{
				ParallelFor(LevelToColorToContactMap[Level][i].Num(), [&](int32 ConstraintIndex) {
					auto Constraint = LevelToColorToContactMap[Level][i][ConstraintIndex];
					if (InParticles.Sleeping(Constraint.ParticleIndex))
					{
						check(InParticles.Sleeping(Constraint.LevelsetIndex));
						return;
					}

					MCollided[Constraint.LevelsetIndex] = true;
					MCollided[Constraint.ParticleIndex] = true;

					TVector<T, d> VectorToPoint1 = Constraint.Location - InParticles.X(Constraint.ParticleIndex);
					TVector<T, d> VectorToPoint2 = Constraint.Location - InParticles.X(Constraint.LevelsetIndex);
					TVector<T, d> Body1Velocity = InParticles.V(Constraint.ParticleIndex) + TVector<T, d>::CrossProduct(InParticles.W(Constraint.ParticleIndex), VectorToPoint1);
					TVector<T, d> Body2Velocity = InParticles.V(Constraint.LevelsetIndex) + TVector<T, d>::CrossProduct(InParticles.W(Constraint.LevelsetIndex), VectorToPoint2);
					TVector<T, d> RelativeVelocity = Body1Velocity - Body2Velocity;
					if (TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal) < 0) // ignore separating constraints
					{
						PMatrix<T, d, d> Factor =
							(InParticles.InvM(Constraint.ParticleIndex) > FLT_MIN ? ComputeFactorMatrix(VectorToPoint1, InParticles.InvI(Constraint.ParticleIndex), InParticles.InvM(Constraint.ParticleIndex)) : PMatrix<T, d, d>(0)) +
							(InParticles.InvM(Constraint.LevelsetIndex) > FLT_MIN ? ComputeFactorMatrix(VectorToPoint2, InParticles.InvI(Constraint.LevelsetIndex), InParticles.InvM(Constraint.LevelsetIndex)) : PMatrix<T, d, d>(0));
						TVector<T, d> Impulse;
						// Resting contact if very close to the surface
						T Restitution = RelativeVelocity.SizeSquared() < (2 * 980) ? 0 : MRestitution;
						if (MFriction)
						{
							T RelativeNormalVelocity = TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal);
							if (RelativeNormalVelocity > 0)
							{
								RelativeNormalVelocity = 0;
							}
							TVector<T, d> VelocityChange = -(Restitution * RelativeNormalVelocity * Constraint.Normal + RelativeVelocity);
							T NormalVelocityChange = TVector<T, d>::DotProduct(VelocityChange, Constraint.Normal);
							PMatrix<T, d, d> FactorInverse = Factor.Inverse();
							TVector<T, d> MinimalImpulse = FactorInverse * VelocityChange;
							// Friction should stop the object
							if ((VelocityChange - NormalVelocityChange * Constraint.Normal).Size() < MFriction * NormalVelocityChange)
							{
								// @todo(mlentine): Apply Rolling Friction
								Impulse = MinimalImpulse;
							}
							else
							{
								TVector<T, d> Tangent = (RelativeVelocity - TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal) * Constraint.Normal).GetSafeNormal();
								TVector<T, d> DirectionalFactor = Factor * (Constraint.Normal - MFriction * Tangent);
								T ImpulseDenominator = TVector<T, d>::DotProduct(Constraint.Normal, DirectionalFactor);
								check(ImpulseDenominator > FLT_MIN);
								Impulse = FactorInverse * -(1 + Restitution) * RelativeNormalVelocity * DirectionalFactor / ImpulseDenominator;
							}
						}
						else
						{
							T ImpulseDenominator = TVector<T, d>::DotProduct(Constraint.Normal, Factor * Constraint.Normal);
							check(ImpulseDenominator > FLT_MIN);
							TVector<T, d> ImpulseNumerator = -(1 + Restitution) * TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal) * Constraint.Normal;
							Impulse = ImpulseNumerator / ImpulseDenominator;
						}
						Impulse = GetEnergyClampedImpulse(InParticles, Constraint, Impulse, VectorToPoint1, VectorToPoint2, Body1Velocity, Body2Velocity);
						TVector<T, d> AngularImpulse1 = TVector<T, d>::CrossProduct(VectorToPoint1, Impulse);
						TVector<T, d> AngularImpulse2 = TVector<T, d>::CrossProduct(VectorToPoint2, Impulse);
						InParticles.V(Constraint.ParticleIndex) += InParticles.InvM(Constraint.ParticleIndex) * Impulse;
						InParticles.W(Constraint.ParticleIndex) += InParticles.InvI(Constraint.ParticleIndex) * AngularImpulse1;
						InParticles.V(Constraint.LevelsetIndex) -= InParticles.InvM(Constraint.LevelsetIndex) * Impulse;
						InParticles.W(Constraint.LevelsetIndex) -= InParticles.InvI(Constraint.LevelsetIndex) * AngularImpulse2;
					}
				});
			}
		}
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::ApplyPushOut(TPBDRigidParticles<T, d>& InParticles, const int32 Island)
{
	// @todo : The current implementation supports just the no-level approach.

	const TArray<typename TPBDContactGraph<T, d>::ContactMap>& LevelToColorToContactMap = MContactGraph.GetContactMapAt(Island);
	int32 MaxColor = MContactGraph.GetMaxColorAt(Island);
	int32 MaxLevel = MContactGraph.GetMaxLevelAt(Island);

	bool NeedsAnotherIteration = false;
	TArray<bool> IsTemporarilyStatic;
	IsTemporarilyStatic.Init(false, InParticles.Size());
	for (int32 Iteration = 0; (Iteration == 0 || NeedsAnotherIteration) && Iteration < MNumIterations; ++Iteration)
	{
		NeedsAnotherIteration = false;
		for (int32 Level = 0; Level <= MaxLevel; ++Level)
		{
			for (int32 i = 0; i <= MaxColor; ++i)
			{
				if (LevelToColorToContactMap[Level].Contains(i))
				{
					ParallelFor(LevelToColorToContactMap[Level][i].Num(), [&](int32 ConstraintIndex) {
						auto Constraint = LevelToColorToContactMap[Level][i][ConstraintIndex];
						if (InParticles.Sleeping(Constraint.ParticleIndex))
						{
							check(InParticles.Sleeping(Constraint.LevelsetIndex));
							return;
						}
						for (int32 PairIteration = 0; PairIteration < MPairIterations; ++PairIteration)
						{
							UpdateConstraint(InParticles, Constraint);
							if (Constraint.Phi >= MThickness)
								break;
							NeedsAnotherIteration = true;
							TVector<T, d> VectorToPoint1 = Constraint.Location - InParticles.X(Constraint.ParticleIndex);
							TVector<T, d> VectorToPoint2 = Constraint.Location - InParticles.X(Constraint.LevelsetIndex);
							PMatrix<T, d, d> Factor =
								((InParticles.InvM(Constraint.ParticleIndex) && !IsTemporarilyStatic[Constraint.ParticleIndex]) ? ComputeFactorMatrix(VectorToPoint1, InParticles.InvI(Constraint.ParticleIndex), InParticles.InvM(Constraint.ParticleIndex)) : PMatrix<T, d, d>(0)) +
								((InParticles.InvM(Constraint.LevelsetIndex) && !IsTemporarilyStatic[Constraint.LevelsetIndex]) ? ComputeFactorMatrix(VectorToPoint2, InParticles.InvI(Constraint.LevelsetIndex), InParticles.InvM(Constraint.LevelsetIndex)) : PMatrix<T, d, d>(0));
							TVector<T, d> Impulse = PMatrix<T, d, d>(Factor.Inverse()) * ((-Constraint.Phi + MThickness) * ((T)(Iteration + 1) / (T)MNumIterations) * Constraint.Normal);
							TVector<T, d> AngularImpulse1 = TVector<T, d>::CrossProduct(VectorToPoint1, Impulse);
							TVector<T, d> AngularImpulse2 = TVector<T, d>::CrossProduct(VectorToPoint2, -Impulse);
							if (!IsTemporarilyStatic[Constraint.ParticleIndex])
							{
								InParticles.X(Constraint.ParticleIndex) += InParticles.InvM(Constraint.ParticleIndex) * Impulse;
								InParticles.R(Constraint.ParticleIndex) = TRotation<T, d>::FromVector(InParticles.InvI(Constraint.ParticleIndex) * AngularImpulse1) * InParticles.R(Constraint.ParticleIndex);
								InParticles.R(Constraint.ParticleIndex).Normalize();
							}
							if (!IsTemporarilyStatic[Constraint.LevelsetIndex])
							{
								InParticles.X(Constraint.LevelsetIndex) -= InParticles.InvM(Constraint.LevelsetIndex) * Impulse;
								InParticles.R(Constraint.LevelsetIndex) = TRotation<T, d>::FromVector(InParticles.InvI(Constraint.LevelsetIndex) * AngularImpulse2) * InParticles.R(Constraint.LevelsetIndex);
								InParticles.R(Constraint.LevelsetIndex).Normalize();
							}
						}
					});
				}
			}
#if USE_SHOCK_PROPOGATION
			for (int32 i = 0; i <= MaxColor; ++i)
			{
				if (LevelToColorToContactMap[Level].Contains(i))
				{
					ParallelFor(LevelToColorToContactMap[Level][i].Num(), [&](int32 ConstraintIndex) {
						auto Constraint = LevelToColorToContactMap[Level][i][ConstraintIndex];
						if (Iteration == MNumIterations - 1)
						{
							if (!InParticles.InvM(Constraint.ParticleIndex) || IsTemporarilyStatic[Constraint.ParticleIndex])
							{
								IsTemporarilyStatic[Constraint.LevelsetIndex] = true;
							}
							if (!InParticles.InvM(Constraint.LevelsetIndex) || IsTemporarilyStatic[Constraint.LevelsetIndex])
							{
								IsTemporarilyStatic[Constraint.ParticleIndex] = true;
							}
						}
					});
				}
			}
#endif
		}
	}
}

template<class T, int d>
bool NearestPoint(TArray<Pair<TVector<T, d>, TVector<T, d>>>& Points, TVector<T, d>& Direction, TVector<T, d>& ClosestPoint)
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
			ClosestPoint = (1 - Alpha) * Points[0].Second + Alpha * Points[1].Second;
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
					ClosestPoint = Points[0].Second * Bary.X + Points[1].Second * Bary.Y + Points[2].Second * Bary.Z;
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
		check(Points.Num() > 1 && Points.Num() < 4);
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
			return NearestPoint(Points, Direction, ClosestPoint);
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
			return NearestPoint(Points, Direction, ClosestPoint);
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
			return NearestPoint(Points, Direction, ClosestPoint);
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
		ClosestPoint = (Bary[0] * Points[0].Second + Bary[1] * Points[1].Second + Bary[2] * Points[2].Second + Bary[3] * Points[3].Second) / Denom;
		return true;
	}
	check(Points.Num() > 1 && Points.Num() < 4);
	return false;
}

template<class T, int d>
void UpdateLevelsetConstraintHelper(const TPBDRigidParticles<T, d>& InParticles, const int32 j, const TRigidTransform<T, d>& LocalToWorld1, const TRigidTransform<T, d>& LocalToWorld2, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	TVector<T, d> WorldSpacePoint = LocalToWorld1.TransformPosition(InParticles.CollisionParticles(Constraint.ParticleIndex).X(j));
	TVector<T, d> Body2SpacePoint = LocalToWorld2.InverseTransformPosition(WorldSpacePoint);
	TVector<T, d> LocalNormal;
	T LocalPhi = InParticles.Geometry(Constraint.LevelsetIndex)->PhiWithNormal(Body2SpacePoint, LocalNormal);
	if (LocalPhi < Thickness && LocalPhi < Constraint.Phi)
	{
		Constraint.Phi = LocalPhi;
		Constraint.Normal = LocalToWorld2.TransformVector(LocalNormal);
		Constraint.Location = WorldSpacePoint;
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Phi = MThickness + KINDA_SMALL_NUMBER;
	TRigidTransform<T, d> LocalToWorld1(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	TRigidTransform<T, d> LocalToWorld2(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
	if (InParticles.Geometry(Constraint.LevelsetIndex)->HasBoundingBox())
	{
		TBox<T, d> ImplicitBox = InParticles.Geometry(Constraint.LevelsetIndex)->BoundingBox().TransformedBox(LocalToWorld2 * LocalToWorld1.Inverse());
		TArray<int32> PotentialParticles = InParticles.CollisionParticles(Constraint.ParticleIndex).FindAllIntersections(ImplicitBox);
		for (int32 j = 0; j < PotentialParticles.Num(); ++j)
		{
			UpdateLevelsetConstraintHelper(InParticles, PotentialParticles[j], LocalToWorld1, LocalToWorld2, MThickness, Constraint);
		}
	}
	else
	{
		for (uint32 j = 0; j < InParticles.CollisionParticles(Constraint.ParticleIndex).Size(); ++j)
		{
			UpdateLevelsetConstraintHelper(InParticles, j, LocalToWorld1, LocalToWorld2, MThickness, Constraint);
		}
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	static int32 MaxIterations = 100;
	Constraint.Phi = MThickness + KINDA_SMALL_NUMBER;
	TRigidTransform<T, d> LocalToWorld1(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	TRigidTransform<T, d> LocalToWorld2(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
	TVector<T, d> Direction = LocalToWorld1.GetTranslation() - LocalToWorld2.GetTranslation();
	TVector<T, d> SupportA = LocalToWorld1.TransformPosition(InParticles.Geometry(Constraint.ParticleIndex)->Support(LocalToWorld1.InverseTransformVector(-Direction)));
	TVector<T, d> SupportB = LocalToWorld2.TransformPosition(InParticles.Geometry(Constraint.LevelsetIndex)->Support(LocalToWorld2.InverseTransformVector(Direction)));
	TVector<T, d> Point = SupportB - SupportA;
	TArray<Pair<TVector<T, d>, TVector<T, d>>> Points = {MakePair(Point, SupportA)};
	Direction = -Point;
	for (int32 i = 0; i < MaxIterations; ++i)
	{
		SupportA = LocalToWorld1.TransformPosition(InParticles.Geometry(Constraint.ParticleIndex)->Support(LocalToWorld1.InverseTransformVector(-Direction)));
		SupportB = LocalToWorld2.TransformPosition(InParticles.Geometry(Constraint.LevelsetIndex)->Support(LocalToWorld2.InverseTransformVector(Direction)));
		Point = SupportB - SupportA;
		if (TVector<T, d>::DotProduct(Point, Direction) < 0)
		{
			break;
		}
		Points.Add(MakePair(Point, SupportA));
		TVector<T, d> ClosestPoint;
		if (NearestPoint(Points, Direction, ClosestPoint))
		{
			TVector<T, d> Body1Location = LocalToWorld1.InverseTransformPosition(ClosestPoint);
			TVector<T, d> Normal;
			T Phi = InParticles.Geometry(Constraint.ParticleIndex)->PhiWithNormal(Body1Location, Normal);
			Normal = LocalToWorld1.TransformVector(Normal);
			Constraint.Location = ClosestPoint - Phi * Normal;
			TVector<T, d> Body2Location = LocalToWorld2.InverseTransformPosition(Constraint.Location);
			Constraint.Phi = InParticles.Geometry(Constraint.LevelsetIndex)->PhiWithNormal(Body2Location, Constraint.Normal);
			Constraint.Normal = LocalToWorld2.TransformVector(Constraint.Normal);
			break;
		}
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Phi = MThickness + KINDA_SMALL_NUMBER;
	const TRigidTransform<T, d> Box1Transform(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	const TRigidTransform<T, d> Box2Transform(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
	const auto& Box1 = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TBox<T, d>>();
	const auto& Box2 = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TBox<T, d>>();
	const auto Box2SpaceBox1 = Box1.TransformedBox(Box1Transform * Box2Transform.Inverse());
	const auto Box1SpaceBox2 = Box2.TransformedBox(Box2Transform * Box1Transform.Inverse());
	if (Box1SpaceBox2.Intersects(Box1) && Box2SpaceBox1.Intersects(Box2))
	{
		const TVector<T, d> Box1Center = (Box1Transform * Box2Transform.Inverse()).TransformPosition(Box1.Center());
		if (Box2.SignedDistance(Box1Center) < MThickness)
		{
			TSphere<T, d> Sphere1(Box1Transform.TransformPosition(Box1.Center()), Box1.Extents().Min() / 2);
			TSphere<T, d> Sphere2(Box2Transform.TransformPosition(Box2.Center()), Box2.Extents().Min() / 2);
			const TVector<T, d> Direction = Sphere1.Center() - Sphere2.Center();
			if (Direction.Size() < (Sphere1.Radius() + Sphere2.Radius()))
			{
				Constraint.Normal = Direction.GetSafeNormal();
				Constraint.Location = Sphere1.Center() - Sphere1.Radius() * Constraint.Normal;
				Constraint.Phi = TVector<T, d>(Constraint.Location - Sphere2.Center()).Size() - Sphere2.Radius();
			}
		}
		if (Constraint.Phi >= MThickness)
		{
			UpdateLevelsetConstraintGJK(InParticles, Constraint);
			//check(Constraint.Phi < MThickness);
			// For now revert to doing all points vs lsv check until we can figure out a good way to get the deepest point without needing this
			// UpdateLevelsetConstraint(InParticles, Constraint);
		}
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	static T Epsilon = 1e-4;
	Constraint.Phi = MThickness + KINDA_SMALL_NUMBER;
	const TRigidTransform<T, d> BoxTransform(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	const TRigidTransform<T, d> PlaneTransform(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
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
		if (NewPhi < Constraint.Phi + Epsilon)
		{
			if (NewPhi <= Constraint.Phi - Epsilon)
			{
				PotentialConstraints.Reset();
			}
			Constraint.Phi = NewPhi;
			Constraint.Normal = PlaneTransform.TransformVector(Normal);
			Constraint.Location = PlaneTransform.TransformPosition(Corners[i]);
			PotentialConstraints.Add(Constraint.Location);
		}
	}
	if (PotentialConstraints.Num() > 1)
	{
		TVector<T, d> AverageLocation(0);
		for (const auto& Location : PotentialConstraints)
		{
			AverageLocation += Location;
		}
		Constraint.Location = AverageLocation / PotentialConstraints.Num();
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Phi = MThickness + KINDA_SMALL_NUMBER;
	const TRigidTransform<T, d> Sphere1Transform(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	const TRigidTransform<T, d> Sphere2Transform(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
	const auto& Sphere1 = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TSphere<T, d>>();
	const auto& Sphere2 = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TSphere<T, d>>();
	const TVector<T, d> Center1 = Sphere1Transform.TransformPosition(Sphere1.Center());
	const TVector<T, d> Center2 = Sphere2Transform.TransformPosition(Sphere2.Center());
	const TVector<T, d> Direction = Center1 - Center2;
	if (Direction.Size() < (Sphere1.Radius() + Sphere2.Radius()))
	{
		Constraint.Normal = Direction.GetSafeNormal();
		Constraint.Location = Center1 - Sphere1.Radius() * Constraint.Normal;
		Constraint.Phi = TVector<T, d>(Constraint.Location - Center2).Size() - Sphere2.Radius();
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	Constraint.Phi = MThickness + KINDA_SMALL_NUMBER;
	const TRigidTransform<T, d> SphereTransform(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	const TRigidTransform<T, d> PlaneTransform(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
	const auto& ObjectSphere = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TSphere<T, d>>();
	const auto& ObjectPlane = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TPlane<T, d>>();
	const TRigidTransform<T, d> SphereToPlaneTransform(PlaneTransform.Inverse() * SphereTransform);
	const TVector<T, d> SphereCenter = SphereToPlaneTransform.TransformPosition(ObjectSphere.Center());
	Constraint.Phi = ObjectPlane.PhiWithNormal(SphereCenter, Constraint.Normal);
	Constraint.Phi -= ObjectSphere.Radius();
	Constraint.Location = SphereCenter - Constraint.Normal * ObjectSphere.Radius();
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	const TRigidTransform<T, d> SphereTransform(InParticles.X(Constraint.ParticleIndex), InParticles.R(Constraint.ParticleIndex));
	const TRigidTransform<T, d> BoxTransform(InParticles.X(Constraint.LevelsetIndex), InParticles.R(Constraint.LevelsetIndex));
	const auto& ObjectSphere = *InParticles.Geometry(Constraint.ParticleIndex)->template GetObject<TSphere<T, d>>();
	const auto& ObjectBox = *InParticles.Geometry(Constraint.LevelsetIndex)->template GetObject<TBox<T, d>>();
	const TRigidTransform<T, d> SphereToBoxTransform(SphereTransform * BoxTransform.Inverse());
	const TVector<T, d> SphereCenter = SphereToBoxTransform.TransformPosition(ObjectSphere.Center());
	Constraint.Phi = ObjectBox.PhiWithNormal(SphereCenter, Constraint.Normal);
	Constraint.Phi -= ObjectSphere.Radius();
	Constraint.Location = SphereCenter - Constraint.Normal * ObjectSphere.Radius();
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex)
{
	if (!InParticles.CollisionParticles(ParticleIndex).Size())
	{
		int32 TmpIndex = ParticleIndex;
		ParticleIndex = LevelsetIndex;
		LevelsetIndex = TmpIndex;
	}
	// Find Deepest Point
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = ParticleIndex;
	Constraint.LevelsetIndex = LevelsetIndex;
	UpdateLevelsetConstraint(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = ParticleIndex;
	Constraint.LevelsetIndex = LevelsetIndex;
	UpdateLevelsetConstraintGJK(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Box1Index, int32 Box2Index)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = Box1Index;
	Constraint.LevelsetIndex = Box2Index;
	UpdateBoxConstraint(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 BoxIndex, int32 PlaneIndex)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = BoxIndex;
	Constraint.LevelsetIndex = PlaneIndex;
	UpdateBoxPlaneConstraint(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Sphere1Index, int32 Sphere2Index)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = Sphere1Index;
	Constraint.LevelsetIndex = Sphere2Index;
	UpdateSphereConstraint(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 PlaneIndex)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = SphereIndex;
	Constraint.LevelsetIndex = PlaneIndex;
	UpdateSpherePlaneConstraint(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 BoxIndex)
{
	FRigidBodyContactConstraint Constraint;
	Constraint.ParticleIndex = SphereIndex;
	Constraint.LevelsetIndex = BoxIndex;
	UpdateSphereBoxConstraint(InParticles, Constraint);
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Body1Index, int32 Body2Index)
{
	if (InParticles.Geometry(Body1Index)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TBox<T, d>::GetType())
	{
		return ComputeBoxConstraint(InParticles, Body1Index, Body2Index);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TSphere<T, d>::GetType())
	{
		return ComputeSphereConstraint(InParticles, Body1Index, Body2Index);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType())
	{
		return ComputeBoxPlaneConstraint(InParticles, Body1Index, Body2Index);
	}
	else if (InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Body1Index)->GetType() == TBox<T, d>::GetType())
	{
		return ComputeBoxPlaneConstraint(InParticles, Body2Index, Body1Index);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType())
	{
		return ComputeSpherePlaneConstraint(InParticles, Body1Index, Body2Index);
	}
	else if (InParticles.Geometry(Body2Index)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType())
	{
		return ComputeSpherePlaneConstraint(InParticles, Body2Index, Body1Index);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TBox<T, d>::GetType())
	{
		return ComputeSphereBoxConstraint(InParticles, Body1Index, Body2Index);
	}
	else if (InParticles.Geometry(Body2Index)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Body1Index)->GetType() == TSphere<T, d>::GetType())
	{
		return ComputeSphereBoxConstraint(InParticles, Body2Index, Body1Index);
	}
	else if (InParticles.Geometry(Body1Index)->IsConvex() && InParticles.Geometry(Body2Index)->IsConvex())
	{
		return ComputeLevelsetConstraintGJK(InParticles, Body1Index, Body2Index);
	}
	return ComputeLevelsetConstraint(InParticles, Body1Index, Body2Index);
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateConstraint(const TPBDRigidParticles<T, d>& InParticles, FRigidBodyContactConstraint& Constraint)
{
	if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TBox<T, d>::GetType())
	{
		UpdateBoxConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TSphere<T, d>::GetType())
	{
		UpdateSphereConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TPlane<T, d>::GetType())
	{
		UpdateBoxPlaneConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TPlane<T, d>::GetType())
	{
		UpdateSpherePlaneConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TSphere<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TBox<T, d>::GetType())
	{
		UpdateSphereBoxConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TBox<T, d>::GetType())
	{
		int32 Tmp = Constraint.ParticleIndex;
		Constraint.ParticleIndex = Constraint.LevelsetIndex;
		Constraint.LevelsetIndex = Tmp;
		UpdateBoxPlaneConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TPlane<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TSphere<T, d>::GetType())
	{
		int32 Tmp = Constraint.ParticleIndex;
		Constraint.ParticleIndex = Constraint.LevelsetIndex;
		Constraint.LevelsetIndex = Tmp;
		UpdateSpherePlaneConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->GetType() == TBox<T, d>::GetType() && InParticles.Geometry(Constraint.LevelsetIndex)->GetType() == TSphere<T, d>::GetType())
	{
		int32 Tmp = Constraint.ParticleIndex;
		Constraint.ParticleIndex = Constraint.LevelsetIndex;
		Constraint.LevelsetIndex = Tmp;
		UpdateSphereBoxConstraint(InParticles, Constraint);
	}
	else if (InParticles.Geometry(Constraint.ParticleIndex)->IsConvex() && InParticles.Geometry(Constraint.LevelsetIndex)->IsConvex())
	{
		UpdateLevelsetConstraintGJK(InParticles, Constraint);
	}
	else
	{
		UpdateLevelsetConstraint(InParticles, Constraint);
	}
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, TSet<int32>& GlobalActiveIndices, const int32 Island) const
{
	MContactGraph.SleepInactive(InParticles, ActiveIndices, GlobalActiveIndices, Island);
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TSet<int32>& ActiveIndices)
{
	MContactGraph.UpdateIslandsFromConstraints(InParticles, IslandParticles, ActiveIndices, MConstraints);
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island)
{
	MContactGraph.UpdateAccelerationStructures(InParticles, ActiveIndices, Island);
}

template class Apeiron::TPBDCollisionConstraint<float, 3>;
