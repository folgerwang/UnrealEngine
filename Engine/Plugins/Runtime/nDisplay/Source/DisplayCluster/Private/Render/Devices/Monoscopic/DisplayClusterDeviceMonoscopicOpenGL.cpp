// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceMonoscopicOpenGL.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "Misc/DisplayClusterLog.h"



FDisplayClusterDeviceMonoscopicOpenGL::FDisplayClusterDeviceMonoscopicOpenGL()
{

}

FDisplayClusterDeviceMonoscopicOpenGL::~FDisplayClusterDeviceMonoscopicOpenGL()
{

}

bool FDisplayClusterDeviceMonoscopicOpenGL::Present(int32& InOutSyncInterval)
{
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("FDisplayClusterDeviceQuadBufferStereoOpenGL::Present"));

	const int halfSizeX = BackBuffSize.X / 2;
	const int dstX1 = 0;
	const int dstX2 = halfSizeX;

	// Convert to left bottom origin and flip Y
	const int dstY1 = ViewportSize.Y;
	const int dstY2 = 0;

	FOpenGLViewport* pOglViewport = static_cast<FOpenGLViewport*>(CurrentViewport->GetViewportRHI().GetReference());
	check(pOglViewport);
	FPlatformOpenGLContext* const pContext = pOglViewport->GetGLContext();
	check(pContext && pContext->DeviceContext);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, pContext->ViewportFramebuffer);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(
		0, 0, halfSizeX, BackBuffSize.Y,
		dstX1, dstY1, dstX2, dstY2,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);
	
	// Perform buffers swap logic
	SwapBuffers(pOglViewport, InOutSyncInterval);
	REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP();

	return false;
}
