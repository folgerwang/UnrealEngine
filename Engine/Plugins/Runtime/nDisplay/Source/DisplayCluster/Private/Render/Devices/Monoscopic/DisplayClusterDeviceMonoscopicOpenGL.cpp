// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceMonoscopicOpenGL.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "Misc/DisplayClusterLog.h"

#include <utility>


FDisplayClusterDeviceMonoscopicOpenGL::FDisplayClusterDeviceMonoscopicOpenGL()
{

}

FDisplayClusterDeviceMonoscopicOpenGL::~FDisplayClusterDeviceMonoscopicOpenGL()
{

}

bool FDisplayClusterDeviceMonoscopicOpenGL::Present(int32& InOutSyncInterval)
{
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("FDisplayClusterDeviceQuadBufferStereoOpenGL::Present"));

	const int halfSizeY = BackBuffSize.Y / 2;
	int dstX1 = 0;
	int dstX2 = BackBuffSize.X;

	// Convert to left bottom origin and flip Y
	int dstY1 = ViewportSize.Y;
	int dstY2 = 0;

	if (bFlipHorizontal)
		std::swap(dstX1, dstX2);

	if (bFlipVertical)
		std::swap(dstY1, dstY2);


	FOpenGLViewport* pOglViewport = static_cast<FOpenGLViewport*>(CurrentViewport->GetViewportRHI().GetReference());
	check(pOglViewport);
	FPlatformOpenGLContext* const pContext = pOglViewport->GetGLContext();
	check(pContext && pContext->DeviceContext);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, pContext->ViewportFramebuffer);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(
		0, 0, BackBuffSize.X, halfSizeY,
		dstX1, dstY1, dstX2, dstY2,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);
	
	// Perform buffers swap logic
	SwapBuffers(pOglViewport, InOutSyncInterval);
	REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP();

	return false;
}
