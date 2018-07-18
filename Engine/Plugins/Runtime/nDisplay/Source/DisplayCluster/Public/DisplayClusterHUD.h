// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/HUD.h"
#include "DisplayClusterHUD.generated.h"


/**
 * Extended HUD
 */
UCLASS()
class DISPLAYCLUSTER_API ADisplayClusterHUD
	: public AHUD
{
	GENERATED_BODY()

public:
	ADisplayClusterHUD(const FObjectInitializer& ObjectInitializer);

public:
	virtual void BeginPlay() override;

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;
};

