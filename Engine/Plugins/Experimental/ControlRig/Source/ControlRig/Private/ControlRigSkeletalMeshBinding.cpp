// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSkeletalMeshBinding.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"

void FControlRigSkeletalMeshBinding::BindToObject(UObject* InObject)
{
	// If we are binding to an actor, find the first skeletal mesh component
	if (AActor* Actor = Cast<AActor>(InObject))
	{
		if (USkeletalMeshComponent* Component = Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			SkeletalMeshComponent = Component;
		}
	}
	else if (USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InObject))
	{
		SkeletalMeshComponent = Component;
	}
}

void FControlRigSkeletalMeshBinding::UnbindFromObject()
{
	SkeletalMeshComponent = nullptr;
}

bool FControlRigSkeletalMeshBinding::IsBoundToObject(UObject* InObject) const
{
	if (AActor* Actor = Cast<AActor>(InObject))
	{
		if (USkeletalMeshComponent* Component = Actor->FindComponentByClass<USkeletalMeshComponent>())
		{
			return SkeletalMeshComponent.Get() == Component;
		}
	}
	else if (USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InObject))
	{
		return SkeletalMeshComponent.Get() == Component;
	}

	return false;
}

UObject* FControlRigSkeletalMeshBinding::GetBoundObject() const
{
	return SkeletalMeshComponent.Get();
}

AActor* FControlRigSkeletalMeshBinding::GetHostingActor() const
{
	if (SkeletalMeshComponent.Get())
	{
		return SkeletalMeshComponent->GetOwner();
	}

	return nullptr;
}
