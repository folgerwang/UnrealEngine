// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceDebug.h"


FDisplayClusterDeviceDebug::FDisplayClusterDeviceDebug()
{
}

FDisplayClusterDeviceDebug::~FDisplayClusterDeviceDebug()
{
}


void FDisplayClusterDeviceDebug::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const int rHeight = SizeY / 4;

	if (StereoPass == EStereoscopicPass::eSSP_LEFT_EYE)
	{
		SizeY -= rHeight;
	}
	else
	{
		Y = SizeY - rHeight;
		SizeY = rHeight;
	}
}
