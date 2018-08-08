// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/GeometryParticles.h"
#include "Apeiron/PerParticleRule.h"
#include "Apeiron/Transform.h"

#include <memory>

namespace Apeiron
{
template<class T, int d>
class TPerParticlePBDCollisionConstraint : public TPerParticleRule<T, d>
{
	struct VelocityConstraint
	{
		TVector<T, d> Velocity;
		TVector<T, d> Normal;
	};

  public:
	TPerParticlePBDCollisionConstraint(const TKinematicGeometryParticles<T, d>& InParticles, TArrayCollectionArray<bool>& Collided, const T Thickness = (T)0, const T Friction = (T)0)
	    : MParticles(InParticles), MCollided(Collided), MThickness(Thickness), MFriction(Friction) {}
	virtual ~TPerParticlePBDCollisionConstraint() {}

	inline void Apply(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const override //-V762
	{
		if (InParticles.InvM(Index) == 0)
			return;
		for (uint32 i = 0; i < MParticles.Size(); ++i)
		{
			TVector<T, d> Normal;
			TRigidTransform<T, d> Frame(MParticles.X(i), MParticles.R(i));
			T Phi = MParticles.Geometry(i)->PhiWithNormal(Frame.InverseTransformPosition(InParticles.P(Index)), Normal);
			if (Phi < MThickness)
			{
				InParticles.P(Index) += (-Phi + MThickness) * Frame.TransformVector(Normal);
				if (MFriction > 0)
				{
					VelocityConstraint Constraint;
					TVector<T, d> VectorToPoint = InParticles.P(Index) - MParticles.X(i);
					Constraint.Velocity = MParticles.V(i) + TVector<T, d>::CrossProduct(MParticles.W(i), VectorToPoint);
					Constraint.Normal = Frame.TransformVector(Normal);
					MVelocityConstraints.Add(Index, Constraint);
				}
			}
		}
	}

	void ApplyFriction(TPBDParticles<T, d>& InParticles, const T Dt, const int32 Index) const
	{
		check(MFriction > 0);
		if (!MVelocityConstraints.Contains(Index))
		{
			return;
		}
		T VN = TVector<T, d>::DotProduct(InParticles.V(Index), MVelocityConstraints[Index].Normal);
		T VNBody = TVector<T, d>::DotProduct(MVelocityConstraints[Index].Velocity, MVelocityConstraints[Index].Normal);
		TVector<T, d> VTBody = MVelocityConstraints[Index].Velocity - VNBody * MVelocityConstraints[Index].Normal;
		TVector<T, d> VTRelative = InParticles.V(Index) - VN * MVelocityConstraints[Index].Normal - VTBody;
		T VTRelativeSize = VTRelative.Size();
		T VNMax = FGenericPlatformMath::Max(VN, VNBody);
		T VNDelta = VNMax - VN;
		T Friction = MFriction * VNDelta < VTRelativeSize ? MFriction * VNDelta / VTRelativeSize : 1;
		InParticles.V(Index) = VNMax * MVelocityConstraints[Index].Normal + VTBody + VTRelative * (1 - Friction);
	}

  private:
	// TODO(mlentine): Need a bb hierarchy
	const TKinematicGeometryParticles<T, d>& MParticles;
	TArrayCollectionArray<bool>& MCollided;
	mutable TMap<int32, VelocityConstraint> MVelocityConstraints;
	const T MThickness, MFriction;
};
}
