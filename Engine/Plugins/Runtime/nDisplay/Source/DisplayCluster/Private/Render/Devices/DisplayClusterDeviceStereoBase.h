// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceBase.h"


/**
 * Base stereo render device
 */
class FDisplayClusterDeviceStereoBase
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceStereoBase();
	virtual ~FDisplayClusterDeviceStereoBase();

public:
	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return true; }
};
