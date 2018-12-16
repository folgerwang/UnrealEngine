// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Swap synchronization protocol
 */
class IPDisplayClusterSwapSyncProtocol
{
public:
	// Swap sync barrier
	virtual void WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime) = 0;
};

