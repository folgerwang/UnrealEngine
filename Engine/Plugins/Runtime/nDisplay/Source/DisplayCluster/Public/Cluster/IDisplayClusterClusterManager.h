// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Public cluster manager interface
 */
struct IDisplayClusterClusterManager
{
	virtual ~IDisplayClusterClusterManager()
	{ }

	virtual bool IsMaster()         const = 0;
	virtual bool IsSlave()          const = 0;
	virtual bool IsStandalone()     const = 0;
	virtual bool IsCluster()        const = 0;
	virtual FString GetNodeId()     const = 0;
	virtual uint32 GetNodesAmount() const = 0;
};
