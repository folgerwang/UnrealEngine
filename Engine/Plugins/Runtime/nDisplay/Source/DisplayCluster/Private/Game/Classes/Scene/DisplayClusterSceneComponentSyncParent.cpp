// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSceneComponentSyncParent.h"
#include "GameFramework/Actor.h"


UDisplayClusterSceneComponentSyncParent::UDisplayClusterSceneComponentSyncParent(const FObjectInitializer& ObjectInitializer) :
	UDisplayClusterSceneComponentSync(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDisplayClusterSceneComponentSyncParent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


void UDisplayClusterSceneComponentSyncParent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// ...
}

void UDisplayClusterSceneComponentSyncParent::DestroyComponent(bool bPromoteChildren)
{
	Super::DestroyComponent(bPromoteChildren);
}



//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterSyncObject
//////////////////////////////////////////////////////////////////////////////////////////////
FString UDisplayClusterSceneComponentSyncParent::GetSyncId() const
{
	return FString::Printf(TEXT("SP_%s.%s"), *GetOwner()->GetName(), *GetAttachParent()->GetName());
}


bool UDisplayClusterSceneComponentSyncParent::IsDirty() const
{
	USceneComponent* const pParent = GetAttachParent();
	return (LastSyncLoc != pParent->RelativeLocation || LastSyncRot != pParent->RelativeRotation || LastSyncScale != pParent->RelativeScale3D);
}

void UDisplayClusterSceneComponentSyncParent::ClearDirty()
{
	USceneComponent* const pParent = GetAttachParent();
	LastSyncLoc = pParent->RelativeLocation;
	LastSyncRot = pParent->RelativeRotation;
	LastSyncScale = pParent->RelativeScale3D;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// UDisplayClusterSceneComponentSync
//////////////////////////////////////////////////////////////////////////////////////////////
FTransform UDisplayClusterSceneComponentSyncParent::GetSyncTransform() const
{
	return GetAttachParent()->GetRelativeTransform();
}

void UDisplayClusterSceneComponentSyncParent::SetSyncTransform(const FTransform& t)
{
	GetAttachParent()->SetRelativeTransform(t);
}
