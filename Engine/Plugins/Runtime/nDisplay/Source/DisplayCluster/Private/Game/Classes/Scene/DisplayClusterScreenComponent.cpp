// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterScreenComponent.h"
#include "DisplayClusterSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameEngine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/IPDisplayClusterGameManager.h"
#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"
#include "EngineDefines.h"


UDisplayClusterScreenComponent::UDisplayClusterScreenComponent(const FObjectInitializer& ObjectInitializer) :
	UDisplayClusterSceneComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

#if WITH_EDITOR
	if (GEngine && GEngine->IsEditor())
	{
		const IPDisplayClusterGameManager* const GameMgr = GDisplayCluster->GetPrivateGameMgr();
		if (GameMgr)
		{
			const ADisplayClusterSettings* const pDisplayClusterSettings = GameMgr->GetDisplayClusterSceneSettings();
			if (pDisplayClusterSettings && pDisplayClusterSettings->bEditorShowProjectionScreens)
			{
				ScreenGeometryComponent = CreateDefaultSubobject<UStaticMeshComponent>(FName(*(GetName() + FString("_impl"))));
				check(ScreenGeometryComponent);

				if (ScreenGeometryComponent)
				{
					static ConstructorHelpers::FObjectFinder<UStaticMesh> screenMesh(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
					static ConstructorHelpers::FObjectFinder<UMaterial>   screenMat(TEXT("Material'/Engine/Engine_MI_Shaders/M_Shader_SimpleTranslucent.M_Shader_SimpleTranslucent'"));

					ScreenGeometryComponent->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
					ScreenGeometryComponent->SetStaticMesh(screenMesh.Object);
					ScreenGeometryComponent->SetMobility(EComponentMobility::Movable);
					ScreenGeometryComponent->SetMaterial(0, screenMat.Object);
					ScreenGeometryComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}
#endif
}


void UDisplayClusterScreenComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}


void UDisplayClusterScreenComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UDisplayClusterScreenComponent::SetSettings(const FDisplayClusterConfigSceneNode* pConfig)
{
	const FDisplayClusterConfigScreen* pScreenCfg = static_cast<const FDisplayClusterConfigScreen*>(pConfig);
	Size = pScreenCfg->Size;

	Super::SetSettings(pConfig);
}

bool UDisplayClusterScreenComponent::ApplySettings()
{
	Super::ApplySettings();

#if WITH_EDITOR
	if (ScreenGeometryComponent)
	{
		ScreenGeometryComponent->RegisterComponent();
		ScreenGeometryComponent->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		ScreenGeometryComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator, false);
	}
#endif

	SetRelativeScale3D(FVector(0.0001f, Size.X, Size.Y));

	return true;
}
