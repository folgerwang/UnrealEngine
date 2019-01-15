// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlSlave.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Misc/DisplayClusterLog.h"
#include "Network/Service/ClusterEvents/DisplayClusterClusterEventsClient.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncClient.h"
#include "Network/Service/SwapSync/DisplayClusterSwapSyncClient.h"

#include "DisplayClusterGlobals.h"


FDisplayClusterClusterNodeCtrlSlave::FDisplayClusterClusterNodeCtrlSlave(const FString& ctrlName, const FString& nodeName) :
	FDisplayClusterClusterNodeCtrlBase(ctrlName, nodeName)
{
}

FDisplayClusterClusterNodeCtrlSlave::~FDisplayClusterClusterNodeCtrlSlave()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterNodeController
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterNodeCtrlSlave::WaitForGameStart()
{
	ClusterSyncClient->WaitForGameStart();
}

void FDisplayClusterClusterNodeCtrlSlave::WaitForFrameStart()
{
	ClusterSyncClient->WaitForFrameStart();
}

void FDisplayClusterClusterNodeCtrlSlave::WaitForFrameEnd()
{
	ClusterSyncClient->WaitForFrameEnd();
}

void FDisplayClusterClusterNodeCtrlSlave::WaitForTickEnd()
{
	ClusterSyncClient->WaitForTickEnd();
}

void FDisplayClusterClusterNodeCtrlSlave::GetDeltaTime(float& deltaTime)
{
	ClusterSyncClient->GetDeltaTime(deltaTime);
}

void FDisplayClusterClusterNodeCtrlSlave::GetTimecode(FTimecode& timecode, FFrameRate& frameRate)
{
	ClusterSyncClient->GetTimecode(timecode, frameRate);
}

void FDisplayClusterClusterNodeCtrlSlave::GetSyncData(FDisplayClusterMessage::DataType& data)
{
	ClusterSyncClient->GetSyncData(data);
}

void FDisplayClusterClusterNodeCtrlSlave::GetInputData(FDisplayClusterMessage::DataType& data)
{
	ClusterSyncClient->GetInputData(data);
}

void FDisplayClusterClusterNodeCtrlSlave::GetEventsData(FDisplayClusterMessage::DataType& data)
{
	ClusterSyncClient->GetEventsData(data);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterSwapSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterNodeCtrlSlave::WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime)
{
	check(SwapSyncClient.IsValid());
	SwapSyncClient->WaitForSwapSync(pThreadWaitTime, pBarrierWaitTime);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterEventsProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterNodeCtrlSlave::EmitClusterEvent(const FDisplayClusterClusterEvent& Event)
{
	ClusterEventsClient->EmitClusterEvent(Event);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlSlave::InitializeServers()
{
	if (!FDisplayClusterClusterNodeCtrlBase::InitializeServers())
	{
		return false;
	}

	// Slave servers initialization
	// ...

	return true;
}

bool FDisplayClusterClusterNodeCtrlSlave::StartServers()
{
	if (!FDisplayClusterClusterNodeCtrlBase::StartServers())
	{
		return false;
	}

	// Slave servers start
	// ...

	return true;
}

void FDisplayClusterClusterNodeCtrlSlave::StopServers()
{
	FDisplayClusterClusterNodeCtrlBase::StopServers();

	// Slave servers stop
	// ...
}

bool FDisplayClusterClusterNodeCtrlSlave::InitializeClients()
{
	if (!FDisplayClusterClusterNodeCtrlBase::InitializeClients())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing slave clients..."), *GetControllerName());

	// Instantiate local clients
	ClusterSyncClient.Reset(new FDisplayClusterClusterSyncClient);
	SwapSyncClient.Reset(new FDisplayClusterSwapSyncClient);
	ClusterEventsClient.Reset(new FDisplayClusterClusterEventsClient);

	return ClusterSyncClient.IsValid() && SwapSyncClient.IsValid() && ClusterEventsClient.IsValid();
}


#define START_CLIENT(CLN, ADDR, PORT, TRY_AMOUNT, TRY_DELAY, RESULT) \
	if (CLN->Connect(ADDR, PORT, TRY_AMOUNT, TRY_DELAY)) \
	{ \
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s connected to the server %s:%d"), *CLN->GetName(), *ADDR, PORT); \
	} \
	else \
	{ \
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s couldn't connect to the server %s:%d"), *CLN->GetName(), *ADDR, PORT); \
		RESULT = false; \
	}

bool FDisplayClusterClusterNodeCtrlSlave::StartClients()
{
	if (!FDisplayClusterClusterNodeCtrlBase::StartClients())
	{
		return false;
	}

	UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s - initializing slave clients..."), *GetControllerName());

	// Master config
	FDisplayClusterConfigClusterNode MasterCfg;
	if (GDisplayCluster->GetPrivateConfigMgr()->GetMasterClusterNode(MasterCfg) == false)
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("No master node configuration data found"));
		return false;
	}

	const FDisplayClusterConfigNetwork CfgNetwork = GDisplayCluster->GetPrivateConfigMgr()->GetConfigNetwork();

	bool Result = true;
	START_CLIENT(ClusterSyncClient, MasterCfg.Addr, MasterCfg.Port_CS, CfgNetwork.ClientConnectTriesAmount, CfgNetwork.ClientConnectRetryDelay, Result);
	START_CLIENT(SwapSyncClient, MasterCfg.Addr, MasterCfg.Port_SS, CfgNetwork.ClientConnectTriesAmount, CfgNetwork.ClientConnectRetryDelay, Result);
	START_CLIENT(ClusterEventsClient, MasterCfg.Addr, MasterCfg.Port_CE, CfgNetwork.ClientConnectTriesAmount, CfgNetwork.ClientConnectRetryDelay, Result);

	return Result;
}

#undef START_CLIENT


void FDisplayClusterClusterNodeCtrlSlave::StopClients()
{
	FDisplayClusterClusterNodeCtrlBase::StopClients();

	ClusterEventsClient->Disconnect();
	ClusterSyncClient->Disconnect();
	SwapSyncClient->Disconnect();
}
