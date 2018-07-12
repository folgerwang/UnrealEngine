// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterSceneComponent.h"
#include "DisplayClusterCameraComponent.generated.h"


/**
 * Camera component
 */
UCLASS( ClassGroup=(Custom) )
class DISPLAYCLUSTER_API UDisplayClusterCameraComponent
	: public UDisplayClusterSceneComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterCameraComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void SetSettings(const FDisplayClusterConfigSceneNode* pConfig) override;
	virtual bool ApplySettings() override;

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
