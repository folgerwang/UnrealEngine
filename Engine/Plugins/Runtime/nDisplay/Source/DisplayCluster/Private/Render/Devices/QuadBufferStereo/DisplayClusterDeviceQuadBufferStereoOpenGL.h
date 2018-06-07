// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterDeviceQuadBufferStereoBase.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"


/**
 * Frame sequenced active stereo (OpenGL 3 and 4)
 */
class FDisplayClusterDeviceQuadBufferStereoOpenGL : public FDisplayClusterDeviceQuadBufferStereoBase
{
public:
	FDisplayClusterDeviceQuadBufferStereoOpenGL();
	virtual ~FDisplayClusterDeviceQuadBufferStereoOpenGL();

protected:
	virtual void SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy);
	virtual bool Present(int32& InOutSyncInterval) override;
	void SwapBuffers(FOpenGLViewport* pOglViewport, int32& InOutSyncInterval);

private:
	// Set up swap interval for upcoming buffer swap
	void UpdateSwapInterval(int32 swapInt) const;
	// Joins swap groups and binds to a swap barrier
	bool InitializeNvidiaSwapLock();

	// Implementation of swap policies
	void internal_SwapBuffersPolicyNone(FOpenGLViewport* pOglViewport);
	void internal_SwapBuffersPolicySoftSwapSync(FOpenGLViewport* pOglViewport);
	void internal_SwapBuffersPolicyNvSwapSync(FOpenGLViewport* pOglViewport);

private:
	// State of nv_swap initialization
	bool bNvSwapInitialized = false;
};
