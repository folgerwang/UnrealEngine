// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoD3D11.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "Misc/DisplayClusterLog.h"
#include "D3D11Viewport.h"
#include "RHI.h"
#include "RHICommandList.h"


FDisplayClusterDeviceQuadBufferStereoD3D11::FDisplayClusterDeviceQuadBufferStereoD3D11() :
	FDisplayClusterDeviceQuadBufferStereoBase()
{
	dxgi_present_parameters = { 0, nullptr, nullptr, nullptr };
}

FDisplayClusterDeviceQuadBufferStereoD3D11::~FDisplayClusterDeviceQuadBufferStereoD3D11()
{
}

bool FDisplayClusterDeviceQuadBufferStereoD3D11::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	// get backbuffer
	FD3D11Viewport* viewport = static_cast<FD3D11Viewport*>(MainViewport->GetViewportRHI().GetReference());

#if !WITH_EDITOR
	// Issue frame event
	viewport->IssueFrameEvent();
	// Wait until GPU finish last frame commands
	viewport->WaitForFrameEventCompletion();
#endif

	// Sync all nodes
	exec_BarrierWait();

	// present
	if (viewport)
	{
		IDXGISwapChain1* swapchain1 = (IDXGISwapChain1*)viewport->GetSwapChain();
		swapchain1->Present1(GetSwapInt(), 0, &dxgi_present_parameters);
	}
	
	return false;
}

void FDisplayClusterDeviceQuadBufferStereoD3D11::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture, FVector2D WindowSize) const
{
	check(IsInRenderingThread());

	//calculate sub regions to copy
	const int halfSizeX = BackBuffSize.X / 2;

	FResolveParams copyParamsLeft;
	copyParamsLeft.DestArrayIndex = 0;
	copyParamsLeft.SourceArrayIndex = 0;
	copyParamsLeft.Rect.X1 = 0;
	copyParamsLeft.Rect.Y1 = 0;
	copyParamsLeft.Rect.X2 = halfSizeX;
	copyParamsLeft.Rect.Y2 = BackBuffSize.Y;
	copyParamsLeft.DestRect.X1 = 0;
	copyParamsLeft.DestRect.Y1 = 0;
	copyParamsLeft.DestRect.X2 = halfSizeX;
	copyParamsLeft.DestRect.Y2 = BackBuffSize.Y;

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("CopyToResolveTarget [L]: [%d,%d - %d,%d] -> [%d,%d - %d,%d]"),
		copyParamsLeft.Rect.X1, copyParamsLeft.Rect.Y1, copyParamsLeft.Rect.X2, copyParamsLeft.Rect.Y2,
		copyParamsLeft.DestRect.X1, copyParamsLeft.DestRect.Y1, copyParamsLeft.DestRect.X2, copyParamsLeft.DestRect.Y2);

	RHICmdList.CopyToResolveTarget(SrcTexture, BackBuffer, copyParamsLeft);

	FResolveParams copyParamsRight;
	copyParamsRight.DestArrayIndex = 1;
	copyParamsRight.SourceArrayIndex = 0;

	copyParamsRight.Rect = copyParamsLeft.Rect;
	copyParamsRight.Rect.X1 = halfSizeX;
	copyParamsRight.Rect.X2 = BackBuffSize.X;

	copyParamsRight.DestRect = copyParamsLeft.DestRect;

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("CopyToResolveTarget [R]: [%d,%d - %d,%d] -> [%d,%d - %d,%d]"),
		copyParamsRight.Rect.X1, copyParamsRight.Rect.Y1, copyParamsRight.Rect.X2, copyParamsRight.Rect.Y2,
		copyParamsRight.DestRect.X1, copyParamsRight.DestRect.Y1, copyParamsRight.DestRect.X2, copyParamsRight.DestRect.Y2);

	RHICmdList.CopyToResolveTarget(SrcTexture, BackBuffer, copyParamsRight);
}
