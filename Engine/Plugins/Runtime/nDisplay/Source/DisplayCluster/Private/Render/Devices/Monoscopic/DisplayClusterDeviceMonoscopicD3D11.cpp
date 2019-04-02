// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/Monoscopic/DisplayClusterDeviceMonoscopicD3D11.h"

#include "Misc/DisplayClusterLog.h"

#include "D3D11Viewport.h"


FDisplayClusterDeviceMonoscopicD3D11::FDisplayClusterDeviceMonoscopicD3D11()
	: FDisplayClusterDeviceMonoscopicBase()
	, dxgi_present_parameters { 0, nullptr, nullptr, nullptr }
{
}

FDisplayClusterDeviceMonoscopicD3D11::~FDisplayClusterDeviceMonoscopicD3D11()
{
}

bool FDisplayClusterDeviceMonoscopicD3D11::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	FD3D11Viewport* Viewport = static_cast<FD3D11Viewport*>(MainViewport->GetViewportRHI().GetReference());

#if !WITH_EDITOR
	// Issue frame event
	Viewport->IssueFrameEvent();
	// Wait until GPU finish last frame commands
	Viewport->WaitForFrameEventCompletion();
#endif

	// Sync all nodes
	exec_BarrierWait();
	
	IDXGISwapChain* SwapChain = (IDXGISwapChain*)Viewport->GetSwapChain();
	SwapChain->Present(GetSwapInt(), 0);

	return false;
}
