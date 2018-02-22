// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshUtilitiesLibrary.h"
#include "LODUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalMeshUtilitiesLibrary, Verbose, All);

bool USkeletalMeshUtilitiesLibrary::RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount /*= 0*/, bool bRegenerateEvenIfImported /*= false*/)
{
	if(SkeletalMesh == nullptr)
	{
		UE_LOG(LogSkeletalMeshUtilitiesLibrary, Warning, TEXT("NULL skeletal mesh passed to RegenerateLOD"));
		return false;
	}

	return FLODUtilities::RegenerateLOD(SkeletalMesh, NewLODCount, bRegenerateEvenIfImported);
}

