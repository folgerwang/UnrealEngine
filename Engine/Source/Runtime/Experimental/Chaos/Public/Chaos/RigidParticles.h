// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/BVHParticles.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/KinematicGeometryParticles.h"
#include "Chaos/Matrix.h"
#include "Chaos/Particles.h"
#include "Chaos/Rotation.h"
#include "Chaos/TriangleMesh.h"

namespace Chaos
{
template<class T, int d>
class CHAOS_API TRigidParticles : public TKinematicGeometryParticles<T, d>
{
  public:
	using TArrayCollection::Size;
    using TParticles<T, d>::X;
    using TGeometryParticles<T, d>::R;

	TRigidParticles()
	    : TKinematicGeometryParticles<T, d>()
	{
		TArrayCollection::AddArray(&MF);
		TArrayCollection::AddArray(&MT);
		TArrayCollection::AddArray(&MI);
		TArrayCollection::AddArray(&MInvI);
		TArrayCollection::AddArray(&MM);
		TArrayCollection::AddArray(&MInvM);
		TArrayCollection::AddArray(&MCollisionParticles);
		TArrayCollection::AddArray(&MDisabled);
		TArrayCollection::AddArray(&MSleeping);
		TArrayCollection::AddArray(&MIsland);
	}
	TRigidParticles(const TRigidParticles<T, d>& Other) = delete;
	TRigidParticles(TRigidParticles<T, d>&& Other)
	    : TKinematicGeometryParticles<T, d>(MoveTemp(Other)), MF(MoveTemp(Other.MF)), MT(MoveTemp(Other.MT)), MI(MoveTemp(Other.MI)), MInvI(MoveTemp(Other.MInvI)), MM(MoveTemp(Other.MM)), MInvM(MoveTemp(Other.MInvM)), MCollisionParticles(MoveTemp(Other.MCollisionParticles)), MDisabled(MoveTemp(Other.MDisabled)), MSleeping(MoveTemp(Other.MSleeping))
	{
		TArrayCollection::AddArray(&MF);
		TArrayCollection::AddArray(&MT);
		TArrayCollection::AddArray(&MI);
		TArrayCollection::AddArray(&MInvI);
		TArrayCollection::AddArray(&MM);
		TArrayCollection::AddArray(&MInvM);
		TArrayCollection::AddArray(&MCollisionParticles);
		TArrayCollection::AddArray(&MDisabled);
		TArrayCollection::AddArray(&MSleeping);
		TArrayCollection::AddArray(&MIsland);
	}
	~TRigidParticles()
	{
	}

	const TVector<T, d>& Torque(const int32 Index) const { return MT[Index]; }
	TVector<T, d>& Torque(const int32 Index) { return MT[Index]; }

	const TVector<T, d>& F(const int32 Index) const { return MF[Index]; }
	TVector<T, d>& F(const int32 Index) { return MF[Index]; }

	const PMatrix<T, d, d>& I(const int32 Index) const { return MI[Index]; }
	PMatrix<T, d, d>& I(const int32 Index) { return MI[Index]; }

	const PMatrix<T, d, d>& InvI(const int32 Index) const { return MInvI[Index]; }
	PMatrix<T, d, d>& InvI(const int32 Index) { return MInvI[Index]; }

	const T M(const int32 Index) const { return MM[Index]; }
	T& M(const int32 Index) { return MM[Index]; }

	const T InvM(const int32 Index) const { return MInvM[Index]; }
	T& InvM(const int32 Index) { return MInvM[Index]; }

	int32 CollisionParticlesSize(int32 Index) const { return MCollisionParticles[Index] == nullptr ? 0 : MCollisionParticles[Index]->Size(); }
	void CollisionParticlesInitIfNeeded(const int32 Index);
	
	const TUniquePtr<TBVHParticles<T, d>>& CollisionParticles(const int32 Index) const { return MCollisionParticles[Index]; }
	TUniquePtr<TBVHParticles<T, d>>& CollisionParticles(const int32 Index) { return MCollisionParticles[Index]; }

	const bool Sleeping(const int32 Index) const { return MSleeping[Index]; }

	const bool Disabled(const int32 Index) const { return MDisabled[Index]; }
	bool& Disabled(const int32 Index) { return MDisabled[Index]; }

	const int32 Island(const int32 Index) const { return MIsland[Index]; }
	int32& Island(const int32 Index) { return MIsland[Index]; }

	FString ToString(int32 index) const
	{
		FString BaseString = TKinematicGeometryParticles<T, d>::ToString(index);
		return FString::Printf(TEXT("%s, MF:%s, MT:%s, MI:%s, MInvI:%s, MM:%f, MInvM:%f, MCollisionParticles(num):%d, MDisabled:%d, MSleepring:%d, MIsland:%d"), *BaseString, *F(index).ToString(), *Torque(index).ToString(), *I(index).ToString(), *InvI(index).ToString(), M(index), InvM(index), CollisionParticlesSize(index), Disabled(index), Sleeping(index), Island(index));
	}

  private:
	TArrayCollectionArray<TVector<T, d>> MF;
	TArrayCollectionArray<TVector<T, d>> MT;
	TArrayCollectionArray<PMatrix<T, d, d>> MI;
	TArrayCollectionArray<PMatrix<T, d, d>> MInvI;
	TArrayCollectionArray<T> MM;
	TArrayCollectionArray<T> MInvM;
	TArrayCollectionArray<TUniquePtr<TBVHParticles<T, d>>> MCollisionParticles;
	TArrayCollectionArray<bool> MDisabled;
	TArrayCollectionArray<int32> MIsland;
protected:
	TArrayCollectionArray<bool> MSleeping;

};
}
