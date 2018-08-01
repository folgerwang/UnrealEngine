// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayCluster.h"
#include "IPDisplayClusterManager.h"

struct IPDisplayClusterRenderManager;
struct IPDisplayClusterClusterManager;
struct IPDisplayClusterInputManager;
struct IPDisplayClusterConfigManager;
struct IPDisplayClusterGameManager;

class ADisplayClusterGameMode;
class ADisplayClusterSettings;


/**
 * Private module interface
 */
struct IPDisplayCluster
	: public IDisplayCluster
	, public IPDisplayClusterManager
{
	virtual ~IPDisplayCluster() = 0
	{ }

	virtual IPDisplayClusterRenderManager*    GetPrivateRenderMgr() const = 0;
	virtual IPDisplayClusterClusterManager*   GetPrivateClusterMgr() const = 0;
	virtual IPDisplayClusterInputManager*     GetPrivateInputMgr() const = 0;
	virtual IPDisplayClusterConfigManager*    GetPrivateConfigMgr() const = 0;
	virtual IPDisplayClusterGameManager*      GetPrivateGameMgr() const = 0;
};
