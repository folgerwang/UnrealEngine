// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterTcpListener.h"

#include "Misc/DisplayClusterLog.h"
#include "HAL/RunnableThread.h"

#include "Common/TcpSocketBuilder.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterHelpers.h"


FDisplayClusterTcpListener::FDisplayClusterTcpListener(const FString& name) :
	Name(name)
{
}


FDisplayClusterTcpListener::~FDisplayClusterTcpListener()
{
	// Just free resources by stopping the listening
	StopListening();
}


bool FDisplayClusterTcpListener::StartListening(const FString& addr, const int32 port)
{
	FScopeLock lock(&InternalsSyncScope);

	if (bIsListening == true)
	{
		return true;
	}

	FIPv4Endpoint ep;
	if (!DisplayClusterHelpers::net::GenIPv4Endpoint(addr, port, ep))
	{
		return false;
	}

	return StartListening(ep);
}

bool FDisplayClusterTcpListener::StartListening(const FIPv4Endpoint& ep)
{
	FScopeLock lock(&InternalsSyncScope);

	if (bIsListening == true)
	{
		return true;
	}

	// Save new endpoint
	Endpoint = ep;

	// Create listening thread
	ThreadObj = FRunnableThread::Create(this, *(Name + FString("_thread")), 1 * 1024, TPri_Normal);
	ensure(ThreadObj);

	// Update state
	bIsListening = true;
	
	return bIsListening;
}


void FDisplayClusterTcpListener::StopListening()
{
	FScopeLock lock(&InternalsSyncScope);

	if (bIsListening == false)
	{
		return;
	}

	// Ask runnable to stop
	Stop();

	// Wait for thread finish and release it then
	if (ThreadObj)
	{
		ThreadObj->WaitForCompletion();
		delete ThreadObj;
		ThreadObj = nullptr;
	}
}

bool FDisplayClusterTcpListener::IsActive() const
{
	return bIsListening;
}

bool FDisplayClusterTcpListener::Init()
{
	// Create socket
	SocketObj = FTcpSocketBuilder(*Name).AsBlocking().BoundToEndpoint(Endpoint).Listening(128);
	if (!SocketObj)
	{
		// Just exit. No need to perform some notification from this thread to the cluster manager to notify
		// about this fail. Just kill the application. 
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Couldn't start listener socket"));
		return false;
	}

	return true;
}

uint32 FDisplayClusterTcpListener::Run()
{
	TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	if (SocketObj)
	{
		while (FSocket* pSock = SocketObj->Accept(*RemoteAddress, TEXT("FDisplayClusterTcpListener client")))
		{
			if (OnConnectionAcceptedDelegate.IsBound())
			{
				if (!OnConnectionAcceptedDelegate.Execute(pSock, FIPv4Endpoint(RemoteAddress)))
				{
					pSock->Close();
					ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(pSock);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Socket is not initialized"));
		return 0;
	}

	return 0;
}

void FDisplayClusterTcpListener::Stop()
{
	// Close the socket to unblock thread
	if (SocketObj)
	{
		SocketObj->Close();
	}
}

void FDisplayClusterTcpListener::Exit()
{
	// Release the socket
	if (SocketObj)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(SocketObj);
		SocketObj = nullptr;
	}
}
