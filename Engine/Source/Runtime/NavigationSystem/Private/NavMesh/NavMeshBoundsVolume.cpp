// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Engine/CollisionProfile.h"
#include "Components/BrushComponent.h"

ANavMeshBoundsVolume::ANavMeshBoundsVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->Mobility = EComponentMobility::Static;

	BrushColor = FColor(200, 200, 200, 255);
	SupportedAgents.MarkInitialized();

	bColored = true;
}

#if WITH_EDITOR

void ANavMeshBoundsVolume::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (GIsEditor && NavSys)
	{
		const FName PropName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : FName();
		const FName MemberName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : FName();

		if (PropName == GET_MEMBER_NAME_CHECKED(ABrush, BrushBuilder)
			|| MemberName == GET_MEMBER_NAME_CHECKED(ANavMeshBoundsVolume, SupportedAgents)
			|| MemberName == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation)
			|| MemberName == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation)
			|| MemberName == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D))
		{
			NavSys->OnNavigationBoundsUpdated(this);
		}
	}
}

void ANavMeshBoundsVolume::PostEditUndo()
{
	Super::PostEditUndo();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (GIsEditor && NavSys)
	{
		NavSys->OnNavigationBoundsUpdated(this);
	}
}

#endif // WITH_EDITOR

void ANavMeshBoundsVolume::PostRegisterAllComponents() 
{
	Super::PostRegisterAllComponents();
	
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys && Role == ROLE_Authority)
	{
		NavSys->OnNavigationBoundsAdded(this);
	}
}

void ANavMeshBoundsVolume::PostUnregisterAllComponents() 
{
	Super::PostUnregisterAllComponents();
	
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys && Role == ROLE_Authority)
	{
		NavSys->OnNavigationBoundsRemoved(this);
	}
}
