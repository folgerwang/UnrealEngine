// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NavSystemConfigOverride.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "AI/NavigationSystemBase.h"
#include "TimerManager.h"

#if WITH_EDITORONLY_DATA
#include "UObject/ConstructorHelpers.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Editor.h"
#endif // WITH_EDITORONLY_DATA


ANavSystemConfigOverride::ANavSystemConfigOverride(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> NoteTextureObject;
			FName ID_Notes;
			FText NAME_Notes;
			FConstructorStatics()
				: NoteTextureObject(TEXT("/Engine/EditorResources/S_Note"))
				, ID_Notes(TEXT("Notes"))
				, NAME_Notes(NSLOCTEXT("SpriteCategory", "Notes", "Notes"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.NoteTextureObject.Get();
			SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Notes;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Notes;
			SpriteComponent->SetupAttachment(RootComponent);
			SpriteComponent->Mobility = EComponentMobility::Static;
		}
	}
#endif // WITH_EDITORONLY_DATA

	bHidden = true;
	bCanBeDamaged = false;

	bNetLoadOnClient = false;
}

void ANavSystemConfigOverride::PostLoad()
{
	Super::PostLoad();

	UWorld* World = GetWorld();
	if (World && NavigationSystemConfig)
	{
		AWorldSettings* WorldSetting = World->GetWorldSettings();
		if (WorldSetting)
		{
			WorldSetting->SetNavigationSystemConfigOverride(NavigationSystemConfig);
		}

		if (World->bIsWorldInitialized
			&& NavigationSystemConfig
#if WITH_EDITOR
			&& (GIsEditorLoadingPackage == false)
#endif // WITH_EDITOR
			)
		{
			World->SetNavigationSystem(nullptr);

			const FNavigationSystemRunMode RunMode = World->WorldType == EWorldType::Editor
				? FNavigationSystemRunMode::EditorMode
				: (World->WorldType == EWorldType::PIE
					? FNavigationSystemRunMode::PIEMode
					: FNavigationSystemRunMode::GameMode)
				;

			if (RunMode == FNavigationSystemRunMode::EditorMode)
			{
				FNavigationSystem::AddNavigationSystemToWorld(*World, RunMode, NavigationSystemConfig, /*bInitializeForWorld=*/false);
#if WITH_EDITOR
				UNavigationSystemBase* NewNavSys = World->GetNavigationSystem();
				if (NewNavSys)
				{
					GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this
						, &ANavSystemConfigOverride::InitializeForWorld, NewNavSys, World, RunMode));
				}
#endif // WITH_EDITOR
			}
			else
			{
				FNavigationSystem::AddNavigationSystemToWorld(*World, RunMode, NavigationSystemConfig, /*bInitializeForWorld=*/true);
			}
		}
	}
}

void ANavSystemConfigOverride::PostInitProperties()
{
	Super::PostInitProperties();
}

#if WITH_EDITOR
void ANavSystemConfigOverride::InitializeForWorld(UNavigationSystemBase* NewNavSys, UWorld* World, const FNavigationSystemRunMode RunMode)
{
	if (NewNavSys && World)
	{
		NewNavSys->InitializeForWorld(*World, RunMode);
	}
}

void ANavSystemConfigOverride::ApplyChanges()
{
	UWorld* World = GetWorld();
	if (World)
	{
		AWorldSettings* WorldSetting = World->GetWorldSettings();
		if (WorldSetting)
		{
			WorldSetting->SetNavigationSystemConfigOverride(NavigationSystemConfig);
		}

		// recreate nav sys
		World->SetNavigationSystem(nullptr);
		FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode, NavigationSystemConfig, /*bInitializeForWorld=*/true);
	}
}

void ANavSystemConfigOverride::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bNetLoadOnClient = bLoadOnClient;
}
#endif // WITH_EDITOR