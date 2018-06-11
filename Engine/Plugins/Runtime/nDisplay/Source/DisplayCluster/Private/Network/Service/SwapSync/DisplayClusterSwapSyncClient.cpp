// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSwapSyncClient.h"
#include "DisplayClusterSwapSyncMsg.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterSwapSyncClient::FDisplayClusterSwapSyncClient() :
	FDisplayClusterClient(FString("CLN_SS"))
{
}

FDisplayClusterSwapSyncClient::FDisplayClusterSwapSyncClient(const FString& name) :
	FDisplayClusterClient(name)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterSwapSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterSwapSyncClient::WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime)
{
	static const TSharedPtr<FDisplayClusterMessage> request(new FDisplayClusterMessage(FDisplayClusterSwapSyncMsg::WaitForSwapSync::name, FDisplayClusterSwapSyncMsg::TypeRequest, FDisplayClusterSwapSyncMsg::ProtocolName));
	TSharedPtr<FDisplayClusterMessage> response = SendRecvMsg(request);

	if (response.IsValid())
	{
		if (pThreadWaitTime)
		{
			if (!response->GetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argThreadTime), *pThreadWaitTime))
			{
				UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Argument %s not available"), FDisplayClusterSwapSyncMsg::WaitForSwapSync::argThreadTime);
			}
		}

		if (pBarrierWaitTime)
		{
			if (!response->GetArg(FString(FDisplayClusterSwapSyncMsg::WaitForSwapSync::argBarrierTime), *pBarrierWaitTime))
			{
				UE_LOG(LogDisplayClusterNetwork, Error, TEXT("Argument %s not available"), FDisplayClusterSwapSyncMsg::WaitForSwapSync::argBarrierTime);
			}
		}
	}
}

