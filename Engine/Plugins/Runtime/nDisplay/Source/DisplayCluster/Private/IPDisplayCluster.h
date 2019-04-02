// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayCluster.h"
#include "IPDisplayClusterManager.h"

class IPDisplayClusterRenderManager;
class IPDisplayClusterClusterManager;
class IPDisplayClusterInputManager;
class IPDisplayClusterConfigManager;
class IPDisplayClusterGameManager;

class ADisplayClusterGameMode;
class ADisplayClusterSettings;


/**
 * Private module interface
 */
class IPDisplayCluster
	: public IDisplayCluster
	, public IPDisplayClusterManager
{
public:
	virtual ~IPDisplayCluster() = 0
	{ }

	virtual IPDisplayClusterRenderManager*    GetPrivateRenderMgr() const = 0;
	virtual IPDisplayClusterClusterManager*   GetPrivateClusterMgr() const = 0;
	virtual IPDisplayClusterInputManager*     GetPrivateInputMgr() const = 0;
	virtual IPDisplayClusterConfigManager*    GetPrivateConfigMgr() const = 0;
	virtual IPDisplayClusterGameManager*      GetPrivateGameMgr() const = 0;
};
