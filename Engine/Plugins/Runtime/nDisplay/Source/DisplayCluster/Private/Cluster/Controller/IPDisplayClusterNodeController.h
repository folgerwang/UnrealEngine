// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/Protocol/IPDisplayClusterClusterSyncProtocol.h"
#include "Network/Protocol/IPDisplayClusterSwapSyncProtocol.h"


/**
 * Node controller interface
 */
struct IPDisplayClusterNodeController
	: public IPDisplayClusterClusterSyncProtocol
	, public IPDisplayClusterSwapSyncProtocol
{
	virtual ~IPDisplayClusterNodeController()
	{ }

	virtual bool Initialize() = 0;
	virtual void Release() = 0;

	virtual bool IsMaster() const = 0;
	virtual bool IsSlave() const = 0;
	virtual bool IsStandalone() const = 0;
	virtual bool IsCluster() const = 0;
	virtual FString GetNodeId() const = 0;
	virtual FString GetControllerName() const = 0;
};

