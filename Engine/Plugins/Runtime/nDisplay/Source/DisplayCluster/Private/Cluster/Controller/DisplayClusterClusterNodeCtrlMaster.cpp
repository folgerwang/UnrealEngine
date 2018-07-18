
// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterClusterNodeCtrlMaster.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncService.h"
#include "Network/Service/SwapSync/DisplayClusterSwapSyncService.h"

#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


FDisplayClusterClusterNodeCtrlMaster::FDisplayClusterClusterNodeCtrlMaster(const FString& ctrlName, const FString& nodeName) :
	FDisplayClusterClusterNodeCtrlSlave(ctrlName, nodeName)
{
}

FDisplayClusterClusterNodeCtrlMaster::~FDisplayClusterClusterNodeCtrlMaster()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterNodeController
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterNodeController
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterNodeCtrlMaster::GetSyncData(FDisplayClusterMessage::DataType& data)
{
	// Override slave implementation with empty one.
	// There is no need to sync on master side since it have source data being synced.
}

void FDisplayClusterClusterNodeCtrlMaster::GetInputData(FDisplayClusterMessage::DataType& data)
{
	// Override slave implementation with empty one.
	// There is no need to sync on master side since it have source data being synced.
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlMaster::InitializeServers()
{
	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	if (!FDisplayClusterClusterNodeCtrlSlave::InitializeServers())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing master servers..."), *GetControllerName());

	// Get config data
	FDisplayClusterConfigClusterNode masterCfg;
	if (GDisplayCluster->GetPrivateConfigMgr()->GetMasterClusterNode(masterCfg) == false)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("No master node configuration data found"));
		return false;
	}

	// Instantiate node servers
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Servers: addr %s, port_cs %d, port_ss %d"), *masterCfg.Addr, masterCfg.Port_CS, masterCfg.Port_SS);
	ClusterSyncServer.Reset(new FDisplayClusterClusterSyncService(masterCfg.Addr, masterCfg.Port_CS));
	SwapSyncServer.Reset(new FDisplayClusterSwapSyncService(masterCfg.Addr, masterCfg.Port_SS));

	return ClusterSyncServer.IsValid() && SwapSyncServer.IsValid();
}

bool FDisplayClusterClusterNodeCtrlMaster::StartServers()
{
	if (!FDisplayClusterClusterNodeCtrlSlave::StartServers())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - starting master servers..."), *GetControllerName());

	// CS server start
	if (ClusterSyncServer->Start())
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s started"), *ClusterSyncServer->GetName());
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s failed to start"), *ClusterSyncServer->GetName());
	}

	// SS server start
	if (SwapSyncServer->Start())
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s started"), *SwapSyncServer->GetName());
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s failed to start"), *SwapSyncServer->GetName());
	}

	// Start the servers
	return ClusterSyncServer->IsRunning() && SwapSyncServer->IsRunning();
}

void FDisplayClusterClusterNodeCtrlMaster::StopServers()
{
	FDisplayClusterClusterNodeCtrlSlave::StopServers();

	ClusterSyncServer->Shutdown();
	SwapSyncServer->Shutdown();
}

bool FDisplayClusterClusterNodeCtrlMaster::InitializeClients()
{
	if (!FDisplayClusterClusterNodeCtrlSlave::InitializeClients())
	{
		return false;
	}

	// Master clients initialization
	// ...

	return true;
}

bool FDisplayClusterClusterNodeCtrlMaster::StartClients()
{
	if (!FDisplayClusterClusterNodeCtrlSlave::StartClients())
	{
		return false;
	}

	// Master clients start
	// ...

	return true;
}

void FDisplayClusterClusterNodeCtrlMaster::StopClients()
{
	FDisplayClusterClusterNodeCtrlSlave::StopClients();

	// Master clients stop
	// ...
}

