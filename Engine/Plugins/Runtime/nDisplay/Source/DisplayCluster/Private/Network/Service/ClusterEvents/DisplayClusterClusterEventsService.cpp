// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterEvents/DisplayClusterClusterEventsService.h"
#include "Network/Service/ClusterEvents/DisplayClusterClusterEventsMsg.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/DisplayClusterClusterEvent.h"
#include "Misc/DisplayClusterLog.h"
#include "Network/Session/DisplayClusterSessionExternal.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"

#include "DisplayClusterGlobals.h"


FDisplayClusterClusterEventsService::FDisplayClusterClusterEventsService(const FString& InAddr, const int32 InPort) :
	FDisplayClusterService(FString("SRV_CE"), InAddr, InPort)
{
	ResponseErrorMissedMandatoryFields = MakeShareable(new FJsonObject());
	ResponseErrorMissedMandatoryFields->SetNumberField(FString(FDisplayClusterClusterEventsMsg::ArgError), (double)EDisplayClusterJsonError::MissedMandatoryFields);

	ResponseErrorUnknown = MakeShareable(new FJsonObject());
	ResponseErrorUnknown->SetNumberField(FString(FDisplayClusterClusterEventsMsg::ArgError), (double)EDisplayClusterJsonError::UnknownError);

	ResponseOk = MakeShareable(new FJsonObject());
	ResponseOk->SetNumberField(FString(FDisplayClusterClusterEventsMsg::ArgError), (double)EDisplayClusterJsonError::Ok);
}

FDisplayClusterClusterEventsService::~FDisplayClusterClusterEventsService()
{
	Shutdown();
}


bool FDisplayClusterClusterEventsService::Start()
{
	return FDisplayClusterServer::Start();
}

void FDisplayClusterClusterEventsService::Shutdown()
{
	return FDisplayClusterServer::Shutdown();
}

FDisplayClusterSessionBase* FDisplayClusterClusterEventsService::CreateSession(FSocket* InSocket, const FIPv4Endpoint& InEP)
{
	return new FDisplayClusterSessionExternal(InSocket, this, GetName() + FString("_session_external") + InEP.ToString());
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterEventsService::NotifySessionOpen(FDisplayClusterSessionBase* InSession)
{
	FDisplayClusterService::NotifySessionOpen(InSession);
}

void FDisplayClusterClusterEventsService::NotifySessionClose(FDisplayClusterSessionBase* InSession)
{
	FDisplayClusterService::NotifySessionClose(InSession);
}

TSharedPtr<FJsonObject> FDisplayClusterClusterEventsService::ProcessJson(const TSharedPtr<FJsonObject>& Request)
{
	IPDisplayClusterClusterManager* const ClusterMgr = GDisplayCluster->GetPrivateClusterMgr();
	
	bool MandatoryFieldsExist = true;

	FString EventName;
	if (!Request->TryGetStringField(FString(FDisplayClusterClusterEventsMsg::ArgName), EventName))
	{
		MandatoryFieldsExist = false;
	}

	FString EventType;
	if (!Request->TryGetStringField(FString(FDisplayClusterClusterEventsMsg::ArgType), EventType))
	{
		MandatoryFieldsExist = false;
	}

	FString EventCategory;
	if (!Request->TryGetStringField(FString(FDisplayClusterClusterEventsMsg::ArgCategory), EventCategory))
	{
		MandatoryFieldsExist = false;
	}

	if (MandatoryFieldsExist == false)
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Json message doesn't have a mandatory field"), *GetName());
		return ResponseErrorMissedMandatoryFields;
	}

	// Convert Json request to internal cluster event structure
	FDisplayClusterClusterEvent ClusterEvent;
	FJsonObjectConverter::JsonObjectToUStruct(Request.ToSharedRef(), &ClusterEvent);

	// Emit the event
	EmitClusterEvent(ClusterEvent);

	return ResponseOk;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterEventsProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterEventsService::EmitClusterEvent(const FDisplayClusterClusterEvent& Event)
{
	GDisplayCluster->GetPrivateClusterMgr()->EmitClusterEvent(Event, true);
}
