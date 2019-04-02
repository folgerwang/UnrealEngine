// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/DisplayClusterServer.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterServer::FDisplayClusterServer(const FString& InName, const FString& InAddr, const int32 InPort) :
	Name(InName),
	Address(InAddr),
	Port(InPort),
	Listener(InName + FString("_listener"))
{
	check(InPort > 0 && InPort < 0xffff);
	
	// Bind connection handler method
	Listener.OnConnectionAccepted().BindRaw(this, &FDisplayClusterServer::ConnectionHandler);
}

FDisplayClusterServer::~FDisplayClusterServer()
{
	// Call from child .dtor
	Shutdown();

	// Now we can safely free all memory from our sessions. Need to be refactored in future.
	for (auto* Session : Sessions)
	{
		delete Session;
	}
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

bool FDisplayClusterServer::ConnectionHandler(FSocket* InSock, const FIPv4Endpoint& InEP)
{
	FScopeLock lock(&InternalsSyncScope);
	check(InSock);

	if (IsRunning() && IsConnectionAllowed(InSock, InEP))
	{
		InSock->SetLinger(false, 0);
		InSock->SetNonBlocking(false);

		int32 newSize = static_cast<int32>(DisplayClusterConstants::net::SocketBufferSize);
		int32 setSize;
		InSock->SetReceiveBufferSize(newSize, setSize);
		InSock->SetSendBufferSize(newSize, setSize);

		FDisplayClusterSessionBase* Session = CreateSession(InSock, InEP);
		Session->StartSession();
		Sessions.Add(Session);

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterSessionListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterServer::NotifySessionOpen(FDisplayClusterSessionBase* InSession)
{
}

void FDisplayClusterServer::NotifySessionClose(FDisplayClusterSessionBase* InSession)
{
	// We come here from a Session object so we can't delete it right now. The delete operation should
	// be performed later when the Session completely finished. This should be refactored in future.
	// For now we just hold all 'dead' session objects in the Sessions array and free memory when
	// this server is shutting down (look at the destructor).
}
