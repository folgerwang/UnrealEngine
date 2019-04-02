// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Service/SwapSync/DisplayClusterSwapSyncService.h"
#include "Network/Service/SwapSync/DisplayClusterSwapSyncMsg.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"

#include "Network/Session/DisplayClusterSessionInternal.h"

#include "DisplayClusterGlobals.h"


FDisplayClusterSwapSyncService::FDisplayClusterSwapSyncService(const FString& InAddr, const int32 InPort) :
	FDisplayClusterService(FString("SRV_SS"), InAddr, InPort),
	BarrierSwap(GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("SwapSync_barrier"), GDisplayCluster->GetConfigMgr()->GetConfigNetwork().BarrierWaitTimeout)
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

FDisplayClusterSessionBase* FDisplayClusterSwapSyncService::CreateSession(FSocket* InSocket, const FIPv4Endpoint& InEP)
{
	return new FDisplayClusterSessionInternal(InSocket, this, GetName() + FString("_session_") + InEP.ToString());
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterSwapSyncService::NotifySessionOpen(FDisplayClusterSessionBase* InSession)
{
	FDisplayClusterService::NotifySessionOpen(InSession);
}

void FDisplayClusterSwapSyncService::NotifySessionClose(FDisplayClusterSessionBase* InSession)
{
	// Unblock waiting threads to allow current Tick() finish
	BarrierSwap.Deactivate();

	FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, GetName() + FString(" - Connection interrupted. Application exit requested."));
	FDisplayClusterService::NotifySessionClose(InSession);
}

TSharedPtr<FDisplayClusterMessage> FDisplayClusterSwapSyncService::ProcessMessage(const TSharedPtr<FDisplayClusterMessage>& Request)
{
	// Check the pointer
	if (Request.IsValid() == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Couldn't process the message"), *GetName());
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - Processing message %s"), *GetName(), *Request->ToString());

	// Check protocol and type
	if (Request->GetProtocol() != FDisplayClusterSwapSyncMsg::ProtocolName || Request->GetType() != FDisplayClusterSwapSyncMsg::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Unsupported message type: %s"), *GetName(), *Request->ToString());
		return nullptr;
	}

	// Initialize response message
	TSharedPtr<FDisplayClusterMessage> Response = MakeShareable(new FDisplayClusterMessage(Request->GetName(), FDisplayClusterSwapSyncMsg::TypeResponse, Request->GetProtocol()));

	// Dispatch the message
	if (Request->GetName() == FDisplayClusterSwapSyncMsg::WaitForSwapSync::name)
	{
		double tTime = 0.f;
		double bTime = 0.f;

		WaitForSwapSync(&tTime, &bTime);

		Response->SetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argThreadTime),  tTime);
		Response->SetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argBarrierTime), bTime);

		return Response;
	}

	// Being here means that we have no appropriate dispatch logic for this message
	UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("%s - A dispatcher for this message hasn't been implemented yet <%s>"), *GetName(), *Request->ToString());
	return nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterSwapSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterSwapSyncService::WaitForSwapSync(double* ThreadWaitTime, double* BarrierWaitTime)
{
	if (BarrierSwap.Wait(ThreadWaitTime, BarrierWaitTime) != FDisplayClusterBarrier::WaitResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Error on swap barrier. Exit required."));
	}
}
