// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/DisplayClusterDeviceMonoscopicBase.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterDeviceMonoscopicBase::FDisplayClusterDeviceMonoscopicBase()
	: FDisplayClusterDeviceBase(1)
{
}

FDisplayClusterDeviceMonoscopicBase::~FDisplayClusterDeviceMonoscopicBase()
{
}

void FDisplayClusterDeviceMonoscopicBase::AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const int CurrentViewportIndex = DecodeViewportIndex(StereoPassType);
	const EStereoscopicPass DecodedPass = DecodeStereoscopicPass(StereoPassType);
	const FDisplayClusterViewportArea ViewportArea = RenderViewports[CurrentViewportIndex].GetViewportArea();

	X = ViewportArea.GetLocation().X;
	SizeX = ViewportArea.GetSize().X;

	Y = ViewportArea.GetLocation().Y;
	SizeY = ViewportArea.GetSize().Y;

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Adjusted view rect: ViewportIdx=%d, StereoPass=%d, [%d,%d - %d,%d]"), CurrentViewportIndex, int(DecodedPass), X, SizeX, Y, SizeY);
}
