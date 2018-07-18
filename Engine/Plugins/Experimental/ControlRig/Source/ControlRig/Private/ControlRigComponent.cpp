// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigComponent.h"
#include "ControlRig.h"
#include "ComponentInstanceDataCache.h"

#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif
////////////////////////////////////////////////////////////////////////////////////////

/** Used to store animation ControlRig data during recompile of BP */
class FControlRigComponentInstanceData : public FActorComponentInstanceData
{
public:
	FControlRigComponentInstanceData(const UControlRigComponent* SourceComponent)
		: FActorComponentInstanceData(SourceComponent)
		, AnimControlRig(SourceComponent->ControlRig)
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
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

	virtual void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap) override
	{
		FActorComponentInstanceData::FindAndReplaceInstances(OldToNewInstanceMap);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FActorComponentInstanceData::AddReferencedObjects(Collector);
		if (AnimControlRig)
		{
			Collector.AddReferencedObject(AnimControlRig);
		}
	}

	bool ContainsData() const
	{
		return (AnimControlRig != nullptr);
	}

	// stored object
	UControlRig* AnimControlRig;
};

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

FActorComponentInstanceData* UControlRigComponent::GetComponentInstanceData() const
{
	FControlRigComponentInstanceData* InstanceData = new FControlRigComponentInstanceData(this);

	if (!InstanceData->ContainsData())
	{
		delete InstanceData;
		InstanceData = nullptr;
	}

	return InstanceData;
}
