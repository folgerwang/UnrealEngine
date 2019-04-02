// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Network/Service/DisplayClusterService.h"
#include "Network/Protocol/IPDisplayClusterSwapSyncProtocol.h"
#include "Network/DisplayClusterMessage.h"

#include "Misc/DisplayClusterBarrier.h"


/**
 * Swap synchronization server
 */
class FDisplayClusterSwapSyncService
	: public  FDisplayClusterService
	, private IPDisplayClusterSwapSyncProtocol
{
public:
	FDisplayClusterSwapSyncService(const FString& InAddr, const int32 InPort);
	virtual ~FDisplayClusterSwapSyncService();

public:
	virtual bool Start() override;
	virtual void Shutdown() override;

protected:
	virtual FDisplayClusterSessionBase* CreateSession(FSocket* InSocket, const FIPv4Endpoint& InEP) override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void NotifySessionOpen(FDisplayClusterSessionBase* InSession) override;
	virtual void NotifySessionClose(FDisplayClusterSessionBase* InSession) override;
	virtual TSharedPtr<FDisplayClusterMessage> ProcessMessage(const TSharedPtr<FDisplayClusterMessage>& Request) override;

private:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterSwapSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForSwapSync(double* ThreadWaitTime, double* BarrierWaitTime) override;


private:
	// Swap sync barrier
	FDisplayClusterBarrier BarrierSwap;
};
