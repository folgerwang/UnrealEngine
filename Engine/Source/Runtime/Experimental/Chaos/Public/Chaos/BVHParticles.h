// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/BoundingVolumeHierarchy.h"
#include "Chaos/Particles.h"

extern int32 CollisionParticlesBVHDepth;

namespace Chaos
{
template<class T, int d>
class TBVHParticles : public TParticles<T, d>
{
  public:
	using TArrayCollection::Size;

	TBVHParticles()
	    : TParticles<T, d>(), MBVH(*this, CollisionParticlesBVHDepth){}
	TBVHParticles(const TBVHParticles<T, d>& Other) = delete;
	TBVHParticles(TBVHParticles<T, d>&& Other)
	    : TParticles<T, d>(MoveTemp(Other)), MBVH(MoveTemp(Other.MBVH))
	{
	}
	~TBVHParticles()
	{
	}

	TBVHParticles& operator=(TBVHParticles<T, d>&& Other)
	{
		MBVH = MoveTemp(Other.MBVH);
		TParticles<T, d>::operator=(static_cast<TParticles<T, d>&&>(Other));
		return *this;
	}

	void UpdateAccelerationStructures()
	{
		MBVH.UpdateHierarchy();
	}

	const TArray<int32> FindAllIntersections(const TBox<T, d>& Object) const
	{
		return MBVH.FindAllIntersections(Object);
	}

  private:
	TBoundingVolumeHierarchy<TParticles<T, d>, T, d> MBVH;
};
}
