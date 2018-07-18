// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigStaticMeshControl.h"
#include "Components/StaticMeshComponent.h"
#include "Units/RigUnit_Control_StaticMesh.h"

AControlRigStaticMeshControl::AControlRigStaticMeshControl(const FObjectInitializer& ObjectInitializer)
	: AControlRigControl(ObjectInitializer)
{
	Scene = ObjectInitializer.CreateEditorOnlyDefaultSubobject<USceneComponent>(this, TEXT("Scene"));
	Mesh = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(this, TEXT("Mesh"));

#if WITH_EDITORONLY_DATA
	SetRootComponent(Scene);
	if (Mesh)
	{
		Mesh->SetupAttachment(Scene);
		Mesh->SetMobility(EComponentMobility::Movable);
	}
#endif // WITH_EDITORONLY_DATA
}

void AControlRigStaticMeshControl::SetTransform(const FTransform& InTransform)
{
	Super::SetTransform(InTransform);

	// Default behavior just mimics the passed-in transform with the actor
	SetActorTransform(InTransform);
}

void AControlRigStaticMeshControl::TickControl(float InDeltaSeconds, FRigUnit_Control& InRigUnit, UScriptStruct* InRigUnitStruct)
{
#if WITH_EDITORONLY_DATA
	if(InRigUnitStruct == FRigUnit_Control_StaticMesh::StaticStruct())
	{
		FRigUnit_Control_StaticMesh& MeshControlUnit = *static_cast<FRigUnit_Control_StaticMesh*>(&InRigUnit);

		Mesh->SetStaticMesh(MeshControlUnit.StaticMesh);
		if(MeshControlUnit.Materials.Num() > 0)
		{
			for(int32 MaterialIndex = 0; MaterialIndex < MeshControlUnit.Materials.Num(); ++MaterialIndex)
			{
				UMaterialInterface* Material = MeshControlUnit.Materials[MaterialIndex];
				Mesh->SetMaterial(MaterialIndex, Material);
			}
		}
		else
		{
			Mesh->EmptyOverrideMaterials();
		}

		Mesh->SetRelativeTransform(MeshControlUnit.MeshTransform);
	}
#endif // WITH_EDITORONLY_DATA
}