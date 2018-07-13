// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterSceneComponentSync.h"
#include "DisplayClusterSceneComponentSyncParent.generated.h"


/**
 * Synchronization component. Synchronizes parent scene component.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DISPLAYCLUSTER_API UDisplayClusterSceneComponentSyncParent
	: public UDisplayClusterSceneComponentSync
{
	GENERATED_BODY()

public:
	UDisplayClusterSceneComponentSyncParent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void BeginPlay() override;
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;
	virtual void DestroyComponent(bool bPromoteChildren) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterSyncObject
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString GetSyncId() const override;
	virtual bool IsDirty() const override;
	virtual void ClearDirty() override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// UDisplayClusterSceneComponentSync
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FTransform GetSyncTransform() const override;
	virtual void SetSyncTransform(const FTransform& t) override;
};
