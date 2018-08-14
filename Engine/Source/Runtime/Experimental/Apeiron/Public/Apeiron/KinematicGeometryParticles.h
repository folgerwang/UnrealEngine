// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/ArrayCollectionArray.h"
#include "Apeiron/GeometryParticles.h"

namespace Apeiron
{
template<class T, int d>
class TKinematicGeometryParticles : public TGeometryParticles<T, d>
{
  public:
	TKinematicGeometryParticles()
	    : TGeometryParticles<T, d>()
	{
		TArrayCollection::AddArray(&MV);
		TArrayCollection::AddArray(&MW);
	}
	TKinematicGeometryParticles(const TKinematicGeometryParticles<T, d>& Other) = delete;
	TKinematicGeometryParticles(TKinematicGeometryParticles<T, d>&& Other)
	    : TGeometryParticles<T, d>(MoveTemp(Other)), MV(MoveTemp(Other.MV)), MW(MoveTemp(Other.MW))
	{
		TArrayCollection::AddArray(&MV);
		TArrayCollection::AddArray(&MW);
	}
	~TKinematicGeometryParticles() {}

	const TVector<T, d>& V(const int32 Index) const { return MV[Index]; }
	TVector<T, d>& V(const int32 Index) { return MV[Index]; }

	const TVector<T, d>& W(const int32 Index) const { return MW[Index]; }
	TVector<T, d>& W(const int32 Index) { return MW[Index]; }

  private:
	TArrayCollectionArray<TVector<T, d>> MV;
	TArrayCollectionArray<TVector<T, d>> MW;
};
}
