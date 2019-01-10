// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VRScoutingInteractor.h"

#include "Components/StaticMeshComponent.h"
#include "ViewportWorldInteraction.h"
#include "VREditorMode.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Selection.h"
#endif

#define LOCTEXT_NAMESPACE "UVRScoutingInteractor"

UVRScoutingInteractor::UVRScoutingInteractor() :
	Super(),
	FlyingIndicatorComponent(nullptr)
{
}

float UVRScoutingInteractor::GetSlideDelta_Implementation() const
{
		return 0.0f;
}

void UVRScoutingInteractor::SetupComponent_Implementation( AActor* OwningActor )
{
	Super::SetupComponent_Implementation(OwningActor);

	// Flying Mesh
	FlyingIndicatorComponent = NewObject<UStaticMeshComponent>(OwningActor);
	OwningActor->AddOwnedComponent(FlyingIndicatorComponent);
	FlyingIndicatorComponent->SetupAttachment(HandMeshComponent);

	FlyingIndicatorComponent->RegisterComponent();

	FlyingIndicatorComponent->SetMobility(EComponentMobility::Movable);
	FlyingIndicatorComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlyingIndicatorComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	FlyingIndicatorComponent->SetVisibility(false);
	FlyingIndicatorComponent->SetCastShadow(false);
}

void UVRScoutingInteractor::Shutdown_Implementation()
{
	Super::Shutdown_Implementation();

	FlyingIndicatorComponent = nullptr;
}

TArray<AActor*> UVRScoutingInteractor::GetSelectedActors()
{
#if WITH_EDITOR
	if (GEditor != nullptr)
	{
		TArray<AActor*> SelectedActors;
		SelectedActors.Reserve(GEditor->GetSelectedActorCount());

		for (auto It = GEditor->GetSelectedActorIterator(); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				SelectedActors.Emplace(Actor);
			}
		}

		return SelectedActors;
	}
#endif
	return TArray<AActor*>();
}

#undef LOCTEXT_NAMESPACE
