// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Session/DisplayClusterSessionInternal.h"
#include "Network/DisplayClusterMessage.h"

#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterConstants.h"


FDisplayClusterSessionInternal::FDisplayClusterSessionInternal(FSocket* InSocket, IDisplayClusterSessionListener* InListener, const FString& InName) :
	FDisplayClusterSessionBase(InSocket, InListener, InName)
{
}

FDisplayClusterSessionInternal::~FDisplayClusterSessionInternal()
{
}

uint32 FDisplayClusterSessionInternal::Run()
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session thread %s started"), *GetName());

	while (IsOpen())
	{
		TSharedPtr<FDisplayClusterMessage> Request = RecvMsg();
		if (!Request.IsValid())
		{
			UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("Couldn't receive an internal message"), *GetName());
			break;
		}

		// Process the message
		TSharedPtr<FDisplayClusterMessage> Response = GetListener()->ProcessMessage(Request);
		if (Response.IsValid())
		{
			UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Internal message has been processed"), *GetName());
			SendMsg(Response);
		}
		else
		{
			// We must terminate the socket (and the whole cluster) if something wrong to communication
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("An error occurred while processing an internal message. Closing the socket."), *GetName());
			break;
		}
	}

	Stop();

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Session thread %s finished"), *GetName());
	return 0;
}
