// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/Actor.h"

// Sets default values for this component's properties
ULiveLinkComponent::ULiveLinkComponent()
	: bIsDirty(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;
	bTickInEditor = true;
}

void ULiveLinkComponent::OnRegister()
{
	bIsDirty = true;
	Super::OnRegister();
}


// Called every frame
void ULiveLinkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// If we have been recently registered then ensure all Skeletal Mesh Components on the actor run in editor
	if (bIsDirty)
	{
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		GetOwner()->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			SkeletalMeshComponent->SetUpdateAnimationInEditor(true);
		}
		bIsDirty = false;
	}
	
	if (OnLiveLinkUpdated.IsBound())
	{
		FEditorScriptExecutionGuard ScriptGuard;

		OnLiveLinkUpdated.Broadcast(DeltaTime);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool ULiveLinkComponent::HasLiveLinkClient()
{
	if (LiveLinkClient == nullptr)
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		}
	}

	return (LiveLinkClient != nullptr);
}

void ULiveLinkComponent::GetAvailableSubjectNames(TArray<FName>& SubjectNames)
{
	SubjectNames.Empty();
	if (HasLiveLinkClient())
	{
		LiveLinkClient->GetSubjectNames(SubjectNames);
	}
}

void ULiveLinkComponent::GetSubjectData(const FName SubjectName, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle)
{
	bSuccess = false;
	if (HasLiveLinkClient())
	{
		if (const FLiveLinkSubjectFrame* SubjectFrame = LiveLinkClient->GetSubjectData(SubjectName))
		{
			SubjectFrameHandle.SetCachedFrame(MakeShared<FCachedSubjectFrame>(SubjectFrame));
			bSuccess = true;
		}
	}
}
