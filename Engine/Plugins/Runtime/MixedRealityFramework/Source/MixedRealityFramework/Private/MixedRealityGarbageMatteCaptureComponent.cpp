// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityGarbageMatteCaptureComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "MixedRealityConfigurationSaveGame.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "MixedRealityUtilLibrary.h"
#include "UObject/ConstructorHelpers.h"

/* UMixedRealityGarbageMatteCaptureComponent
 *****************************************************************************/

//------------------------------------------------------------------------------
UMixedRealityGarbageMatteCaptureComponent::UMixedRealityGarbageMatteCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCaptureEveryFrame = true;
	PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	PostProcessBlendWeight = 0.0f;
	ShowFlags.SetAtmosphericFog(false);
	ShowFlags.SetFog(false);

	ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> DefaultTarget(TEXT("/MixedRealityFramework/T_MRGarbageMatteRenderTarget"));
	TextureTarget = DefaultTarget.Object;

	GarbageMatteActorClass = AMixedRealityGarbageMatteActor::StaticClass();
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (GarbageMatteActor)
	{
		GarbageMatteActor->Destroy();
		GarbageMatteActor = nullptr;
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

//------------------------------------------------------------------------------
const AActor* UMixedRealityGarbageMatteCaptureComponent::GetViewOwner() const
{
	// This lets SetOnlyOwnerSee on the garbage matte actor make it visible only to this capture component.
	// Basically the "owner" Actor's pointer is being used as an ID for who renders the actor.
	return GarbageMatteActor;
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::SetTrackingOrigin(USceneComponent* InTrackingOrigin)
{
	TrackingOriginPtr = InTrackingOrigin;
	
	if (GarbageMatteActor)
	{
		GarbageMatteActor->AttachToComponent(InTrackingOrigin, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::ApplyCalibrationData_Implementation(const UMixedRealityCalibrationData* ConfigData)
{
	if (ConfigData)
	{
		if (GarbageMatteActor == nullptr)
		{
			SetGarbageMatteActor( SpawnNewGarbageMatteActor(TrackingOriginPtr.Get()) );
		}

		FOVAngle = ConfigData->LensData.FOV;

		if (ensure(GarbageMatteActor))
		{
			GarbageMatteActor->ApplyCalibrationData(ConfigData->GarbageMatteSaveDatas);
		}
	}
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::GetGarbageMatteData(TArray<FGarbageMatteSaveData>& GarbageMatteDataOut)
{
	if (GarbageMatteActor)
	{
		GarbageMatteActor->GetGarbageMatteData(GarbageMatteDataOut);
	}
	else
	{
		GarbageMatteDataOut.Empty();
	}
}

//------------------------------------------------------------------------------
AMixedRealityGarbageMatteActor* UMixedRealityGarbageMatteCaptureComponent::SpawnNewGarbageMatteActor_Implementation(USceneComponent* InTrackingOrigin)
{
	AMixedRealityGarbageMatteActor* NewGarbageMatteActor = nullptr;

	UWorld* MyWorld = GetWorld();
	if (MyWorld && MyWorld->IsGameWorld())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = TEXT("MR_GarbageMatteActor");

		UClass* SpawnClass = GarbageMatteActorClass;
		if (SpawnClass == nullptr)
		{
			SpawnClass = AMixedRealityGarbageMatteActor::StaticClass();
		}

		AActor* SpawnedActor = MyWorld->SpawnActor(GarbageMatteActorClass, /*Location =*/nullptr, /*Rotation =*/nullptr, SpawnParameters);
		if (ensure(SpawnedActor) && InTrackingOrigin) 
		{
			SpawnedActor->AttachToComponent(InTrackingOrigin, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		NewGarbageMatteActor = CastChecked<AMixedRealityGarbageMatteActor>(SpawnedActor, ECastCheckedType::NullAllowed);
	}
	return NewGarbageMatteActor;
}

//------------------------------------------------------------------------------
void UMixedRealityGarbageMatteCaptureComponent::SetGarbageMatteActor(AMixedRealityGarbageMatteActor* NewActor)
{
	TArray<FGarbageMatteSaveData> GarbageMatteData;
	if (GarbageMatteActor)
	{
		GarbageMatteActor->GetGarbageMatteData(GarbageMatteData);

		ShowOnlyActors.Remove(GarbageMatteActor);
		GarbageMatteActor->Destroy();
		GarbageMatteActor = nullptr;
	}

	GarbageMatteActor = NewActor;

	if (NewActor)
	{
		ShowOnlyActors.Add(NewActor);
		NewActor->ApplyCalibrationData(GarbageMatteData);

		if (TrackingOriginPtr.IsValid())
		{
			NewActor->AttachToComponent(TrackingOriginPtr.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}
}

/* AMixedRealityGarbageMatteActor
 *****************************************************************************/

//------------------------------------------------------------------------------
AMixedRealityGarbageMatteActor::AMixedRealityGarbageMatteActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultGarbageMatteMesh;
		ConstructorHelpers::FObjectFinder<UMaterial>   DefaultGarbageMatteMaterial;

		FConstructorStatics()
			: DefaultGarbageMatteMesh(TEXT("/MixedRealityFramework/GarbageMattePlane"))
			, DefaultGarbageMatteMaterial(TEXT("/MixedRealityFramework/GarbageMatteRuntimeMaterial"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

	GarbageMatteMesh = ConstructorStatics.DefaultGarbageMatteMesh.Object;
	GarbageMatteMaterial = ConstructorStatics.DefaultGarbageMatteMaterial.Object;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CapturePoint"));
}

//------------------------------------------------------------------------------
void AMixedRealityGarbageMatteActor::ApplyCalibrationData(const TArray<FGarbageMatteSaveData>& GarbageMatteData)
{
	for (UPrimitiveComponent* OldGarbageMatte : GarbageMattes)
	{
		OldGarbageMatte->DestroyComponent();
	}
	GarbageMattes.Empty(GarbageMatteData.Num());

	for (const FGarbageMatteSaveData& Data : GarbageMatteData)
	{
		AddNewGabageMatte(Data);
	}
}

//------------------------------------------------------------------------------
UPrimitiveComponent* AMixedRealityGarbageMatteActor::AddNewGabageMatte(const FGarbageMatteSaveData& GarbageMatteData)
{
	UPrimitiveComponent* NewMatte = CreateGarbageMatte(GarbageMatteData);
	GarbageMattes.Add(NewMatte);

	return NewMatte;
}

//------------------------------------------------------------------------------
UPrimitiveComponent* AMixedRealityGarbageMatteActor::CreateGarbageMatte_Implementation(const FGarbageMatteSaveData& GarbageMatteData)
{
	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(this, NAME_None);
	MeshComponent->SetStaticMesh(GarbageMatteMesh);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
	MeshComponent->SetRelativeTransform(GarbageMatteData.Transform);
	MeshComponent->SetMaterial(0, GarbageMatteMaterial);
	MeshComponent->SetOnlyOwnerSee(true);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->RegisterComponent();

	return MeshComponent;
}

//------------------------------------------------------------------------------
void AMixedRealityGarbageMatteActor::GetGarbageMatteData(TArray<FGarbageMatteSaveData>& GarbageMatteDataOut)
{
	GarbageMatteDataOut.Empty(GarbageMattes.Num());

	FGarbageMatteSaveData GarbageMatteData;
	for (const UPrimitiveComponent* GarbageMatte : GarbageMattes)
	{
		GarbageMatteData.Transform = GarbageMatte->GetRelativeTransform();
		GarbageMatteDataOut.Add(GarbageMatteData);
	}
}
