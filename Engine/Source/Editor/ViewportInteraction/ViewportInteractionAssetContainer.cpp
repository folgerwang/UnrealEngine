// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ViewportInteractionAssetContainer.h"
#include "SlateBrush.h"

FViewportInteractionAssetContainer::FViewportInteractionAssetContainer()
{
}

FViewportInteractionAssetContainer::~FViewportInteractionAssetContainer()
{
}

const FName FViewportInteractionAssetContainer::TypeName(TEXT("FViewportInteractionSoundsStyle"));

const FViewportInteractionAssetContainer& FViewportInteractionAssetContainer::GetDefault()
{
	static FViewportInteractionAssetContainer Default;
	return Default;
}

void FViewportInteractionAssetContainer::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
}


UViewportInteractionStyleContainer::UViewportInteractionStyleContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}
