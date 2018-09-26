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
#include "Engine/Classes/GameFramework/PlayerController.h"
#include "Engine/Classes/GameFramework/PlayerState.h"
#include "Engine/Classes/Engine/Player.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysScene_PhysX.h"
#include "Components/SkeletalMeshComponent.h"

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif 

namespace CharacterMovementCVars
{
	extern int32 NetShowCorrections;
	extern float NetCorrectionLifetime;

	static int32 SkipPhysicsReplication = 0;
	static FAutoConsoleVariableRef CVarSkipPhysicsReplication(TEXT("p.SkipPhysicsReplication"), SkipPhysicsReplication, TEXT(""));

	static float NetPingExtrapolation = -1.0f;
	static FAutoConsoleVariableRef CVarNetPingExtrapolation(TEXT("p.NetPingExtrapolation"), NetPingExtrapolation, TEXT(""));

	static float NetPingLimit = -1.f;
	static FAutoConsoleVariableRef CVarNetPingLimit(TEXT("p.NetPingLimit"), NetPingLimit, TEXT(""));

	static float ErrorPerLinearDifference = -1.0f;
	static FAutoConsoleVariableRef CVarErrorPerLinearDifference(TEXT("p.ErrorPerLinearDifference"), ErrorPerLinearDifference, TEXT(""));

	static float ErrorPerAngularDifference = -1.0f;
	static FAutoConsoleVariableRef CVarErrorPerAngularDifference(TEXT("p.ErrorPerAngularDifference"), ErrorPerAngularDifference, TEXT(""));

	static float ErrorAccumulationSeconds = -1.0f;
	static FAutoConsoleVariableRef CVarErrorAccumulation(TEXT("p.ErrorAccumulationSeconds"), ErrorAccumulationSeconds, TEXT(""));

	static float ErrorAccumulationDistanceSq = -1.0f;
	static FAutoConsoleVariableRef CVarErrorAccumulationDistanceSq(TEXT("p.ErrorAccumulationDistanceSq"), ErrorAccumulationDistanceSq, TEXT(""));

	static float ErrorAccumulationSimilarity = -1.f;
	static FAutoConsoleVariableRef CVarErrorAccumulationSimilarity(TEXT("p.ErrorAccumulationSimilarity"), ErrorAccumulationSimilarity, TEXT(""));

	static float MaxLinearHardSnapDistance = -1.f;
	static FAutoConsoleVariableRef CVarMaxLinearHardSnapDistance(TEXT("p.MaxLinearHardSnapDistance"), MaxLinearHardSnapDistance, TEXT(""));

	static float MaxRestoredStateError = -1.0f;
	static FAutoConsoleVariableRef CVarMaxRestoredStateError(TEXT("p.MaxRestoredStateError"), MaxRestoredStateError, TEXT(""));

	static float PositionLerp = -1.0f;
	static FAutoConsoleVariableRef CVarLinSet(TEXT("p.PositionLerp"), PositionLerp, TEXT(""));

	static float LinearVelocityCoefficient = -1.0f;
	static FAutoConsoleVariableRef CVarLinLerp(TEXT("p.LinearVelocityCoefficient"), LinearVelocityCoefficient, TEXT(""));

	static float AngleLerp = -1.0f;
	static FAutoConsoleVariableRef CVarAngSet(TEXT("p.AngleLerp"), AngleLerp, TEXT(""));

	static float AngularVelocityCoefficient = -1.0f;
	static FAutoConsoleVariableRef CVarAngLerp(TEXT("p.AngularVelocityCoefficient"), AngularVelocityCoefficient, TEXT(""));

	static int32 AlwaysHardSnap = 0;
	static FAutoConsoleVariableRef CVarAlwaysHardSnap(TEXT("p.AlwaysHardSnap"), AlwaysHardSnap, TEXT(""));

	static int32 AlwaysResetPhysics = 0;
	static FAutoConsoleVariableRef CVarAlwaysResetPhysics(TEXT("p.AlwaysResetPhysics"), AlwaysResetPhysics, TEXT(""));
}

namespace PhysicsReplicationCVars
{
	static int32 SkipSkeletalRepOptimization = 1;
	static FAutoConsoleVariableRef CVarSkipSkeletalRepOptimization(TEXT("p.SkipSkeletalRepOptimization"), SkipSkeletalRepOptimization, TEXT("If true, we don't move the skeletal mesh component during replication. This is ok because the skeletal mesh already polls physx after its results"));
}

bool FPhysicsReplication::ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection, const float PingSecondsOneWay)
{
	if (CharacterMovementCVars::SkipPhysicsReplication)
	{
		return false;
	}

	if (!BI->IsInstanceSimulatingPhysics())
	{
		return false;
	}

	//
	// NOTES:
	//
	// The operation of this method has changed since 4.18.
	//
	// When a new remote physics state is received, this method will
	// be called on tick until the local state is within an adequate
	// tolerance of the new state.
	//
	// The received state is extrapolated based on ping, by some
	// adjustable amount.
	//
	// A correction velocity is added new state's velocity, and assigned
	// to the body. The correction velocity scales with the positional
	// difference, so without the interference of external forces, this
	// will result in an exponentially decaying correction.
	//
	// Generally it is not needed and will interrupt smoothness of
	// the replication, but stronger corrections can be obtained by
	// adjusting position lerping.
	//
	// If progress is not being made towards equilibrium, due to some
	// divergence in physics states between the owning and local sims,
	// an error value is accumulated, representing the amount of time
	// spent in an unresolvable state.
	//
	// Once the error value has exceeded some threshold (0.5 seconds
	// by default), a hard snap to the target physics state is applied.
	//

	bool bRestoredState = true;
	const FRigidBodyState NewState = PhysicsTarget.TargetState;
	const float NewQuatSizeSqr = NewState.Quaternion.SizeSquared();

	// failure cases
	if (!BI->IsInstanceSimulatingPhysics())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Physics replicating on non-simulated body. (%s)"), *BI->GetBodyDebugName());
		return bRestoredState;
	}
	else if (NewQuatSizeSqr < KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Invalid zero quaternion set for body. (%s)"), *BI->GetBodyDebugName());
		return bRestoredState;
	}
	else if (FMath::Abs(NewQuatSizeSqr - 1.f) > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Quaternion (%f %f %f %f) with non-unit magnitude detected. (%s)"),
			NewState.Quaternion.X, NewState.Quaternion.Y, NewState.Quaternion.Z, NewState.Quaternion.W, *BI->GetBodyDebugName());
		return bRestoredState;
	}

	// Grab configuration variables from engine config or from CVars if overriding is turned on.
	const float NetPingExtrapolation = CharacterMovementCVars::NetPingExtrapolation >= 0.0f ? CharacterMovementCVars::NetPingExtrapolation : ErrorCorrection.PingExtrapolation;
	const float NetPingLimit = CharacterMovementCVars::NetPingLimit > 0.0f ? CharacterMovementCVars::NetPingLimit : ErrorCorrection.PingLimit;
	const float ErrorPerLinearDiff = CharacterMovementCVars::ErrorPerLinearDifference >= 0.0f ? CharacterMovementCVars::ErrorPerLinearDifference : ErrorCorrection.ErrorPerLinearDifference;
	const float ErrorPerAngularDiff = CharacterMovementCVars::ErrorPerAngularDifference >= 0.0f ? CharacterMovementCVars::ErrorPerAngularDifference : ErrorCorrection.ErrorPerAngularDifference;
	const float MaxRestoredStateError = CharacterMovementCVars::MaxRestoredStateError >= 0.0f ? CharacterMovementCVars::MaxRestoredStateError : ErrorCorrection.MaxRestoredStateError;
	const float ErrorAccumulationSeconds = CharacterMovementCVars::ErrorAccumulationSeconds >= 0.0f ? CharacterMovementCVars::ErrorAccumulationSeconds : ErrorCorrection.ErrorAccumulationSeconds;
	const float ErrorAccumulationDistanceSq = CharacterMovementCVars::ErrorAccumulationDistanceSq >= 0.0f ? CharacterMovementCVars::ErrorAccumulationDistanceSq : ErrorCorrection.ErrorAccumulationDistanceSq;
	const float ErrorAccumulationSimilarity = CharacterMovementCVars::ErrorAccumulationSimilarity >= 0.0f ? CharacterMovementCVars::ErrorAccumulationSimilarity : ErrorCorrection.ErrorAccumulationSimilarity;
	const float PositionLerp = CharacterMovementCVars::PositionLerp >= 0.0f ? CharacterMovementCVars::PositionLerp : ErrorCorrection.PositionLerp;
	const float LinearVelocityCoefficient = CharacterMovementCVars::LinearVelocityCoefficient >= 0.0f ? CharacterMovementCVars::LinearVelocityCoefficient : ErrorCorrection.LinearVelocityCoefficient;
	const float AngleLerp = CharacterMovementCVars::AngleLerp >= 0.0f ? CharacterMovementCVars::AngleLerp : ErrorCorrection.AngleLerp;
	const float AngularVelocityCoefficient = CharacterMovementCVars::AngularVelocityCoefficient >= 0.0f ? CharacterMovementCVars::AngularVelocityCoefficient : ErrorCorrection.AngularVelocityCoefficient;
	const float MaxLinearHardSnapDistance = CharacterMovementCVars::MaxLinearHardSnapDistance >= 0.f ? CharacterMovementCVars::MaxLinearHardSnapDistance : ErrorCorrection.MaxLinearHardSnapDistance;

	// Get Current state
	FRigidBodyState CurrentState;
	BI->GetRigidBodyState(CurrentState);
	const FQuat InvCurrentQuat = CurrentState.Quaternion.Inverse();

	/////// EXTRAPOLATE APPROXIMATE TARGET VALUES ///////

	// Starting from the last known authoritative position, and
	// extrapolate an approximation using the last known velocity
	// and ping.
	const float PingSeconds = FMath::Clamp(PingSecondsOneWay, 0.f, NetPingLimit);
	const float ExtrapolationDeltaSeconds = PingSeconds * NetPingExtrapolation;
	const FVector ExtrapolationDeltaPos = NewState.LinVel * ExtrapolationDeltaSeconds;
	const FVector_NetQuantize100 TargetPos = NewState.Position + ExtrapolationDeltaPos;
	float NewStateAngVel;
	FVector NewStateAngVelAxis;
	NewState.AngVel.FVector::ToDirectionAndLength(NewStateAngVelAxis, NewStateAngVel);
	NewStateAngVel = FMath::DegreesToRadians(NewStateAngVel);
	const FQuat ExtrapolationDeltaQuaternion = FQuat(NewStateAngVelAxis, NewStateAngVel * ExtrapolationDeltaSeconds);
	FQuat TargetQuat = ExtrapolationDeltaQuaternion * NewState.Quaternion;

	/////// COMPUTE DIFFERENCES ///////

	const FVector LinDiff = TargetPos - CurrentState.Position;
	const float LinDiffSize = LinDiff.Size();
	FVector AngDiffAxis;
	float AngDiff;
	const FQuat DeltaQuat = InvCurrentQuat * TargetQuat;
	DeltaQuat.ToAxisAndAngle(AngDiffAxis, AngDiff);
	AngDiff = FMath::RadiansToDegrees(FMath::UnwindRadians(AngDiff));

	/////// ACCUMULATE ERROR IF NOT APPROACHING SOLUTION ///////

	const float Error = (LinDiffSize * ErrorPerLinearDiff) + (AngDiff * ErrorPerAngularDiff);
	bRestoredState = Error < MaxRestoredStateError;
	if (bRestoredState)
	{
		PhysicsTarget.AccumulatedErrorSeconds = 0.0f;
	}
	else
	{
		//
		// The heuristic for error accumulation here is:
		// 1. Did the physics tick from the previous step fail to
		//    move the body towards a resolved position?
		// 2. Was the linear error in the same direction as the
		//    previous frame?
		// 3. Is the linear error large enough to accumulate error?
		//
		// If these conditions are met, then "error" time will accumulate.
		// Once error has accumulated for a certain number of seconds,
		// a hard-snap to the target will be performed.
		//
		// TODO: Rotation while moving linearly can still mess up this
		// heuristic. We need to account for it.
		//

		// Project the change in position from the previous tick onto the
		// linear error from the previous tick. This value roughly represents
		// how much correction was performed over the previous physics tick.
		const float PrevProgress = FVector::DotProduct(
			FVector(CurrentState.Position) - PhysicsTarget.PrevPos,
			(PhysicsTarget.PrevPosTarget - PhysicsTarget.PrevPos).GetSafeNormal());

		// Project the current linear error onto the linear error from the
		// previous tick. This value roughly represents how little the direction
		// of the linear error state has changed, and how big the error is.
		const float PrevSimilarity = FVector::DotProduct(
			TargetPos - FVector(CurrentState.Position),
			PhysicsTarget.PrevPosTarget - PhysicsTarget.PrevPos);

		// If the conditions from the heuristic outlined above are met, accumulate
		// error. Otherwise, reduce it.
		if (PrevProgress < ErrorAccumulationDistanceSq &&
			PrevSimilarity > ErrorAccumulationSimilarity)
		{
			PhysicsTarget.AccumulatedErrorSeconds += DeltaSeconds;
		}
		else
		{
			PhysicsTarget.AccumulatedErrorSeconds = FMath::Max(PhysicsTarget.AccumulatedErrorSeconds - DeltaSeconds, 0.0f);
		}
	}

	PhysicsTarget.PrevPosTarget = TargetPos;
	PhysicsTarget.PrevPos = FVector(CurrentState.Position);

	// Hard snap if error accumulation or linear error is big enough, and clear the error accumulator.
	const bool bHardSnap =
		LinDiffSize > MaxLinearHardSnapDistance ||
		PhysicsTarget.AccumulatedErrorSeconds > ErrorAccumulationSeconds ||
		CharacterMovementCVars::AlwaysHardSnap;
	if (bHardSnap)
	{
		PhysicsTarget.AccumulatedErrorSeconds = 0.0f;
		bRestoredState = true;
	}

	/////// SIMPLE EXPONENTIAL MATCH ///////

	const FVector NewLinVel = bHardSnap ? FVector(NewState.LinVel) : FVector(NewState.LinVel) + (LinDiff * LinearVelocityCoefficient * DeltaSeconds);
	const FVector NewAngVel = bHardSnap ? FVector(NewState.AngVel) : FVector(NewState.AngVel) + (AngDiffAxis * AngDiff * AngularVelocityCoefficient * DeltaSeconds);

	const FVector NewPos = FMath::Lerp(CurrentState.Position, TargetPos, bHardSnap ? 1.0f : PositionLerp);
	const FQuat NewAng = FQuat::Slerp(CurrentState.Quaternion, TargetQuat, bHardSnap ? 1.0f : AngleLerp);

	/////// UPDATE BODY ///////

	// Store sleeping state
	const bool bShouldSleep = (NewState.Flags & ERigidBodyFlags::Sleeping) != 0;
	const bool bWasAwake = BI->IsInstanceAwake();
	const bool bAutoWake = !bShouldSleep;

	// Set the new transform
	const bool bResetPhysics = CharacterMovementCVars::AlwaysResetPhysics || bHardSnap;
	const ETeleportType PhysicsTeleportMode = bResetPhysics ? ETeleportType::ResetPhysics : ETeleportType::TeleportPhysics;
	BI->SetBodyTransform(FTransform(NewAng, NewPos), PhysicsTeleportMode, bAutoWake);

	// Set the new velocities
	BI->SetLinearVelocity(NewLinVel, false, bAutoWake);
	BI->SetAngularVelocityInRadians(FMath::DegreesToRadians(NewAngVel), false, bAutoWake);

	/////// SLEEP UPDATE ///////

	if (bShouldSleep && !bWasAwake)
	{
		BI->PutInstanceToSleep();
	}

#if !UE_BUILD_SHIPPING
	if (CharacterMovementCVars::NetShowCorrections != 0)
	{
		PhysicsTarget.ErrorHistory.bAutoAdjustMinMax = false;
		PhysicsTarget.ErrorHistory.MinValue = 0.0f;
		PhysicsTarget.ErrorHistory.MaxValue = 1.0f;
		PhysicsTarget.ErrorHistory.AddSample(PhysicsTarget.AccumulatedErrorSeconds / ErrorAccumulationSeconds);
		if (UWorld* OwningWorld = GetOwningWorld())
		{
			FColor Color = FColor::White;
			DrawDebugDirectionalArrow(OwningWorld, CurrentState.Position, TargetPos, 5.0f, Color, true, CharacterMovementCVars::NetCorrectionLifetime, 0, 1.5f);
			DrawDebugFloatHistory(*OwningWorld, PhysicsTarget.ErrorHistory, NewPos + FVector(0.0f, 0.0f, 100.0f), FVector2D(100.0f, 50.0f), FColor::White);
		}
	}
#endif

	return bRestoredState;
}

UWorld* FPhysicsReplication::GetOwningWorld()
{
	return PhysScene ? PhysScene->GetOwningWorld() : nullptr;
}

const UWorld* FPhysicsReplication::GetOwningWorld() const
{
	return PhysScene ? PhysScene->GetOwningWorld() : nullptr;
}

float FPhysicsReplication::GetLocalPing() const
{
	if (const UWorld* World = GetOwningWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			if (APlayerState* PlayerState = PlayerController->PlayerState)
			{
				return PlayerState->ExactPing;
			}
		}
	}
	return 0.0f;
}

float FPhysicsReplication::GetOwnerPing(const AActor* const Owner, const FReplicatedPhysicsTarget& Target) const
{
	//
	// NOTE: At the moment, we have no real way to objectively access the ping of the
	// authoritative simulation owner to the server, which is what this function
	// claims to return.
	//
	// In order to actually use ping to extrapolate replication, we need to access
	// it with something along the lines of the disabled code below.
	//
#if false
	if (UPlayer* OwningPlayer = OwningActor->GetNetOwningPlayer())
	{
		if (UWorld* World = GetOwningWorld())
		{
			if (APlayerController* PlayerController = OwningPlayer->GetPlayerController(World))
			{
				if (APlayerState* PlayerState = PlayerController->PlayerState)
				{
					return PlayerState->ExactPing;
				}
			}
		}
	}
#endif

	return 0.0f;
}

void FPhysicsReplication::OnTick(float DeltaSeconds, TMap<TWeakObjectPtr<UPrimitiveComponent>, FReplicatedPhysicsTarget>& ComponentsToTargets)
{
	const FRigidBodyErrorCorrection& PhysicErrorCorrection = UPhysicsSettings::Get()->PhysicErrorCorrection;

	// Get the ping between this PC & the server
	const float LocalPing = GetLocalPing();

	for (auto Itr = ComponentsToTargets.CreateIterator(); Itr; ++Itr)
	{
		bool bRemoveItr = false;
		if (UPrimitiveComponent* PrimComp = Itr.Key().Get())
		{
			if (FBodyInstance* BI = PrimComp->GetBodyInstance(Itr.Value().BoneName))
			{
				FReplicatedPhysicsTarget& PhysicsTarget = Itr.Value();
				FRigidBodyState& UpdatedState = PhysicsTarget.TargetState;

				AActor* OwningActor = PrimComp->GetOwner();
				if (OwningActor && OwningActor->Role == ROLE_SimulatedProxy)	//TODO: can we avoid the replication all together?
				{
					// Get the ping of the guy who owns this thing. If nobody is,
					// then it's server authoritative.
					const float OwnerPing = GetOwnerPing(OwningActor, PhysicsTarget);

					// Get the total ping - this approximates the time since the update was
					// actually generated on the machine that is doing the authoritative sim.
					// NOTE: We divide by 2 to approximate 1-way ping from 2-way ping.
					const float PingSecondsOneWay = (LocalPing + OwnerPing) * 0.5f * 0.001f;

					if (UpdatedState.Flags & ERigidBodyFlags::NeedsUpdate)
					{
						const bool bRestoredState = ApplyRigidBodyState(DeltaSeconds, BI, PhysicsTarget, PhysicErrorCorrection, PingSecondsOneWay);

						// Need to update the component to match new position.
						if (PhysicsReplicationCVars::SkipSkeletalRepOptimization == 0 || Cast<USkeletalMeshComponent>(PrimComp) == nullptr)	//simulated skeletal mesh does its own polling of physics results so we don't need to call this as it'll happen at the end of the physics sim
						{
							PrimComp->SyncComponentToRBPhysics();
						}

						if (bRestoredState)
						{
							bRemoveItr = true;
						}
					}
				}
			}
		}

		if (bRemoveItr)
		{
			OnTargetRestored(Itr.Key().Get(), Itr.Value());
			Itr.RemoveCurrent();
		}
	}
}

void FPhysicsReplication::Tick(float DeltaSeconds)
{
	OnTick(DeltaSeconds, ComponentToTargets);
}

FPhysicsReplication::FPhysicsReplication(FPhysScene* InPhysicsScene)
	: PhysScene(InPhysicsScene)
{

}

void FPhysicsReplication::SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget)
{
	if (UWorld* OwningWorld = GetOwningWorld())
	{
		//TODO: there's a faster way to compare this
		TWeakObjectPtr<UPrimitiveComponent> TargetKey(Component);
		FReplicatedPhysicsTarget* Target = ComponentToTargets.Find(TargetKey);
		if (!Target)
		{
			// First time we add a target, set it's previous and correction
			// positions to the target position to avoid math with uninitialized
			// memory.
			Target = &ComponentToTargets.Add(TargetKey);
			Target->PrevPos = ReplicatedTarget.Position;
			Target->PrevPosTarget = ReplicatedTarget.Position;
		}

		Target->TargetState = ReplicatedTarget;
		Target->BoneName = BoneName;
		Target->ArrivedTimeSeconds = OwningWorld->GetTimeSeconds();

		ensure(!Target->PrevPos.ContainsNaN());
		ensure(!Target->PrevPosTarget.ContainsNaN());
		ensure(!Target->TargetState.Position.ContainsNaN());
	}
}

void FPhysicsReplication::RemoveReplicatedTarget(UPrimitiveComponent* Component)
{
	ComponentToTargets.Remove(Component);
}
