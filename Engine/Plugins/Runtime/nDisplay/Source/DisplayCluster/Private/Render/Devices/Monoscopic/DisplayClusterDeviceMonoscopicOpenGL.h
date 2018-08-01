// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoOpenGL.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"


/**
 * Monoscopic emulation device (OpenGL 3 and 4)
 */
class FDisplayClusterDeviceMonoscopicOpenGL : public FDisplayClusterDeviceQuadBufferStereoOpenGL
{
public:
	FDisplayClusterDeviceMonoscopicOpenGL();
	virtual ~FDisplayClusterDeviceMonoscopicOpenGL();

protected:
	virtual bool Present(int32& InOutSyncInterval) override;
};
