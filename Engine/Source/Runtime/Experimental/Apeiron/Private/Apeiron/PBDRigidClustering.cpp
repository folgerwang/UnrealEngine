// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/PBDRigidClustering.h"

#include "Apeiron/ImplicitObjectTransformed.h"
#include "Apeiron/ImplicitObjectUnion.h"
#include "Apeiron/PBDRigidsEvolution.h"
#include "Apeiron/ParallelFor.h"
#include "ProfilingDebugging/ScopedTimers.h"

using namespace Apeiron;

template<class T, int d>
TPBDRigidClustering<T, d>::TPBDRigidClustering(TPBDRigidsEvolution<T, d>& InEvolution, TPBDRigidParticles<T, d>& InParticles)
    : MEvolution(InEvolution), MParticles(InParticles)
{
	MParticles.AddArray(&MClusterIds);
	MParticles.AddArray(&MStrains);
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

template<class T, int d>
void TPBDRigidClustering<T, d>::UpdatePositionRecursive(TPBDRigidParticles<T, d>& MParticlesInput, TArray<bool> & Processed, const uint32 & Index)
{
	if (!Processed[Index] && MParticles.Disabled(Index) )
	{
		if (MClusterIds[Index].Id >= 0 )
		{
			UpdatePositionRecursive(MParticlesInput, Processed, MClusterIds[Index].Id);

			check(MChildToParent.Contains(Index));
			TRigidTransform<T, d> ParentFrame(MParticlesInput.X(MClusterIds[Index].Id), MParticlesInput.R(MClusterIds[Index].Id));
			// This is backwards as unreal matrices are applied in backwards order.
			auto ChildFrame = MChildToParent[Index] * ParentFrame;
			MParticlesInput.X(Index) = ChildFrame.GetTranslation();
			MParticlesInput.R(Index) = ChildFrame.GetRotation();
		}
	}
	Processed[Index] = true;
}

template<class T, int d>
void TPBDRigidClustering<T, d>::UpdatePosition(TPBDRigidParticles<T, d>& MParticlesInput, const T Dt)
{
	TArray<bool> Processed;
	Processed.Init(false, MParticlesInput.Size());

	ParallelFor(MParticlesInput.Size(), [&](int32 Index) {
		if (MClusterIds[Index].Id >= 0 )
		{
			UpdatePositionRecursive(MParticlesInput, Processed, Index);
		}
	});
}

template<class T, int d>
void TPBDRigidClustering<T, d>::AdvanceClustering(const T Dt, TPBDCollisionConstraint<T, d>& CollisionRule)
{
	UE_LOG(LogApeiron, Verbose, TEXT("START FRAME with Dt %f"), Dt);

	double FrameTime = 0, Time = 0;
	FDurationTimer Timer(Time);

	// Break clusters as a result of collisions
	Time = 0;
	Timer.Start();
	TMap<uint32, TSet<uint32> > DeactivatedParents;
	TSet<uint32> ActivatedChildren;
	TSet<int32> IslandsToRecollide;
	TArray<uint32> ClusterParentArray;
	MParentToChildren.GenerateKeyArray(ClusterParentArray);
	// Find all clusters to be broken and islands to be rewound
	for (const auto& ParentIndex : ClusterParentArray)
	{
		if (MParticles.Sleeping(ParentIndex) || MParticles.Disabled(ParentIndex))
			continue;
		if (CalculatePseudoMomentum(MParticles, ParentIndex) < MStrains[ParentIndex])
			continue;

		// global strain based releasing 
		int32 Island = MParticles.Island(ParentIndex);
		ActivatedChildren = DeactivateClusterParticle(ParentIndex);
		if (!IslandsToRecollide.Contains(Island))
		{
			IslandsToRecollide.Add(Island);
		}
		DeactivatedParents.Add(ParentIndex, ActivatedChildren);
	}

	// Rewind active particles
	TArray<TSet<int32>>& MIslandParticles = MEvolution.IslandParticles();
	ParallelFor(IslandsToRecollide.Num(), [&](int32 Island) {
		TArray<int32> ActiveIndices = MIslandParticles[Island].Array();
		for (const auto Index : ActiveIndices)
		{
			MParticles.X(Index) = MParticles.P(Index);
			MParticles.R(Index) = MParticles.Q(Index);
		}
	});
	TArray<uint32> DeactivatedParentsArray;
	DeactivatedParents.GetKeys(DeactivatedParentsArray);
	ParallelFor(DeactivatedParentsArray.Num(), [&](int32 Index) {
		int32 ParentIndex = DeactivatedParentsArray[Index];
		MParticles.X(ParentIndex) = MParticles.P(ParentIndex);
		MParticles.R(ParentIndex) = MParticles.Q(ParentIndex);
		// we only need this for the children that break.
		// need to update to not do this for the children that left the cluster
		UpdateChildAttributes(ParentIndex, DeactivatedParents[ParentIndex]);
	});
	// Update collision constraints based on newly activate children
	CollisionRule.RemoveConstraints(TSet<uint32>(DeactivatedParentsArray));
	TSet<uint32> AllIslandParticles;
	for (const auto& Island : IslandsToRecollide)
	{
		for (const auto& Index : MIslandParticles[Island])
		{
			if (!AllIslandParticles.Contains(Index))
			{
				AllIslandParticles.Add(Index);
			}
		}
	}
	// @todo(mlentine): We can precompute internal constraints which can filter some from the narrow phase tests but may not help much
	CollisionRule.UpdateConstraints(MParticles, ActivatedChildren, AllIslandParticles.Array());
	// Resolve collisions
	ParallelFor(IslandsToRecollide.Num(), [&](int32 Island) {
		TArray<int32> ActiveIndices = MIslandParticles[Island].Array();
		// @todo(mlentine): This is heavy handed and probably can be simplified as we know only a little bit changed.
		CollisionRule.UpdateAccelerationStructures(MParticles, ActiveIndices, Island);
		CollisionRule.ApplyPushOut(MParticles, Island);
	});

	Timer.Stop();
	UE_LOG(LogApeiron, Verbose, TEXT("Cluster Break Update Time is %f"), Time);
}

template<class T, int d>
int32 TPBDRigidClustering<T, d>::CreateClusterParticle(const TArray<uint32>& Children)
{
	int32 NewIndex = MParticles.Size();
	MParticles.AddParticles(1);

	//
	// Update clustering data structures.
	//
	TSet<int32>& MActiveIndices = MEvolution.ActiveIndices();
	if (MActiveIndices.Num())
	{
		MActiveIndices.Add(NewIndex);
		for (const auto Child : Children)
		{
			MActiveIndices.Remove(Child);
		}
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

	UpdateIslandParticles(NewIndex);

	return NewIndex;
}

// @todo(mlentine): This should be extracted out to be able to be used outside clustering.
template<class T, int d>
TRotation<T, d> TransformToLocalSpace(PMatrix<T, d, d>& Inertia)
{
	// Extract Eigenvalues
	T OffDiagSize = FMath::Square(Inertia.M[1][0]) + FMath::Square(Inertia.M[2][0]) + FMath::Square(Inertia.M[2][1]);
	if (OffDiagSize == 0)
	{
		return TRotation<T, d>(TVector<T, d>(0), 1);
	}
	T Trace = (Inertia.M[0][0] + Inertia.M[1][1] + Inertia.M[2][2]) / 3;
	T Size = FMath::Sqrt((FMath::Square(Inertia.M[0][0] - Trace) + FMath::Square(Inertia.M[1][1] - Trace) + FMath::Square(Inertia.M[2][2] - Trace) + 2 * OffDiagSize) / 6);
	PMatrix<T, d, d> NewMat = (Inertia - FMatrix::Identity * Trace) * (1 / Size);
	T HalfDeterminant = NewMat.Determinant() / 2;
	T Angle = HalfDeterminant <= -1 ? PI / 3 : (HalfDeterminant >= 1 ? 0 : acos(HalfDeterminant) / 3);
	T m00 = Trace + 2 * Size * cos(Angle), m11 = Trace + 2 * Size * cos(Angle + (2 * PI / 3)), m22 = 3 * Trace - m00 - m11;

	// Extract Eigenvectors
	bool DoSwap = ((m00 - m11) > (m11 - m22)) ? false : true;
    TVector<T, d> Eigenvector0 = (Inertia.SubtractDiagonal(DoSwap ? m22 : m00)).SymmetricCofactorMatrix().LargestColumnNormalized();
    TVector<T, d> Orthogonal = Eigenvector0.GetOrthogonalVector().GetSafeNormal();
	PMatrix<T, d, d - 1> Cofactors(Orthogonal, TVector<T, d>::CrossProduct(Eigenvector0, Orthogonal));
	PMatrix<T, d, d - 1> CofactorsScaled = Inertia * Cofactors;
	PMatrix<T, d - 1, d - 1> IR(
		CofactorsScaled.M[0] * Cofactors.M[0] + CofactorsScaled.M[1] * Cofactors.M[1] + CofactorsScaled.M[2] * Cofactors.M[2],
		CofactorsScaled.M[3] * Cofactors.M[0] + CofactorsScaled.M[4] * Cofactors.M[1] + CofactorsScaled.M[5] * Cofactors.M[2],
		CofactorsScaled.M[3] * Cofactors.M[3] + CofactorsScaled.M[4] * Cofactors.M[4] + CofactorsScaled.M[5] * Cofactors.M[5]);
	PMatrix<T, d - 1, d - 1> IM1 = IR.SubtractDiagonal(DoSwap ? m00 : m22);
	T OffDiag = IM1.M[1] * IM1.M[1];
	T IM1Scale0 = IM1.M[3] * IM1.M[3] + OffDiag;
	T IM1Scale1 = IM1.M[0] * IM1.M[0] + OffDiag;
	TVector<T, d - 1> SmallEigenvector2 = IM1Scale0 > IM1Scale1 ? (TVector<T, d - 1>(IM1.M[3], -IM1.M[1]) / IM1Scale0) : (IM1Scale1 > 0 ? (TVector<T, d - 1>(-IM1.M[1], IM1.M[0]) / IM1Scale1) : TVector<T, d - 1>(1, 0));
	TVector<T, d> Eigenvector2 = Cofactors * SmallEigenvector2;
    TVector<T, d> Eigenvector1 = TVector<T, d>::CrossProduct(Eigenvector2, Eigenvector0);

	// Return results
	Inertia = PMatrix<T, d, d>(m00, 0, 0, m11, 0, m22);
	return DoSwap ? TRotation<T, d>(PMatrix<T, d, d>(Eigenvector2, Eigenvector1, -Eigenvector0)) : TRotation<T, d>(PMatrix<T, d, d>(Eigenvector0, Eigenvector1, Eigenvector2));
}

template<class T, int d>
void TPBDRigidClustering<T, d>::UpdateMassProperties(const TArray<uint32>& Children, const uint32 NewIndex)
{
	TArray<TUniquePtr<TImplicitObject<T, d>>> Objects;

	// Kinematic Clusters
	bool HasInfiniteMass = false;
	for (const auto Child : Children)
	{
		if (MParticles.InvM(Child) == 0)
		{
			MParticles.X(NewIndex) = MParticles.X(Child);
			MParticles.R(NewIndex) = MParticles.R(Child);
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
		MParticles.InvM(NewIndex) = 1 / MParticles.M(NewIndex);
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
		MParticles.R(NewIndex) = TransformToLocalSpace(MParticles.I(NewIndex));
		MParticles.InvI(NewIndex) = MParticles.I(NewIndex).Inverse();
	}
	for (const auto Child : Children)
	{
		// This is backwards as unreal matrices are applied in backwards order.
		auto Frame = TRigidTransform<T, d>(MParticles.X(Child), MParticles.R(Child)) * TRigidTransform<T, d>(MParticles.X(NewIndex), MParticles.R(NewIndex)).Inverse();
		uint32 StartIndex = MParticles.CollisionParticles(NewIndex).Size();
		MParticles.CollisionParticles(NewIndex).AddParticles(MParticles.CollisionParticles(Child).Size());
		for (uint32 i = StartIndex; i < MParticles.CollisionParticles(NewIndex).Size(); ++i)
		{
			MParticles.CollisionParticles(NewIndex).X(i) = Frame.TransformPosition(MParticles.CollisionParticles(Child).X(i - StartIndex));
		}
		Objects.Add(TUniquePtr<TImplicitObject<T, d>>(new TImplicitObjectTransformed<T, d>(MParticles.Geometry(Child), Frame.Inverse())));
		MParticles.Disabled(Child) = true;
		MClusterIds[Child] = ClusterId(NewIndex);
		MChildToParent.Add(Child, Frame);
	}
	if (MParticles.Geometry(NewIndex))
	{
		delete MParticles.Geometry(NewIndex);
	}
	MParticles.Geometry(NewIndex) = new TBox<T, d>(TImplicitObjectUnion<T, d>(MoveTemp(Objects)).BoundingBox());
}

template<class T, int d>
void TPBDRigidClustering<T, d>::UpdateIslandParticles(const uint32 ClusterIndex)
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

template<class T, int d>
void TPBDRigidClustering<T, d>::UpdateChildAttributes(const uint32 ClusterIndex, const TSet<uint32> & Children)
{
	TRigidTransform<T, d> ParentTransform(MParticles.X(ClusterIndex), MParticles.R(ClusterIndex));
	for (const auto& Child : Children)
	{
		if (MParticles.InvM(Child) == 0)
			continue;
		// This needs to be backwards
		auto ChildFrame = MChildToParent[Child] * ParentTransform;
		MParticles.X(Child) = ChildFrame.GetTranslation();
		MParticles.R(Child) = ChildFrame.GetRotation();
		MParticles.V(Child) = MParticles.V(ClusterIndex) + TVector<float, 3>::CrossProduct(MParticles.W(ClusterIndex), MParticles.X(Child) - MParticles.X(ClusterIndex));
		MParticles.W(Child) = MParticles.W(ClusterIndex);
	}
}

template<class T, int d>
TSet<uint32> TPBDRigidClustering<T, d>::DeactivateClusterParticle(const uint32 ClusterIndex)
{
	TSet<uint32> DeactivatedChildern;
	TArray<TSet<int32>>& MIslandParticles = MEvolution.IslandParticles();
	TSet<int32>& MActiveIndices = MEvolution.ActiveIndices();

	MParticles.Disabled(ClusterIndex) = true;
	int32 Island = MParticles.Island(ClusterIndex);
	MIslandParticles[Island].Remove(ClusterIndex);
	MActiveIndices.Remove(ClusterIndex);
	for (const auto& Child : MParentToChildren[ClusterIndex])
	{
		MActiveIndices.Add(Child);
		MIslandParticles[Island].Add(Child);
		MParticles.Disabled(Child) = false;
		DeactivatedChildern.Add(Child);
	}
	MParentToChildren.Remove(ClusterIndex);
	MClusterIds[ClusterIndex] = -1;
	return DeactivatedChildern;
}

template class Apeiron::TPBDRigidClustering<float, 3>;
