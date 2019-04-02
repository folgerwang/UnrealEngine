// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigComponent.h"
#include "ControlRig.h"
#include "ComponentInstanceDataCache.h"

#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif
////////////////////////////////////////////////////////////////////////////////////////

void FControlRigComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) 
{
	FActorComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
	UControlRigComponent* NewComponent = CastChecked<UControlRigComponent>(Component);

	UControlRig* NewControlRig = NewComponent->ControlRig;
	if (NewControlRig && AnimControlRig)
	{
		// it will just copy same property if not same class
		TArray<uint8> SavedPropertyBuffer;
		FObjectWriter Writer(AnimControlRig, SavedPropertyBuffer);
		FObjectReader Reader(NewComponent->ControlRig, SavedPropertyBuffer);
	}
}

void FControlRigComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	FActorComponentInstanceData::AddReferencedObjects(Collector);
	if (AnimControlRig)
	{
		Collector.AddReferencedObject(AnimControlRig);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
UControlRigComponent::UControlRigComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}

#if WITH_EDITOR
void UControlRigComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigComponent, ControlRig))
	{
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
		{
			FBlueprintEditorUtils::ReconstructAllNodes(Blueprint);
		}
	}
}
#endif

void UControlRigComponent::OnRegister()
{
	Super::OnRegister();

	if (ControlRig)
	{
		OnPreInitialize();
		ControlRig->Initialize();
		OnPostInitialize();
	}
}

void UControlRigComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UControlRigComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// @TODO: Add task to perform evaluation rather than performing it here.
	// @TODO: Double buffer ControlRig?
	if (ControlRig)
	{
		ControlRig->SetDeltaTime(DeltaTime);

		// Call pre-evaluation callbacks (e.g. for copying input data into the rig)
		OnPreEvaluate();
		ControlRig->PreEvaluate_GameThread();

		// If we multi-thread rig evaluation, then this call should be made on worker threads, but pre/post evaluate should be called on the game thread
		ControlRig->Evaluate_AnyThread();

		// Call post-evaluation callbacks (e.g. for copying output data out of the rig)
		ControlRig->PostEvaluate_GameThread();
		OnPostEvaluate();
	}
}

UControlRig* UControlRigComponent::BP_GetControlRig() const
{
	return ControlRig;
}

void UControlRigComponent::OnPreInitialize_Implementation()
{
	OnPreInitializeDelegate.Broadcast(this);
}

void UControlRigComponent::OnPostInitialize_Implementation()
{
	OnPostInitializeDelegate.Broadcast(this);
}

void UControlRigComponent::OnPreEvaluate_Implementation()
{
	OnPreEvaluateDelegate.Broadcast(this);
}

void UControlRigComponent::OnPostEvaluate_Implementation()
{
	OnPostEvaluateDelegate.Broadcast(this);
}

TStructOnScope<FActorComponentInstanceData> UControlRigComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<FActorComponentInstanceData, FControlRigComponentInstanceData>(this);
}
