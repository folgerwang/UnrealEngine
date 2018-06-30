// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSwapSyncService.h"
#include "DisplayClusterSwapSyncMsg.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


FDisplayClusterSwapSyncService::FDisplayClusterSwapSyncService(const FString& addr, const int32 port) :
	FDisplayClusterService(FString("SRV_SS"), addr, port),
	BarrierSwap(GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("SwapSync_barrier"), DisplayClusterConstants::net::BarrierWaitTimeout)
{
}

FDisplayClusterSwapSyncService::~FDisplayClusterSwapSyncService()
{
	Shutdown();
}


bool FDisplayClusterSwapSyncService::Start()
{
	BarrierSwap.Activate();

	return FDisplayClusterServer::Start();
}

void FDisplayClusterSwapSyncService::Shutdown()
{
	BarrierSwap.Deactivate();

	return FDisplayClusterServer::Shutdown();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterSwapSyncService::NotifySessionOpen(FDisplayClusterSession* pSession)
{
	FDisplayClusterService::NotifySessionOpen(pSession);
}

void FDisplayClusterSwapSyncService::NotifySessionClose(FDisplayClusterSession* pSession)
{
	// Unblock waiting threads to allow current Tick() finish
	BarrierSwap.Deactivate();

	FDisplayClusterService::NotifySessionClose(pSession);
}

FDisplayClusterMessage::Ptr FDisplayClusterSwapSyncService::ProcessMessage(FDisplayClusterMessage::Ptr msg)
{
	// Check the pointer
	if (msg.IsValid() == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Couldn't process the message"), *GetName());
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - Processing message %s"), *GetName(), *msg->ToString());

	// Check protocol and type
	if (msg->GetProtocol() != FDisplayClusterSwapSyncMsg::ProtocolName || msg->GetType() != FDisplayClusterSwapSyncMsg::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Unsupported message type: %s"), *GetName(), *msg->ToString());
		return nullptr;
	}

	// Initialize response message
	FDisplayClusterMessage::Ptr response = FDisplayClusterMessage::Ptr(new FDisplayClusterMessage(msg->GetName(), FDisplayClusterSwapSyncMsg::TypeResponse, msg->GetProtocol()));

	// Dispatch the message
	if (msg->GetName() == FDisplayClusterSwapSyncMsg::WaitForSwapSync::name)
	{
		double tTime = 0.f;
		double bTime = 0.f;

		WaitForSwapSync(&tTime, &bTime);

		response->SetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argThreadTime),  tTime);
		response->SetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argBarrierTime), bTime);

		return response;
	}

	// Being here means that we have no appropriate dispatch logic for this message
	UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("%s - A dispatcher for this message hasn't been implemented yet <%s>"), *GetName(), *msg->ToString());
	return nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterSwapSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterSwapSyncService::WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime)
{
	if (BarrierSwap.Wait(pThreadWaitTime, pBarrierWaitTime) != FDisplayClusterBarrier::WaitResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Error on swap barrier. Exit required."));
	}
}
