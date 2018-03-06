// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsReplication.cpp: Code for updating body instance physics state based on replication
=============================================================================*/ 

#include "PhysicsReplication.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsPublic.h"

namespace CharacterMovementCVars
{
	extern int32 NetShowCorrections;
	extern float NetCorrectionLifetime;
}

bool FPhysicsReplication::ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, const FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection)
{
	bool bRestoredState = true;
	bool bTeleport = true;
	FVector DeltaPos = FVector::ZeroVector;
	FRigidBodyState NewState = PhysicsTarget.TargetState;

	if (BI->IsInstanceSimulatingPhysics())
	{
		// failure cases
		const float QuatSizeSqr = NewState.Quaternion.SizeSquared();
		if (QuatSizeSqr < KINDA_SMALL_NUMBER)
		{

			UE_LOG(LogPhysics, Warning, TEXT("Invalid zero quaternion set for body. (%s)"), *BI->GetBodyDebugName());
			return bRestoredState;
		}
		else if (FMath::Abs(QuatSizeSqr - 1.f) > KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogPhysics, Warning, TEXT("Quaternion (%f %f %f %f) with non-unit magnitude detected. (%s)"),
				NewState.Quaternion.X, NewState.Quaternion.Y, NewState.Quaternion.Z, NewState.Quaternion.W, *BI->GetBodyDebugName());
			return bRestoredState;
		}

		FRigidBodyState CurrentState;
		BI->GetRigidBodyState(CurrentState);


		NewState.Position = FMath::Lerp(CurrentState.Position, NewState.Position, ErrorCorrection.LinearInterpAlpha);
		NewState.LinVel = FMath::Lerp(CurrentState.LinVel, NewState.LinVel, ErrorCorrection.LinearInterpAlpha);

		const bool bShouldSleep = (NewState.Flags & ERigidBodyFlags::Sleeping) != 0;
		/////// POSITION CORRECTION ///////

		// Find out how much of a correction we are making
		DeltaPos = NewState.Position - CurrentState.Position;
		const float DeltaMagSq = DeltaPos.SizeSquared();
		const float BodyLinearSpeedSq = CurrentState.LinVel.SizeSquared();

		// Snap position by default (big correction, or we are moving too slowly)
		FVector UpdatedPos = NewState.Position;
		FVector FixLinVel = FVector::ZeroVector;

		// If its a small correction and velocity is above threshold, only make a partial correction, 
		// and calculate a velocity that would fix it over 'fixTime'.
		//TODO: make this work with hard teleporting
		//if (DeltaMagSq < ErrorCorrection.LinearDeltaThresholdSq  && BodyLinearSpeedSq >= ErrorCorrection.BodySpeedThresholdSq)
		{
			//UpdatedPos =  FMath::Lerp(CurrentState.Position, NewState.Position, ErrorCorrection.LinearInterpAlpha);
			FixLinVel = (NewState.Position - UpdatedPos) * ErrorCorrection.LinearRecipFixTime;
			//FixLinVel = FMath::Lerp(CurrentState.LinVel, NewState.LinVel, ErrorCorrection.LinearInterpAlpha);
			bTeleport = false;
		}

		// Get the linear correction
		DeltaPos = UpdatedPos - CurrentState.Position;

		/////// ORIENTATION CORRECTION ///////
		// Get quaternion that takes us from old to new
		const FQuat InvCurrentQuat = CurrentState.Quaternion.Inverse();
		const FQuat DeltaQuat = NewState.Quaternion * InvCurrentQuat;

		FVector DeltaAxis;
		float DeltaAng;	// radians
		DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAng);
		DeltaAng = FMath::UnwindRadians(DeltaAng);

		// Snap rotation by default (big correction, or we are moving too slowly)
		FQuat UpdatedQuat = NewState.Quaternion;
		FVector FixAngVel = FVector::ZeroVector; // degrees per second

												 // If the error is small, and we are moving, try to move smoothly to it
		if (FMath::Abs(DeltaAng) < ErrorCorrection.AngularDeltaThreshold)
		{
			UpdatedQuat = FMath::Lerp(CurrentState.Quaternion, NewState.Quaternion, ErrorCorrection.AngularInterpAlpha);
			FixAngVel = DeltaAxis.GetSafeNormal() * FMath::RadiansToDegrees(DeltaAng) * (1.f - ErrorCorrection.AngularInterpAlpha) * ErrorCorrection.AngularRecipFixTime;
		}

		/////// BODY UPDATE ///////
		FVector NewVelocity = NewState.LinVel + FixLinVel;
		if (bTeleport)
		{
			NewVelocity = NewState.LinVel;
		}

		BI->SetBodyTransform(FTransform(UpdatedQuat, UpdatedPos), ETeleportType::TeleportPhysics);
		BI->SetLinearVelocity(NewVelocity, false);
		BI->SetAngularVelocityInRadians(FMath::DegreesToRadians(NewState.AngVel + FixAngVel), false);

		// state is restored when no velocity corrections are required
		bRestoredState = (FixLinVel.SizeSquared() < KINDA_SMALL_NUMBER) && (FixAngVel.SizeSquared() < KINDA_SMALL_NUMBER);

		/////// SLEEP UPDATE ///////
		const bool bIsAwake = BI->IsInstanceAwake();
		if (bIsAwake && (bShouldSleep && bRestoredState))
		{
			BI->PutInstanceToSleep();
		}
		else if (!bIsAwake)
		{
			BI->WakeInstance();
		}
	}

#if !UE_BUILD_SHIPPING
	if (CharacterMovementCVars::NetShowCorrections != 0)
	{
		FVector Origin;
		FVector Extents;
		FBox Bounds = BI->GetBodyBounds();
		Bounds.GetCenterAndExtents(Origin, Extents);
		if (UWorld* OwningWorld = PhysScene->GetOwningWorld())
		{
			DrawDebugBox(OwningWorld, Origin + DeltaPos, Extents, FQuat::Identity, FColor(100, 255, 100), true, CharacterMovementCVars::NetCorrectionLifetime);
			DrawDebugBox(OwningWorld, Origin, Extents, FQuat::Identity, bTeleport ? FColor(255, 100, 100) : FColor(255.f, 0.f, 255.f), true, CharacterMovementCVars::NetCorrectionLifetime);
		}
	}
#endif

	return bRestoredState;
}

void FPhysicsReplication::OnTick(float DeltaSeconds, TMap<FBodyInstance *, FReplicatedPhysicsTarget>& BodyToTargetMap)
{
	
	const FRigidBodyErrorCorrection& PhysicErrorCorrection = UPhysicsSettings::Get()->PhysicErrorCorrection;

	for (auto Itr = BodyToTargetMap.CreateIterator(); Itr; ++Itr)
	{
		FBodyInstance* BI = Itr.Key();
		FReplicatedPhysicsTarget& PhysicsTarget = Itr.Value();
		FRigidBodyState& UpdatedState = PhysicsTarget.TargetState;

		bool bUpdated = false;

		if (UPrimitiveComponent* OwnerComp = BI->OwnerComponent.Get())
		{
			AActor* OwningActor = OwnerComp->GetOwner();
			if (OwningActor && OwningActor->Role == ROLE_SimulatedProxy)	//TODO: can we avoid the replication all together?
			{

				// force update if simulation is sleeping on server
				if ((UpdatedState.Flags & ERigidBodyFlags::Sleeping) && BI->IsInstanceAwake())
				{
					UpdatedState.Flags |= ERigidBodyFlags::NeedsUpdate;
				}

				if (UpdatedState.Flags & ERigidBodyFlags::NeedsUpdate)
				{
					const bool bRestoredState = ApplyRigidBodyState(DeltaSeconds, BI, PhysicsTarget, PhysicErrorCorrection);
					if (bRestoredState)
					{
						//UpdatedState.Flags &= ~ERigidBodyFlags::NeedsUpdate;	//TODO: needs fixing to properly clean up
					}

					bUpdated = true;


					// Need to update the component to match new position.
					OwnerComp->SyncComponentToRBPhysics();
				}
			}
		}
	}
}

void FPhysicsReplication::Tick(float DeltaSeconds)
{
	OnTick(DeltaSeconds, BodiesToTargets);
}

FPhysicsReplication::FPhysicsReplication(FPhysScene* InPhysicsScene)
	: PhysScene(InPhysicsScene)
{

}

void FPhysicsReplication::SetReplicatedTarget(FBodyInstance* BI, const FRigidBodyState& ReplicatedTarget)
{
	if (UWorld* OwningWorld = PhysScene->GetOwningWorld())
	{
		BodiesToTargets.FindOrAdd(BI) = { ReplicatedTarget,  OwningWorld->GetTimeSeconds() };
	}
}