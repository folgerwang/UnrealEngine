// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSceneComponentSync.h"

#include "IPDisplayCluster.h"
#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Game/IPDisplayClusterGameManager.h"
#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterGlobals.h"


UDisplayClusterSceneComponentSync::UDisplayClusterSceneComponentSync(const FObjectInitializer& ObjectInitializer) :
	USceneComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDisplayClusterSceneComponentSync::BeginPlay()
{
	Super::BeginPlay();

	if (!GDisplayCluster->IsModuleInitialized())
	{
		return;
	}

	// Generate unique sync id
	SyncId = GetSyncId();

	GameMgr = GDisplayCluster->GetPrivateGameMgr();
	if (GameMgr && GameMgr->IsDisplayClusterActive())
	{
		// Register sync object
		ClusterMgr = GDisplayCluster->GetPrivateClusterMgr();
		if (ClusterMgr)
		{
			UE_LOG(LogDisplayClusterGame, Log, TEXT("Registering sync object %s..."), *SyncId);
			ClusterMgr->RegisterSyncObject(this);
		}
		else
		{
			UE_LOG(LogDisplayClusterGame, Warning, TEXT("Couldn't register %s scene component sync. Looks like we're in non-DisplayCluster mode."), *SyncId);
		}
	}
}


void UDisplayClusterSceneComponentSync::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// ...
}

void UDisplayClusterSceneComponentSync::DestroyComponent(bool bPromoteChildren)
{
	if (GDisplayCluster->IsModuleInitialized())
	{
		if (GameMgr && GameMgr->IsDisplayClusterActive())
		{
			if (ClusterMgr)
			{
				UE_LOG(LogDisplayClusterGame, Log, TEXT("Unregistering sync object %s..."), *SyncId);
				ClusterMgr->UnregisterSyncObject(this);
			}
		}
	}

	Super::DestroyComponent(bPromoteChildren);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterSyncObject
//////////////////////////////////////////////////////////////////////////////////////////////
FString UDisplayClusterSceneComponentSync::GetSyncId() const
{
	return FString::Printf(TEXT("S_%s"), *GetOwner()->GetName());
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStringSerializable
//////////////////////////////////////////////////////////////////////////////////////////////
FString UDisplayClusterSceneComponentSync::SerializeToString() const
{
	return GetSyncTransform().ToString();
}

bool UDisplayClusterSceneComponentSync::DeserializeFromString(const FString& data)
{
	FTransform t;
	if (!t.InitFromString(data))
	{
		UE_LOG(LogDisplayClusterGame, Error, TEXT("Unable to deserialize transform data"));
		return false;
	}

	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("%s: applying transform data <%s>"), *SyncId, *t.ToHumanReadableString());
	SetSyncTransform(t);

	return true;
}
