// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Cluster/DisplayClusterClusterManager.h"

#include "Cluster/IDisplayClusterClusterSyncObject.h"
#include "Cluster/IDisplayClusterClusterEventListener.h"
#include "Cluster/Controller/DisplayClusterNodeCtrlStandalone.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlMaster.h"
#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSlave.h"

#include "Config/IPDisplayClusterConfigManager.h"

#include "Dom/JsonObject.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterTypesConverter.h"

#include "Input/IPDisplayClusterInputManager.h"

#include "UObject/Interface.h"

#include "DisplayClusterBuildConfig.h"
#include "DisplayClusterGlobals.h"

#include "SocketSubsystem.h"


FDisplayClusterClusterManager::FDisplayClusterClusterManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	ObjectsToSync.Reserve(64);

	// Set main event handler. This is an entry point for any incoming cluster events.
	OnClusterEvent.AddRaw(this, &FDisplayClusterClusterManager::OnClusterEventHandler);
}

FDisplayClusterClusterManager::~FDisplayClusterClusterManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterManager::Init(EDisplayClusterOperationMode OperationMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	CurrentOperationMode = OperationMode;
	
	return true;
}

void FDisplayClusterClusterManager::Release()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);
}

bool FDisplayClusterClusterManager::StartSession(const FString& configPath, const FString& nodeId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	ConfigPath = configPath;
	ClusterNodeId = nodeId;

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
#ifdef DISPLAY_CLUSTER_USE_AUTOMATIC_NODE_ID_RESOLVE
		if (ClusterNodeId.IsEmpty())
		{
			UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Node name was not specified. Trying to resolve address from available interfaces..."));

			// Try to find the node ID by address (this won't work if you want to run several cluster nodes on the same address)
			FString resolvedNodeId;
			if (GetResolvedNodeId(resolvedNodeId))
			{
				DisplayClusterHelpers::str::DustCommandLineValue(resolvedNodeId);
				ClusterNodeId = resolvedNodeId;
			}
			else
			{
				UE_LOG(LogDisplayClusterCluster, Error, TEXT("Unable to resolve node ID by local addresses"));
				return false;
			}
		}
#endif
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Standalone)
	{
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		if (ConfigPath.IsEmpty() || ClusterNodeId.IsEmpty())
		{
			UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Wrong config path and/or node ID. Using default standalone config."));

#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
			ConfigPath = FString(DisplayClusterStrings::misc::DbgStubConfig);
			ClusterNodeId     = FString(DisplayClusterStrings::misc::DbgStubNodeId);
#endif
		}
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Disabled)
	{
		return true;
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Unknown operation mode"));
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Node ID: %s"), *ClusterNodeId);

	// Node name must be specified in cluster mode
	if (ClusterNodeId.IsEmpty())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Node name was not specified"));
		return false;
	}

	// Save nodes amount
	NodesAmount = GDisplayCluster->GetPrivateConfigMgr()->GetClusterNodesAmount();

	// Instantiate node controller
	Controller = CreateController();

	if (!Controller)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't create a controller."));
		return false;
	}

	// Initialize the controller
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Initializing the controller..."));
	if (!Controller->Initialize())
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't initialize a controller."));
		Controller.Reset();
		return false;
	}

	return true;
}

void FDisplayClusterClusterManager::EndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&InternalsSyncScope);
		if (Controller)
		{
			Controller->Release();
			Controller.Reset();
		}
	}
}

bool FDisplayClusterClusterManager::StartScene(UWorld* pWorld)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	check(pWorld);
	CurrentWorld = pWorld;

	return true;
}

void FDisplayClusterClusterManager::EndScene()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ObjectsToSyncCritSec);
		ObjectsToSync.Reset();
	}
}

void FDisplayClusterClusterManager::PreTick(float DeltaSeconds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	// Clear cached data from previous game frame
	{
		FScopeLock lock(&ObjectsToSyncCritSec);
		SyncObjectsCache.Empty(SyncObjectsCache.Num() | 0x07);
	}

	// Move cluster events from the primary pool to the output pool. These will be synchronized on the current frame.
	{
		FScopeLock lock(& ClusterEventsCritSec);
		ClusterEventsPoolOut = MoveTemp(ClusterEventsPoolMain);
		ClusterEventsPoolMain.Empty(ClusterEventsPoolOut.Num() | 0x07);
		ClusterEventsCacheOut.Empty(ClusterEventsPoolOut.Num() | 0x07);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
IPDisplayClusterNodeController* FDisplayClusterClusterManager::GetController() const
{
	FScopeLock lock(&InternalsSyncScope);
	return Controller ? Controller.Get() : nullptr;
}

bool FDisplayClusterClusterManager::IsMaster() const
{
	return Controller ? Controller->IsMaster() : false;
}

bool FDisplayClusterClusterManager::IsSlave() const
{
	return Controller ? Controller->IsSlave() : false;
}

bool FDisplayClusterClusterManager::IsStandalone() const
{
	return Controller ? Controller->IsStandalone() : false;
}

bool FDisplayClusterClusterManager::IsCluster() const
{
	return Controller ? Controller->IsCluster() : false;
}

void FDisplayClusterClusterManager::AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);
		ClusterEventListeners.Add(Listener);
	}
}

void FDisplayClusterClusterManager::RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);
		if (ClusterEventListeners.Contains(Listener))
		{
			ClusterEventListeners.Remove(Listener);
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Cluster event listeners left: %d"), ClusterEventListeners.Num());
		}
	}
}

void FDisplayClusterClusterManager::AddClusterEventListener(const FOnClusterEventListener& Listener)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);
		OnClusterEvent.Add(Listener);
	}
}

void FDisplayClusterClusterManager::RemoveClusterEventListener(const FOnClusterEventListener& Listener)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);
		OnClusterEvent.Remove(Listener.GetHandle());
	}
}

void FDisplayClusterClusterManager::EmitClusterEvent(const FDisplayClusterClusterEvent& Event, bool MasterOnly)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);

		// [Master] Since we receive cluster events asynchronously, we push it to a primary events pool
		if (IsMaster())
		{
			// New category event
			if (!ClusterEventsPoolMain.Contains(Event.Category))
			{
				ClusterEventsPoolMain.Add(Event.Category);
				ClusterEventsPoolMain[Event.Category].Add(Event.Type);
				ClusterEventsPoolMain[Event.Category][Event.Type].Add(Event.Name, Event);
			}
			// New type event
			else if (!ClusterEventsPoolMain[Event.Category].Contains(Event.Type))
			{
				ClusterEventsPoolMain[Event.Category].Add(Event.Type);
				ClusterEventsPoolMain[Event.Category][Event.Type].Add(Event.Name, Event);
			}
			else
			{
				ClusterEventsPoolMain[Event.Category][Event.Type].Add(Event.Name, Event);
			}
		}
		// [Slave] Send event to the master
		else
		{
			// An event will be emitted from a slave node if it's explicitly specified by MasterOnly=false
			if (!MasterOnly)
			{
				Controller->EmitClusterEvent(Event);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterManager::RegisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ObjectsToSyncCritSec);
		ObjectsToSync.Add(pSyncObj);
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Registered sync object: %s"), *pSyncObj->GetSyncId());
}

void FDisplayClusterClusterManager::UnregisterSyncObject(IDisplayClusterClusterSyncObject* pSyncObj)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ObjectsToSyncCritSec);
		ObjectsToSync.Remove(pSyncObj);
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Unregistered sync object: %s"), *pSyncObj->GetSyncId());
}

void FDisplayClusterClusterManager::ExportSyncData(FDisplayClusterMessage::DataType& data) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ObjectsToSyncCritSec);

		// Cache the data for current frame.
		// There is no check for ObjectsToSync emptiness because we always have at least one
		// shared transform which is AFDisplayClusterPawn.
		if (SyncObjectsCache.Num() == 0)
		{
			for (auto obj : ObjectsToSync)
			{
				if (obj->IsDirty())
				{
					UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Adding object to sync: %s"), *obj->GetSyncId());
					SyncObjectsCache.Add(obj->GetSyncId(), obj->SerializeToString());
					obj->ClearDirty();
				}
			}
		}
	}

	data = SyncObjectsCache;
}

void FDisplayClusterClusterManager::ImportSyncData(const FDisplayClusterMessage::DataType& data)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	if (data.Num() > 0)
	{
		for (auto it = data.CreateConstIterator(); it; ++it)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("sync-data: %s=%s"), *it->Key, *it->Value);
		}

		for (auto obj : ObjectsToSync)
		{
			const FString syncId = obj->GetSyncId();
			if (!data.Contains(syncId))
			{
				UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("%s has nothing to update"), *syncId);
				continue;
			}

			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Found %s in sync data. Applying..."), *syncId);
			if (!obj->DeserializeFromString(data[syncId]))
			{
				UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't apply sync data for sync object %s"), *syncId);
			}
		}
	}
}

void FDisplayClusterClusterManager::ExportEventsData(FDisplayClusterMessage::DataType& data) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);

		// Cache the events data for current frame.
		if (ClusterEventsPoolOut.Num() != 0)
		{
			int ObjID = 0;
			for (const auto& CategorytMap : ClusterEventsPoolOut)
			{
				for (const auto& TypeMap : CategorytMap.Value)
				{
					for (const auto& NamedEvent : TypeMap.Value)
					{
						UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Adding event to sync: %s::%s"), *NamedEvent.Value.Name, *NamedEvent.Value.Type);
						ClusterEventsCacheOut.Add(FString::Printf(TEXT("EVENT_%d"), ObjID++), NamedEvent.Value.SerializeToString());
					}
				}
			}

			// Clear the output pool since we have all data cached already
			ClusterEventsPoolOut.Empty(ClusterEventsPoolOut.Num() | 0x07);
		}
	}

	data = ClusterEventsCacheOut;
}

void FDisplayClusterClusterManager::ImportEventsData(const FDisplayClusterMessage::DataType& data)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	if (data.Num() > 0)
	{
		FScopeLock lock(&ClusterEventsCritSec);

		for (const auto& it : data)
		{
			UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("evt-data: %s=%s"), *it.Key, *it.Value);

			FDisplayClusterClusterEvent ClusterEvent;
			if (ClusterEvent.DeserializeFromString(it.Value) == false)
			{
				UE_LOG(LogDisplayClusterCluster, Warning, TEXT("Couldn't deserialize cluster event: %s=%s"), *it.Key, *it.Value);
				continue;
			}

			// Fire event
			OnClusterEvent.Broadcast(ClusterEvent);
		}
	}
}

void FDisplayClusterClusterManager::SyncObjects()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	//@note:
	// Don't use FScopeLock lock(&ObjectsToSyncCritSec) here because
	// a) There are no race conditions at this point. We're in single-threaded mode right now (see UDisplayClusterGameEngine::Tick())
	// b) Performance

	// No need to do the sync for master
	if (IsSlave())
	{
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (objects)..."));
		TMap<FString, FString> data;
		Controller->GetSyncData(data);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available %d records (objects)."), data.Num());

		// Perform data load (objects state update)
		ImportSyncData(data);
	}
}

void FDisplayClusterClusterManager::SyncInput()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	// No need to do the sync for master
	if (IsSlave())
	{
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (input)..."));
		TMap<FString, FString> data;
		Controller->GetInputData(data);
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available %d records (input)."), data.Num());

		// Perform data load (objects state update)
		GDisplayCluster->GetPrivateInputMgr()->ImportInputData(data);
	}
}

void FDisplayClusterClusterManager::SyncEvents()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	// No need to do the sync for master
	if (IsSlave())
	{
		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading synchronization data (events)..."));
		TMap<FString, FString> data;

		{
			FScopeLock lock(&ClusterEventsCritSec);
			Controller->GetEventsData(data);
		}

		UE_LOG(LogDisplayClusterCluster, Verbose, TEXT("Downloading finished. Available %d records (events)."), data.Num());

		// Perform data load (objects state update)
		GDisplayCluster->GetPrivateClusterMgr()->ImportEventsData(data);
	}
	else
	{
		TMap<FString, FString> EventsData;
		ExportEventsData(EventsData);
		ImportEventsData(EventsData);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterClusterManager::TController FDisplayClusterClusterManager::CreateController() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Current operation mode: %s"), *FDisplayClusterTypesConverter::ToString(CurrentOperationMode));

	// Instantiate appropriate controller depending on operation mode and cluster role
	FDisplayClusterNodeCtrlBase* pController = nullptr;
	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster)
	{
		FDisplayClusterConfigClusterNode nodeCfg;
		if (GDisplayCluster->GetPrivateConfigMgr()->GetClusterNode(ClusterNodeId, nodeCfg) == false)
		{
			UE_LOG(LogDisplayClusterCluster, Error, TEXT("Configuration data for node %s not found"), *ClusterNodeId);
			return nullptr;
		}

		if (nodeCfg.IsMaster)
		{
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating cluster master controller..."));
			pController = new FDisplayClusterClusterNodeCtrlMaster(FString("[CTRL-M]"), ClusterNodeId);
		}
		else
		{
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating cluster slave controller..."));
			pController = new FDisplayClusterClusterNodeCtrlSlave(FString("[CTRL-S]"), ClusterNodeId);
		}
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Standalone)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating standalone controller"));
		pController = new FDisplayClusterNodeCtrlStandalone(FString("[CTRL-STNDA]"), FString("standalone"));
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Instantiating cluster master controller..."));
		//pController = new FDisplayClusterNodeCtrlStandalone(FString("[CTRL-STNDA]"), ClusterNodeId);
		pController = new FDisplayClusterNodeCtrlStandalone(FString("[CTRL-STNDA]"), FString("standalone"));
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Disabled)
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("Controller is not required"));
		return nullptr;
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Unknown operation mode"));
		return nullptr;
	}

	// Return the controller
	return TController(pController);
}

bool FDisplayClusterClusterManager::GetResolvedNodeId(FString& id) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	TArray<TSharedPtr<FInternetAddr>> addrs;
	if (!ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalAdapterAddresses(addrs))
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't get local addresses list. Cannot find node ID by its address."));
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Cluster manager init error"));
		return false;
	}

	if (addrs.Num() <= 0)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("No local addresses found"));
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Cluster manager init error"));
		return false;
	}

	const TArray<FDisplayClusterConfigClusterNode> cnodes = GDisplayCluster->GetPrivateConfigMgr()->GetClusterNodes();

	// Look for associated node in config
	const FDisplayClusterConfigClusterNode* const pNode = cnodes.FindByPredicate([addrs](const FDisplayClusterConfigClusterNode& node)
	{
		for (auto addr : addrs)
		{
			const FIPv4Endpoint ep(addr);
			const FString epaddr = ep.Address.ToString();
			UE_LOG(LogDisplayClusterCluster, Log, TEXT("Comparing addresses: %s - %s"), *epaddr, *node.Addr);

			//@note: don't add "127.0.0.1" or "localhost" here. There will be a bug. It has been proved already.
			if (epaddr == node.Addr)
			{
				return true;
			}
		}

		return false;
	});

	if (!pNode)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("Couldn't find any local address in config file"));
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Cluster manager init error"));
		return false;
	}

	// Ok, we found the node ID by address (this won't work if you want to run several cluster nodes on the same address)
	id = pNode->Id;
	return true;
}

// This is cluster events root dispatcher. It forwards events to both BP and C++ event handlers.
void FDisplayClusterClusterManager::OnClusterEventHandler(const FDisplayClusterClusterEvent& Event)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterCluster);

	{
		FScopeLock lock(&ClusterEventsCritSec);
		for (auto Listener : ClusterEventListeners)
		{
			Listener->Execute_OnClusterEvent(Listener.GetObject(), Event);
		}
	}
}
