// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Particles.h"
#include "Chaos/Rotation.h"

namespace Chaos
{
template<class T, int d>
class TGeometryParticles : public TParticles<T, d>
{
  public:
	using TArrayCollection::Size;

	TGeometryParticles()
	    : TParticles<T, d>()
	{
		TArrayCollection::AddArray(&MR);
		TArrayCollection::AddArray(&MGeometry);
	}
	TGeometryParticles(const TGeometryParticles<T, d>& Other) = delete;
	TGeometryParticles(TGeometryParticles<T, d>&& Other)
	    : TParticles<T, d>(MoveTemp(Other)), MR(MoveTemp(Other.MR)), MGeometry(MoveTemp(Other.MGeometry))
	{
		TArrayCollection::AddArray(&MR);
		TArrayCollection::AddArray(&MGeometry);
	}
	~TGeometryParticles()
	{
		for (uint32 i = 0; i < Size(); ++i)
		{
			//TODO(mlentine): Deleting causing heap corruption now?
			//delete MGeometry[i];
		}
	}

	const TRotation<T, d>& R(const int32 Index) const { return MR[Index]; }
	TRotation<T, d>& R(const int32 Index) { return MR[Index]; }

	TImplicitObject<T, d>* const& Geometry(const int32 Index) const { return MGeometry[Index]; }
	TImplicitObject<T, d>*& Geometry(const int32 Index) { return MGeometry[Index]; }

	FString ToString(int32 index) const
	{
		FString BaseString = TParticles<T, d>::ToString(index);
		return FString::Printf(TEXT("%s, MR:%s, MGeometry:%s"), *BaseString, *R(index).ToString(), (Geometry(index) ? *(Geometry(index)->ToString()) : TEXT("none")));
	}

  private:
	TArrayCollectionArray<TRotation<T, d>> MR;
	TArrayCollectionArray<TImplicitObject<T, d>*> MGeometry;
};
}
