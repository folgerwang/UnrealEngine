// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/StaticMeshSolverCallbacks.h"

#if INCLUDE_CHAOS

DEFINE_LOG_CATEGORY_STATIC(GC_SC_Logging, NoLogging, All);

int8 FStaticMeshSolverCallbacks::Invalid = -1;

FStaticMeshSolverCallbacks::FStaticMeshSolverCallbacks()
	: FSolverCallbacks()
	, InitializedState(false)
	, RigidBodyId(INDEX_NONE)
{
}

void FStaticMeshSolverCallbacks::Initialize()
{
	InitializedState = false;
}


bool FStaticMeshSolverCallbacks::IsSimulating() const
{
	return Parameters.bSimulating;
}


void FStaticMeshSolverCallbacks::Reset()
{
	InitializedState = false;
}


void FStaticMeshSolverCallbacks::CreateRigidBodyCallback(FSolverCallbacks::FParticlesType& Particles)
{
	if (!InitializedState && Parameters.bSimulating)
	{
		FTransform WorldTransform = Parameters.InitialTransform;

		FBox Bounds(ForceInitToZero);
		for(const FVector& VertPosition : Parameters.MeshVertexPositions)
		{
			Bounds += VertPosition;
		}

		Scale = WorldTransform.GetScale3D();
		CenterOfMass = Bounds.GetCenter();
		Bounds = Bounds.InverseTransformBy(FTransform(CenterOfMass));
		Bounds.Min *= Scale;
		Bounds.Max *= Scale;
		checkSlow((Bounds.Max + Bounds.Min).Size() < FLT_EPSILON);

		RigidBodyId = Particles.Size();
		Particles.AddParticles(1);

		Particles.InvM(RigidBodyId) = 0.f;
		ensure(Parameters.Mass >= 0.f);
		Particles.M(RigidBodyId) = Parameters.Mass;
		if(Parameters.Mass > FLT_EPSILON)
		{
			Particles.InvM(RigidBodyId) = 1.f;
		}

		Particles.X(RigidBodyId) = WorldTransform.TransformPosition(CenterOfMass);
		Particles.V(RigidBodyId) = Chaos::TVector<float, 3>(Parameters.InitialLinearVelocity);
		Particles.R(RigidBodyId) = WorldTransform.GetRotation().GetNormalized();
		Particles.W(RigidBodyId) = Chaos::TVector<float, 3>(Parameters.InitialAngularVelocity);
		Particles.P(RigidBodyId) = Particles.X(RigidBodyId);
		Particles.Q(RigidBodyId) = Particles.R(RigidBodyId);

		FVector SideSquared(Bounds.GetSize()[0] * Bounds.GetSize()[0], Bounds.GetSize()[1] * Bounds.GetSize()[1], Bounds.GetSize()[2] * Bounds.GetSize()[2]);
		FVector Inertia((SideSquared.Y + SideSquared.Z) / 12.f, (SideSquared.X + SideSquared.Z) / 12.f, (SideSquared.X + SideSquared.Y) / 12.f);
		Particles.I(RigidBodyId) = Chaos::PMatrix<float, 3, 3>(Inertia.X, 0.f, 0.f, 0.f, Inertia.Y, 0.f, 0.f, 0.f, Inertia.Z);
		Particles.InvI(RigidBodyId) = Chaos::PMatrix<float, 3, 3>(1.f / Inertia.X, 0.f, 0.f, 0.f, 1.f / Inertia.Y, 0.f, 0.f, 0.f, 1.f / Inertia.Z);


		if(Parameters.ObjectType == EObjectTypeEnum::Chaos_Object_Sleeping)
		{
			Particles.SetSleeping(RigidBodyId, true);
		}
		else if(Parameters.ObjectType != EObjectTypeEnum::Chaos_Object_Dynamic)
		{
			Particles.InvM(RigidBodyId) = 0.f;
			Particles.InvI(RigidBodyId) = Chaos::PMatrix<float, 3, 3>(0);
		}

		Particles.Geometry(RigidBodyId) = new Chaos::TBox<float, 3>(Bounds.Min, Bounds.Max);

		InitializedState = true;
	}
}

/**/
void FStaticMeshSolverCallbacks::BindParticleCallbackMapping(const int32 & CallbackIndex, FSolverCallbacks::IntArray & ParticleCallbackMap)
{
	if (RigidBodyId != Invalid)
	{
		ParticleCallbackMap[RigidBodyId] = CallbackIndex;
	}
}

void FStaticMeshSolverCallbacks::EndFrameCallback(const float EndFrame)
{
	bool IsControlled = Parameters.ObjectType == EObjectTypeEnum::Chaos_Object_Kinematic;
	if (Parameters.bSimulating && !IsControlled && Parameters.TargetTransform)
	{
		const Chaos::TPBDRigidParticles<float, 3>& Particles = GetSolver()->GetRigidParticles();

		Parameters.TargetTransform->SetTranslation((FVector)Particles.X(RigidBodyId));
		Parameters.TargetTransform->SetRotation((FQuat)Particles.R(RigidBodyId));
	}
}

void FStaticMeshSolverCallbacks::UpdateKinematicBodiesCallback(const FSolverCallbacks::FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy)
{
	bool IsControlled = Parameters.ObjectType == EObjectTypeEnum::Chaos_Object_Kinematic;
	if (IsControlled && Parameters.bSimulating)
	{
		bool bFirst = !Proxy.Ids.Num();
		if (bFirst)
		{
			Proxy.Ids.Add(RigidBodyId);
			Proxy.Position.SetNum(1);
			Proxy.NextPosition.SetNum(1);
			Proxy.Rotation.SetNum(1);
			Proxy.NextRotation.SetNum(1);
		}
		const FTransform& Transform = Parameters.InitialTransform;
		Proxy.Position[0] = Chaos::TVector<float,3>(Transform.GetTranslation());
		Proxy.NextPosition[0] = Proxy.Position[0] + Chaos::TVector<float,3>(Parameters.InitialLinearVelocity) * Dt;
		Proxy.Rotation[0] = Chaos::TRotation<float, 3>(Transform.GetRotation().GetNormalized());
		Proxy.NextRotation[0] = Proxy.Rotation[0];
	}
}

#endif
