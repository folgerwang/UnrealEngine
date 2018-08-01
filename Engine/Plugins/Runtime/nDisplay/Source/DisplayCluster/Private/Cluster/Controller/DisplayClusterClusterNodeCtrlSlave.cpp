// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterClusterNodeCtrlSlave.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Misc/DisplayClusterLog.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncClient.h"
#include "Network/Service/SwapSync/DisplayClusterSwapSyncClient.h"

#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


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

void FDisplayClusterClusterNodeCtrlSlave::GetSyncData(FDisplayClusterMessage::DataType& data)
{
	ClusterSyncClient->GetSyncData(data);
}

void FDisplayClusterClusterNodeCtrlSlave::GetInputData(FDisplayClusterMessage::DataType& data)
{
	ClusterSyncClient->GetInputData(data);
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

	return ClusterSyncClient.IsValid() && SwapSyncClient.IsValid();
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

	// CS client
	if (ClusterSyncClient->Connect(MasterCfg.Addr, MasterCfg.Port_CS))
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s connected to the server %s:%d"), *ClusterSyncClient->GetName(), *MasterCfg.Addr, MasterCfg.Port_CS);
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s couldn't connect to the server %s:%d"), *ClusterSyncClient->GetName(), *MasterCfg.Addr, MasterCfg.Port_CS);
		// No need to wait again for next client connection
		return false;
	}

	// SS client
	if (SwapSyncClient->Connect(MasterCfg.Addr, MasterCfg.Port_SS))
	{
		UE_LOG(LogDisplayClusterCluster, Log, TEXT("%s connected to the server %s:%d"), *SwapSyncClient->GetName(), *MasterCfg.Addr, MasterCfg.Port_SS);
	}
	else
	{
		UE_LOG(LogDisplayClusterCluster, Error, TEXT("%s couldn't connect to the server %s:%d"), *SwapSyncClient->GetName(), *MasterCfg.Addr, MasterCfg.Port_SS);
		return false;
	}

	return ClusterSyncClient->IsConnected() && SwapSyncClient->IsConnected();
}

void FDisplayClusterClusterNodeCtrlSlave::StopClients()
{
	FDisplayClusterClusterNodeCtrlBase::StopClients();

	ClusterSyncClient->Disconnect();
	SwapSyncClient->Disconnect();
}
