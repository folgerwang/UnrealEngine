// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/IDisplayClusterRenderManager.h"
#include "IPDisplayClusterManager.h"


/**
 * Render manager interface
 */
struct IPDisplayClusterRenderManager
	: public IDisplayClusterRenderManager
	, public IPDisplayClusterManager
{
	virtual ~IPDisplayClusterRenderManager()
	{ }

};
