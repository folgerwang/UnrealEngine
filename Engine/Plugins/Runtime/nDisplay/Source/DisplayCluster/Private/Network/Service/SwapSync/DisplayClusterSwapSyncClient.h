// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterClient.h"
#include "Network/Protocol/IPDisplayClusterSwapSyncProtocol.h"


/**
 * Swap synchronization client
 */
class FDisplayClusterSwapSyncClient
	: public FDisplayClusterClient
	, public IPDisplayClusterSwapSyncProtocol
{
public:
	FDisplayClusterSwapSyncClient();
	FDisplayClusterSwapSyncClient(const FString& name);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterSwapSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime) override;
};

