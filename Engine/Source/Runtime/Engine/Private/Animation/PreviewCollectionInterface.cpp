// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/PreviewCollectionInterface.h"
#include "Animation/AnimInstance.h"

UPreviewCollectionInterface::UPreviewCollectionInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void IPreviewCollectionInterface::GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList) const
{
	TArray<TSubclassOf<UAnimInstance>> AnimBP;
	GetPreviewSkeletalMeshes(OutList, AnimBP);
}
