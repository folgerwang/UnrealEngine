// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterClusterSyncClient.h"
#include "DisplayClusterClusterSyncMsg.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/ScopeLock.h"


FDisplayClusterClusterSyncClient::FDisplayClusterClusterSyncClient() :
	FDisplayClusterClient(FString("CLN_CS"))
{
}

FDisplayClusterClusterSyncClient::FDisplayClusterClusterSyncClient(const FString& name) :
	FDisplayClusterClient(name)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterSyncClient::WaitForGameStart()
{
	static TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::WaitForGameStart::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response;

	{
		FScopeLock lock(&GetSyncObj());
		SendMsg(request);
		response = RecvMsg();
	}

	if (!response.IsValid())
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Warning, TEXT("No response"));
		return;
	}
}

void FDisplayClusterClusterSyncClient::WaitForFrameStart()
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::WaitForFrameStart::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);
}

void FDisplayClusterClusterSyncClient::WaitForFrameEnd()
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::WaitForFrameEnd::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);
}

void FDisplayClusterClusterSyncClient::WaitForTickEnd()
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::WaitForTickEnd::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);
}

void FDisplayClusterClusterSyncClient::GetDeltaTime(float& deltaTime)
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::GetDeltaTime::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);

	if (!response.IsValid())
	{
		return;
	}

	// Extract sync data from response message
	if (response->GetArg(FDisplayClusterClusterSyncMsg::GetDeltaTime::argDeltaTime, deltaTime) == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("Couldn't extract an argument: %s"), FDisplayClusterClusterSyncMsg::GetDeltaTime::argDeltaTime);
	}
}

void FDisplayClusterClusterSyncClient::GetTimecode(FTimecode& timecode, FFrameRate& frameRate)
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::GetTimecode::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);

	if (!response.IsValid())
	{
		return;
	}

	// Extract sync data from response message
	if (response->GetArg(FDisplayClusterClusterSyncMsg::GetTimecode::argTimecode, timecode) == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("Couldn't extract an argument: %s"), FDisplayClusterClusterSyncMsg::GetTimecode::argTimecode);
	}
	if (response->GetArg(FDisplayClusterClusterSyncMsg::GetTimecode::argFrameRate, frameRate) == false)
	{
		UE_LOG(LogDisplayClusterNetworkMsg, Error, TEXT("Couldn't extract an argument: %s"), FDisplayClusterClusterSyncMsg::GetTimecode::argTimecode);
	}
}

void FDisplayClusterClusterSyncClient::GetSyncData(FDisplayClusterMessage::DataType& data)
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::GetSyncData::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);

	if (!response.IsValid())
	{
		return;
	}

	// Extract sync data from response message
	data = response->GetArgs();
}

void FDisplayClusterClusterSyncClient::GetInputData(FDisplayClusterMessage::DataType& data)
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterClusterSyncMsg::GetInputData::name, FDisplayClusterClusterSyncMsg::TypeRequest, FDisplayClusterClusterSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);

	if (!response.IsValid())
	{
		return;
	}

	// Extract sync data from response message
	data = response->GetArgs();
}

