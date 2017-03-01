// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorAssetContainer.h"
#include "SlateBrush.h"

FVREditorAssetContainer::FVREditorAssetContainer()
{
}

FVREditorAssetContainer::~FVREditorAssetContainer()
{
}

const FName FVREditorAssetContainer::TypeName(TEXT("FVREditorAssetContainer"));

const FVREditorAssetContainer& FVREditorAssetContainer::GetDefault()
{
	static FVREditorAssetContainer Default;
	return Default;
}

void FVREditorAssetContainer::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
}


UVREditorStyleContainer::UVREditorStyleContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}
