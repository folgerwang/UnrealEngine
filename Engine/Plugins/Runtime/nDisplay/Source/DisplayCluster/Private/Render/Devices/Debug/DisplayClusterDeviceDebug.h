// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceBase.h"


/**
 * Debug stereoscopic device (for development and test purposes)
 */
class FDisplayClusterDeviceDebug : public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceDebug();
	virtual ~FDisplayClusterDeviceDebug();

protected:
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
};


