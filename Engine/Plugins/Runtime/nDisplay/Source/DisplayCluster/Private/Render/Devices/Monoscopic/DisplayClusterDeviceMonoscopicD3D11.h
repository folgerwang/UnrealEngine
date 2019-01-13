// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceMonoscopicBase.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"

#include "dxgi1_2.h"


/**
 * Monoscopic render device (DirectX 11)
 */
class FDisplayClusterDeviceMonoscopicD3D11
	: public FDisplayClusterDeviceMonoscopicBase
{
public:
	FDisplayClusterDeviceMonoscopicD3D11();
	virtual ~FDisplayClusterDeviceMonoscopicD3D11();

protected:
	virtual bool Present(int32& InOutSyncInterval) override;

private:
	DXGI_PRESENT_PARAMETERS dxgi_present_parameters;
};
