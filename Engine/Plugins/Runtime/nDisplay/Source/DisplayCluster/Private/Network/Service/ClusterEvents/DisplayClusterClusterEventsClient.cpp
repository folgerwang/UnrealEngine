// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Service/ClusterEvents/DisplayClusterClusterEventsClient.h"
#include "Network/Service/ClusterEvents/DisplayClusterClusterEventsMsg.h"
#include "Cluster/DisplayClusterClusterEvent.h"

#include "Dom/JsonObject.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterClusterEventsClient::FDisplayClusterClusterEventsClient() :
	FDisplayClusterClient(FString("CLN_CE"))
{
}

FDisplayClusterClusterEventsClient::FDisplayClusterClusterEventsClient(const FString& name) :
	FDisplayClusterClient(name)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterEventsProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterEventsClient::EmitClusterEvent(const FDisplayClusterClusterEvent& Event)
{
	TSharedPtr<FJsonObject> Request = MakeShareable(new FJsonObject);

	Request->SetStringField(FString(FDisplayClusterClusterEventsMsg::ArgName), Event.Name);
	Request->SetStringField(FString(FDisplayClusterClusterEventsMsg::ArgType), Event.Type);
	Request->SetStringField(FString(FDisplayClusterClusterEventsMsg::ArgCategory), Event.Category);
	
	for (const auto& Parameter : Event.Parameters)
	{
		Request->SetStringField(Parameter.Key, Parameter.Value);
	}

	const bool Result = SendJson(Request);
	if (!Result)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("Couldn't send cluster event"));
	}
}
