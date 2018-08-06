// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ArrayCollectionArray.h"
#include "Apeiron/BVHParticles.h"
#include "Apeiron/ImplicitObject.h"
#include "Apeiron/KinematicGeometryParticles.h"
#include "Apeiron/Matrix.h"
#include "Apeiron/Rotation.h"

namespace Apeiron
{
template<class T, int d>
class TRigidParticles : public TKinematicGeometryParticles<T, d>
{
  public:
	using TArrayCollection::Size;

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
	    : TKinematicGeometryParticles<T, d>(MoveTemp(Other)), MF(MoveTemp(Other.MF)), MT(MoveTemp(Other.MT)), MI(MoveTemp(Other.MI)), MInvI(MoveTemp(Other.MInvI)), MM(MoveTemp(Other.MM)), MInvM(MoveTemp(Other.MInvM)), MCollisionParticles(MoveTemp(Other.MCollisionParticles)), MSleeping(MoveTemp(Other.MSleeping)), MDisabled(MoveTemp(Other.MDisabled))
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

	const TBVHParticles<T, d>& CollisionParticles(const int32 Index) const { return MCollisionParticles[Index]; }
	TBVHParticles<T, d>& CollisionParticles(const int32 Index) { return MCollisionParticles[Index]; }

	const bool Sleeping(const int32 Index) const { return MSleeping[Index]; }
	bool& Sleeping(const int32 Index) { return MSleeping[Index]; }

	const bool Disabled(const int32 Index) const { return MDisabled[Index]; }
	bool& Disabled(const int32 Index) { return MDisabled[Index]; }

	const int32 Island(const int32 Index) const { return MIsland[Index]; }
	int32& Island(const int32 Index) { return MIsland[Index]; }

  private:
	TArrayCollectionArray<TVector<T, d>> MF;
	TArrayCollectionArray<TVector<T, d>> MT;
	TArrayCollectionArray<PMatrix<T, d, d>> MI;
	TArrayCollectionArray<PMatrix<T, d, d>> MInvI;
	TArrayCollectionArray<T> MM;
	TArrayCollectionArray<T> MInvM;
	TArrayCollectionArray<TBVHParticles<T, d>> MCollisionParticles;
	TArrayCollectionArray<bool> MSleeping;
	TArrayCollectionArray<bool> MDisabled;
	TArrayCollectionArray<int32> MIsland;
};
}
