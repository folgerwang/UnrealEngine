// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterClusterSyncService.h"
#include "DisplayClusterClusterSyncMsg.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Input/IPDisplayClusterInputManager.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


FDisplayClusterClusterSyncService::FDisplayClusterClusterSyncService(const FString& addr, const int32 port) :
	FDisplayClusterService(FString("SRV_CS"), addr, port),
	BarrierGameStart  (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("GameStart_barrier"),  DisplayClusterConstants::net::BarrierGameStartWaitTimeout),
	BarrierFrameStart (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("FrameStart_barrier"), DisplayClusterConstants::net::BarrierWaitTimeout),
	BarrierFrameEnd   (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("FrameEnd_barrier"),   DisplayClusterConstants::net::BarrierWaitTimeout),
	BarrierTickEnd    (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("TickEnd_barrier"),    DisplayClusterConstants::net::BarrierWaitTimeout)
{
}

FDisplayClusterClusterSyncService::~FDisplayClusterClusterSyncService()
{
	Shutdown();
}


bool FDisplayClusterClusterSyncService::Start()
{
	BarrierGameStart.Activate();
	BarrierFrameStart.Activate();
	BarrierFrameEnd.Activate();
	BarrierTickEnd.Activate();

	return FDisplayClusterServer::Start();
}

void FDisplayClusterClusterSyncService::Shutdown()
{
	BarrierGameStart.Deactivate();
	BarrierFrameStart.Deactivate();
	BarrierFrameEnd.Deactivate();
	BarrierTickEnd.Deactivate();

	return FDisplayClusterServer::Shutdown();
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterSyncService::NotifySessionOpen(FDisplayClusterSession* pSession)
{
	FDisplayClusterService::NotifySessionOpen(pSession);
}

void FDisplayClusterClusterSyncService::NotifySessionClose(FDisplayClusterSession* pSession)
{
	// Unblock waiting threads to allow current Tick() finish
	BarrierGameStart.Deactivate();
	BarrierFrameStart.Deactivate();
	BarrierFrameEnd.Deactivate();
	BarrierTickEnd.Deactivate();

	FDisplayClusterService::NotifySessionClose(pSession);
}

FDisplayClusterMessage::Ptr FDisplayClusterClusterSyncService::ProcessMessage(FDisplayClusterMessage::Ptr msg)
{
	// Check the pointer
	if (msg.IsValid() == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Couldn't process the message"), *GetName());
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - Processing message %s"), *GetName(), *msg->ToString());

	// Check protocol and type
	if (msg->GetProtocol() != FDisplayClusterClusterSyncMsg::ProtocolName || msg->GetType() != FDisplayClusterClusterSyncMsg::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Unsupported message type: %s"), *GetName(), *msg->ToString());
		return nullptr;
	}

	// Initialize response message
	FDisplayClusterMessage::Ptr response = FDisplayClusterMessage::Ptr(new FDisplayClusterMessage(msg->GetName(), FDisplayClusterClusterSyncMsg::TypeResponse, msg->GetProtocol()));

	// Dispatch the message
	const FString msgName = msg->GetName();
	if (msgName == FDisplayClusterClusterSyncMsg::WaitForGameStart::name)
	{
		WaitForGameStart();
		return response;
	}
	else if (msgName == FDisplayClusterClusterSyncMsg::WaitForFrameStart::name)
	{
		WaitForFrameStart();
		return response;
	}
	else if (msgName == FDisplayClusterClusterSyncMsg::WaitForFrameEnd::name)
	{
		WaitForFrameEnd();
		return response;
	}
	else if (msgName == FDisplayClusterClusterSyncMsg::WaitForTickEnd::name)
	{
		WaitForTickEnd();
		return response;
	}
	else if (msgName == FDisplayClusterClusterSyncMsg::GetDeltaTime::name)
	{
		float deltaTime = 0.0f;
		GetDeltaTime(deltaTime);
		response->SetArg(FDisplayClusterClusterSyncMsg::GetDeltaTime::argDeltaTime, deltaTime);
		return response;
	}
	else if (msgName == FDisplayClusterClusterSyncMsg::GetSyncData::name)
	{
		FDisplayClusterMessage::DataType data;
		GetSyncData(data);

		response->SetArgs(data);
		return response;
	}
	else if (msgName == FDisplayClusterClusterSyncMsg::GetInputData::name)
	{
		FDisplayClusterMessage::DataType data;
		GetInputData(data);

		response->SetArgs(data);
		return response;
	}

	// Being here means that we have no appropriate dispatch logic for this message
	UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("%s - A dispatcher for this message hasn't been implemented yet <%s>"), *GetName(), *msg->ToString());
	return nullptr;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterSyncService::WaitForGameStart()
{
	if (BarrierGameStart.Wait() != FDisplayClusterBarrier::WaitResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Error on game start barrier. Exit required."));
	}
}

void FDisplayClusterClusterSyncService::WaitForFrameStart()
{
	if (BarrierFrameStart.Wait() != FDisplayClusterBarrier::WaitResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Error on frame start barrier. Exit required."));
	}
}

void FDisplayClusterClusterSyncService::WaitForFrameEnd()
{
	if (BarrierFrameEnd.Wait() != FDisplayClusterBarrier::WaitResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Error on frame end barrier. Exit required."));
	}
}

void FDisplayClusterClusterSyncService::WaitForTickEnd()
{
	if (BarrierTickEnd.Wait() != FDisplayClusterBarrier::WaitResult::Ok)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Error on tick end barrier. Exit required."));
	}
}

void FDisplayClusterClusterSyncService::GetDeltaTime(float& deltaTime)
{
	deltaTime = GDisplayCluster->GetPrivateClusterMgr()->GetDeltaTime();
}

void FDisplayClusterClusterSyncService::GetSyncData(FDisplayClusterMessage::DataType& data)
{
	GDisplayCluster->GetPrivateClusterMgr()->ExportSyncData(data);
}

void FDisplayClusterClusterSyncService::GetInputData(FDisplayClusterMessage::DataType& data)
{
	GDisplayCluster->GetPrivateInputMgr()->ExportInputData(data);
}
