// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	FDisplayClusterSwapSyncService(const FString& addr, const int32 port);
	virtual ~FDisplayClusterSwapSyncService();

public:
	virtual bool Start() override;
	virtual void Shutdown() override;

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void NotifySessionOpen(FDisplayClusterSession* pSession) override;
	virtual void NotifySessionClose(FDisplayClusterSession* pSession) override;
	virtual FDisplayClusterMessage::Ptr ProcessMessage(FDisplayClusterMessage::Ptr msg) override;

private:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterSwapSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime) override;


private:
	// Swap sync barrier
	FDisplayClusterBarrier BarrierSwap;
};

