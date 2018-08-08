// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ArrayCollection.h"
#include "Apeiron/ArrayCollectionArray.h"
#include "Apeiron/Vector.h"

namespace Apeiron
{
template<class T, int d>
class TPBDRigidsEvolution;

template<class T, int d>
class TParticles : public TArrayCollection
{
	friend class TPBDRigidsEvolution<T, d>;

  public:
	TParticles() { AddArray(&MX); }
	TParticles(const TParticles<T, d>& Other) = delete;
	TParticles(TParticles<T, d>&& Other)
	    : TArrayCollection(), MX(MoveTemp(Other.MX))
	{
		AddElements(Other.Size());
		AddArray(&MX);
		Other.MSize = 0;
	}
	~TParticles() {}

	void AddParticles(const int Num)
	{
		AddElements(Num);
	}

	TParticles& operator=(TParticles<T, d>&& Other)
	{
		MX = MoveTemp(Other.MX);
		AddElements(Other.Size());
		Other.MSize = 0;
		return *this;
	}

	const TArrayCollectionArray<TVector<T, d>>& X() const { return MX; }
	const TVector<T, d>& X(const int32 Index) const { return MX[Index]; }
	TVector<T, d>& X(const int32 Index) { return MX[Index]; }

  private:
	TArrayCollectionArray<TVector<T, d>> MX;
};
}
