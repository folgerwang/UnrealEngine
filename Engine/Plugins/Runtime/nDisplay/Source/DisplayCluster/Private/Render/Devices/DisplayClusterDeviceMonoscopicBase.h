// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceBase.h"


/**
 * Base monoscopic render device
 */
class FDisplayClusterDeviceMonoscopicBase
	: public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceMonoscopicBase();
	virtual ~FDisplayClusterDeviceMonoscopicBase();

public:
	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return false; }

	virtual void AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
};
