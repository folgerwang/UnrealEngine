// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPDisplayClusterClusterManager.h"
#include "Network/DisplayClusterMessage.h"

class ADisplayClusterGameMode;
class ADisplayClusterSettings;


/**
 * Cluster manager. Responsible for network communication and data replication.
 */
class FDisplayClusterClusterManager
	: public    IPDisplayClusterClusterManager
{
public:
	FDisplayClusterClusterManager();
	virtual ~FDisplayClusterClusterManager();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Init(EDisplayClusterOperationMode OperationMode) override;
	virtual void Release() override;
	virtual bool StartSession(const FString& configPath, const FString& nodeId) override;
	virtual void EndSession() override;
	virtual bool StartScene(UWorld* pWorld) override;
	virtual void EndScene() override;
	virtual void PreTick(float DeltaSeconds) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsMaster()     const override;
	virtual bool IsSlave()      const override;
	virtual bool IsStandalone() const override;
	virtual bool IsCluster()    const override;

	virtual FString GetNodeId() const override
	{ return ClusterNodeId; }

	virtual uint32 GetNodesAmount() const override
	{ return NodesAmount; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual IPDisplayClusterNodeController* GetController() const override;

	virtual float GetDeltaTime() const override
	{ return DeltaTime; }

	virtual void  SetDeltaTime(float deltaTime) override
	{ DeltaTime = deltaTime; }

	virtual void RegisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj) override;
	virtual void UnregisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj) override;

	virtual void ExportSyncData(FDisplayClusterMessage::DataType& data) const override;
	virtual void ImportSyncData(const FDisplayClusterMessage::DataType& data) override;

	virtual void SyncObjects() override;
	virtual void SyncInput()   override;

private:
	bool GetResolvedNodeId(FString& id) const;

	typedef TUniquePtr<IPDisplayClusterNodeController> TController;

	// Factory method
	TController CreateController() const;

private:
	// Controller implementation
	TController Controller;
	// Cluster/node props
	uint32 NodesAmount = 0;
	// Current time delta for sync
	float DeltaTime = 0.f;

	// Current operation mode
	EDisplayClusterOperationMode CurrentOperationMode;
	// Current config path
	FString ConfigPath;
	// Current node ID
	FString ClusterNodeId;
	// Current world
	UWorld* CurrentWorld;

	// Sync transforms
	TSet<IDisplayClusterClusterSyncObject*>   ObjectsToSync;
	mutable FDisplayClusterMessage::DataType  SyncObjectsCache;
	mutable FCriticalSection                  ObjectsToSyncCritSec;

	mutable FCriticalSection InternalsSyncScope;
};

