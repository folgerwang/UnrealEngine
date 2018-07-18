// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Cluster/IDisplayClusterClusterSyncObject.h"
#include "DisplayClusterSceneComponentSync.generated.h"

struct IPDisplayClusterGameManager;
struct IPDisplayClusterClusterManager;


/**
 * Abstract synchronization component
 */
UCLASS(Abstract)
class DISPLAYCLUSTER_API UDisplayClusterSceneComponentSync
	: public USceneComponent
	, public IDisplayClusterClusterSyncObject
{
	GENERATED_BODY()

public:
	UDisplayClusterSceneComponentSync(const FObjectInitializer& ObjectInitializer);
	
	virtual ~UDisplayClusterSceneComponentSync()
	{ }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterSyncObject
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString GetSyncId() const override;
	
	virtual bool IsDirty() const override
	{ return true; }

	virtual void ClearDirty() override
	{ }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterStringSerializable
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString SerializeToString() const override final;
	virtual bool    DeserializeFromString(const FString& data) override final;

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void DestroyComponent(bool bPromoteChildren) override;

protected:
	virtual FTransform GetSyncTransform() const
	{ return FTransform(); }

	virtual void SetSyncTransform(const FTransform& t)
	{ }

protected:
	IPDisplayClusterGameManager*    GameMgr = nullptr;
	IPDisplayClusterClusterManager* ClusterMgr = nullptr;

protected:
	// Caching state
	FVector  LastSyncLoc;
	FRotator LastSyncRot;
	FVector  LastSyncScale;

private:
	FString SyncId;
};
