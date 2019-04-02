// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlMaster.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncService.h"
#include "Network/Service/SwapSync/DisplayClusterSwapSyncService.h"
#include "Network/Service/ClusterEvents/DisplayClusterClusterEventsService.h"

#include "Misc/App.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterGlobals.h"


FDisplayClusterClusterNodeCtrlMaster::FDisplayClusterClusterNodeCtrlMaster(const FString& ctrlName, const FString& nodeName) :
	FDisplayClusterClusterNodeCtrlSlave(ctrlName, nodeName)
{
}

FDisplayClusterClusterNodeCtrlMaster::~FDisplayClusterClusterNodeCtrlMaster()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterNodeCtrlMaster::GetTimecode(FTimecode& timecode, FFrameRate& frameRate)
{
	// This values are updated in UEngine::UpdateTimeAndHandleMaxTickRate (via UpdateTimecode).
	timecode = FApp::GetTimecode();
	frameRate = FApp::GetTimecodeFrameRate();
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterEventsProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterNodeCtrlMaster::EmitClusterEvent(const FDisplayClusterClusterEvent& Event)
{
	UE_LOG(LogDisplayClusterCluster, Warning, TEXT("This should never be called!"));
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

void FDisplayClusterClusterNodeCtrlMaster::GetEventsData(FDisplayClusterMessage::DataType& data)
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
	UE_LOG(LogDisplayClusterCluster, Log, TEXT("Servers: addr %s, port_cs %d, port_ss %d, port_ce %d"), *masterCfg.Addr, masterCfg.Port_CS, masterCfg.Port_SS, masterCfg.Port_CE);
	ClusterSyncServer.Reset(new FDisplayClusterClusterSyncService(masterCfg.Addr, masterCfg.Port_CS));
	SwapSyncServer.Reset(new FDisplayClusterSwapSyncService(masterCfg.Addr, masterCfg.Port_SS));
	ClusterEventsServer.Reset(new FDisplayClusterClusterEventsService(masterCfg.Addr, masterCfg.Port_CE));

	return ClusterSyncServer.IsValid() && SwapSyncServer.IsValid() && ClusterEventsServer.IsValid();
}



#define SERVER_START(SRV, RESULT) \
	if (SRV->Start()) \
	{ \
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s started"), *SRV->GetName()); \
	} \
	else \
	{ \
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s failed to start"), *SRV->GetName()); \
		RESULT = false; \
	} \

bool FDisplayClusterClusterNodeCtrlMaster::StartServers()
{
	if (!FDisplayClusterClusterNodeCtrlSlave::StartServers())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - starting master servers..."), *GetControllerName());

	bool Result = true;
	SERVER_START(ClusterSyncServer, Result);
	SERVER_START(SwapSyncServer, Result);
	SERVER_START(ClusterEventsServer, Result);

	// Start the servers
	return Result;
}

#undef SERVER_START


void FDisplayClusterClusterNodeCtrlMaster::StopServers()
{
	FDisplayClusterClusterNodeCtrlSlave::StopServers();

	ClusterSyncServer->Shutdown();
	SwapSyncServer->Shutdown();
	ClusterEventsServer->Shutdown();
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

