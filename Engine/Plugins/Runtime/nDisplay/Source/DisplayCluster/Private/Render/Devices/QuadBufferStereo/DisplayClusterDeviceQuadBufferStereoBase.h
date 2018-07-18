// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Devices/DisplayClusterDeviceBase.h"


/**
 * Abstract frame sequenced active stereo device
 */
class FDisplayClusterDeviceQuadBufferStereoBase : public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceQuadBufferStereoBase();
	virtual ~FDisplayClusterDeviceQuadBufferStereoBase();

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual bool ShouldUseSeparateRenderTarget() const override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;	

protected:
	mutable FCriticalSection InternalsSyncScope;
};
