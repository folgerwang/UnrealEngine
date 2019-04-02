// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Network/Service/SwapSync/DisplayClusterSwapSyncClient.h"
#include "Network/Service/SwapSync/DisplayClusterSwapSyncMsg.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterSwapSyncClient::FDisplayClusterSwapSyncClient() :
	FDisplayClusterClient(FString("CLN_SS"))
{
}

FDisplayClusterSwapSyncClient::FDisplayClusterSwapSyncClient(const FString& InName) :
	FDisplayClusterClient(InName)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterSwapSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterSwapSyncClient::WaitForSwapSync(double* ThreadWaitTime, double* BarrierWaitTime)
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterSwapSyncMsg::WaitForSwapSync::name, FDisplayClusterSwapSyncMsg::TypeRequest, FDisplayClusterSwapSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);

	if (response.IsValid())
	{
		if (ThreadWaitTime)
		{
			if (!response->GetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argThreadTime), *ThreadWaitTime))
			{
				UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Argument %s not available"), FDisplayClusterSwapSyncMsg::WaitForSwapSync::argThreadTime);
			}
		}

		if (BarrierWaitTime)
		{
			if (!response->GetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argBarrierTime), *BarrierWaitTime))
			{
				UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Argument %s not available"), FDisplayClusterSwapSyncMsg::WaitForSwapSync::argBarrierTime);
			}
		}
	}
}

