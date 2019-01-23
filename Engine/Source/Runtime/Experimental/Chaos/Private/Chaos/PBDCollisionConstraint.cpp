// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDCollisionConstraint.h"

#include "Chaos/BoundingVolume.h"
#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/Defines.h"
#include "Chaos/Pair.h"
#include "Chaos/PBDContactGraph.h"
#include "Chaos/Sphere.h"
#include "Chaos/Transform.h"
#include "ChaosStats.h"
#include "ChaosLog.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/PBDCollisionTypes.h"

#define USE_SHOCK_PROPOGATION 1

int32 CollisionParticlesBVHDepth = 4;
FAutoConsoleVariableRef CVarCollisionParticlesBVHDepth(TEXT("p.CollisionParticlesBVHDepth"), CollisionParticlesBVHDepth, TEXT("The maximum depth for collision particles bvh"));

int32 EnableCollisions = 1;
FAutoConsoleVariableRef CVarEnableCollisions(TEXT("p.EnableCollisions"), EnableCollisions, TEXT("Enable/Disable collisions on the Chaos solver."));

extern int32 UseLevelsetCollision;

namespace Chaos
{
template<class T, int d>
TPBDCollisionConstraint<T, d>::TPBDCollisionConstraint(TPBDRigidParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const int32 PushOutIterations /*= 1*/, const int32 PushOutPairIterations /*= 1*/, const T Thickness /*= (T)0*/, const T Restitution /*= (T)0*/, const T Friction /*= (T)0*/)
    : MCollided(Collided), MContactGraph(InParticles), MNumIterations(PushOutIterations), MPairIterations(PushOutPairIterations), MThickness(Thickness), MRestitution(Restitution), MFriction(Friction), MAngularFriction(0), bUseCCD(false)
{
}

DECLARE_CYCLE_STAT(TEXT("ComputeConstraints"), STAT_ComputeConstraints, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("ComputeConstraintsNP"), STAT_ComputeConstraintsNP, STATGROUP_Chaos);
//DECLARE_CYCLE_STAT(TEXT("ComputeConstraintsNP2"), STAT_ComputeConstraintsNP2, STATGROUP_Chaos);
//DECLARE_CYCLE_STAT(TEXT("ComputeConstraintsNP3"), STAT_ComputeConstraintsNP3, STATGROUP_Chaos);
template<class T, int d>
void TPBDCollisionConstraint<T, d>::ComputeConstraints(const TPBDRigidParticles<T, d>& InParticles, T Dt)
{
	SCOPE_CYCLE_COUNTER(STAT_ComputeConstraints);	
	if (!EnableCollisions) return;
	// Broad phase
//	float TotalTested = 0;
//	float TotalRejected = 0;
	TBoundingVolume<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles, true, Dt * BoundsThicknessMultiplier); 	//todo(ocohen): should we pass MThickness into this structure?
	{
		SCOPE_CYCLE_COUNTER(STAT_ComputeConstraintsNP);

		
		// Narrow phase
		FCriticalSection CriticalSection;
		PhysicsParallelFor(InParticles.Size(), [&](int32 Body1Index)
		{
			if (InParticles.Disabled(Body1Index))
			{
				return;
			}

			if (InParticles.InvM(Body1Index) == 0)
			{
				return;
			}

			//SCOPE_CYCLE_COUNTER(STAT_ComputeConstraintsNP2);

			TArray<int32> PotentialIntersections;

			const bool bBody1Bounded = InParticles.Geometry(Body1Index)->HasBoundingBox();

			TBox<T, d> Box1;
			const T Box1Thickness = ComputeThickness(InParticles, Dt, Body1Index).Size();

			if (bBody1Bounded)
			{
				Box1 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body1Index);
				PotentialIntersections = Hierarchy.FindAllIntersections(Box1);
			}
			else
			{
				PotentialIntersections = Hierarchy.GlobalObjects();
			}

			for (int32 i = 0; i < PotentialIntersections.Num(); ++i)
			{
				const int32 Body2Index = PotentialIntersections[i];
				const bool bBody2Bounded = InParticles.Geometry(Body2Index)->HasBoundingBox();

				if (Body1Index == Body2Index || ((bBody1Bounded == bBody2Bounded) && InParticles.InvM(Body1Index) && InParticles.InvM(Body2Index) && Body2Index > Body1Index))
				{
					//if both are dynamic, assume index order matters
					continue;
				}

				if (bBody1Bounded && bBody2Bounded)
				{
					const TBox<T, d>& Box2 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body2Index);

					if (!Box1.Intersects(Box2))
					{
						continue;
					}
				}
//				++TotalTested;


				const TVector<T, d> Box2Thickness = ComputeThickness(InParticles, Dt, Body2Index);
				const T UseThickness = FMath::Max(Box1Thickness, Box2Thickness.Size());// + MThickness

				auto Constraint = ComputeConstraint(InParticles, Body1Index, Body2Index, UseThickness);

				//if (true || !InParticles.Geometry(Body1Index)->HasBoundingBox() || !InParticles.Geometry(Body2Index)->HasBoundingBox())
				{
					//SCOPE_CYCLE_COUNTER(STAT_ComputeConstraintsNP3);
					//use narrow phase to determine if constraint is needed. Without this we can't do shock propagation
					UpdateConstraint(InParticles, UseThickness, Constraint);
					if (Constraint.Phi < UseThickness)
					{
						CriticalSection.Lock();
						MConstraints.Add(Constraint);
						CriticalSection.Unlock();
					}
//					else
//					{
//						++TotalRejected;
//					}
				}
				/*else
				{
					CriticalSection.Lock();
					MConstraints.Add(Constraint);
					CriticalSection.Unlock();
				}*/


			}
		});
	}
	MContactGraph.ComputeGraph(InParticles, MConstraints);
//	if (TotalTested > 0)
//	{
		//UE_LOG(LogChaos, Warning, TEXT("ComputeConstraints: rejected:%f out of rejected:%f = %.3f"), TotalRejected, TotalTested, TotalRejected / TotalTested);
//	}
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

DECLARE_CYCLE_STAT(TEXT("UpdateConstraints"), STAT_UpdateConstraints, STATGROUP_Chaos);

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateConstraints(const TPBDRigidParticles<T, d>& InParticles, T Dt, const TSet<uint32>& AddedParticles, const TArray<uint32>& ActiveParticles)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateConstraints);
	double Time = 0;
	FDurationTimer Timer(Time);

	//
	// Broad phase
	//

	// @todo(mlentine): We only need to construct the hierarchy for the islands we care about
	TBoundingVolume<TPBDRigidParticles<T, d>, T, d> Hierarchy(InParticles, ActiveParticles, true, Dt * BoundsThicknessMultiplier); 	//todo(ocohen): should we pass MThickness into this structure?
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
		const T Box1Thickness = ComputeThickness(InParticles, Dt, Body1Index).Size();

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

			if (InParticles.InvM(Body1Index) && InParticles.InvM(Body2Index) && (InParticles.Island(Body1Index) != InParticles.Island(Body2Index)))	//todo(ocohen): this is a hack - we should not even consider dynamics from other islands
			{
				continue;
			}

			const auto& Box2 = Hierarchy.GetWorldSpaceBoundingBox(InParticles, Body2Index);
			if (InParticles.Geometry(Body1Index)->HasBoundingBox() && InParticles.Geometry(Body2Index)->HasBoundingBox() && !Box1.Intersects(Box2))
			{
				continue;
			}

			//todo(ocohen): this should not be needed in theory, but in practice we accidentally merge islands. We should be doing this test within an island for clusters
			if (InParticles.Island(Body1Index) >= 0 && InParticles.Island(Body2Index) >= 0 && InParticles.Island(Body1Index) != InParticles.Island(Body2Index))
			{
				continue;
			}

			const TVector<T, d> Box2Thickness = ComputeThickness(InParticles, Dt, Body2Index);
			const T UseThickness = FMath::Max(Box1Thickness, Box2Thickness.Size());// + MThickness

			auto Constraint = ComputeConstraint(InParticles, Body1Index, Body2Index, UseThickness);

			//if (true || !InParticles.Geometry(Body1Index)->HasBoundingBox() || !InParticles.Geometry(Body2Index)->HasBoundingBox())
			{
				//use narrow phase to determine if constraint is needed. Without this we can't do shock propagation
				UpdateConstraint(InParticles, UseThickness, Constraint);
				if (Constraint.Phi < UseThickness)
				{

					CriticalSection.Lock();
					MConstraints.Add(Constraint);
					CriticalSection.Unlock();
				}
			}
			/*else
			{
			CriticalSection.Lock();
			MConstraints.Add(Constraint);
			CriticalSection.Unlock();
			}*/
		}
	});
	MContactGraph.Reset(InParticles, MConstraints);
	Timer.Stop();
	UE_LOG(LogChaos, Verbose, TEXT("\tPBDCollisionConstraint Update %d Constraints with Potential Collisions %f"), MConstraints.Num(), Time);
}

template<class T>
PMatrix<T, 3, 3> ComputeFactorMatrix(const TVector<T, 3>& V, const PMatrix<T, 3, 3>& M, const T& Im)
{
	// Rigid objects rotational contribution to the impulse.
	// Vx*M*VxT+Im
	check(Im > FLT_MIN)
	return PMatrix<T, 3, 3>(
		-V[2] * (-V[2] * M.M[1][1] + V[1] * M.M[2][1]) + V[1] * (-V[2] * M.M[2][1] + V[1] * M.M[2][2]) + Im,
		V[2] * (-V[2] * M.M[1][0] + V[1] * M.M[2][0]) - V[0] * (-V[2] * M.M[2][1] + V[1] * M.M[2][2]),
		-V[1] * (-V[2] * M.M[1][0] + V[1] * M.M[2][0]) + V[0] * (-V[2] * M.M[1][1] + V[1] * M.M[2][1]),
		V[2] * (V[2] * M.M[0][0] - V[0] * M.M[2][0]) - V[0] * (V[2] * M.M[2][0] - V[0] * M.M[2][2]) + Im,
		-V[1] * (V[2] * M.M[0][0] - V[0] * M.M[2][0]) + V[0] * (V[2] * M.M[1][0] - V[0] * M.M[2][1]),
		-V[1] * (-V[1] * M.M[0][0] + V[0] * M.M[1][0]) + V[0] * (-V[1] * M.M[1][0] + V[0] * M.M[1][1]) + Im);
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
		IInvJr0 = InParticles.Q(Constraint.ParticleIndex).RotateVector(InParticles.InvI(Constraint.ParticleIndex) * InParticles.Q(Constraint.ParticleIndex).UnrotateVector(Jr0));
		ImpulseRatioNumerator0 = TVector<T, d>::DotProduct(Impulse, InParticles.V(Constraint.ParticleIndex) - KinematicVelocity) + TVector<T, d>::DotProduct(IInvJr0, InParticles.I(Constraint.ParticleIndex) * InParticles.W(Constraint.ParticleIndex));
		ImpulseRatioDenom0 = ImpulseSize / InParticles.M(Constraint.ParticleIndex) + TVector<T, d>::DotProduct(Jr0, IInvJr0);
	}
	if (InParticles.InvM(Constraint.LevelsetIndex))
	{
		Jr1 = TVector<T, d>::CrossProduct(VectorToPoint2, Impulse);
		IInvJr1 = InParticles.Q(Constraint.LevelsetIndex).RotateVector(InParticles.InvI(Constraint.LevelsetIndex) * InParticles.Q(Constraint.LevelsetIndex).UnrotateVector(Jr1));
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

DECLARE_CYCLE_STAT(TEXT("Apply"), STAT_Apply, STATGROUP_ChaosWide);
template<class T, int d>
void TPBDCollisionConstraint<T, d>::Apply(TPBDRigidParticles<T, d>& InParticles, const T Dt, const int32 Island)
{
	SCOPE_CYCLE_COUNTER(STAT_Apply);
	// @todo : The current implementation supports just the no-level approach.

	TArray<typename TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ContactMap>& LevelToColorToContactMap = MContactGraph.GetContactMapAt(Island);
	int32 MaxColor = MContactGraph.GetMaxColorAt(Island);
	int32 MaxLevel = MContactGraph.GetMaxLevelAt(Island);
	for (int32 Level = 0; Level <= MaxLevel; ++Level)
	{
		for (int32 i = 0; i <= MaxColor; ++i)
		{
			if (LevelToColorToContactMap[Level].Contains(i))
			{
				PhysicsParallelFor(LevelToColorToContactMap[Level][i].Num(), [&](int32 ConstraintIndex) {
					FRigidBodyContactConstraint& Constraint = LevelToColorToContactMap[Level][i][ConstraintIndex];
					if (InParticles.Sleeping(Constraint.ParticleIndex))
					{
						check(InParticles.Sleeping(Constraint.LevelsetIndex) || InParticles.InvM(Constraint.LevelsetIndex) == 0);
						return;
					}
					const_cast<TPBDCollisionConstraint<T, d>*>(this)->UpdateConstraint(InParticles, MThickness, Constraint);
					if (Constraint.Phi >= MThickness)
					{
						return;
					}
					MCollided[Constraint.LevelsetIndex] = true;
					MCollided[Constraint.ParticleIndex] = true;
					TVector<T, d> VectorToPoint1 = Constraint.Location - InParticles.P(Constraint.ParticleIndex);
					TVector<T, d> VectorToPoint2 = Constraint.Location - InParticles.P(Constraint.LevelsetIndex);
					TVector<T, d> Body1Velocity = InParticles.V(Constraint.ParticleIndex) + TVector<T, d>::CrossProduct(InParticles.W(Constraint.ParticleIndex), VectorToPoint1);
					TVector<T, d> Body2Velocity = InParticles.V(Constraint.LevelsetIndex) + TVector<T, d>::CrossProduct(InParticles.W(Constraint.LevelsetIndex), VectorToPoint2);
					TVector<T, d> RelativeVelocity = Body1Velocity - Body2Velocity;
					if (TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal) < 0) // ignore separating constraints
					{
						PMatrix<T, d, d> WorldSpaceInvI1 = (InParticles.Q(Constraint.ParticleIndex) * FMatrix::Identity) * InParticles.InvI(Constraint.ParticleIndex) * (InParticles.Q(Constraint.ParticleIndex) * FMatrix::Identity).GetTransposed();
						PMatrix<T, d, d> WorldSpaceInvI2 = (InParticles.Q(Constraint.LevelsetIndex) * FMatrix::Identity) * InParticles.InvI(Constraint.LevelsetIndex) * (InParticles.Q(Constraint.LevelsetIndex) * FMatrix::Identity).GetTransposed();
						PMatrix<T, d, d> Factor =
							(InParticles.InvM(Constraint.ParticleIndex) > FLT_MIN ? ComputeFactorMatrix(VectorToPoint1, WorldSpaceInvI1, InParticles.InvM(Constraint.ParticleIndex)) : PMatrix<T, d, d>(0)) +
							(InParticles.InvM(Constraint.LevelsetIndex) > FLT_MIN ? ComputeFactorMatrix(VectorToPoint2, WorldSpaceInvI2, InParticles.InvM(Constraint.LevelsetIndex)) : PMatrix<T, d, d>(0));
						TVector<T, d> Impulse;
						TVector<T, d> AngularImpulse(0);
						// Resting contact if very close to the surface
						T Restitution = RelativeVelocity.Size() < (2 * 980 * Dt) ? 0 : MRestitution;
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
							if ((VelocityChange - NormalVelocityChange * Constraint.Normal).Size() <= MFriction * NormalVelocityChange)
							{
								Impulse = MinimalImpulse;
								if (MAngularFriction)
								{
									TVector<T, d> RelativeAngularVelocity = InParticles.W(Constraint.ParticleIndex) - InParticles.W(Constraint.LevelsetIndex);
									T AngularNormal = TVector<T, d>::DotProduct(RelativeAngularVelocity, Constraint.Normal);
									TVector<T, d> AngularTangent = RelativeAngularVelocity - AngularNormal * Constraint.Normal;
									TVector<T, d> FinalAngularVelocity = FMath::Sign(AngularNormal) * FMath::Max((T)0, FMath::Abs(AngularNormal) - MAngularFriction * NormalVelocityChange) * Constraint.Normal + FMath::Max((T)0, AngularTangent.Size() - MAngularFriction * NormalVelocityChange) * AngularTangent.GetSafeNormal();
									TVector<T, d> Delta = FinalAngularVelocity - RelativeAngularVelocity;
									if (!InParticles.InvM(Constraint.ParticleIndex))
									{
										PMatrix<T, d, d> WorldSpaceI2 = (InParticles.Q(Constraint.LevelsetIndex) * FMatrix::Identity) * InParticles.I(Constraint.LevelsetIndex) * (InParticles.Q(Constraint.LevelsetIndex) * FMatrix::Identity).GetTransposed();
										TVector<T, d> ImpulseDelta = InParticles.M(Constraint.LevelsetIndex) * TVector<T, d>::CrossProduct(VectorToPoint2, Delta);
										Impulse += ImpulseDelta;
										AngularImpulse += WorldSpaceI2 * Delta - TVector<T, d>::CrossProduct(VectorToPoint2, ImpulseDelta);
									}
									else if (!InParticles.InvM(Constraint.LevelsetIndex))
									{
										PMatrix<T, d, d> WorldSpaceI1 = (InParticles.Q(Constraint.ParticleIndex) * FMatrix::Identity) * InParticles.I(Constraint.ParticleIndex) * (InParticles.Q(Constraint.ParticleIndex) * FMatrix::Identity).GetTransposed();
										TVector<T, d> ImpulseDelta = InParticles.M(Constraint.ParticleIndex) * TVector<T, d>::CrossProduct(VectorToPoint1, Delta);
										Impulse += ImpulseDelta;
										AngularImpulse += WorldSpaceI1 * Delta - TVector<T, d>::CrossProduct(VectorToPoint1, ImpulseDelta);
									}
									else
									{
										PMatrix<T, d, d> Cross1(0, VectorToPoint1.Z, -VectorToPoint1.Y, -VectorToPoint1.Z, 0, VectorToPoint1.X, VectorToPoint1.Y, -VectorToPoint1.X, 0);
										PMatrix<T, d, d> Cross2(0, VectorToPoint2.Z, -VectorToPoint2.Y, -VectorToPoint2.Z, 0, VectorToPoint2.X, VectorToPoint2.Y, -VectorToPoint2.X, 0);
										PMatrix<T, d, d> CrossI1 = Cross1 * WorldSpaceInvI1;
										PMatrix<T, d, d> CrossI2 = Cross2 * WorldSpaceInvI2;
										PMatrix<T, d, d> Diag1 = CrossI1 * Cross1.GetTransposed() + CrossI2 * Cross2.GetTransposed();
										Diag1.M[0][0] += InParticles.InvM(Constraint.ParticleIndex) + InParticles.InvM(Constraint.LevelsetIndex);
										Diag1.M[1][1] += InParticles.InvM(Constraint.ParticleIndex) + InParticles.InvM(Constraint.LevelsetIndex);
										Diag1.M[2][2] += InParticles.InvM(Constraint.ParticleIndex) + InParticles.InvM(Constraint.LevelsetIndex);
										PMatrix<T, d, d> OffDiag1 = (CrossI1 + CrossI2) * -1;
										PMatrix<T, d, d> Diag2 = (WorldSpaceInvI1 + WorldSpaceInvI2).Inverse();
										PMatrix<T, d, d> OffDiag1Diag2 = OffDiag1 * Diag2;
										TVector<T, d> ImpulseDelta = PMatrix<T, d, d>((Diag1 - OffDiag1Diag2 * OffDiag1.GetTransposed()).Inverse()) * ((OffDiag1Diag2 * -1) * Delta);
										Impulse += ImpulseDelta;
										AngularImpulse += Diag2 * (Delta - PMatrix<T, d, d>(OffDiag1.GetTransposed()) * ImpulseDelta);
									}
								}
							}
							else
							{
								TVector<T, d> Tangent = (RelativeVelocity - TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal) * Constraint.Normal).GetSafeNormal();
								TVector<T, d> DirectionalFactor = Factor * (Constraint.Normal - MFriction * Tangent);
								T ImpulseDenominator = TVector<T, d>::DotProduct(Constraint.Normal, DirectionalFactor);
								if (!ensureMsgf(FMath::Abs(ImpulseDenominator) > SMALL_NUMBER, TEXT("Constraint:%s\n\nParticle:%s\n\nLevelset:%s\n\nDirectionalFactor:%s, ImpulseDenominator:%f"),
									*Constraint.ToString(),
									*InParticles.ToString(Constraint.ParticleIndex),
									*InParticles.ToString(Constraint.LevelsetIndex),
									*DirectionalFactor.ToString(), ImpulseDenominator))
								{
									ImpulseDenominator = (T)1;
								}

								Impulse = FactorInverse * -(1 + Restitution) * RelativeNormalVelocity * DirectionalFactor / ImpulseDenominator;
							}
						}
						else
						{
							T ImpulseDenominator = TVector<T, d>::DotProduct(Constraint.Normal, Factor * Constraint.Normal);
							TVector<T, d> ImpulseNumerator = -(1 + Restitution) * TVector<T, d>::DotProduct(RelativeVelocity, Constraint.Normal) * Constraint.Normal;
							if (!ensureMsgf(FMath::Abs(ImpulseDenominator) > SMALL_NUMBER, TEXT("Constraint:%s\n\nParticle:%s\n\nLevelset:%s\n\nFactor*Constraint.Normal:%s, ImpulseDenominator:%f"),
								*Constraint.ToString(),
								*InParticles.ToString(Constraint.ParticleIndex),
								*InParticles.ToString(Constraint.LevelsetIndex),
								*(Factor*Constraint.Normal).ToString(), ImpulseDenominator))
							{
								ImpulseDenominator = (T)1;
							}
							Impulse = ImpulseNumerator / ImpulseDenominator;
						}
						Impulse = GetEnergyClampedImpulse(InParticles, Constraint, Impulse, VectorToPoint1, VectorToPoint2, Body1Velocity, Body2Velocity);
						Constraint.AccumulatedImpulse += Impulse;
						TVector<T, d> AngularImpulse1 = TVector<T, d>::CrossProduct(VectorToPoint1, Impulse) + AngularImpulse;
						TVector<T, d> AngularImpulse2 = TVector<T, d>::CrossProduct(VectorToPoint2, -Impulse) - AngularImpulse;
						// Velocity update for next step
						InParticles.V(Constraint.ParticleIndex) += InParticles.InvM(Constraint.ParticleIndex) * Impulse;
						InParticles.W(Constraint.ParticleIndex) += WorldSpaceInvI1 * AngularImpulse1;
						InParticles.V(Constraint.LevelsetIndex) -= InParticles.InvM(Constraint.LevelsetIndex) * Impulse;
						InParticles.W(Constraint.LevelsetIndex) += WorldSpaceInvI2 * AngularImpulse2;
						// Position update as part of pbd
						InParticles.P(Constraint.ParticleIndex) += (InParticles.InvM(Constraint.ParticleIndex) * Impulse) * Dt;
						InParticles.Q(Constraint.ParticleIndex) += TRotation<T, d>(WorldSpaceInvI1 * AngularImpulse1, 0.f) * InParticles.Q(Constraint.ParticleIndex) * Dt * T(0.5);
						InParticles.Q(Constraint.ParticleIndex).Normalize();
						InParticles.P(Constraint.LevelsetIndex) -= (InParticles.InvM(Constraint.LevelsetIndex) * Impulse) * Dt;
						InParticles.Q(Constraint.LevelsetIndex) += TRotation<T, d>(WorldSpaceInvI2 * AngularImpulse2, 0.f) * InParticles.Q(Constraint.LevelsetIndex) * Dt * T(0.5);
						InParticles.Q(Constraint.LevelsetIndex).Normalize();
					}
				});
			}
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("ApplyPushOut"), STAT_ApplyPushOut, STATGROUP_ChaosWide);
template<class T, int d>
void TPBDCollisionConstraint<T, d>::ApplyPushOut(TPBDRigidParticles<T, d>& InParticles, const T Dt, const TArray<int32>& ActiveIndices, const int32 Island)
{
	SCOPE_CYCLE_COUNTER(STAT_ApplyPushOut);
	// @todo : The current implementation supports just the no-level approach.

	TArray<typename TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ContactMap>& LevelToColorToContactMap = MContactGraph.GetContactMapAt(Island);
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
					PhysicsParallelFor(LevelToColorToContactMap[Level][i].Num(), [&](int32 ConstraintIndex) {
						TRigidBodyContactConstraint<T, d>& Constraint = LevelToColorToContactMap[Level][i][ConstraintIndex];
						if (InParticles.Sleeping(Constraint.ParticleIndex))
						{
							check(InParticles.Sleeping(Constraint.LevelsetIndex) || InParticles.InvM(Constraint.LevelsetIndex) == 0);
							return;
						}
						for (int32 PairIteration = 0; PairIteration < MPairIterations; ++PairIteration)
						{
							UpdateConstraint(InParticles, MThickness, Constraint);
							if (Constraint.Phi >= MThickness)
							{
								break;
							}
							NeedsAnotherIteration = true;
							PMatrix<T, d, d> WorldSpaceInvI1 = (InParticles.Q(Constraint.ParticleIndex) * FMatrix::Identity) * InParticles.InvI(Constraint.ParticleIndex) * (InParticles.Q(Constraint.ParticleIndex) * FMatrix::Identity).GetTransposed();
							PMatrix<T, d, d> WorldSpaceInvI2 = (InParticles.Q(Constraint.LevelsetIndex) * FMatrix::Identity) * InParticles.InvI(Constraint.LevelsetIndex) * (InParticles.Q(Constraint.LevelsetIndex) * FMatrix::Identity).GetTransposed();
							TVector<T, d> VectorToPoint1 = Constraint.Location - InParticles.P(Constraint.ParticleIndex);
							TVector<T, d> VectorToPoint2 = Constraint.Location - InParticles.P(Constraint.LevelsetIndex);
							PMatrix<T, d, d> Factor =
								((InParticles.InvM(Constraint.ParticleIndex) && !IsTemporarilyStatic[Constraint.ParticleIndex]) ? ComputeFactorMatrix(VectorToPoint1, WorldSpaceInvI1, InParticles.InvM(Constraint.ParticleIndex)) : PMatrix<T, d, d>(0)) +
								((InParticles.InvM(Constraint.LevelsetIndex) && !IsTemporarilyStatic[Constraint.LevelsetIndex]) ? ComputeFactorMatrix(VectorToPoint2, WorldSpaceInvI2, InParticles.InvM(Constraint.LevelsetIndex)) : PMatrix<T, d, d>(0));
							T Numerator = FMath::Min((T)(Iteration + 2), (T)MNumIterations);
							T ScalingFactor = Numerator / (T)MNumIterations;
							TVector<T, d> Impulse = PMatrix<T, d, d>(Factor.Inverse()) * ((-Constraint.Phi + MThickness) * ScalingFactor * Constraint.Normal);
							TVector<T, d> AngularImpulse1 = TVector<T, d>::CrossProduct(VectorToPoint1, Impulse);
							TVector<T, d> AngularImpulse2 = TVector<T, d>::CrossProduct(VectorToPoint2, -Impulse);
							if (!IsTemporarilyStatic[Constraint.ParticleIndex])
							{
								InParticles.P(Constraint.ParticleIndex) += InParticles.InvM(Constraint.ParticleIndex) * Impulse;
								InParticles.Q(Constraint.ParticleIndex) = TRotation<T, d>::FromVector(WorldSpaceInvI1 * AngularImpulse1) * InParticles.Q(Constraint.ParticleIndex);
								InParticles.Q(Constraint.ParticleIndex).Normalize();
							}
							if (!IsTemporarilyStatic[Constraint.LevelsetIndex])
							{
								InParticles.P(Constraint.LevelsetIndex) -= InParticles.InvM(Constraint.LevelsetIndex) * Impulse;
								InParticles.Q(Constraint.LevelsetIndex) = TRotation<T, d>::FromVector(WorldSpaceInvI2 * AngularImpulse2) * InParticles.Q(Constraint.LevelsetIndex);
								InParticles.Q(Constraint.LevelsetIndex).Normalize();
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
					PhysicsParallelFor(LevelToColorToContactMap[Level][i].Num(), [&](int32 ConstraintIndex) {
						auto Constraint = LevelToColorToContactMap[Level][i][ConstraintIndex];
						if (Iteration == MNumIterations - 1)
						{
							if (!InParticles.InvM(Constraint.ParticleIndex) || IsTemporarilyStatic[Constraint.ParticleIndex])
							{
								IsTemporarilyStatic[Constraint.LevelsetIndex] = true;
							}else if (!InParticles.InvM(Constraint.LevelsetIndex) || IsTemporarilyStatic[Constraint.LevelsetIndex])
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

DECLARE_CYCLE_STAT(TEXT("CopyOutConstraints"), STAT_CopyOutConstraints, STATGROUP_Chaos);

template<class T, int d>
void TPBDCollisionConstraint<T, d>::CopyOutConstraints(int32 NumIslands)
{
	SCOPE_CYCLE_COUNTER(STAT_CopyOutConstraints);
	MConstraints.Reset();
	for (int32 Island = 0; Island < NumIslands; ++Island)
	{
		//todo(ocohen): could be part of the parallel for, but need output buffer to be thread safe
		const TArray<typename TPBDContactGraph<FRigidBodyContactConstraint, T, d>::ContactMap>& LevelToColorToContactMap = MContactGraph.GetContactMapAt(Island);
		int32 MaxColor = MContactGraph.GetMaxColorAt(Island);
		int32 MaxLevel = MContactGraph.GetMaxLevelAt(Island);

		for (int32 Level = 0; Level <= MaxLevel; ++Level)
		{
			for (int32 Color = 0; Color <= MaxColor; ++Color)
			{
				if (LevelToColorToContactMap[Level].Contains(Color))
				{
					MConstraints.Append(LevelToColorToContactMap[Level][Color]);
				}
			}
		}
	}
}

template<class T, int d>
bool TPBDCollisionConstraint<T, d>::NearestPoint(TArray<Pair<TVector<T, d>, TVector<T, d>>>& Points, TVector<T, d>& Direction, TVector<T, d>& ClosestPoint)
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

template<class T_PARTICLES, class T, int d>
TVector<T, d> GetPosition(const T_PARTICLES& InParticles)
{
	check(false);
	return TVector<T, d>();
}

template<class T, int d>
TVector<T, d> GetPosition(const TParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.X(Index);
}

template<class T, int d>
TVector<T, d> GetPosition(const TPBDRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.P(Index);
}

template<class T_PARTICLES, class T, int d>
TRotation<T, d> GetRotation(const T_PARTICLES& InParticles)
{
	check(false);
	return TRotation<T, d>();
}

template<class T, int d>
TRotation<T, d> GetRotation(const TRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.R(Index);
}

template<class T, int d>
TRotation<T, d> GetRotation(const TPBDRigidParticles<T, d>& InParticles, const int32 Index)
{
	return InParticles.Q(Index);
}

template<class T_PARTICLES, class T, int d>
TRigidTransform<T, d> GetTransform(const T_PARTICLES& InParticles)
{
	check(false);
	return TRigidTransform<T, d>();
}

template<class T, int d>
TRigidTransform<T, d> GetTransform(const TRigidParticles<T, d>& InParticles, const int32 Index)
{
	return TRigidTransform<T, d>(InParticles.X(Index), InParticles.R(Index));
}

template<class T, int d>
TRigidTransform<T, d> GetTransform(const TPBDRigidParticles<T, d>& InParticles, const int32 Index)
{
	return TRigidTransform<T, d>(InParticles.P(Index), InParticles.Q(Index));
}

#if 0
template<class T, int d>
void UpdateLevelsetConstraintHelperCCD(const TRigidParticles<T, d>& InParticles, const int32 j, const TRigidTransform<T, d>& LocalToWorld1, const TRigidTransform<T, d>& LocalToWorld2, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	if (InParticles.CollisionParticles(Constraint.ParticleIndex))
	{
		const TRigidTransform<T, d> PreviousLocalToWorld1 = GetTransform(InParticles, Constraint.ParticleIndex);
		TVector<T, d> WorldSpacePointStart = PreviousLocalToWorld1.TransformPosition(InParticles.CollisionParticles(Constraint.ParticleIndex)->X(j));
		TVector<T, d> WorldSpacePointEnd = LocalToWorld1.TransformPosition(InParticles.CollisionParticles(Constraint.ParticleIndex)->X(j));
		TVector<T, d> Body2SpacePointStart = LocalToWorld2.InverseTransformPosition(WorldSpacePointStart);
		TVector<T, d> Body2SpacePointEnd = LocalToWorld2.InverseTransformPosition(WorldSpacePointEnd);
		Pair<TVector<T, d>, bool> PointPair = InParticles.Geometry(Constraint.LevelsetIndex)->FindClosestIntersection(Body2SpacePointStart, Body2SpacePointEnd, Thickness);
		if (PointPair.Second)
		{
			const TVector<T, d> WorldSpaceDelta = WorldSpacePointEnd - TVector<T, d>(LocalToWorld2.TransformPosition(PointPair.First));
			Constraint.Phi = -WorldSpaceDelta.Size();
			Constraint.Normal = LocalToWorld2.TransformVector(InParticles.Geometry(Constraint.LevelsetIndex)->Normal(PointPair.First));
			// @todo(mlentine): Should we be using the actual collision point or that point evolved to the current time step?
			Constraint.Location = WorldSpacePointEnd;
		}
	}
}
#endif

template <typename T, int d>
bool SampleObjectHelper(const TImplicitObject<T, d>& Object, const TRigidTransform<T,d>& ObjectTransform, const TRigidTransform<T, d>& SampleToObjectTransform, const TVector<T, d>& SampleParticle, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	TVector<T, d> LocalPoint = SampleToObjectTransform.TransformPositionNoScale(SampleParticle);
	TVector<T, d> LocalNormal;
	T LocalPhi = Object.PhiWithNormal(LocalPoint, LocalNormal);
	if (LocalPhi < Constraint.Phi)
	{
		Constraint.Phi = LocalPhi;
		return true;
	}
	return false;
}

template <typename T, int d>
void SampleObjectNormalAverageHelper(const TImplicitObject<T, d>& Object, const TRigidTransform<T, d>& ObjectTransform, const TRigidTransform<T, d>& SampleToObjectTransform, const TVector<T, d>& SampleParticle, T Thickness, T& TotalThickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	TVector<T, d> LocalPoint = SampleToObjectTransform.TransformPositionNoScale(SampleParticle);
	TVector<T, d> LocalNormal;
	T LocalPhi = Object.PhiWithNormal(LocalPoint, LocalNormal);
	T LocalThickness = LocalPhi - Thickness;
	if (LocalThickness < -KINDA_SMALL_NUMBER)
	{
		Constraint.Location += LocalPoint * LocalThickness;
		TotalThickness += LocalThickness;
	}
}

DECLARE_CYCLE_STAT(TEXT("UpdateLevelsetOther"), STAT_UpdateLevelsetOther, STATGROUP_ChaosWide);
DECLARE_CYCLE_STAT(TEXT("UpdateLevelsetFindParticles"), STAT_UpdateLevelsetFindParticles, STATGROUP_ChaosWide);
DECLARE_CYCLE_STAT(TEXT("UpdateLevelsetBVHTraversal"), STAT_UpdateLevelsetBVHTraversal, STATGROUP_ChaosWide);
DECLARE_CYCLE_STAT(TEXT("UpdateLevelsetSignedDistance"), STAT_UpdateLevelsetSignedDistance, STATGROUP_ChaosWide);
DECLARE_CYCLE_STAT(TEXT("UpdateLevelsetPlane"), STAT_UpdateLevelsetPlane, STATGROUP_ChaosWide);
DECLARE_CYCLE_STAT(TEXT("SampleObject"), STAT_SampleObject, STATGROUP_ChaosWide);

int32 NormalAveraging = 1;
FAutoConsoleVariableRef CVarNormalAveraging(TEXT("p.NormalAveraging"), NormalAveraging, TEXT(""));

template <typename T, int d>
void SampleObject(const TImplicitObject<T, d>& Object, const TRigidTransform<T, d>& ObjectTransform, const TBVHParticles<T, d>& SampleParticles, const TRigidTransform<T, d>& SampleParticlesTransform, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	SCOPE_CYCLE_COUNTER(STAT_SampleObject);
	TRigidBodyContactConstraint<T, d> AvgConstraint;
	AvgConstraint.ParticleIndex = Constraint.ParticleIndex;
	AvgConstraint.LevelsetIndex = Constraint.LevelsetIndex;
	AvgConstraint.Location = TVector<T, d>::ZeroVector;
	AvgConstraint.Normal = TVector<T, d>::ZeroVector;
	AvgConstraint.Phi = Thickness;
	T TotalThickness = T(0);

	int32 DeepestParticle = -1;

	const TRigidTransform<T, d> SampleToObjectTM = SampleParticlesTransform.GetRelativeTransform(ObjectTransform);
	if (Object.HasBoundingBox())
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateLevelsetOther);
		TBox<T, d> ImplicitBox = Object.BoundingBox().TransformedBox(ObjectTransform * SampleParticlesTransform.Inverse());
		ImplicitBox.Thicken(Thickness);
		TArray<int32> PotentialParticles;
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateLevelsetFindParticles);
			PotentialParticles = SampleParticles.FindAllIntersections(ImplicitBox);
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateLevelsetSignedDistance);
			for (int32 i : PotentialParticles)
			{
				if (NormalAveraging)
				{
					SampleObjectNormalAverageHelper(Object, ObjectTransform, SampleToObjectTM, SampleParticles.X(i), Thickness, TotalThickness, AvgConstraint);
				}
				else
				{
					if (SampleObjectHelper(Object, ObjectTransform, SampleToObjectTM, SampleParticles.X(i), Thickness, AvgConstraint))
					{
						DeepestParticle = i;
					}
				}
			}
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateLevelsetPlane);
		int32 NumParticles = SampleParticles.Size();
		for (int32 i = 0; i < NumParticles; ++i)
		{
			if (NormalAveraging)
			{
				SampleObjectNormalAverageHelper(Object, ObjectTransform, SampleToObjectTM, SampleParticles.X(i), Thickness, TotalThickness, AvgConstraint);
			}
			else
			{
				if (SampleObjectHelper(Object, ObjectTransform, SampleToObjectTM, SampleParticles.X(i), Thickness, AvgConstraint))
				{
					DeepestParticle = i;
				}
			}
		}
	}

	if (NormalAveraging)
	{
		if (TotalThickness < -KINDA_SMALL_NUMBER)
		{
			TVector<T, d> LocalPoint = AvgConstraint.Location / TotalThickness;
			TVector<T, d> LocalNormal;
			const T NewPhi = Object.PhiWithNormal(LocalPoint, LocalNormal);
			if (NewPhi < Constraint.Phi)
			{
				Constraint.Phi = NewPhi;
				Constraint.Location = ObjectTransform.TransformPositionNoScale(LocalPoint);
				Constraint.Normal = ObjectTransform.TransformVectorNoScale(LocalNormal);
			}
		}
		else
		{
			check(AvgConstraint.Phi >= Thickness);
		}
	}
	else if(AvgConstraint.Phi < Constraint.Phi)
	{
		check(DeepestParticle >= 0);
		TVector<T,d> LocalPoint = SampleToObjectTM.TransformPositionNoScale(SampleParticles.X(DeepestParticle));
		TVector<T, d> LocalNormal;
		Constraint.Phi = Object.PhiWithNormal(LocalPoint, LocalNormal);
		Constraint.Location = ObjectTransform.TransformPositionNoScale(LocalPoint);
		Constraint.Normal = ObjectTransform.TransformVectorNoScale(LocalNormal);
			
	}
}

template <typename T, int d>
bool UpdateBoxPlaneConstraint(const TBox<T, d>& Box, const TRigidTransform<T, d>& BoxTransform, const TPlane<T, d>& Plane, const TRigidTransform<T, d>& PlaneTransform, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	
#if USING_CODE_ANALYSIS
	MSVC_PRAGMA( warning( push ) )
	MSVC_PRAGMA( warning( disable : ALL_CODE_ANALYSIS_WARNINGS ) )
#endif	// USING_CODE_ANALYSIS

	bool bApplied = false;
	const TRigidTransform<T, d> BoxToPlaneTransform(BoxTransform * PlaneTransform.Inverse());
	const TVector<T, d> Extents = Box.Extents();
	constexpr int32 NumCorners = 2 + 2 * d;
	constexpr T Epsilon = KINDA_SMALL_NUMBER;

	TVector<T, d> Corners[NumCorners];
	int32 CornerIdx = 0;
	Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Max());
	Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Min());
	for (int32 j = 0; j < d; ++j)
	{
		Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Min() + TVector<T, d>::AxisVector(j) * Extents);
		Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Max() - TVector<T, d>::AxisVector(j) * Extents);
	}

#if USING_CODE_ANALYSIS
	MSVC_PRAGMA( warning( pop ) )
#endif	// USING_CODE_ANALYSIS

	TVector<T, d> PotentialConstraints[NumCorners];
	int32 NumConstraints = 0;
	for (int32 i = 0; i < NumCorners; ++i)
	{
		TVector<T, d> Normal;
		const T NewPhi = Plane.PhiWithNormal(Corners[i], Normal);
		if (NewPhi < Constraint.Phi + Epsilon)
		{
			if (NewPhi <= Constraint.Phi - Epsilon)
			{
				NumConstraints = 0;
			}
			Constraint.Phi = NewPhi;
			Constraint.Normal = PlaneTransform.TransformVector(Normal);
			Constraint.Location = PlaneTransform.TransformPosition(Corners[i]);
			PotentialConstraints[NumConstraints++] = Constraint.Location;
			bApplied = true;
		}
	}
	if (NumConstraints > 1)
	{
		TVector<T, d> AverageLocation(0);
		for (int32 ConstraintIdx = 0; ConstraintIdx < NumConstraints; ++ConstraintIdx)
		{
			AverageLocation += PotentialConstraints[ConstraintIdx];
		}
		Constraint.Location = AverageLocation / NumConstraints;
	}

	return bApplied;
}

template <typename T, int d>
void UpdateSphereConstraint(const TSphere<T, d>& Sphere1, const TRigidTransform<T, d>& Sphere1Transform, const TSphere<T, d>& Sphere2, const TRigidTransform<T, d>& Sphere2Transform, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	const TVector<T, d> Center1 = Sphere1Transform.TransformPosition(Sphere1.Center());
	const TVector<T, d> Center2 = Sphere2Transform.TransformPosition(Sphere2.Center());
	const TVector<T, d> Direction = Center1 - Center2;
	const T Size = Direction.Size();
	const T NewPhi = Size - (Sphere1.Radius() + Sphere2.Radius());
	if (NewPhi < Constraint.Phi)
	{
		Constraint.Normal = Size > SMALL_NUMBER ? Direction / Size : TVector<T, d>(0, 0, 1);
		Constraint.Phi = NewPhi;
		Constraint.Location = Center1 - Sphere1.Radius() * Constraint.Normal;
	}
}

template <typename T, int d>
void UpdateSpherePlaneConstraint(const TSphere<T, d>& Sphere, const TRigidTransform<T, d>& SphereTransform, const TPlane<T, d>& Plane, const TRigidTransform<T, d>& PlaneTransform, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	const TRigidTransform<T, d> SphereToPlaneTransform(PlaneTransform.Inverse() * SphereTransform);
	const TVector<T, d> SphereCenter = SphereToPlaneTransform.TransformPosition(Sphere.Center());

	TVector<T, d> NewNormal;
	T NewPhi = Plane.PhiWithNormal(SphereCenter, NewNormal);
	NewPhi -= Sphere.Radius();

	if (NewPhi < Constraint.Phi)
	{
		Constraint.Phi = NewPhi;
		Constraint.Normal = PlaneTransform.TransformVectorNoScale(NewNormal);
		Constraint.Location = SphereCenter - Constraint.Normal * Sphere.Radius();
	}
}

template <typename T, int d>
bool UpdateSphereBoxConstraint(const TSphere<T, d>& Sphere, const TRigidTransform<T, d>& SphereTransform, const TBox<T, d>& Box, const TRigidTransform<T, d>& BoxTransform, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	const TRigidTransform<T, d> SphereToBoxTransform(SphereTransform * BoxTransform.Inverse());
	const TVector<T, d> SphereCenterInBox = SphereToBoxTransform.TransformPosition(Sphere.Center());

	TVector<T, d> NewNormal;
	T NewPhi = Box.PhiWithNormal(SphereCenterInBox, NewNormal);
	NewPhi -= Sphere.Radius();

	if (NewPhi < Constraint.Phi)
	{
		Constraint.Phi = NewPhi;
		Constraint.Normal = BoxTransform.TransformVectorNoScale(NewNormal);
		Constraint.Location = SphereTransform.TransformPosition(Sphere.Center()) - Constraint.Normal * Sphere.Radius();
		return true;
	}

	return false;
}

DECLARE_CYCLE_STAT(TEXT("FindRelevantShapes"), STAT_FindRelevantShapes, STATGROUP_ChaosWide);
template <typename T, int d>
TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> FindRelevantShapes(const TImplicitObject<T,d>& ParticleObj, const TRigidTransform<T,d>& ParticlesTM, const TImplicitObject<T,d>& LevelsetObj, const TRigidTransform<T,d>& LevelsetTM, const T Thickness)
{
	SCOPE_CYCLE_COUNTER(STAT_FindRelevantShapes);
	TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> RelevantShapes;
	//find all levelset inner objects
	if (ParticleObj.HasBoundingBox())
	{
		const TRigidTransform<T, d> ParticlesToLevelsetTM = ParticlesTM * LevelsetTM.Inverse();
		TBox<T, d> ParticleBoundsInLevelset = ParticleObj.BoundingBox().TransformedBox(ParticlesToLevelsetTM);
		ParticleBoundsInLevelset.Thicken(Thickness);
		{
			LevelsetObj.FindAllIntersectingObjects(RelevantShapes, ParticleBoundsInLevelset);
		}
	}
	else
	{
		LevelsetObj.AccumulateAllImplicitObjects(RelevantShapes, TRigidTransform<T, d>::Identity);
	}

	return RelevantShapes;
}

DECLARE_CYCLE_STAT(TEXT("UpdateUnionUnionConstraint"), STAT_UpdateUnionUnionConstraint, STATGROUP_ChaosWide);
template<typename T_PARTICLES, typename T, int d>
void UpdateUnionUnionConstraint(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateUnionUnionConstraint);
	Constraint.Phi = Thickness;

	TRigidTransform<T, d> ParticlesTM = GetTransform(InParticles, Constraint.ParticleIndex);
	TRigidTransform<T, d> LevelsetTM = GetTransform(InParticles, Constraint.LevelsetIndex);

	const TImplicitObject<T, d>* ParticleObj = InParticles.Geometry(Constraint.ParticleIndex);
	const TImplicitObject<T,d>* LevelsetObj = InParticles.Geometry(Constraint.LevelsetIndex);
	const TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> LevelsetShapes = FindRelevantShapes(*ParticleObj, ParticlesTM, *LevelsetObj, LevelsetTM, Thickness);

	for (const Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>& LevelsetObjPair : LevelsetShapes)
	{
		const TImplicitObject<T, d>& LevelsetInnerObj = *LevelsetObjPair.First;
		const TRigidTransform<T, d>& LevelsetInnerObjTM = LevelsetObjPair.Second * LevelsetTM;

		//now find all particle inner objects
		const TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> ParticleShapes = FindRelevantShapes(LevelsetInnerObj, LevelsetInnerObjTM, *ParticleObj, ParticlesTM, Thickness);

		//for each inner obj pair, update constraint
		for (const Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>& ParticlePair : ParticleShapes)
		{
			const TImplicitObject<T, d>& ParticleInnerObj = *ParticlePair.First;
			const TRigidTransform<T, d> ParticleInnerObjTM = ParticlePair.Second * ParticlesTM;
			UpdateConstraintImp(InParticles, ParticleInnerObj, ParticleInnerObjTM, LevelsetInnerObj, LevelsetInnerObjTM, Thickness, Constraint);
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("UpdateSingleUnionConstraint"), STAT_UpdateSingleUnionConstraint, STATGROUP_ChaosWide);
template<typename T_PARTICLES, typename T, int d>
void UpdateSingleUnionConstraint(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateSingleUnionConstraint);
	Constraint.Phi = Thickness;

	TRigidTransform<T, d> ParticlesTM = GetTransform(InParticles, Constraint.ParticleIndex);
	TRigidTransform<T, d> LevelsetTM = GetTransform(InParticles, Constraint.LevelsetIndex);

	const TImplicitObject<T, d>* ParticleObj = InParticles.Geometry(Constraint.ParticleIndex);
	const TImplicitObject<T, d>* LevelsetObj = InParticles.Geometry(Constraint.LevelsetIndex);
	const TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> LevelsetShapes = FindRelevantShapes(*ParticleObj, ParticlesTM, *LevelsetObj, LevelsetTM, Thickness);
	
	for (const Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>& LevelsetObjPair : LevelsetShapes)
	{
		const TImplicitObject<T, d>& LevelsetInnerObj = *LevelsetObjPair.First;
		const TRigidTransform<T, d> LevelsetInnerObjTM = LevelsetObjPair.Second * LevelsetTM;
		UpdateConstraintImp(InParticles, *ParticleObj, ParticlesTM, LevelsetInnerObj, LevelsetInnerObjTM, Thickness, Constraint);
	}
}

DECLARE_CYCLE_STAT(TEXT("UpdateLevelsetConstraint"), STAT_UpdateLevelsetConstraint, STATGROUP_ChaosWide);
template<typename T, int d>
template<typename T_PARTICLES>
void TPBDCollisionConstraint<T, d>::UpdateLevelsetConstraint(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateLevelsetConstraint);
	Constraint.Phi = Thickness;

	TRigidTransform<T, d> ParticlesTM = GetTransform(InParticles, Constraint.ParticleIndex);
	TRigidTransform<T, d> LevelsetTM = GetTransform(InParticles, Constraint.LevelsetIndex);

	const TBVHParticles<T, d>* SampleParticles = nullptr;
	SampleParticles = InParticles.CollisionParticles(Constraint.ParticleIndex).Get();

	if(SampleParticles)
	{
		SampleObject(*InParticles.Geometry(Constraint.LevelsetIndex), LevelsetTM, *SampleParticles, ParticlesTM, Thickness, Constraint);
	}
}

DECLARE_CYCLE_STAT(TEXT("UpdateUnionLevelsetConstraint"), STAT_UpdateUnionLevelsetConstraint, STATGROUP_ChaosWide);
template<typename T_PARTICLES, typename T, int d>
void UpdateUnionLevelsetConstraint(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateUnionLevelsetConstraint);
	Constraint.Phi = Thickness;

	TRigidTransform<T, d> ParticlesTM = GetTransform(InParticles, Constraint.ParticleIndex);
	TRigidTransform<T, d> LevelsetTM = GetTransform(InParticles, Constraint.LevelsetIndex);

	const TImplicitObject<T, d>* ParticleObj = InParticles.Geometry(Constraint.ParticleIndex);
	const TImplicitObject<T, d>* LevelsetObj = InParticles.Geometry(Constraint.LevelsetIndex);
	TArray<Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>> LevelsetShapes = FindRelevantShapes(*ParticleObj, ParticlesTM, *LevelsetObj, LevelsetTM, Thickness);

	const TBVHParticles<T, d>& SampleParticles = *InParticles.CollisionParticles(Constraint.ParticleIndex).Get();
	for (const Pair<const TImplicitObject<T, d>*, TRigidTransform<T, d>>& LevelsetObjPair : LevelsetShapes)
	{
		const TImplicitObject<T, d>* Object = LevelsetObjPair.First;
		const TRigidTransform<T, d> ObjectTM = LevelsetObjPair.Second * LevelsetTM;
		SampleObject(*Object, ObjectTM, SampleParticles, ParticlesTM, Thickness, Constraint);
	}
}

template<typename T, int d>
template<typename T_PARTICLES>
void TPBDCollisionConstraint<T, d>::UpdateLevelsetConstraintGJK(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	static int32 MaxIterations = 100;
	Constraint.Phi = Thickness;
	const TRigidTransform<T, d> LocalToWorld1 = GetTransform(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> LocalToWorld2 = GetTransform(InParticles, Constraint.LevelsetIndex);
	TVector<T, d> Direction = LocalToWorld1.GetTranslation() - LocalToWorld2.GetTranslation();
	TVector<T, d> SupportA = LocalToWorld1.TransformPosition(InParticles.Geometry(Constraint.ParticleIndex)->Support(LocalToWorld1.InverseTransformVector(-Direction), Thickness));
	TVector<T, d> SupportB = LocalToWorld2.TransformPosition(InParticles.Geometry(Constraint.LevelsetIndex)->Support(LocalToWorld2.InverseTransformVector(Direction), Thickness));
	TVector<T, d> Point = SupportB - SupportA;
	TArray<Pair<TVector<T, d>, TVector<T, d>>> Points = {MakePair(Point, SupportA)};
	Direction = -Point;
	for (int32 i = 0; i < MaxIterations; ++i)
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

template <typename T, int d>
void UpdateBoxConstraint(const TBox<T, d>& Box1, const TRigidTransform<T, d>& Box1Transform, const TBox<T, d>& Box2, const TRigidTransform<T, d>& Box2Transform, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	TBox<T,d> Box2SpaceBox1 = Box1.TransformedBox(Box1Transform * Box2Transform.Inverse());
	TBox<T,d> Box1SpaceBox2 = Box2.TransformedBox(Box2Transform * Box1Transform.Inverse());
	Box2SpaceBox1.Thicken(Thickness);
	Box1SpaceBox2.Thicken(Thickness);
	if (Box1SpaceBox2.Intersects(Box1) && Box2SpaceBox1.Intersects(Box2))
	{
		const TVector<T, d> Box1Center = (Box1Transform * Box2Transform.Inverse()).TransformPosition(Box1.Center());
		bool bDeepOverlap = false;
		if (Box2.SignedDistance(Box1Center) < 0)
		{
			//If Box1 is overlapping Box2 by this much the signed distance approach will fail (box1 gets sucked into box2). In this case just use two spheres
			TSphere<T, d> Sphere1(Box1Transform.TransformPosition(Box1.Center()), Box1.Extents().Min() / 2);
			TSphere<T, d> Sphere2(Box2Transform.TransformPosition(Box2.Center()), Box2.Extents().Min() / 2);
			const TVector<T, d> Direction = Sphere1.Center() - Sphere2.Center();
			T Size = Direction.Size();
			if (Size < (Sphere1.Radius() + Sphere2.Radius()))
			{
				const T NewPhi = Size - (Sphere1.Radius() + Sphere2.Radius());;
				if (NewPhi < Constraint.Phi)
				{
					bDeepOverlap = true;
					Constraint.Normal = Size > SMALL_NUMBER ? Direction / Size : TVector<T, d>(0, 0, 1);
					Constraint.Phi = NewPhi;
					Constraint.Location = Sphere1.Center() - Sphere1.Radius() * Constraint.Normal;
				}
			}
		}
		if (!bDeepOverlap || Constraint.Phi >= 0)
		{
			//if we didn't have deep penetration use signed distance per particle. If we did have deep penetration but the spheres did not overlap use signed distance per particle

			//UpdateLevelsetConstraintGJK(InParticles, Thickness, Constraint);
			//check(Constraint.Phi < MThickness);
			// For now revert to doing all points vs lsv check until we can figure out a good way to get the deepest point without needing this
			{
				const TArray<TVector<T, d>> SampleParticles = Box1.ComputeLocalSamplePoints();
				const TRigidTransform<T, d> Box1ToBox2Transform = Box1Transform.GetRelativeTransform(Box2Transform);
				int32 NumParticles = SampleParticles.Num();
				for (int32 i = 0; i < NumParticles; ++i)
				{
					SampleObjectHelper(Box2, Box2Transform, Box1ToBox2Transform, SampleParticles[i], Thickness, Constraint);
				}
			}
		}
	}
}


template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeLevelsetConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex, const T Thickness)
{
	if (!InParticles.CollisionParticlesSize(ParticleIndex))
	{
		int32 TmpIndex = ParticleIndex;
		ParticleIndex = LevelsetIndex;
		LevelsetIndex = TmpIndex;
	}
	//todo(ocohen):if both have collision particles, use the one with fewer?
	// Find Deepest Point
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = ParticleIndex;
	Constraint.LevelsetIndex = LevelsetIndex;
	return Constraint;
}

template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeLevelsetConstraintGJK(const TPBDRigidParticles<T, d>& InParticles, int32 ParticleIndex, int32 LevelsetIndex, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = ParticleIndex;
	Constraint.LevelsetIndex = LevelsetIndex;
	return Constraint;
}

template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Box1Index, int32 Box2Index, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = Box1Index;
	Constraint.LevelsetIndex = Box2Index;
	return Constraint;
}

template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeBoxPlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 BoxIndex, int32 PlaneIndex, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = BoxIndex;
	Constraint.LevelsetIndex = PlaneIndex;
	return Constraint;
}

template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeSphereConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Sphere1Index, int32 Sphere2Index, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = Sphere1Index;
	Constraint.LevelsetIndex = Sphere2Index;
	return Constraint;
}

template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeSpherePlaneConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 PlaneIndex, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = SphereIndex;
	Constraint.LevelsetIndex = PlaneIndex;
	return Constraint;
}

template<class T, int d>
TRigidBodyContactConstraint<T, d> ComputeSphereBoxConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 SphereIndex, int32 BoxIndex, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = SphereIndex;
	Constraint.LevelsetIndex = BoxIndex;
	return Constraint;
}

template <typename T, int d>
TRigidBodyContactConstraint<T, d> ComputeSingleUnionConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 NonUnionIndex, int32 UnionIndex, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = NonUnionIndex;
	Constraint.LevelsetIndex = UnionIndex;
	return Constraint;
}

template <typename T, int d>
TRigidBodyContactConstraint<T, d> ComputeUnionUnionConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Union1Index, int32 Union2Index, const T Thickness)
{
	TRigidBodyContactConstraint<T, d> Constraint;
	Constraint.ParticleIndex = Union1Index;
	Constraint.LevelsetIndex = Union2Index;
	//todo(ocohen): some heuristic for determining the order?
	return Constraint;
}

template<class T, int d>
typename TPBDCollisionConstraint<T, d>::FRigidBodyContactConstraint TPBDCollisionConstraint<T, d>::ComputeConstraint(const TPBDRigidParticles<T, d>& InParticles, int32 Body1Index, int32 Body2Index, const T Thickness)
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
	else if (InParticles.Geometry(Body1Index)->GetType() < TImplicitObjectUnion<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TImplicitObjectUnion<T, d>::GetType())
	{
		return ComputeSingleUnionConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
	else if (InParticles.Geometry(Body1Index)->GetType() == TImplicitObjectUnion<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() < TImplicitObjectUnion<T, d>::GetType())
	{
		return ComputeSingleUnionConstraint(InParticles, Body2Index, Body1Index, Thickness);
	}
	else if(InParticles.Geometry(Body1Index)->GetType() == TImplicitObjectUnion<T, d>::GetType() && InParticles.Geometry(Body2Index)->GetType() == TImplicitObjectUnion<T, d>::GetType())
	{
		return ComputeUnionUnionConstraint(InParticles, Body1Index, Body2Index, Thickness);
	}
#if 0
	else if (InParticles.Geometry(Body1Index)->IsConvex() && InParticles.Geometry(Body2Index)->IsConvex())
	{
		return ComputeLevelsetConstraintGJK(InParticles, Body1Index, Body2Index, Thickness);
	}
#endif
	return ComputeLevelsetConstraint(InParticles, Body1Index, Body2Index, Thickness);
}

DECLARE_CYCLE_STAT(TEXT("UpdateConstraint"), STAT_UpdateConstraint, STATGROUP_ChaosWide);

template <typename T_PARTICLES, typename T, int d>
void UpdateConstraintImp(const T_PARTICLES& InParticles, const TImplicitObject<T, d>& ParticleObject, const TRigidTransform<T, d>& ParticleTM, const TImplicitObject<T, d>& LevelsetObject, const TRigidTransform<T, d>& LevelsetTM, T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	if (ParticleObject.GetType() == TBox<T, d>::GetType() && LevelsetObject.GetType() == TBox<T, d>::GetType())
	{
		UpdateBoxConstraint(*ParticleObject.template GetObject<TBox<T,d>>(), ParticleTM, *LevelsetObject.template GetObject<TBox<T, d>>(), LevelsetTM, Thickness, Constraint);
	}
	else if (ParticleObject.GetType() == TSphere<T, d>::GetType() && LevelsetObject.GetType() == TSphere<T, d>::GetType())
	{
		UpdateSphereConstraint(*ParticleObject.template GetObject<TSphere<T, d>>(), ParticleTM, *LevelsetObject.template GetObject<TSphere<T, d>>(), LevelsetTM, Thickness, Constraint);
	}
	else if (ParticleObject.GetType() == TBox<T, d>::GetType() && LevelsetObject.GetType() == TPlane<T, d>::GetType())
	{
		UpdateBoxPlaneConstraint(*ParticleObject.template GetObject<TBox<T, d>>(), ParticleTM, *LevelsetObject.template GetObject<TPlane<T, d>>(), LevelsetTM, Thickness, Constraint);
	}
	else if (ParticleObject.GetType() == TSphere<T, d>::GetType() && LevelsetObject.GetType() == TPlane<T, d>::GetType())
	{
		UpdateSpherePlaneConstraint(*ParticleObject.template GetObject<TSphere<T, d>>(), ParticleTM, *LevelsetObject.template GetObject<TPlane<T, d>>(), LevelsetTM, Thickness, Constraint);
	}
	else if (ParticleObject.GetType() == TSphere<T, d>::GetType() && LevelsetObject.GetType() == TBox<T, d>::GetType())
	{
		UpdateSphereBoxConstraint(*ParticleObject.template GetObject<TSphere<T, d>>(), ParticleTM, *LevelsetObject.template GetObject<TBox<T, d>>(), LevelsetTM, Thickness, Constraint);
	}
	else if (ParticleObject.GetType() == TPlane<T, d>::GetType() && LevelsetObject.GetType() == TBox<T, d>::GetType())
	{
		TRigidBodyContactConstraint<T, d> TmpConstraint = Constraint;
		UpdateBoxPlaneConstraint(*LevelsetObject.template GetObject<TBox<T, d>>(), LevelsetTM, *ParticleObject.template GetObject<TPlane<T, d>>(), ParticleTM, Thickness, TmpConstraint);
		if (TmpConstraint.Phi < Constraint.Phi)
		{
			Constraint = TmpConstraint;
			Constraint.Normal = -Constraint.Normal;
		}
	}
	else if (ParticleObject.GetType() == TPlane<T, d>::GetType() && LevelsetObject.GetType() == TSphere<T, d>::GetType())
	{
		TRigidBodyContactConstraint<T, d> TmpConstraint = Constraint;
		UpdateSpherePlaneConstraint(*LevelsetObject.template GetObject<TSphere<T, d>>(), LevelsetTM, *ParticleObject.template GetObject<TPlane<T, d>>(), ParticleTM, Thickness, TmpConstraint);
		if (TmpConstraint.Phi < Constraint.Phi)
		{
			Constraint = TmpConstraint;
			Constraint.Normal = -Constraint.Normal;
		}
	}
	else if (ParticleObject.GetType() == TBox<T, d>::GetType() && LevelsetObject.GetType() == TSphere<T, d>::GetType())
	{
		TRigidBodyContactConstraint<T, d> TmpConstraint = Constraint;
		UpdateSphereBoxConstraint(*LevelsetObject.template GetObject<TSphere<T, d>>(), LevelsetTM, *ParticleObject.template GetObject<TBox<T, d>>(), ParticleTM, Thickness, TmpConstraint);
		if (TmpConstraint.Phi < Constraint.Phi)
		{
			Constraint = TmpConstraint;
			Constraint.Normal = -Constraint.Normal;
		}
	}
	else if (ParticleObject.GetType() < TImplicitObjectUnion<T, d>::GetType() && LevelsetObject.GetType() == TImplicitObjectUnion<T, d>::GetType())
	{
		return UpdateSingleUnionConstraint(InParticles, Thickness, Constraint);
	}
	else if (ParticleObject.GetType() == TImplicitObjectUnion<T, d>::GetType() && LevelsetObject.GetType() < TImplicitObjectUnion<T, d>::GetType())
	{
		check(false);	//should not be possible to get this ordering (see ComputeConstraint)
	}
	else if (ParticleObject.GetType() == TImplicitObjectUnion<T, d>::GetType() && LevelsetObject.GetType() == TImplicitObjectUnion<T, d>::GetType())
	{
		return UpdateUnionUnionConstraint(InParticles, Thickness, Constraint);
	}
#if 0
	else if (ParticleObject.IsConvex() && LevelsetObject.IsConvex())
	{
		UpdateLevelsetConstraintGJK(InParticles, Thickness, Constraint);
	}
#endif
	else if (LevelsetObject.IsUnderlyingUnion())
	{
		UpdateUnionLevelsetConstraint(InParticles, Thickness, Constraint);
	}
	else
	{
		TPBDCollisionConstraint<T, d>::UpdateLevelsetConstraint(InParticles, Thickness, Constraint);
	}
}

template<typename T, int d>
template<typename T_PARTICLES>
void TPBDCollisionConstraint<T, d>::UpdateConstraint(const T_PARTICLES& InParticles, const T Thickness, TRigidBodyContactConstraint<T, d>& Constraint)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateConstraint);
	Constraint.Phi = Thickness;
	const TRigidTransform<T, d> ParticleTM = GetTransform(InParticles, Constraint.ParticleIndex);
	const TRigidTransform<T, d> LevelsetTM = GetTransform(InParticles, Constraint.LevelsetIndex);

	UpdateConstraintImp(InParticles, *InParticles.Geometry(Constraint.ParticleIndex), ParticleTM, *InParticles.Geometry(Constraint.LevelsetIndex), LevelsetTM, Thickness, Constraint);
}

template<class T, int d>
bool TPBDCollisionConstraint<T, d>::SleepInactive(TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, int32& IslandSleepCount, const int32 Island, const T LinearSleepThreshold, const T AngularSleepThreshold) const
{
	return MContactGraph.SleepInactive(InParticles, ActiveIndices, IslandSleepCount, Island, LinearSleepThreshold, AngularSleepThreshold);
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateIslandsFromConstraints(TPBDRigidParticles<T, d>& InParticles, TArray<TSet<int32>>& IslandParticles, TArray<int32>& IslandSleepCounts, TSet<int32>& ActiveIndices)
{
	MContactGraph.UpdateIslandsFromConstraints(InParticles, IslandParticles, IslandSleepCounts, ActiveIndices, MConstraints);
}

template<class T, int d>
void TPBDCollisionConstraint<T, d>::UpdateAccelerationStructures(const TPBDRigidParticles<T, d>& InParticles, const TArray<int32>& ActiveIndices, const int32 Island)
{
	MContactGraph.UpdateAccelerationStructures(InParticles, ActiveIndices, Island);
}

template class TPBDCollisionConstraint<float, 3>;
template void TPBDCollisionConstraint<float, 3>::UpdateConstraint<TPBDRigidParticles<float, 3>>(const TPBDRigidParticles<float, 3>&, const float, FRigidBodyContactConstraint& Constraint);
template void TPBDCollisionConstraint<float, 3>::UpdateLevelsetConstraint<TPBDRigidParticles<float, 3>>(const TPBDRigidParticles<float, 3>&, const float, FRigidBodyContactConstraint& Constraint);
template void TPBDCollisionConstraint<float, 3>::UpdateLevelsetConstraintGJK<TPBDRigidParticles<float, 3>>(const TPBDRigidParticles<float, 3>&, const float, FRigidBodyContactConstraint& Constraint);
}