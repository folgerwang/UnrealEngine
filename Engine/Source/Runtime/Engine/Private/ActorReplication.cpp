// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/CoreNet.h"
#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/GameNetworkManager.h"
#include "NetworkingDistanceConstants.h"
#include "PhysicsReplication.h"
#include "PhysicsPublic.h"
#include "DrawDebugHelpers.h"

/*-----------------------------------------------------------------------------
	AActor networking implementation.
-----------------------------------------------------------------------------*/

//
// Static variables for networking.
//
namespace ActorReplication
{
	static bool		SavedbHidden;
	static AActor*	SavedOwner;
	static bool		SavedbRepPhysics;
	static ENetRole SavedRole;
}

float AActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	if (bNetUseOwnerRelevancy && Owner)
	{
		// If we should use our owner's priority, pass it through
		return Owner->GetNetPriority(ViewPos, ViewDir, Viewer, ViewTarget, InChannel, Time, bLowBandwidth);
	}

	if (ViewTarget && (this == ViewTarget || Instigator == ViewTarget))
	{
		// If we're the view target or owned by the view target, use a high priority
		Time *= 4.f;
	}
	else if (!bHidden && GetRootComponent() != NULL)
	{
		// If this actor has a location, adjust priority based on location
		FVector Dir = GetActorLocation() - ViewPos;
		float DistSq = Dir.SizeSquared();

		// Adjust priority based on distance and whether actor is in front of viewer
		if ((ViewDir | Dir) < 0.f)
		{
			if (DistSq > NEARSIGHTTHRESHOLDSQUARED)
			{
				Time *= 0.2f;
			}
			else if (DistSq > CLOSEPROXIMITYSQUARED)
			{
				Time *= 0.4f;
			}
		}
		else if ((DistSq < FARSIGHTTHRESHOLDSQUARED) && (FMath::Square(ViewDir | Dir) > 0.5f * DistSq))
		{
			// Compute the amount of distance along the ViewDir vector. Dir is not normalized
			// Increase priority if we're being looked directly at
			Time *= 2.f;
		}
		else if (DistSq > MEDSIGHTTHRESHOLDSQUARED)
		{
			Time *= 0.4f;
		}
	}

	return NetPriority * Time;
}

float AActor::GetReplayPriority(const FVector& ViewPos, const FVector& ViewDir, class AActor* Viewer, AActor* ViewTarget, UActorChannel* const InChannel, float Time)
{
	if (ViewTarget && (this == ViewTarget || Instigator == ViewTarget))
	{
		// If we're the view target or owned by the view target, use a high priority
		Time *= 10.0f;
	}
	else if (!bHidden && GetRootComponent() != NULL)
	{
		// If this actor has a location, adjust priority based on location
		FVector Dir = GetActorLocation() - ViewPos;
		float DistSq = Dir.SizeSquared();

		// Adjust priority based on distance
		if (DistSq < CLOSEPROXIMITYSQUARED)
		{
			Time *= 4.0f;
		}
		else if (DistSq < NEARSIGHTTHRESHOLDSQUARED)
		{
			Time *= 3.0f;
		}
		else if (DistSq < MEDSIGHTTHRESHOLDSQUARED)
		{
			Time *= 2.4f;
		}
		else if (DistSq < FARSIGHTTHRESHOLD)
		{
			Time *= 0.8f;
		}
		else
		{
			Time *= 0.2f;
		}
	}

	// Use NetPriority here to be compatible with live networking.
	return NetPriority * Time;
}

bool AActor::GetNetDormancy(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	// For now, per peer dormancy is not supported
	return false;
}

void AActor::PreNetReceive()
{
	ActorReplication::SavedbHidden = bHidden;
	ActorReplication::SavedOwner = Owner;
	ActorReplication::SavedbRepPhysics = ReplicatedMovement.bRepPhysics;
	ActorReplication::SavedRole = Role;
}

void AActor::PostNetReceive()
{
	if (!bNetCheckedInitialPhysicsState)
	{
		// Initially we need to sync the state regardless of whether bRepPhysics has "changed" since it may not currently match IsSimulatingPhysics().
		SyncReplicatedPhysicsSimulation();
		ActorReplication::SavedbRepPhysics = ReplicatedMovement.bRepPhysics;
		bNetCheckedInitialPhysicsState = true;
	}

	ExchangeB( bHidden, ActorReplication::SavedbHidden );
	Exchange ( Owner, ActorReplication::SavedOwner );

	if (bHidden != ActorReplication::SavedbHidden)
	{
		SetActorHiddenInGame(ActorReplication::SavedbHidden);
	}
	if (Owner != ActorReplication::SavedOwner)
	{
		SetOwner(ActorReplication::SavedOwner);
	}

	if (Role != ActorReplication::SavedRole)
	{
		PostNetReceiveRole();
	}
}

void AActor::PostNetReceiveRole()
{
}

static TAutoConsoleVariable<int32> CVarDrawDebugRepMovement(TEXT("Net.RepMovement.DrawDebug"), 0, TEXT(""), ECVF_Default);

void AActor::OnRep_ReplicatedMovement()
{
	// Since ReplicatedMovement and AttachmentReplication are REPNOTIFY_Always (and OnRep_AttachmentReplication may call OnRep_ReplicatedMovement directly),
	// this check is needed since this can still be called on actors for which bReplicateMovement is false - for example, during fast-forward in replay playback.
	// When this happens, the values in ReplicatedMovement aren't valid, and must be ignored.
	if (!bReplicateMovement)
	{
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarDrawDebugRepMovement->GetInt() > 0)
		{
			DrawDebugCapsule(GetWorld(), ReplicatedMovement.Location, GetSimpleCollisionHalfHeight(), GetSimpleCollisionRadius(), ReplicatedMovement.Rotation.Quaternion(), FColor(100, 255, 100), true, 1.f);
		}
#endif

	if (RootComponent)
	{
		if (ActorReplication::SavedbRepPhysics != ReplicatedMovement.bRepPhysics)
		{
			// Turn on/off physics sim to match server.
			SyncReplicatedPhysicsSimulation();
		}

		if (ReplicatedMovement.bRepPhysics)
		{
			// Sync physics state
			checkSlow(RootComponent->IsSimulatingPhysics());
			// If we are welded we just want the parent's update to move us.
			UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
			if (!RootPrimComp || !RootPrimComp->IsWelded())
			{
				PostNetReceivePhysicState();
			}
		}
		else
		{
			// Attachment trumps global position updates, see GatherCurrentMovement().
			if (!RootComponent->GetAttachParent())
			{
				if (Role == ROLE_SimulatedProxy)
				{
#if ENABLE_NAN_DIAGNOSTIC
					if (ReplicatedMovement.Location.ContainsNaN())
					{
						logOrEnsureNanError(TEXT("AActor::OnRep_ReplicatedMovement found NaN in ReplicatedMovement.Location"));
					}
					if (ReplicatedMovement.Rotation.ContainsNaN())
					{
						logOrEnsureNanError(TEXT("AActor::OnRep_ReplicatedMovement found NaN in ReplicatedMovement.Rotation"));
					}
#endif

					PostNetReceiveVelocity(ReplicatedMovement.LinearVelocity);
					PostNetReceiveLocationAndRotation();
				}
			}
		}
	}
}

void AActor::PostNetReceiveLocationAndRotation()
{
	FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(ReplicatedMovement.Location, this);

	if( RootComponent && RootComponent->IsRegistered() && (NewLocation != GetActorLocation() || ReplicatedMovement.Rotation != GetActorRotation()) )
	{
		SetActorLocationAndRotation(NewLocation, ReplicatedMovement.Rotation, /*bSweep=*/ false);
	}
}

void AActor::PostNetReceiveVelocity(const FVector& NewVelocity)
{
}

void AActor::PostNetReceivePhysicState()
{
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
	if (RootPrimComp)
	{
		FRigidBodyState NewState;
		ReplicatedMovement.CopyTo(NewState, this);

		FVector DeltaPos(FVector::ZeroVector);
		RootPrimComp->SetRigidBodyReplicatedTarget(NewState);
	}
}

void AActor::SyncReplicatedPhysicsSimulation()
{
	if (bReplicateMovement && RootComponent && (RootComponent->IsSimulatingPhysics() != ReplicatedMovement.bRepPhysics))
	{
		UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
		if (RootPrimComp)
		{
			RootPrimComp->SetSimulatePhysics(ReplicatedMovement.bRepPhysics);

			if(!ReplicatedMovement.bRepPhysics)
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* PhysScene = World->GetPhysicsScene())
					{
						if (FPhysicsReplication* PhysicsReplication = PhysScene->GetPhysicsReplication())
						{
							PhysicsReplication->RemoveReplicatedTarget(RootPrimComp);
						}
					}
				}
			}
		}
	}
}

bool AActor::IsWithinNetRelevancyDistance(const FVector& SrcLocation) const
{
	return FVector::DistSquared(SrcLocation, GetActorLocation()) < NetCullDistanceSquared;
}

bool AActor::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	if (bAlwaysRelevant || IsOwnedBy(ViewTarget) || IsOwnedBy(RealViewer) || this == ViewTarget || ViewTarget == Instigator)
	{
		return true;
	}
	else if ( bNetUseOwnerRelevancy && Owner)
	{
		return Owner->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}
	else if ( bOnlyRelevantToOwner )
	{
		return false;
	}
	else if ( RootComponent && RootComponent->GetAttachParent() && RootComponent->GetAttachParent()->GetOwner() && (Cast<USkeletalMeshComponent>(RootComponent->GetAttachParent()) || (RootComponent->GetAttachParent()->GetOwner() == Owner)) )
	{
		return RootComponent->GetAttachParent()->GetOwner()->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	}
	else if( bHidden && (!RootComponent || !RootComponent->IsCollisionEnabled()) )
	{
		return false;
	}

	if (!RootComponent)
	{
		UE_LOG(LogNet, Warning, TEXT("Actor %s / %s has no root component in AActor::IsNetRelevantFor. (Make bAlwaysRelevant=true?)"), *GetClass()->GetName(), *GetName() );
		return false;
	}

	return !GetDefault<AGameNetworkManager>()->bUseDistanceBasedRelevancy ||
			IsWithinNetRelevancyDistance(SrcLocation);
}

bool AActor::IsReplayRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation, const float CullDistanceOverrideSq) const
{
	return IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

void AActor::GatherCurrentMovement()
{
	if (bReplicateMovement || (RootComponent && RootComponent->GetAttachParent()))
	{
		AttachmentReplication.AttachParent = nullptr;
		AttachmentReplication.AttachComponent = nullptr;

		UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(GetRootComponent());
		if (RootPrimComp && RootPrimComp->IsSimulatingPhysics())
		{
			FRigidBodyState RBState;
			RootPrimComp->GetRigidBodyState(RBState);

			ReplicatedMovement.FillFrom(RBState, this);
			// Don't replicate movement if we're welded to another parent actor.
			// Their replication will affect our position indirectly since we are attached.
			ReplicatedMovement.bRepPhysics = !RootPrimComp->IsWelded();
		}
		else if (RootComponent != nullptr)
		{
			// If we are attached, don't replicate absolute position, use AttachmentReplication instead.
			if (RootComponent->GetAttachParent() != nullptr)
			{
				// Networking for attachments assumes the RootComponent of the AttachParent actor. 
				// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
				AttachmentReplication.AttachParent = RootComponent->GetAttachParent()->GetAttachmentRootActor();
				if (AttachmentReplication.AttachParent != nullptr)
				{
					AttachmentReplication.LocationOffset = RootComponent->RelativeLocation;
					AttachmentReplication.RotationOffset = RootComponent->RelativeRotation;
					AttachmentReplication.RelativeScale3D = RootComponent->RelativeScale3D;
					AttachmentReplication.AttachComponent = RootComponent->GetAttachParent();
					AttachmentReplication.AttachSocket = RootComponent->GetAttachSocketName();
				}
			}
			else
			{
				ReplicatedMovement.Location = FRepMovement::RebaseOntoZeroOrigin(RootComponent->GetComponentLocation(), this);
				ReplicatedMovement.Rotation = RootComponent->GetComponentRotation();
				ReplicatedMovement.LinearVelocity = GetVelocity();
				ReplicatedMovement.AngularVelocity = FVector::ZeroVector;
			}

			ReplicatedMovement.bRepPhysics = false;
		}
	}
}

void AActor::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass != NULL)
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}

	DOREPLIFETIME( AActor, bReplicateMovement );
	DOREPLIFETIME( AActor, Role );
	DOREPLIFETIME( AActor, RemoteRole );
	DOREPLIFETIME( AActor, Owner );
	DOREPLIFETIME( AActor, bHidden );

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DOREPLIFETIME( AActor, bTearOff );
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	DOREPLIFETIME( AActor, bCanBeDamaged );
	DOREPLIFETIME_CONDITION_NOTIFY( AActor, AttachmentReplication, COND_Custom, REPNOTIFY_Always );

	DOREPLIFETIME( AActor, Instigator );

	DOREPLIFETIME_CONDITION_NOTIFY( AActor, ReplicatedMovement, COND_SimulatedOrPhysics, REPNOTIFY_Always );
}

bool AActor::ReplicateSubobjects(UActorChannel *Channel, FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	check(Channel);
	check(Bunch);
	check(RepFlags);

	bool WroteSomething = false;

	for (UActorComponent* ActorComp : ReplicatedComponents)
	{
		if (ActorComp && ActorComp->GetIsReplicated())
		{
			WroteSomething |= ActorComp->ReplicateSubobjects(Channel, Bunch, RepFlags);		// Lets the component add subobjects before replicating its own properties.
			WroteSomething |= Channel->ReplicateSubobject(ActorComp, *Bunch, *RepFlags);	// (this makes those subobjects 'supported', and from here on those objects may have reference replicated)		
		}
	}
	return WroteSomething;
}

void AActor::GetSubobjectsWithStableNamesForNetworking(TArray<UObject*> &ObjList)
{	
	// For experimenting with replicating ALL stably named components initially
	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component && !Component->IsPendingKill() && Component->IsNameStableForNetworking())
		{
			ObjList.Add(Component);
			Component->GetSubobjectsWithStableNamesForNetworking(ObjList);
		}
	}

	// Sort the list so that we generate the same list on client/server
	struct FCompareComponentNames
	{
		FORCEINLINE bool operator()( UObject& A, UObject& B ) const
		{
			return A.GetName() < B.GetName();
		}
	};

	Sort( ObjList.GetData(), ObjList.Num(), FCompareComponentNames() );
}

void AActor::OnSubobjectCreatedFromReplication(UObject *NewSubobject)
{
	check(NewSubobject);
	if ( UActorComponent * Component = Cast<UActorComponent>(NewSubobject) )
	{
		Component->RegisterComponent();
		Component->SetIsReplicated(true);
	}
}

/** Called on the actor when a subobject is dynamically destroyed via replication */
void AActor::OnSubobjectDestroyFromReplication(UObject *Subobject)
{
	check(Subobject);
	if ( UActorComponent * Component = Cast<UActorComponent>(Subobject) )
	{
		Component->DestroyComponent();
	}
}

bool AActor::IsNameStableForNetworking() const
{
	return IsNetStartupActor() || HasAnyFlags( RF_ClassDefaultObject | RF_ArchetypeObject );
}

bool AActor::IsSupportedForNetworking() const
{
	return true;		// All actors are supported for networking
}

void AActor::OnRep_Owner()
{

}
