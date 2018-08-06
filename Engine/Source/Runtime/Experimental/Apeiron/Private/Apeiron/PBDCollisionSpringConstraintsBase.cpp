// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Apeiron/PBDCollisionSpringConstraintsBase.h"

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Apeiron/ParallelFor.h"
#if PLATFORM_DESKTOP && PLATFORM_64BITS
#include "kDOP.h"
#endif

using namespace Apeiron;

#if PLATFORM_DESKTOP && PLATFORM_64BITS
class FMeshBuildDataProvider
{
  public:
	/** Initialization constructor. */
	FMeshBuildDataProvider(
	    const TkDOPTree<const FMeshBuildDataProvider, uint32>& InkDopTree)
	    : kDopTree(InkDopTree)
	{
	}

	// kDOP data provider interface.

	FORCEINLINE const TkDOPTree<const FMeshBuildDataProvider, uint32>& GetkDOPTree(void) const
	{
		return kDopTree;
	}

	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE float GetDeterminant(void) const
	{
		return 1.0f;
	}

  private:
	const TkDOPTree<const FMeshBuildDataProvider, uint32>& kDopTree;
};
#endif

template<class T, int d>
PBDCollisionSpringConstraintsBase<T, d>::PBDCollisionSpringConstraintsBase(
    const TDynamicParticles<T, d>& InParticles, const TArray<TVector<int32, 3>>& Elements, const TSet<TVector<int32, 2>>& DisabledCollisionElements, const T Dt, const T Height, const T Stiffness)
    : MH(Height), MStiffness(Stiffness)
{
	if (!Elements.Num())
		return;
#if PLATFORM_DESKTOP && PLATFORM_64BITS
	TkDOPTree<const FMeshBuildDataProvider, uint32> DopTree;
	TArray<FkDOPBuildCollisionTriangle<uint32>> BuildTraingleArray;
	for (int32 i = 0; i < Elements.Num(); ++i)
	{
		const auto& Elem = Elements[i];
		BuildTraingleArray.Add(FkDOPBuildCollisionTriangle<uint32>(i, InParticles.X(Elem[0]), InParticles.X(Elem[1]), InParticles.X(Elem[2])));
	}
	DopTree.Build(BuildTraingleArray);
	FMeshBuildDataProvider DopDataProvider(DopTree);
	FCriticalSection CriticalSection;
	ParallelFor(InParticles.Size(), [&](int32 Index) {
		FkHitResult Result;
		const auto& Start = InParticles.X(Index);
		const auto End = Start + InParticles.V(Index) * Dt + InParticles.V(Index).GetSafeNormal() * MH;
		FVector4 Start4(Start.X, Start.Y, Start.Z, 0);
		FVector4 End4(End.X, End.Y, End.Z, 0);
		TkDOPLineCollisionCheck<const FMeshBuildDataProvider, uint32> ray(Start4, End4, true, DopDataProvider, &Result);
		if (DopTree.LineCheck(ray))
		{
			const auto& Elem = Elements[Result.Item];
			if (DisabledCollisionElements.Contains({Index, Elem[0]}) || DisabledCollisionElements.Contains({Index, Elem[1]}) || DisabledCollisionElements.Contains({Index, Elem[2]}))
				return;
			TVector<T, 3> Bary;
			TVector<T, d> P10 = InParticles.X(Elem[1]) - InParticles.X(Elem[0]);
			TVector<T, d> P20 = InParticles.X(Elem[2]) - InParticles.X(Elem[0]);
			TVector<T, d> PP0 = InParticles.X(Index) - InParticles.X(Elem[0]);
			T Size10 = P10.SizeSquared();
			T Size20 = P20.SizeSquared();
			T ProjSides = TVector<T, d>::DotProduct(P10, P20);
			T ProjP1 = TVector<T, d>::DotProduct(PP0, P10);
			T ProjP2 = TVector<T, d>::DotProduct(PP0, P20);
			T Denom = Size10 * Size20 - ProjSides * ProjSides;
			Bary.Y = (Size20 * ProjP1 - ProjSides * ProjP2) / Denom;
			Bary.Z = (Size10 * ProjP2 - ProjSides * ProjP1) / Denom;
			Bary.X = 1.0f - Bary.Z - Bary.Y;
			// TODO(mlentine): In theory this shouldn't happen as this means no collision but I'm not positive that the collision check is always correct.
			//if (Bary.Y < 0 || Bary.Z < 0 || Bary.X < 0) return;
			// TODO(mlentine): Incorporate history
			TVector<T, d> Normal = TVector<T, d>::DotProduct(Result.Normal, PP0) > 0 ? Result.Normal : -Result.Normal;
			CriticalSection.Lock();
			MConstraints.Add({Index, Elem[0], Elem[1], Elem[2]});
			MBarys.Add(Bary);
			MNormals.Add(Normal);
			CriticalSection.Unlock();
		}
	});
#endif
}

template<class T, int d>
TVector<T, d> PBDCollisionSpringConstraintsBase<T, d>::GetDelta(const TPBDParticles<T, d>& InParticles, const int32 i) const
{
	const auto& Constraint = MConstraints[i];
	int32 i1 = Constraint[0];
	int32 i2 = Constraint[1];
	int32 i3 = Constraint[2];
	int32 i4 = Constraint[3];
	T PInvMass = InParticles.InvM(i3) * MBarys[i][1] + InParticles.InvM(i2) * MBarys[i][0] + InParticles.InvM(i4) * MBarys[i][2];
	if (InParticles.InvM(i1) == 0 && PInvMass == 0)
		return TVector<T, d>(0);
	const TVector<T, d>& P1 = InParticles.P(i1);
	const TVector<T, d>& P2 = InParticles.P(i2);
	const TVector<T, d>& P3 = InParticles.P(i3);
	const TVector<T, d>& P4 = InParticles.P(i4);
	const TVector<T, d> P = MBarys[i][0] * P2 + MBarys[i][1] * P3 + MBarys[i][2] * P4 + MH * MNormals[i];
	TVector<T, d> Difference = P1 - P;
	if (TVector<T, d>::DotProduct(Difference, MNormals[i]) > 0)
		return TVector<T, d>(0);
	float Distance = Difference.Size();
	TVector<T, d> Delta = Distance * MNormals[i];
	T CombinedMass = PInvMass + InParticles.InvM(i1);
	if (CombinedMass <= 1e-7)
		return 0;
	check(CombinedMass > 1e-7);
	return MStiffness * Delta / CombinedMass;
}

template class Apeiron::PBDCollisionSpringConstraintsBase<float, 3>;
#endif
