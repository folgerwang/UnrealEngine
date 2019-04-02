// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceStereoBase.h"


/**
 * Top-bottom passive stereoscopic device
 */
class FDisplayClusterDeviceTopBottom
	: public FDisplayClusterDeviceStereoBase
{
public:
	FDisplayClusterDeviceTopBottom();
	virtual ~FDisplayClusterDeviceTopBottom();

protected:
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;

	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return false; }

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRHICustomPresent
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Present(int32& InOutSyncInterval) override;
};
