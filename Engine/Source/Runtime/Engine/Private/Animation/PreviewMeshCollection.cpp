// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/PreviewMeshCollection.h"

void UPreviewMeshCollection::GetPreviewSkeletalMeshes(TArray<USkeletalMesh*>& OutList, TArray<TSubclassOf<UAnimInstance>>& OutAnimBP) const
{
	OutList.Empty();
	OutAnimBP.Empty();
	
	for (int32 MeshIndex = 0; MeshIndex < SkeletalMeshes.Num(); ++MeshIndex)
	{
		const FPreviewMeshCollectionEntry& Entry = SkeletalMeshes[MeshIndex];

		// Load up our valid skeletal meshes
		if (Entry.SkeletalMesh.LoadSynchronous())
		{
			OutList.Add(Entry.SkeletalMesh.Get());
		}
	}

	// no class, but just send it out
	OutAnimBP.AddZeroed(OutList.Num());
}
