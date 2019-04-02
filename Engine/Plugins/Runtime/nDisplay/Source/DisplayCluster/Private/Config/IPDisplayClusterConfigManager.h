// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/IDisplayClusterConfigManager.h"
#include "IPDisplayClusterManager.h"
#include "DisplayClusterBuildConfig.h"


/**
 * Config manager private interface
 */
class IPDisplayClusterConfigManager
	: public IDisplayClusterConfigManager
	, public IPDisplayClusterManager
{
public:
	virtual ~IPDisplayClusterConfigManager()
	{ }

#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	virtual bool IsRunningDebugAuto() const = 0;
#endif
};
