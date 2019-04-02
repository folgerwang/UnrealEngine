// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/Monoscopic/DisplayClusterDeviceMonoscopicOpenGL.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "Misc/DisplayClusterLog.h"



FDisplayClusterDeviceMonoscopicOpenGL::FDisplayClusterDeviceMonoscopicOpenGL()
	: FDisplayClusterDeviceMonoscopicBase()
{
}

FDisplayClusterDeviceMonoscopicOpenGL::~FDisplayClusterDeviceMonoscopicOpenGL()
{
}

bool FDisplayClusterDeviceMonoscopicOpenGL::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	const int srcX1 = 0;
	const int srcY1 = 0;
	const int srcX2 = BackBuffSize.X;
	const int srcY2 = BackBuffSize.Y;

	const int dstX1 = 0;
	const int dstY1 = BackBuffSize.Y;
	const int dstX2 = BackBuffSize.X;
	const int dstY2 = 0;

	FOpenGLViewport* pOglViewport = static_cast<FOpenGLViewport*>(MainViewport->GetViewportRHI().GetReference());
	check(pOglViewport);
	FPlatformOpenGLContext* const pContext = pOglViewport->GetGLContext();
	check(pContext && pContext->DeviceContext);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, pContext->ViewportFramebuffer);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Blit framebuffer: [%d,%d - %d,%d] -> [%d,%d - %d,%d]"), srcX1, srcY1, srcX2, srcY2, dstX1, dstY1, dstX2, dstY2);
	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(
		srcX1, srcY1, srcX2, srcY2,
		dstX1, dstY1, dstX2, dstY2,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);
	
	// Perform buffers swap logic
	SwapBuffers(pOglViewport, InOutSyncInterval);
	REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP();

	return false;
}

void FDisplayClusterDeviceMonoscopicOpenGL::SwapBuffers(FOpenGLViewport* pOglViewport, int32& InOutSyncInterval)
{
	check(pOglViewport && pOglViewport->GetGLContext() && pOglViewport->GetGLContext()->DeviceContext);
	{
		///////////////////////////////////////////////
		// Perform swap policy
		UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Exec swap policy: %d"), (int)SwapSyncPolicy);
		switch (SwapSyncPolicy)
		{
		case EDisplayClusterSwapSyncPolicy::None:
			internal_SwapBuffersPolicyNone(pOglViewport);
			break;

		case EDisplayClusterSwapSyncPolicy::SoftSwapSync:
			internal_SwapBuffersPolicySoftSwapSync(pOglViewport);
			break;

		case EDisplayClusterSwapSyncPolicy::NvSwapSync:
			internal_SwapBuffersPolicyNvSwapSync(pOglViewport);
			break;

		default:
			UE_LOG(LogDisplayClusterRender, Error, TEXT("Unknown swap sync policy: %d"), (int)SwapSyncPolicy);
			break;
		}
	}
}

void FDisplayClusterDeviceMonoscopicOpenGL::UpdateSwapInterval(int32 swapInt) const
{
#if PLATFORM_WINDOWS
	/*
	https://www.opengl.org/registry/specs/EXT/wgl_swap_control.txt
	wglSwapIntervalEXT specifies the minimum number of video frame periods
	per buffer swap for the window associated with the current context.
	The interval takes effect when SwapBuffers or wglSwapLayerBuffer
	is first called subsequent to the wglSwapIntervalEXT call.

	The parameter <interval> specifies the minimum number of video frames
	that are displayed before a buffer swap will occur.

	A video frame period is the time required by the monitor to display a
	full frame of video data.  In the case of an interlaced monitor,
	this is typically the time required to display both the even and odd
	fields of a frame of video data.  An interval set to a value of 2
	means that the color buffers will be swapped at most every other video
	frame.

	If <interval> is set to a value of 0, buffer swaps are not synchronized
	to a video frame.  The <interval> value is silently clamped to
	the maximum implementation-dependent value supported before being
	stored.

	The swap interval is not part of the render context state.  It cannot
	be pushed or popped.  The current swap interval for the window
	associated with the current context can be obtained by calling
	wglGetSwapIntervalEXT.  The default swap interval is 1.
	*/

	// Perform that each frame
	if (!DisplayCluster_wglSwapIntervalEXT_ProcAddress(swapInt))
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Couldn't set swap interval: %d"), swapInt);

#elif
	//@todo: Implementation for Linux
#endif
}

#if PLATFORM_WINDOWS
void FDisplayClusterDeviceMonoscopicOpenGL::internal_SwapBuffersPolicyNone(FOpenGLViewport* pOglViewport)
{
	{
		///////////////////////////////////////////////
		// Swap buffers
		const double wtB = FPlatformTime::Seconds();
		::SwapBuffers(pOglViewport->GetGLContext()->DeviceContext);
		const double wtA = FPlatformTime::Seconds();

		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT SWAP bef: %lf"), wtB);
		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT SWAP aft: %lf"), wtA);
		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT SWAP diff: %lf"), wtA - wtB);
	}
}

void FDisplayClusterDeviceMonoscopicOpenGL::internal_SwapBuffersPolicySoftSwapSync(FOpenGLViewport* pOglViewport)
{
	static double lastSwapBuffersTime = 0;

	// This code is not used in editor and required only for packaged builds. To avoid linking issues it won't be used with editor builds.
#if !WITH_EDITOR
	{
		// Issue frame event
		pOglViewport->IssueFrameEvent();

		// Wait until GPU finish last frame commands
		const double wtB = FPlatformTime::Seconds();
		pOglViewport->WaitForFrameEventCompletion();
		const double wtA = FPlatformTime::Seconds();

		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT EVENT bef: %lf"), wtB);
		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT EVENT aft: %lf"), wtA);
		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT EVENT diff: %lf"), wtA - wtB);
	}
#endif

	// Sync all nodes
	exec_BarrierWait();

	// Update swap interval right before swap buffers call
	UpdateSwapInterval(GetSwapInt());

	{
		///////////////////////////////////////////////
		// Swap buffers
		const double wtB = FPlatformTime::Seconds();
		::SwapBuffers(pOglViewport->GetGLContext()->DeviceContext);
		const double wtA = FPlatformTime::Seconds();

		lastSwapBuffersTime = wtA;

		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT SWAP bef: %lf"), wtB);
		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT SWAP aft: %lf"), wtA);
		UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WAIT SWAP diff: %lf"), wtA - wtB);
	}
}

void FDisplayClusterDeviceMonoscopicOpenGL::internal_SwapBuffersPolicyNvSwapSync(FOpenGLViewport* pOglViewport)
{
	internal_SwapBuffersPolicySoftSwapSync(pOglViewport);
}
#endif // PLATFORM_WINDOWS
