// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "OVR_Plugin_MixedReality.h"
#include "GameFramework/Actor.h"

#include "OculusMR_BoundaryActor.generated.h"

class UOculusMR_BoundaryMeshComponent;

UCLASS(ClassGroup = OculusMR, NotPlaceable, NotBlueprintable)
class AOculusMR_BoundaryActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UOculusMR_BoundaryMeshComponent* BoundaryMeshComponent;

	bool IsBoundaryValid() const;
};
