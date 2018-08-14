// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ArrayCollectionArray.h"
#include "Apeiron/RigidParticles.h"
#include "Apeiron/Rotation.h"

namespace Apeiron
{
template<class T, int d>
class TPBDRigidsEvolution;

template<class T, int d>
class TPBDRigidParticles : public TRigidParticles<T, d>
{
	friend class TPBDRigidsEvolution<T, d>;

  public:
	TPBDRigidParticles()
	    : TRigidParticles<T, d>()
	{
		TArrayCollection::AddArray(&MP);
		TArrayCollection::AddArray(&MQ);
	}
	TPBDRigidParticles(const TPBDRigidParticles<T, d>& Other) = delete;
	TPBDRigidParticles(TPBDRigidParticles<T, d>&& Other)
	    : TRigidParticles<T, d>(MoveTemp(Other)), MP(MoveTemp(Other.MP)), MQ(MoveTemp(Other.MQ))
	{
		TArrayCollection::AddArray(&MP);
		TArrayCollection::AddArray(&MQ);
	}
	~TPBDRigidParticles() {}

	const TVector<T, d>& P(const int32 index) const { return MP[index]; }
	TVector<T, d>& P(const int32 index) { return MP[index]; }

	const TRotation<T, d>& Q(const int32 index) const { return MQ[index]; }
	TRotation<T, d>& Q(const int32 index) { return MQ[index]; }

  private:
	TArrayCollectionArray<TVector<T, d>> MP;
	TArrayCollectionArray<TRotation<T, d>> MQ;
};
}
