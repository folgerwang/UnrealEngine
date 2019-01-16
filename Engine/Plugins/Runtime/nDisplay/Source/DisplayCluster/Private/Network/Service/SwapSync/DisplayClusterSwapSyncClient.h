// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	FDisplayClusterSwapSyncClient(const FString& InName);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IPDisplayClusterSwapSyncProtocol
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void WaitForSwapSync(double* ThreadWaitTime, double* BarrierWaitTime) override;
};

