// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceMonoscopicD3D12.h"

#include "D3D12Viewport.h"
#include "D3D12Resources.h"


FDisplayClusterDeviceMonoscopicD3D12::FDisplayClusterDeviceMonoscopicD3D12():
	FDisplayClusterDeviceQuadBufferStereoD3D12()
{

}

FDisplayClusterDeviceMonoscopicD3D12::~FDisplayClusterDeviceMonoscopicD3D12()
{
	
}

bool FDisplayClusterDeviceMonoscopicD3D12::Present(int32& InOutSyncInterval)
{
	FD3D12Viewport* viewport = static_cast<FD3D12Viewport*>(CurrentViewport->GetViewportRHI().GetReference());

#if !WITH_EDITOR
	// Issue frame event
	viewport->IssueFrameEvent();
	// Wait until GPU finish last frame commands
	viewport->WaitForFrameEventCompletion();
#endif

	// Sync all nodes
	exec_BarrierWait();
	
	IDXGISwapChain1* swapchain1 = (IDXGISwapChain1*)viewport->GetSwapChain();
	swapchain1->Present(GetSwapInt(), 0);

	return false;
}
