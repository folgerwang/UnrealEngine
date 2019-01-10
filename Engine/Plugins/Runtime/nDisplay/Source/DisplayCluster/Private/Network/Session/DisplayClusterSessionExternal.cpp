// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Session/DisplayClusterSessionExternal.h"
#include "Misc/DisplayClusterLog.h"

#include "Dom/JsonObject.h"


FDisplayClusterSessionExternal::FDisplayClusterSessionExternal(FSocket* InSocket, IDisplayClusterSessionListener* InListener, const FString& InName) :
	FDisplayClusterSessionBase(InSocket, InListener, InName)
{
}

FDisplayClusterSessionExternal::~FDisplayClusterSessionExternal()
{
}

uint32 FDisplayClusterSessionExternal::Run()
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session thread %s started"), *GetName());

	while (IsOpen())
	{
		// Receive json based message
		TSharedPtr<FJsonObject> Request = RecvJson();
		if (!Request.IsValid())
		{
			UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Couldn't receive a json based message. Closing connection..."), *GetName());
			break;
		}

		// Process the message
		TSharedPtr<FJsonObject> Response = GetListener()->ProcessJson(Request);
		if (Response.IsValid())
		{
			UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Json based message has been processed"), *GetName());
			SendJson(Response);
		}
	}

	Stop();

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session thread %s finished"), *GetName());
	return 0;
}
