// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ArrayCollectionArray.h"
#include "Apeiron/Particles.h"

namespace Apeiron
{
template<class T, int d>
class TDynamicParticles : public TParticles<T, d>
{
  public:
	TDynamicParticles()
	    : TParticles<T, d>()
	{
		TArrayCollection::AddArray(&MV);
		TArrayCollection::AddArray(&MF);
		TArrayCollection::AddArray(&MM);
		TArrayCollection::AddArray(&MInvM);
	}
	TDynamicParticles(const TDynamicParticles<T, d>& Other) = delete;
	TDynamicParticles(TDynamicParticles<T, d>&& Other)
	    : TParticles<T, d>(MoveTemp(Other)), MV(MoveTemp(Other.MV)), MF(MoveTemp(Other.MF)), MM(MoveTemp(Other.MM)), MInvM(MoveTemp(Other.MInvM))
	{
		TArrayCollection::AddArray(&MV);
		TArrayCollection::AddArray(&MF);
		TArrayCollection::AddArray(&MM);
		TArrayCollection::AddArray(&MInvM);
	}
	~TDynamicParticles() {}

	const TVector<T, d>& V(const int32 Index) const { return MV[Index]; }
	TVector<T, d>& V(const int32 Index) { return MV[Index]; }

	const TVector<T, d>& F(const int32 Index) const { return MF[Index]; }
	TVector<T, d>& F(const int32 Index) { return MF[Index]; }

	const T M(const int32 Index) const { return MM[Index]; }
	T& M(const int32 Index) { return MM[Index]; }

	const T InvM(const int32 Index) const { return MInvM[Index]; }
	T& InvM(const int32 Index) { return MInvM[Index]; }

  private:
	TArrayCollectionArray<TVector<T, d>> MV, MF;
	TArrayCollectionArray<T> MM, MInvM;
};
}
