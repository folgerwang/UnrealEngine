// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/DisplayClusterClient.h"
#include "Common/TcpSocketBuilder.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterClient::FDisplayClusterClient(const FString& InName) :
	FDisplayClusterSocketOps(CreateSocket(InName)),
	Name(InName)
{
}

FDisplayClusterClient::~FDisplayClusterClient()
{
	Disconnect();
}

bool FDisplayClusterClient::Connect(const FString& InAddr, const int32 InPort, const int32 TriesAmount, const float TryDelay)
{
	FScopeLock lock(&GetSyncObj());

	// Generate IPv4 address
	FIPv4Address IPAddr;
	if (!FIPv4Address::Parse(InAddr, IPAddr))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s couldn't parse the address [%s:%d]"), *GetName(), *InAddr, InPort);
		return false;
	}

	// Generate internet address
	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InternetAddr->SetIp(IPAddr.Value);
	InternetAddr->SetPort(InPort);

	// Start connection loop
	int32 TryIdx = 0;
	while(GetSocket()->Connect(*InternetAddr) == false)
	{
		UE_LOG(LogDisplayClusterNetwork, Log, TEXT("%s couldn't connect to the server %s [%d]"), *GetName(), *(InternetAddr->ToString(true)), TryIdx++);
		if (TriesAmount > 0 && TryIdx >= TriesAmount)
		{
			UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s connection attempts limit reached"), *GetName());
			break;
		}

		// Sleep some time before next try
		FPlatformProcess::Sleep(TryDelay / 1000.f);
	}

	return IsOpen();
}

void FDisplayClusterClient::Disconnect()
{
	FScopeLock lock(&GetSyncObj());

	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("%s disconnecting..."), *GetName());

	if (IsOpen())
	{
		GetSocket()->Close();
	}
}

FSocket* FDisplayClusterClient::CreateSocket(const FString& InName, const int32 BuffSize)
{
	FSocket* pSock = FTcpSocketBuilder(*InName).AsBlocking().WithReceiveBufferSize(BuffSize).WithSendBufferSize(BuffSize);
	check(pSock);
	return pSock;
}

bool FDisplayClusterClient::SendMsg(const TSharedPtr<FDisplayClusterMessage>& Msg)
{
	const bool result = FDisplayClusterSocketOps::SendMsg(Msg);
	if (result == false)
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Something wrong with connection (send). The cluster is inconsistent. Exit required."));
	}

	return result;
}

TSharedPtr<FDisplayClusterMessage> FDisplayClusterClient::RecvMsg()
{
	TSharedPtr<FDisplayClusterMessage> Response = FDisplayClusterSocketOps::RecvMsg();
	if (!Response.IsValid())
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Something wrong with connection (recv). The cluster is inconsistent. Exit required."));
	}

	return Response;
}

TSharedPtr<FDisplayClusterMessage> FDisplayClusterClient::SendRecvMsg(const TSharedPtr<FDisplayClusterMessage>& Msg)
{
	SendMsg(Msg);
	TSharedPtr<FDisplayClusterMessage> Response = RecvMsg();

	if (!Response.IsValid())
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("No response"));
	}

	return Response;
}
