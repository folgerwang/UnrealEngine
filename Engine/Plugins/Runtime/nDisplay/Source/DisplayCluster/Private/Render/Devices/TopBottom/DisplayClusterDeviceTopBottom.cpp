// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/TopBottom/DisplayClusterDeviceTopBottom.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceTopBottom::FDisplayClusterDeviceTopBottom()
	: FDisplayClusterDeviceStereoBase()
{
}

FDisplayClusterDeviceTopBottom::~FDisplayClusterDeviceTopBottom()
{
}


void FDisplayClusterDeviceTopBottom::AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const int CurrentViewportIndex = DecodeViewportIndex(StereoPassType);
	const EStereoscopicPass DecodedPass = DecodeStereoscopicPass(StereoPassType);
	const FDisplayClusterViewportArea ViewportArea = RenderViewports[CurrentViewportIndex].GetViewportArea();

	if (DecodedPass == EStereoscopicPass::eSSP_LEFT_EYE)
	{
		Y = ViewportArea.GetLocation().Y / 2;
	}
	else if (DecodedPass == EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		Y = SizeY / 2 + ViewportArea.GetLocation().Y / 2;
	}

	X = ViewportArea.GetLocation().X;
	SizeX = ViewportArea.GetSize().X;
	SizeY = ViewportArea.GetSize().Y / 2;

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Adjusted view rect: ViewportIdx=%d, StereoPass=%d, [%d,%d - %d,%d]"), CurrentViewportIndex, int(DecodedPass), X, SizeX, Y, SizeY);
}


bool FDisplayClusterDeviceTopBottom::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	// Wait for swap sync
	WaitForBufferSwapSync(InOutSyncInterval);

	return true;
}
