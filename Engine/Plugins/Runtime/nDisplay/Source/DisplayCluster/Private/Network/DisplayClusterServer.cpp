// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterServer.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterServer::FDisplayClusterServer(const FString& name, const FString& addr, const int32 port) :
	Name(name),
	Address(addr),
	Port(port),
	Listener(name + FString("_listener"))
{
	check(port > 0 && port < 0xffff);
	
	// Bind connection handler method
	Listener.OnConnectionAccepted().BindRaw(this, &FDisplayClusterServer::ConnectionHandler);
}

FDisplayClusterServer::~FDisplayClusterServer()
{
	// Call from child .dtor
	Shutdown();
}

bool FDisplayClusterServer::Start()
{
	FScopeLock lock(&InternalsSyncScope);

	if (bIsRunning == true)
	{
		return true;
	}

	if (!Listener.StartListening(Address, Port))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s couldn't start the listener [%s:%d]"), *Name, *Address, Port);
		return false;
	}

	// Update server state
	bIsRunning = true;

	return bIsRunning;
}

void FDisplayClusterServer::Shutdown()
{
	FScopeLock lock(&InternalsSyncScope);

	if (bIsRunning == false)
	{
		return;
	}

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("%s stopping the service..."), *Name);

	// Stop connections listening
	Listener.StopListening();
	// Destroy active sessions
	Sessions.Reset();
	// Update server state
	bIsRunning = false;
}

bool FDisplayClusterServer::IsRunning()
{
	FScopeLock lock(&InternalsSyncScope);
	return bIsRunning;
}

bool FDisplayClusterServer::ConnectionHandler(FSocket* pSock, const FIPv4Endpoint& ep)
{
	FScopeLock lock(&InternalsSyncScope);
	check(pSock);

	if (IsRunning() && IsConnectionAllowed(pSock, ep))
	{
		pSock->SetLinger(false, 0);
		pSock->SetNonBlocking(false);

		int32 newSize = static_cast<int32>(DisplayClusterConstants::net::SocketBufferSize);
		int32 setSize;
		pSock->SetReceiveBufferSize(newSize, setSize);
		pSock->SetSendBufferSize(newSize, setSize);

		Sessions.Add(TUniquePtr<FDisplayClusterSession>(new FDisplayClusterSession(pSock, this, GetName() + FString("_session_") + ep.ToString())));
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterServer::NotifySessionOpen(FDisplayClusterSession* pSession)
{
}

void FDisplayClusterServer::NotifySessionClose(FDisplayClusterSession* pSession)
{
}


