// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterClient.h"
#include "Common/TcpSocketBuilder.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterClient::FDisplayClusterClient(const FString& name) :
	FDisplayClusterSocketOps(CreateSocket(name)),
	Name(name)
{
}

FDisplayClusterClient::~FDisplayClusterClient()
{
	Disconnect();
}

bool FDisplayClusterClient::Connect(const FString& addr, const int32 port, const int32 triesAmount, const float delay)
{
	FScopeLock lock(&GetSyncObj());

	// Generate IPv4 address
	FIPv4Address ipAddr;
	if (!FIPv4Address::Parse(addr, ipAddr))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s couldn't parse the address [%s:%d]"), *Name, *addr, port);
		return false;
	}

	// Generate internet address
	TSharedRef<FInternetAddr> internetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	internetAddr->SetIp(ipAddr.Value);
	internetAddr->SetPort(port);

	// Start connection loop
	int32 tryIdx = 0;
	while(GetSocket()->Connect(*internetAddr) == false)
	{
		UE_LOG(LogDisplayClusterNetwork, Log, TEXT("%s couldn't connect to the server %s [%d]"), *Name, *(internetAddr->ToString(true)), tryIdx++);
		if (triesAmount > 0 && tryIdx >= triesAmount)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s connection attempts limit reached"), *Name);
			break;
		}

		// Sleep some time before next try
		FPlatformProcess::Sleep(delay);
	}

	return IsOpen();
}

void FDisplayClusterClient::Disconnect()
{
	FScopeLock lock(&GetSyncObj());

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("%s disconnecting..."), *Name);

	if (IsOpen())
	{
		GetSocket()->Close();
	}
}

FSocket* FDisplayClusterClient::CreateSocket(const FString& name, const int32 bufSize)
{
	FSocket* pSock = FTcpSocketBuilder(*name).AsBlocking().WithReceiveBufferSize(bufSize).WithSendBufferSize(bufSize);
	check(pSock);
	return pSock;
}

bool FDisplayClusterClient::SendMsg(const FDisplayClusterMessage::Ptr& msg)
{
	const bool result = FDisplayClusterSocketOps::SendMsg(msg);
	if (result == false)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Something wrong with connection (send). The cluster is inconsistent. Exit required."));
	}

	return result;
}

FDisplayClusterMessage::Ptr FDisplayClusterClient::RecvMsg()
{
	FDisplayClusterMessage::Ptr response = FDisplayClusterSocketOps::RecvMsg();
	if (!response.IsValid())
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Something wrong with connection (recv). The cluster is inconsistent. Exit required."));
	}

	return response;
}

FDisplayClusterMessage::Ptr FDisplayClusterClient::SendRecvMsg(const FDisplayClusterMessage::Ptr& msg)
{
	FDisplayClusterMessage::Ptr response;

	{
		FScopeLock lock(&GetSyncObj());
		SendMsg(msg);
		response = RecvMsg();
	}

	if (!response.IsValid())
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("No response"));
	}

	return response;
}

