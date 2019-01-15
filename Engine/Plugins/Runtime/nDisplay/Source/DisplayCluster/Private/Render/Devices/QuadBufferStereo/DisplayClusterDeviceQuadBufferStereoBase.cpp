// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoBase.h"

#include "Misc/DisplayClusterLog.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include <utility>


FDisplayClusterDeviceQuadBufferStereoBase::FDisplayClusterDeviceQuadBufferStereoBase() :
	FDisplayClusterDeviceStereoBase()
{
}

FDisplayClusterDeviceQuadBufferStereoBase::~FDisplayClusterDeviceQuadBufferStereoBase()
{
}

bool FDisplayClusterDeviceQuadBufferStereoBase::NeedReAllocateViewportRenderTarget(const class FViewport& Viewport)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	check(IsInGameThread());

	const FIntPoint rtSize = Viewport.GetRenderTargetTextureSizeXY();
	uint32 newSizeX = rtSize.X;
	uint32 newSizeY = rtSize.Y;

	// Perform size calculation
	CalculateRenderTargetSize(Viewport, newSizeX, newSizeY);

	// Render target need to be re-allocated if its current size is invalid
	bool Result = false;
	if (newSizeX != rtSize.X || newSizeY != rtSize.Y)
	{
		Result = true;;
	}

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Is reallocate viewport render target needed: %d"), Result ? 1 : 0);

	// No need to re-allocate
	return Result;
}

void FDisplayClusterDeviceQuadBufferStereoBase::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	InOutSizeX = Viewport.GetSizeXY().X * 2;
	InOutSizeY = Viewport.GetSizeXY().Y;

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Render target size: [%d x %d]"), InOutSizeX, InOutSizeY);

	check(InOutSizeX > 0 && InOutSizeY > 0);
}

void FDisplayClusterDeviceQuadBufferStereoBase::AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const EStereoscopicPass DecodedPass = DecodeStereoscopicPass(StereoPassType);
	const int CurrentViewportIndex = DecodeViewportIndex(StereoPassType);

	FDisplayClusterViewportArea ViewportArea = RenderViewports[CurrentViewportIndex].GetViewportArea();

	X = ViewportArea.GetLocation().X;
	SizeX = ViewportArea.GetSize().X;

	Y = ViewportArea.GetLocation().Y;
	SizeY = ViewportArea.GetSize().Y;

	if (DecodedPass == EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		X += ViewportSize.X;
	}

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Adjusted view rect: ViewportIdx=%d, StereoPass=%d, [%d,%d - %d,%d]"), CurrentViewportIndex, int(DecodedPass), X, SizeX, Y, SizeY);
}
