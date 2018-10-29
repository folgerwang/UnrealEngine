// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceQuadBufferStereoOpenGL.h"

#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceQuadBufferStereoOpenGL::FDisplayClusterDeviceQuadBufferStereoOpenGL() :
	FDisplayClusterDeviceQuadBufferStereoBase()
{
#if PLATFORM_WINDOWS
	DisplayClusterInitCapabilitiesForGL();
#endif
}

FDisplayClusterDeviceQuadBufferStereoOpenGL::~FDisplayClusterDeviceQuadBufferStereoOpenGL()
{
}

void FDisplayClusterDeviceQuadBufferStereoOpenGL::SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy)
{
	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Swap sync policy: %d"), (int)policy);

	switch (policy)
	{
		// Policies below are supported by all child implementations
		case EDisplayClusterSwapSyncPolicy::SoftSwapSync:
		// falls through
		case EDisplayClusterSwapSyncPolicy::NvSwapSync:
		{
			SwapSyncPolicy = policy;
			break;
		}

		default:
			// Forward the policy type to the upper level
			FDisplayClusterDeviceBase::SetSwapSyncPolicy(policy);
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Windows implementation
//////////////////////////////////////////////////////////////////////////////////////////////
#if PLATFORM_WINDOWS
bool FDisplayClusterDeviceQuadBufferStereoOpenGL::Present(int32& InOutSyncInterval)
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

	glDrawBuffer(GL_BACK_LEFT);
	glBlitFramebuffer(
		0, 0, halfSizeX, BackBuffSize.Y,
		dstX1, dstY1, dstX2, dstY2,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);

	glDrawBuffer(GL_BACK_RIGHT);
	glBlitFramebuffer(
		halfSizeX, 0, BackBuffSize.X, BackBuffSize.Y,
		dstX1, dstY1, dstX2, dstY2,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);

	// Perform buffers swap logic
	SwapBuffers(pOglViewport, InOutSyncInterval);
	REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP();

	return false;
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////////
// Linux implementation
//////////////////////////////////////////////////////////////////////////////////////////////
#if PLATFORM_LINUX
//@todo: Implementation for Linux
bool FDisplayClusterDeviceQuadBufferStereoOpenGL::Present(int32& InOutSyncInterval)
{
	// Forward to default implementation (should be a black screen)
	return FDisplayClusterDeviceBase::Present(InOutSyncInterval);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterDeviceBaseComplex
//////////////////////////////////////////////////////////////////////////////////////////////
//@todo: this should be combined somehow with FDisplayClusterDeviceBase::WaitForBufferSwapSync. It seems
//       they both have the same purpose but there is a GL viewport.
void FDisplayClusterDeviceQuadBufferStereoOpenGL::SwapBuffers(FOpenGLViewport* pOglViewport, int32& InOutSyncInterval)
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


#if PLATFORM_WINDOWS
void FDisplayClusterDeviceQuadBufferStereoOpenGL::internal_SwapBuffersPolicyNone(FOpenGLViewport* pOglViewport)
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

void FDisplayClusterDeviceQuadBufferStereoOpenGL::internal_SwapBuffersPolicySoftSwapSync(FOpenGLViewport* pOglViewport)
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

void FDisplayClusterDeviceQuadBufferStereoOpenGL::internal_SwapBuffersPolicyNvSwapSync(FOpenGLViewport* pOglViewport)
{
	// Use barrier once during NV barriers initialization
	if (bNvSwapInitialized == false)
	{
		// Use render barrier to guaranty that all nv barriers are initialized simultaneously
		exec_BarrierWait();
		bNvSwapInitialized = InitializeNvidiaSwapLock();
	}

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
#endif // PLATFORM_WINDOWS

#if PLATFORM_LINUX
void FDisplayClusterDeviceQuadBufferStereoOpenGL::internal_SwapBuffersPolicyNone(FOpenGLViewport* pOglViewport)
{
	//@todo: Implementation for Linux
}

void FDisplayClusterDeviceQuadBufferStereoOpenGL::internal_SwapBuffersPolicySoftSwapSync(FOpenGLViewport* pOglViewport)
{
	//@todo: Implementation for Linux
}

void FDisplayClusterDeviceQuadBufferStereoOpenGL::internal_SwapBuffersPolicyNvSwapSync(FOpenGLViewport* pOglViewport)
{
	//@todo: Implementation for Linux
}
#endif // PLATFORM_LINUX

void FDisplayClusterDeviceQuadBufferStereoOpenGL::UpdateSwapInterval(int32 swapInt) const
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

	If <interval> is set to a value of 0, buffer swaps are not synchron-
	ized to a video frame.  The <interval> value is silently clamped to
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
bool FDisplayClusterDeviceQuadBufferStereoOpenGL::InitializeNvidiaSwapLock()
{
	if (bNvSwapInitialized)
	{
		return true;
	}

	if (!DisplayCluster_wglJoinSwapGroupNV_ProcAddress || !DisplayCluster_wglBindSwapBarrierNV_ProcAddress)
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Group/Barrier functions not available"));
		return false;
	}

	if (!CurrentViewport)
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Viewport RHI hasn't been initialized yet"))
			return false;
	}

	FOpenGLViewport* pOglViewport = static_cast<FOpenGLViewport*>(CurrentViewport->GetViewportRHI().GetReference());
	check(pOglViewport);
	FPlatformOpenGLContext* const pContext = pOglViewport->GetGLContext();
	check(pContext && pContext->DeviceContext);

	GLuint maxGroups = 0;
	GLuint maxBarriers = 0;

	if (!DisplayCluster_wglQueryMaxSwapGroupsNV_ProcAddress(pContext->DeviceContext, &maxGroups, &maxBarriers))
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Couldn't query gr/br limits: %d"), glGetError());
		return false;
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("max_groups=%d max_barriers=%d"), (int)maxGroups, (int)maxBarriers);

	if (!(maxGroups > 0 && maxBarriers > 0))
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("There are no available groups or barriers"));
		return false;
	}

	if (!DisplayCluster_wglJoinSwapGroupNV_ProcAddress(pContext->DeviceContext, 1))
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Couldn't join swap group: %d"), glGetError());
		return false;
	}
	else
	{
		UE_LOG(LogDisplayClusterRender, Log, TEXT("Successfully joined the swap group: 1"));
	}

	if (!DisplayCluster_wglBindSwapBarrierNV_ProcAddress(1, 1))
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Couldn't bind to swap barrier: %d"), glGetError());
		return false;
	}
	else
	{
		UE_LOG(LogDisplayClusterRender, Log, TEXT("Successfully binded to the swap barrier: 1"));
	}

	return true;
}
#elif PLATFORM_LINUX
bool FDisplayClusterDeviceQuadBufferStereoOpenGL::InitializeNvidiaSwapLock()
{
	//@todo: Implementation for Linux
	return false;
}
#endif
