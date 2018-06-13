// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceSideBySide.h"


FDisplayClusterDeviceSideBySide::FDisplayClusterDeviceSideBySide()
{
}

FDisplayClusterDeviceSideBySide::~FDisplayClusterDeviceSideBySide()
{
}


void FDisplayClusterDeviceSideBySide::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{	
	FDisplayClusterDeviceBase::AdjustViewRect(StereoPass, X, Y, SizeX, SizeY);
				
	SizeX /= 2;
	if (StereoPass == EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		X += SizeX;
	}
}

bool FDisplayClusterDeviceSideBySide::Present(int32& InOutSyncInterval)
{
	// Wait for swap sync
	WaitForBufferSwapSync(InOutSyncInterval);

	return true;
}
