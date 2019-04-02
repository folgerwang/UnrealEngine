// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/Monoscopic/DisplayClusterDeviceMonoscopicD3D12.h"

#include "Misc/DisplayClusterLog.h"

#include "D3D12Viewport.h"
#include "D3D12Resources.h"


FDisplayClusterDeviceMonoscopicD3D12::FDisplayClusterDeviceMonoscopicD3D12()
	: FDisplayClusterDeviceMonoscopicBase()
	, dxgi_present_parameters{ 0, nullptr, nullptr, nullptr }
{
}

FDisplayClusterDeviceMonoscopicD3D12::~FDisplayClusterDeviceMonoscopicD3D12()
{
}

bool FDisplayClusterDeviceMonoscopicD3D12::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	FD3D12Viewport* Viewport = static_cast<FD3D12Viewport*>(MainViewport->GetViewportRHI().GetReference());

#if !WITH_EDITOR
	// Issue frame event
	Viewport->IssueFrameEvent();
	// Wait until GPU finish last frame commands
	Viewport->WaitForFrameEventCompletion();
#endif

	// Sync all nodes
	exec_BarrierWait();
	
	IDXGISwapChain1* SwapChain1 = (IDXGISwapChain1*)Viewport->GetSwapChain();
	SwapChain1->Present(GetSwapInt(), 0);

	return false;
}
