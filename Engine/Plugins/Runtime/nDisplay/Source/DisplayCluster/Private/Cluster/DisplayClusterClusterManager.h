// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/DisplayClusterClusterEvent.h"
#include "Network/DisplayClusterMessage.h"
#include "Misc/App.h"

class ADisplayClusterGameMode;
class ADisplayClusterSettings;
class FJsonObject;


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

	virtual void AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener>) override;
	virtual void RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener>) override;

	virtual void AddClusterEventListener(const FOnClusterEventListener& Listener) override;
	virtual void RemoveClusterEventListener(const FOnClusterEventListener& Listener) override;

	virtual void EmitClusterEvent(const FDisplayClusterClusterEvent& Event, bool MasterOnly) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterClusterManager
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual IPDisplayClusterNodeController* GetController() const override;

	virtual float GetDeltaTime() const override
	{ return DeltaTime; }

	virtual void  SetDeltaTime(float deltaTime) override
	{ DeltaTime = deltaTime; }

	virtual void GetTimecode(FTimecode& timecode, FFrameRate& frameRate) const override
	{ timecode = FApp::GetTimecode(); frameRate = FApp::GetTimecodeFrameRate(); }

	virtual void SetTimecode(const FTimecode& timecode, const FFrameRate& frameRate) override
	{ FApp::SetTimecodeAndFrameRate(timecode, frameRate); }
	
	virtual void RegisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj) override;
	virtual void UnregisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj) override;

	virtual void ExportSyncData(FDisplayClusterMessage::DataType& data) const override;
	virtual void ImportSyncData(const FDisplayClusterMessage::DataType& data) override;

	virtual void ExportEventsData(FDisplayClusterMessage::DataType& data) const override;
	virtual void ImportEventsData(const FDisplayClusterMessage::DataType& data) override;

	virtual void SyncObjects() override;
	virtual void SyncInput()   override;
	virtual void SyncEvents()  override;

private:
	bool GetResolvedNodeId(FString& id) const;

	typedef TUniquePtr<IPDisplayClusterNodeController> TController;

	// Factory method
	TController CreateController() const;

	void OnClusterEventHandler(const FDisplayClusterClusterEvent& Event);

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
	TSet<IDisplayClusterClusterSyncObject*>      ObjectsToSync;
	mutable FDisplayClusterMessage::DataType     SyncObjectsCache;
	mutable FCriticalSection                     ObjectsToSyncCritSec;

	// Sync events - types
	typedef TMap<FString, FDisplayClusterClusterEvent> FNamedEventMap;
	typedef TMap<FString, FNamedEventMap>              FTypedEventMap;
	typedef TMap<FString, FTypedEventMap>              FCategoricalMap;
	typedef FCategoricalMap                            FClusterEventsContainer;
	// Sync events - data
	FClusterEventsContainer                      ClusterEventsPoolMain;
	mutable FClusterEventsContainer              ClusterEventsPoolOut;
	mutable FDisplayClusterMessage::DataType     ClusterEventsCacheOut;
	mutable FCriticalSection                     ClusterEventsCritSec;
	FOnClusterEvent                              OnClusterEvent;
	TArray<TScriptInterface<IDisplayClusterClusterEventListener>> ClusterEventListeners;

	mutable FCriticalSection InternalsSyncScope;
};
