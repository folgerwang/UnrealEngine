// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterDeviceQuadBufferStereoBase.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "dxgi1_2.h"


/**
 * Frame sequenced active stereo (DirectX 12)
 */
class FDisplayClusterDeviceQuadBufferStereoD3D12 : public FDisplayClusterDeviceQuadBufferStereoBase
{
public:
	FDisplayClusterDeviceQuadBufferStereoD3D12();
	virtual ~FDisplayClusterDeviceQuadBufferStereoD3D12();

protected:
	virtual bool ShouldUseSeparateRenderTarget() const override;
	virtual void SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy);
	virtual bool Present(int32& InOutSyncInterval) override;

	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture, FVector2D WindowSize) const override;

	//void CopySubregions(bool stereo, FD3D11DeviceContext* d3dContext, ID3D11Texture2D* rttD3DTexture, ID3D11RenderTargetView* leftRTV, ID3D11RenderTargetView* rightRTV);
	DXGI_PRESENT_PARAMETERS dxgi_present_parameters;

private:
	//void ClearTargets(FD3D12DeviceContext* d3dContext, ID3D11RenderTargetView* leftRTV, ID3D11RenderTargetView* rightRTV);
};
