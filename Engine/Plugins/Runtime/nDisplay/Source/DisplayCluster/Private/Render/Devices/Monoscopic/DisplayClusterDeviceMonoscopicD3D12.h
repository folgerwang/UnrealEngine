// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoD3D12.h"
#include "Render/Devices/DisplayClusterDeviceInternals.h"


/**
 * Monoscopic emulation device (DirectX 12)
 */
class FDisplayClusterDeviceMonoscopicD3D12 : public FDisplayClusterDeviceQuadBufferStereoD3D12
{
public:
	FDisplayClusterDeviceMonoscopicD3D12();
	virtual ~FDisplayClusterDeviceMonoscopicD3D12();

	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return false; }

protected:
	virtual bool Present(int32& InOutSyncInterval) override;
};
