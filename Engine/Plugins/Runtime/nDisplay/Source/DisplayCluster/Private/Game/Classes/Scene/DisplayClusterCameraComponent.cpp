// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterCameraComponent.h"


UDisplayClusterCameraComponent::UDisplayClusterCameraComponent(const FObjectInitializer& ObjectInitializer) :
	UDisplayClusterSceneComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UDisplayClusterCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// ...
	
}

void UDisplayClusterCameraComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// ...
}

void UDisplayClusterCameraComponent::SetSettings(const FDisplayClusterConfigSceneNode* pConfig)
{
	Super::SetSettings(pConfig);
}

bool UDisplayClusterCameraComponent::ApplySettings()
{
	return Super::ApplySettings();
}
