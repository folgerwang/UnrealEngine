// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Game/IDisplayClusterGameManager.h"
#include "IPDisplayClusterManager.h"

class ADisplayClusterGameMode;
class ADisplayClusterSettings;


/**
 * Game manager private interface
 */
struct IPDisplayClusterGameManager :
	public IDisplayClusterGameManager,
	public IPDisplayClusterManager
{
	virtual ~IPDisplayClusterGameManager()
	{ }

	virtual bool IsDisplayClusterActive() const = 0;

	virtual void SetDisplayClusterGameMode(ADisplayClusterGameMode* pGameMode) = 0;
	virtual ADisplayClusterGameMode* GetDisplayClusterGameMode() const = 0;

	virtual void SetDisplayClusterSceneSettings(ADisplayClusterSettings* pSceneSettings) = 0;
	virtual ADisplayClusterSettings* GetDisplayClusterSceneSettings() const = 0;
};
