// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterStereoDevice.h"


/**
 * Public render manager interface
 */
struct IDisplayClusterRenderManager
{
	virtual ~IDisplayClusterRenderManager()
	{ }

	virtual IDisplayClusterStereoDevice* GetStereoDevice() const = 0;
};
