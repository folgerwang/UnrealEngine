// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTexturePlane.h"

#include "Components/BoxComponent.h"
#include "RuntimeVirtualTextureProducer.h"
#include "VT/RuntimeVirtualTexture.h"


ARuntimeVirtualTexturePlane::ARuntimeVirtualTexturePlane(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = VirtualTextureComponent = CreateDefaultSubobject<URuntimeVirtualTextureComponent>(TEXT("VirtualTextureComponent"));

#if WITH_EDITORONLY_DATA
	// Add box for visualization of bounds
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(0.5f, 0.5f, 1.f), false);
	Box->SetIsVisualizationComponent(true);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	Box->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	Box->SetGenerateOverlapEvents(false);
	Box->SetupAttachment(VirtualTextureComponent);
#endif
}

#if WITH_EDITOR

void ARuntimeVirtualTexturePlane::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		if (VirtualTextureComponent != nullptr)
		{
			VirtualTextureComponent->UpdateVirtualTexture();
		}
	}
	Super::PostEditMove(bFinished);
}

#endif


URuntimeVirtualTextureComponent::URuntimeVirtualTextureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URuntimeVirtualTextureComponent::OnRegister()
{
	Super::OnRegister();
	UpdateVirtualTexture();
}

void URuntimeVirtualTextureComponent::PostLoad()
{
	Super::PostLoad();
	UpdateVirtualTexture();
}

void URuntimeVirtualTextureComponent::BeginDestroy()
{
	ReleaseVirtualTexture();
	Super::BeginDestroy();
}

void URuntimeVirtualTextureComponent::UpdateVirtualTexture()
{
	if (VirtualTexture != nullptr)
	{
		// The Producer object created here will be passed into the Virtual Texture system which will take ownership
		FVTProducerDescription Desc;
		VirtualTexture->GetProducerDescription(Desc);

		const ERuntimeVirtualTextureMaterialType MaterialType = VirtualTexture->GetMaterialType();

		// Transform is based on bottom left of the box
		FTransform Transform = FTransform(FVector(-0.5f, -0.5f, 0.f)) * GetComponentToWorld();

		FRuntimeVirtualTextureProducer* Producer = new FRuntimeVirtualTextureProducer(Desc, MaterialType, GetScene(), Transform);
		VirtualTexture->Initialize(Producer, Transform);

#if WITH_EDITOR
		// Bind function to ensure we call ReInit again if the virtual texture properties are modified
		static const FName BinderFunction(TEXT("OnVirtualTextureEditProperty"));
		VirtualTexture->OnEditProperty.BindUFunction(this, BinderFunction);
#endif
	}
}

void URuntimeVirtualTextureComponent::ReleaseVirtualTexture()
{
	if (VirtualTexture != nullptr)
	{
		VirtualTexture->Release();

#if WITH_EDITOR
		VirtualTexture->OnEditProperty.Unbind();
#endif
	}
}

#if WITH_EDITOR

void URuntimeVirtualTextureComponent::OnVirtualTextureEditProperty(URuntimeVirtualTexture const* InVirtualTexture)
{
	if (InVirtualTexture == VirtualTexture)
	{
		UpdateVirtualTexture();
	}
}

void URuntimeVirtualTextureComponent::SetRotation()
{
	if (BoundsSourceActor != nullptr)
	{
		// Copy the source actor rotation and notify the parent actor
		SetWorldRotation(BoundsSourceActor->GetTransform().GetRotation());
		GetOwner()->PostEditMove(true);
	}
}

void URuntimeVirtualTextureComponent::SetTransformToBounds()
{
	if (BoundsSourceActor != nullptr)
	{
		// Calculate the bounds in our local rotation space translated to the BoundsSourceActor center
		const FQuat TargetRotation = GetComponentToWorld().GetRotation();
		const FVector InitialPosition = BoundsSourceActor->GetComponentsBoundingBox().GetCenter();
		const FVector InitialScale = FVector(0.5f, 0.5, 1.f);

		FTransform LocalTransform;
		LocalTransform.SetComponents(TargetRotation, InitialPosition, InitialScale);
		FTransform WorldToLocal = LocalTransform.Inverse();

		FBox BoundBox(ForceInit);
		for (const UActorComponent* Component : BoundsSourceActor->GetComponents())
		{
			// Only gather visual components in the bounds calculation
			const UPrimitiveComponent* PrimitiveComponent = Cast<const UPrimitiveComponent>(Component);
			if (PrimitiveComponent != nullptr && PrimitiveComponent->IsRegistered())
			{
				const FTransform ComponentToActor = PrimitiveComponent->GetComponentTransform() * WorldToLocal;
				FBoxSphereBounds LocalSpaceComponentBounds = PrimitiveComponent->CalcBounds(ComponentToActor);
				BoundBox += LocalSpaceComponentBounds.GetBox();
			}
		}

		// Create transform from bounds
		FVector Origin;
		FVector Extent;
		BoundBox.GetCenterAndExtents(Origin, Extent);

		Origin = LocalTransform.TransformPosition(Origin);

		FTransform Transform;
		Transform.SetComponents(TargetRotation, Origin, Extent);

		// Apply final result and notify the parent actor
		SetWorldTransform(Transform);
		GetOwner()->PostEditMove(true);
	}
}

#endif
