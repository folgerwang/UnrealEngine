// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterDeviceQuadBufferStereoBase.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "dxgi1_2.h"


/**
 * Frame sequenced active stereo (DirectX 11)
 */
class FDisplayClusterDeviceQuadBufferStereoD3D11 : public FDisplayClusterDeviceQuadBufferStereoBase
{
public:
	FDisplayClusterDeviceQuadBufferStereoD3D11();
	virtual ~FDisplayClusterDeviceQuadBufferStereoD3D11();

protected:
	virtual bool ShouldUseSeparateRenderTarget() const override;
	virtual void SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy);
	virtual bool Present(int32& InOutSyncInterval) override;

	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture, FVector2D WindowSize) const override;

private:
	DXGI_PRESENT_PARAMETERS dxgi_present_parameters;
};
