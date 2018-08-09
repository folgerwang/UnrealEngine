// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if INCLUDE_APEIRON

#include "Apeiron/Defines.h"
#include "Apeiron/PBDRigidsEvolution.h"
#include "Apeiron/PBDRigidParticles.h"
#include "Apeiron/PBDSpringConstraints.h"
#include "Apeiron/PerParticleGravity.h"
#include "Apeiron/Transform.h"

class AdvanceOneTimeStepTask;
class FPhysInterface_Apeiron;

class FPhysScene_Apeiron
{
public:
    friend class AdvanceOneTimeStepTask;
    friend class FPhysInterface_Apeiron;

	ENGINE_API FPhysScene_Apeiron();

    ENGINE_API void Tick(float DeltaTime);

    const Apeiron::TPBDRigidParticles<float, 3>& GetRigidParticles() const
    {
        return MEvolution->Particles();
    }

    const TSet<TTuple<int32, int32>>& GetDisabledCollisionPairs() const
    {
        return MEvolution->DisabledCollisions();
    }

	void InitializeFromParticleData() { MEvolution->InitializeFromParticleData(); }

	/* Clustering Access */
	int32 CreateClusterParticle(const TArray<uint32>& Children) { return MEvolution->CreateClusterParticle(Children); }
	void SetClusterStrain(const uint32 ClusterId, float Strain) { MEvolution->Strain(ClusterId) = Strain; }

	/**/
	void SetFriction(float Friction) { MEvolution->SetFriction(Friction); }
	void SetRestitution(float Restitution) { MEvolution->SetRestitution(Restitution);}

    void SetKinematicUpdateFunction(TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&, const float, const float, const int32)> KinematicUpdate) { MEvolution->SetKinematicUpdateFunction(KinematicUpdate); }
	void SetStartFrameFunction(TFunction<void(const float)> StartFrame) { MStartFrame = StartFrame; }
	void SetEndFrameFunction(TFunction<void(const float)> EndFrame) { MEndFrame = EndFrame; }
	void SetCreateBodiesFunction(TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&)> CreateBodies) { MCreateBodies = CreateBodies; }
    void SetParameterUpdateFunction(TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&, const float, const int32)> ParameterUpdate) { MParameterUpdate = ParameterUpdate; }
    void SetDisableCollisionsUpdateFunction(TFunction<void(TSet<TTuple<int32, int32>>&)> DisableCollisionsUpdate) { MDisableCollisionsUpdate = DisableCollisionsUpdate; }
    void AddPBDConstraintFunction(TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&, const float)> ConstraintFunction) { MEvolution->AddPBDConstraintFunction(ConstraintFunction); }
    void AddForceFunction(TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&, const float, const int32)> ForceFunction) { MEvolution->AddForceFunction(ForceFunction); }

private:
    TUniquePtr<Apeiron::TPBDRigidsEvolution<float, 3>> MEvolution;
    TFunction<void(const float)> MStartFrame;
	TFunction<void(const float)> MEndFrame;
	TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&)> MCreateBodies;
    TFunction<void(Apeiron::TPBDRigidParticles<float, 3>&, const float, const int32)> MParameterUpdate;
    TFunction<void(TSet<TTuple<int32, int32>>&)> MDisableCollisionsUpdate;
    float MTime;
    float MMaxDeltaTime;
    
    TSharedPtr<FCriticalSection> MCurrentLock;
    TSharedPtr<FEvent> MCurrentEvent;
};

#endif