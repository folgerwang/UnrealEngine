// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Devices/DisplayClusterDeviceStereoBase.h"


/**
 * Abstract frame sequenced active stereo device
 */
class FDisplayClusterDeviceQuadBufferStereoBase
	: public FDisplayClusterDeviceStereoBase
{
public:
	FDisplayClusterDeviceQuadBufferStereoBase();
	virtual ~FDisplayClusterDeviceQuadBufferStereoBase();

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;	
};
