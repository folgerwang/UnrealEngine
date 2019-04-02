// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/IDisplayClusterStereoRendering.h"


/**
 * Public render manager interface
 */
class IDisplayClusterRenderManager
	: public IDisplayClusterStereoRendering
{
public:
	virtual ~IDisplayClusterRenderManager()
	{ }

};
