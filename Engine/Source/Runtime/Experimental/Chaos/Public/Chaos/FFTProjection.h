// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayND.h"
#include "Chaos/FFT.h"
#include "Chaos/UniformGrid.h"

namespace Chaos
{
template<int d>
bool IsPowerOfTwo(const Vector<int32, d>& Counts)
{
	for (int32 i = 0; i < d; ++i)
	{
		if (Counts[i] & (Counts[i] - 1))
			return false;
	}
	return true;
}

template<class T, int d>
class FFTProjection
{
  public:
	FFTProjection() {}
	~FFTProjection() {}

	void Apply(const TUniformGrid<T, d>& Grid, TArrayND<Vector<T, d>, d>& Velocity, const TArrayND<bool, d>& BoundaryConditions, const T dt)
	{
		check(false);
	}
};

template<class T>
class FFTProjection<T, 3>
{
  public:
	FFTProjection(const int32 NumIterations = 1)
	    : MNumIterations(NumIterations) {}
	~FFTProjection() {}

	void Apply(const TUniformGrid<T, 3>& Grid, TArrayND<Vector<T, 3>, 3>& Velocity, const TArrayND<bool, 3>& BoundaryConditions, const T dt)
	{
		check(IsPowerOfTwo(Grid.Counts()));
		int32 size = Grid.Counts().Product();
		Vector<int32, 3> Counts = Grid.Counts();
		Counts[2] = Counts[2] / 2 + 1;
		TArrayND<Complex<T>, 3> u(Counts), v(Counts), w(Counts);
		TArrayND<Vector<T, 3>, 3> VelocitySaved = Velocity.Copy();
		for (int32 iteration = 0; iteration < MNumIterations; ++iteration)
		{
			TFFT<T, 3>::Transform(Grid, Velocity, u, v, w);
			TFFT<T, 3>::MakeDivergenceFree(Grid, u, v, w);
			TFFT<T, 3>::InverseTransform(Grid, Velocity, u, v, w, true);
			for (int32 i = 0; i < size; ++i)
			{
				Velocity[i] = BoundaryConditions[i] ? VelocitySaved[i] : Velocity[i];
			}
		}
	}

  private:
	int32 MNumIterations;
};
}
