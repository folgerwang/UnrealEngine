// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceMonoscopicBase.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"


/**
 * Monoscopic render device (OpenGL3, OpenGL4)
 */
class FDisplayClusterDeviceMonoscopicOpenGL
	: public FDisplayClusterDeviceMonoscopicBase
{
public:
	FDisplayClusterDeviceMonoscopicOpenGL();
	virtual ~FDisplayClusterDeviceMonoscopicOpenGL();

private:
	void SwapBuffers(FOpenGLViewport* pOglViewport, int32& InOutSyncInterval);

	// Set up swap interval for upcoming buffer swap
	void UpdateSwapInterval(int32 swapInt) const;

	// Implementation of swap policies
	void internal_SwapBuffersPolicyNone(FOpenGLViewport* pOglViewport);
	void internal_SwapBuffersPolicySoftSwapSync(FOpenGLViewport* pOglViewport);
	void internal_SwapBuffersPolicyNvSwapSync(FOpenGLViewport* pOglViewport);

protected:
	virtual bool Present(int32& InOutSyncInterval) override;
};
