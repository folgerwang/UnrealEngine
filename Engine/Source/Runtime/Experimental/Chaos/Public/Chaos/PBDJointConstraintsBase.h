// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/Map.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PerParticleRule.h"

namespace Chaos
{
	template<class T, int d>
	class TPBDJointConstraintsBase
	{
	  public:
		TPBDJointConstraintsBase(const T InStiffness = (T)1)
		    : Stiffness(InStiffness)
		{
		}

		TPBDJointConstraintsBase(const TRigidParticles<T, d>& InParticles, const TArray<TVector<T, d>>& Locations, TArray<TVector<int32, 2>>&& InConstraints, const T InStiffness = (T)1)
		    : Constraints(MoveTemp(InConstraints)), Stiffness(InStiffness)
		{
			UpdateDistances(InParticles, Locations);
		}
		virtual ~TPBDJointConstraintsBase() {}
	
		void UpdateDistances(const TRigidParticles<T, d>& InParticles, const TArray<TVector<T, d>>& Locations);
		TVector<T, d> GetDelta(const TPBDRigidParticles<T, d>& InParticles, const TVector<T, d>& WorldSpaceX1, const TVector<T, d>& WorldSpaceX2, const PMatrix<T, d, d>& WorldSpaceInvI1, const PMatrix<T, d, d>& WorldSpaceInvI2, const int32 i) const;
	
	  protected:
		TArray<TVector<int32, 2>> Constraints;
		TArray<TVector<TVector<T, 3>, 2>> Distances;
	
	  private:
		T Stiffness;
	};
}
