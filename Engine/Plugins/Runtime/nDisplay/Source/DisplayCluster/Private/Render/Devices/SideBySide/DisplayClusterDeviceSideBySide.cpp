// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/SidebySide/DisplayClusterDeviceSideBySide.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceSideBySide::FDisplayClusterDeviceSideBySide()
	: FDisplayClusterDeviceStereoBase()
{
}

FDisplayClusterDeviceSideBySide::~FDisplayClusterDeviceSideBySide()
{
}


void FDisplayClusterDeviceSideBySide::AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const int CurrentViewportIndex = DecodeViewportIndex(StereoPassType);
	const EStereoscopicPass DecodedPass = DecodeStereoscopicPass(StereoPassType);
	const FDisplayClusterViewportArea ViewportArea = RenderViewports[CurrentViewportIndex].GetViewportArea();

	if (DecodedPass == EStereoscopicPass::eSSP_LEFT_EYE)
	{
		X = ViewportArea.GetLocation().X / 2;
	}
	else if (DecodedPass == EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		X = SizeX / 2 + ViewportArea.GetLocation().X / 2;
	}

	SizeX = ViewportArea.GetSize().X / 2;
	Y = ViewportArea.GetLocation().Y;
	SizeY = ViewportArea.GetSize().Y;

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Adjusted view rect: ViewportIdx=%d, StereoPass=%d, [%d,%d - %d,%d]"), CurrentViewportIndex, int(DecodedPass), X, SizeX, Y, SizeY);
}

bool FDisplayClusterDeviceSideBySide::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	// Wait for swap sync
	WaitForBufferSwapSync(InOutSyncInterval);

	return true;
}
