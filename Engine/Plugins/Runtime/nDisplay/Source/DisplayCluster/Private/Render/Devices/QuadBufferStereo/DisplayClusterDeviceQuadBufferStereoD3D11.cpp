// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceQuadBufferStereoD3D11.h"
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

bool FDisplayClusterDeviceQuadBufferStereoD3D11::ShouldUseSeparateRenderTarget() const
{
	return true;
}

void FDisplayClusterDeviceQuadBufferStereoD3D11::SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy)
{
	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Swap sync policy: %d"), (int)policy);

	switch (policy)
	{
		case EDisplayClusterSwapSyncPolicy::SoftSwapSync:
			SwapSyncPolicy = policy;
			break;

		default:
			// Forward the policy type to the upper level
			FDisplayClusterDeviceBase::SetSwapSyncPolicy(policy);
			break;
	}
}

bool FDisplayClusterDeviceQuadBufferStereoD3D11::Present(int32& InOutSyncInterval)
{
	// get backbuffer
	FD3D11Viewport* viewport = static_cast<FD3D11Viewport*>(CurrentViewport->GetViewportRHI().GetReference());

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

	RHICmdList.CopyToResolveTarget(SrcTexture, BackBuffer, copyParamsLeft);

	FResolveParams copyParamsRight;
	copyParamsRight.DestArrayIndex = 1;
	copyParamsRight.SourceArrayIndex = 0;

	copyParamsRight.Rect = copyParamsLeft.Rect;
	copyParamsRight.Rect.X1 = halfSizeX;
	copyParamsRight.Rect.X2 = halfSizeX * 2;

	copyParamsRight.DestRect = copyParamsLeft.DestRect;

	RHICmdList.CopyToResolveTarget(SrcTexture, BackBuffer, copyParamsRight);
}
