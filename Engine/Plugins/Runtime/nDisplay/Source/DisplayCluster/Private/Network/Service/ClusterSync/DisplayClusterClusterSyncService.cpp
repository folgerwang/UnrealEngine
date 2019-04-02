// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterSync/DisplayClusterClusterSyncService.h"
#include "Network/Service/ClusterSync/DisplayClusterClusterSyncMsg.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Input/IPDisplayClusterInputManager.h"

#include "Network/Session/DisplayClusterSessionInternal.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterGlobals.h"


FDisplayClusterClusterSyncService::FDisplayClusterClusterSyncService(const FString& InAddr, const int32 InPort) :
	FDisplayClusterService(FString("SRV_CS"), InAddr, InPort),
	BarrierGameStart  (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("GameStart_barrier"),  GDisplayCluster->GetConfigMgr()->GetConfigNetwork().BarrierGameStartWaitTimeout),
	BarrierFrameStart (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("FrameStart_barrier"), GDisplayCluster->GetConfigMgr()->GetConfigNetwork().BarrierWaitTimeout),
	BarrierFrameEnd   (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("FrameEnd_barrier"),   GDisplayCluster->GetConfigMgr()->GetConfigNetwork().BarrierWaitTimeout),
	BarrierTickEnd    (GDisplayCluster->GetPrivateClusterMgr()->GetNodesAmount(), FString("TickEnd_barrier"),    GDisplayCluster->GetConfigMgr()->GetConfigNetwork().BarrierWaitTimeout)
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

FDisplayClusterSessionBase* FDisplayClusterClusterSyncService::CreateSession(FSocket* InSocket, const FIPv4Endpoint& InEP)
{
	return new FDisplayClusterSessionInternal(InSocket, this, GetName() + FString("_session_") + InEP.ToString());
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterSyncService::NotifySessionOpen(FDisplayClusterSessionBase* InSession)
{
	FDisplayClusterService::NotifySessionOpen(InSession);
}

void FDisplayClusterClusterSyncService::NotifySessionClose(FDisplayClusterSessionBase* InSession)
{
	// Unblock waiting threads to allow current Tick() finish
	BarrierGameStart.Deactivate();
	BarrierFrameStart.Deactivate();
	BarrierFrameEnd.Deactivate();
	BarrierTickEnd.Deactivate();

	FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, GetName() + FString(" - Connection interrupted. Application exit requested."));
	FDisplayClusterService::NotifySessionClose(InSession);
}

TSharedPtr<FDisplayClusterMessage> FDisplayClusterClusterSyncService::ProcessMessage(const TSharedPtr<FDisplayClusterMessage>& Request)
{
	// Check the pointer
	if (Request.IsValid() == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Couldn't process the message"), *GetName());
		return nullptr;
	}

	UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s - Processing message %s"), *GetName(), *Request->ToString());

	// Check protocol and type
	if (Request->GetProtocol() != FDisplayClusterClusterSyncMsg::ProtocolName || Request->GetType() != FDisplayClusterClusterSyncMsg::TypeRequest)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("%s - Unsupported message type: %s"), *GetName(), *Request->ToString());
		return nullptr;
	}

	// Initialize response message
	TSharedPtr<FDisplayClusterMessage> Response = MakeShareable(new FDisplayClusterMessage(Request->GetName(), FDisplayClusterClusterSyncMsg::TypeResponse, Request->GetProtocol()));

	// Dispatch the message
	const FString ReqName = Request->GetName();
	if (ReqName == FDisplayClusterClusterSyncMsg::WaitForGameStart::name)
	{
		WaitForGameStart();
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::WaitForFrameStart::name)
	{
		WaitForFrameStart();
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::WaitForFrameEnd::name)
	{
		WaitForFrameEnd();
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::WaitForTickEnd::name)
	{
		WaitForTickEnd();
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::GetDeltaTime::name)
	{
		float deltaTime = 0.0f;
		GetDeltaTime(deltaTime);
		Response->SetArg(FDisplayClusterClusterSyncMsg::GetDeltaTime::argDeltaTime, deltaTime);
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::GetTimecode::name)
	{
		FTimecode timecode;
		FFrameRate frameRate;
		GetTimecode(timecode, frameRate);
		Response->SetArg(FDisplayClusterClusterSyncMsg::GetTimecode::argTimecode, timecode);
		Response->SetArg(FDisplayClusterClusterSyncMsg::GetTimecode::argFrameRate, frameRate);
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::GetSyncData::name)
	{
		FDisplayClusterMessage::DataType data;
		GetSyncData(data);

		Response->SetArgs(data);
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::GetInputData::name)
	{
		FDisplayClusterMessage::DataType data;
		GetInputData(data);

		Response->SetArgs(data);
		return Response;
	}
	else if (ReqName == FDisplayClusterClusterSyncMsg::GetEventsData::name)
	{
		FDisplayClusterMessage::DataType data;
		GetEventsData(data);

		Response->SetArgs(data);
		return Response;
	}

	// Being here means that we have no appropriate dispatch logic for this message
	UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("%s - A dispatcher for this message hasn't been implemented yet <%s>"), *GetName(), *Request->ToString());
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

void FDisplayClusterClusterSyncService::GetDeltaTime(float& DeltaTime)
{
	DeltaTime = GDisplayCluster->GetPrivateClusterMgr()->GetDeltaTime();
}

void FDisplayClusterClusterSyncService::GetTimecode(FTimecode& Timecode, FFrameRate& FrameRate)
{
	GDisplayCluster->GetPrivateClusterMgr()->GetTimecode(Timecode, FrameRate);
}

void FDisplayClusterClusterSyncService::GetSyncData(FDisplayClusterMessage::DataType& Data)
{
	GDisplayCluster->GetPrivateClusterMgr()->ExportSyncData(Data);
}

void FDisplayClusterClusterSyncService::GetInputData(FDisplayClusterMessage::DataType& Data)
{
	GDisplayCluster->GetPrivateInputMgr()->ExportInputData(Data);
}

void FDisplayClusterClusterSyncService::GetEventsData(FDisplayClusterMessage::DataType& Data)
{
	GDisplayCluster->GetPrivateClusterMgr()->ExportEventsData(Data);
}
