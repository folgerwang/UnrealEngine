// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RuntimeVirtualTexturePlaneActorFactory.h"

#include "RuntimeVirtualTexturePlane.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

URuntimeVirtualTexturePlaneActorFactory::URuntimeVirtualTexturePlaneActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("RuntimeVirtualTexturePlaneDisplayName", "Virtual Texture Plane");
	NewActorClass = ARuntimeVirtualTexturePlane::StaticClass();
	bShowInEditorQuickMenu = 1;
}

void URuntimeVirtualTexturePlaneActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	// Good default size to see object in editor
	NewActor->SetActorScale3D(FVector(50.f, 50.f, 1.f));
}

#undef LOCTEXT_NAMESPACE
