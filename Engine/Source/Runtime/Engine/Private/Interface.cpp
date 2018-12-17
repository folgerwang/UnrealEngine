// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UObject/Interface.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"

UInterface_CollisionDataProvider::UInterface_CollisionDataProvider(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UInterface_PostProcessVolume::UInterface_PostProcessVolume( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

}


UInterface_PreviewMeshProvider::UInterface_PreviewMeshProvider( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

}
