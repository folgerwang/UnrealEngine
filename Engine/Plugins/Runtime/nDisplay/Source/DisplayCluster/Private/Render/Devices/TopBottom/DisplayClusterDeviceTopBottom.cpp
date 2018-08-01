// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceTopBottom.h"


FDisplayClusterDeviceTopBottom::FDisplayClusterDeviceTopBottom()
{
}

FDisplayClusterDeviceTopBottom::~FDisplayClusterDeviceTopBottom()
{
}


void FDisplayClusterDeviceTopBottom::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeY /= 2;
	if (StereoPass == EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		Y = SizeY;
	}
}


bool FDisplayClusterDeviceTopBottom::Present(int32& InOutSyncInterval)
{
	// Wait for swap sync
	WaitForBufferSwapSync(InOutSyncInterval);

	return true;
}
