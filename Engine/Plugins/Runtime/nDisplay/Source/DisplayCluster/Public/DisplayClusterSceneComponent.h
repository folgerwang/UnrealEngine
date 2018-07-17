// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Config/DisplayClusterConfigTypes.h"

#include "DisplayClusterSceneComponent.generated.h"


class UDisplayClusterSceneComponentSync;


/**
 * Extended scene component
 */
UCLASS( ClassGroup=(Custom) )
class DISPLAYCLUSTER_API UDisplayClusterSceneComponent
	: public USceneComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterSceneComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void SetSettings(const FDisplayClusterConfigSceneNode* pConfig);
	virtual bool ApplySettings();

	inline FString GetId() const
	{ return Config.Id; }

	inline FString GetParentId() const
	{ return Config.ParentId; }

public:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FDisplayClusterConfigSceneNode Config;
};
