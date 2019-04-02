// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusMR_BoundaryActor.h"

#include "OculusMR_BoundaryMeshComponent.h"

#include "OculusMRPrivate.h"
#include "OculusHMD_Settings.h"
#include "OculusHMD.h"

#define LOCTEXT_NAMESPACE "OculusMR_BoundaryActor"

AOculusMR_BoundaryActor::AOculusMR_BoundaryActor(const FObjectInitializer& ObjectInitializer)
{
	BoundaryMeshComponent = CreateDefaultSubobject<UOculusMR_BoundaryMeshComponent>(TEXT("BoundaryMeshComponent"));
	SetRootComponent(BoundaryMeshComponent);
	BoundaryMeshComponent->SetRelativeTransform(FTransform::Identity);
	BoundaryMeshComponent->SetVisibility(true);
}

bool AOculusMR_BoundaryActor::IsBoundaryValid() const
{
	return BoundaryMeshComponent->IsValid();
}

#undef LOCTEXT_NAMESPACE
