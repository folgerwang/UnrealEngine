// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceBase.h"


/**
 * Side-by-side passive stereoscopic device
 */
class FDisplayClusterDeviceSideBySide : public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceSideBySide();
	virtual ~FDisplayClusterDeviceSideBySide();

protected:
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
protected:
	
protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRHICustomPresent
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Present(int32& InOutSyncInterval) override;	
};
