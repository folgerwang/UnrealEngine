// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSceneComponentSyncThis.h"

#include "GameFramework/Actor.h"


UDisplayClusterSceneComponentSyncThis::UDisplayClusterSceneComponentSyncThis(const FObjectInitializer& ObjectInitializer) :
	UDisplayClusterSceneComponentSync(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDisplayClusterSceneComponentSyncThis::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


void UDisplayClusterSceneComponentSyncThis::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// ...
}

void UDisplayClusterSceneComponentSyncThis::DestroyComponent(bool bPromoteChildren)
{
	Super::DestroyComponent(bPromoteChildren);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterSyncObject
//////////////////////////////////////////////////////////////////////////////////////////////
FString UDisplayClusterSceneComponentSyncThis::GetSyncId() const
{
	return FString::Printf(TEXT("ST_%s"), *GetOwner()->GetName());
}

bool UDisplayClusterSceneComponentSyncThis::IsDirty() const
{
	return (LastSyncLoc != RelativeLocation || LastSyncRot != RelativeRotation || LastSyncScale != RelativeScale3D);
}

void UDisplayClusterSceneComponentSyncThis::ClearDirty()
{
	LastSyncLoc = RelativeLocation;
	LastSyncRot = RelativeRotation;
	LastSyncScale = RelativeScale3D;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// UDisplayClusterSceneComponentSync
//////////////////////////////////////////////////////////////////////////////////////////////
FTransform UDisplayClusterSceneComponentSyncThis::GetSyncTransform() const
{
	return GetRelativeTransform();
}

void UDisplayClusterSceneComponentSyncThis::SetSyncTransform(const FTransform& t)
{
	SetRelativeTransform(t);
}
