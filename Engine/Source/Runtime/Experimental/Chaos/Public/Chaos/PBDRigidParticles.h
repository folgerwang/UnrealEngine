// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/RigidParticles.h"
#include "Chaos/Rotation.h"

namespace Chaos
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
		TArrayCollection::AddArray(&MPreV);
		TArrayCollection::AddArray(&MPreW);
	}
	TPBDRigidParticles(const TPBDRigidParticles<T, d>& Other) = delete;
	TPBDRigidParticles(TPBDRigidParticles<T, d>&& Other)
	    : TRigidParticles<T, d>(MoveTemp(Other)), MP(MoveTemp(Other.MP)), MQ(MoveTemp(Other.MQ))
	{
		TArrayCollection::AddArray(&MP);
		TArrayCollection::AddArray(&MQ);
		TArrayCollection::AddArray(&MPreV);
		TArrayCollection::AddArray(&MPreW);
	}
	~TPBDRigidParticles() {}

	const TVector<T, d>& P(const int32 index) const { return MP[index]; }
	TVector<T, d>& P(const int32 index) { return MP[index]; }

	const TRotation<T, d>& Q(const int32 index) const { return MQ[index]; }
	TRotation<T, d>& Q(const int32 index) { return MQ[index]; }

	const TVector<T, d>& PreV(const int32 index) const { return MPreV[index]; }
	TVector<T, d>& PreV(const int32 index) { return MPreV[index]; }

	const TVector<T, d>& PreW(const int32 index) const { return MPreW[index]; }
	TVector<T, d>& PreW(const int32 index) { return MPreW[index]; }

	void SetSleeping(int32 Index, bool bSleeping)
	{
		if (this->MSleeping[Index] && bSleeping == false)
		{
			PreV(Index) = this->V(Index);
			PreW(Index) = this->W(Index);
		}
		this->MSleeping[Index] = bSleeping;
	}

	FString ToString(int32 index) const
	{
		FString BaseString = TRigidParticles<T, d>::ToString(index);
		return FString::Printf(TEXT("%s, MP:%s, MQ:%s, MPreV:%s, MPreW:%s"), *BaseString, *P(index).ToString(), *Q(index).ToString(), *PreV(index).ToString(), *PreW(index).ToString());
	}

  private:
	TArrayCollectionArray<TVector<T, d>> MP;
	TArrayCollectionArray<TRotation<T, d>> MQ;
	TArrayCollectionArray<TVector<T, d>> MPreV;
	TArrayCollectionArray<TVector<T, d>> MPreW;
};
}
