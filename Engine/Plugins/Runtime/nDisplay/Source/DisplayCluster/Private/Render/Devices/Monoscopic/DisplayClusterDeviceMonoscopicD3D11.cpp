// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceMonoscopicD3D11.h"

#include "D3D11Viewport.h"
#include "D3D11Resources.h"


FDisplayClusterDeviceMonoscopicD3D11::FDisplayClusterDeviceMonoscopicD3D11():
	FDisplayClusterDeviceQuadBufferStereoD3D11()
{

}

FDisplayClusterDeviceMonoscopicD3D11::~FDisplayClusterDeviceMonoscopicD3D11()
{
	
}

bool FDisplayClusterDeviceMonoscopicD3D11::Present(int32& InOutSyncInterval)
{
	FD3D11Viewport* viewport = static_cast<FD3D11Viewport*>(CurrentViewport->GetViewportRHI().GetReference());

#if !WITH_EDITOR
	// Issue frame event
	viewport->IssueFrameEvent();
	// Wait until GPU finish last frame commands
	viewport->WaitForFrameEventCompletion();
#endif

	// Sync all nodes
	exec_BarrierWait();
	
	IDXGISwapChain* swapchain = (IDXGISwapChain*)viewport->GetSwapChain();
	swapchain->Present(GetSwapInt(), 0);

	return false;
}
