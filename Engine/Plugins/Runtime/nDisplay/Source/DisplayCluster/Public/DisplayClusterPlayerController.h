// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "DisplayClusterPlayerController.generated.h"

/**
 * Extended player controller
 */
UCLASS()
class DISPLAYCLUSTER_API ADisplayClusterPlayerController
	: public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
};

