// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceQuadBufferStereoBase.h"

#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include <utility>


FDisplayClusterDeviceQuadBufferStereoBase::FDisplayClusterDeviceQuadBufferStereoBase() :
	FDisplayClusterDeviceBase()
{
}

FDisplayClusterDeviceQuadBufferStereoBase::~FDisplayClusterDeviceQuadBufferStereoBase()
{
}

bool FDisplayClusterDeviceQuadBufferStereoBase::NeedReAllocateViewportRenderTarget(const class FViewport& Viewport)
{
	//UE_LOG(LogDisplayClusterRender, Log, TEXT("FDisplayClusterDeviceMonoscopic::NeedReAllocateViewportRenderTarget"));
	check(IsInGameThread());

	const FIntPoint rtSize = Viewport.GetRenderTargetTextureSizeXY();
	uint32 newSizeX = rtSize.X;
	uint32 newSizeY = rtSize.Y;

	// Perform size calculation
	CalculateRenderTargetSize(Viewport, newSizeX, newSizeY);

	// Render target need to be re-allocated if its current size is invalid
	if (newSizeX != rtSize.X || newSizeY != rtSize.Y)
	{
		return true;
	}

	// No need to re-allocate
	return false;
}

void FDisplayClusterDeviceQuadBufferStereoBase::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	InOutSizeX = Viewport.GetSizeXY().X * 2;
	InOutSizeY = Viewport.GetSizeXY().Y;

	check(InOutSizeX > 0 && InOutSizeY > 0);
}


bool FDisplayClusterDeviceQuadBufferStereoBase::ShouldUseSeparateRenderTarget() const
{
	return true;
}

void FDisplayClusterDeviceQuadBufferStereoBase::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const uint32 screentWidth = SizeX;
	FDisplayClusterDeviceBase::AdjustViewRect(StereoPass, X, Y, SizeX, SizeY);

	if (StereoPass == EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		X += screentWidth;
	}
}

/*
//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStereoDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceQuadBufferStereoBase::SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy)
{
	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Swap sync policy: %d"), (int)policy);

	switch (policy)
	{
		// Policies below are supported by all child implementations
	case EDisplayClusterSwapSyncPolicy::SoftSwapSync:
	case EDisplayClusterSwapSyncPolicy::NvSwapSync:
	{
		SwapSyncPolicy = policy;
		break;
	}

	default:
		// Forward the policy type to the upper level
		FDisplayClusterDeviceBase::SetSwapSyncPolicy(policy);
		break;
	}
}
*/
