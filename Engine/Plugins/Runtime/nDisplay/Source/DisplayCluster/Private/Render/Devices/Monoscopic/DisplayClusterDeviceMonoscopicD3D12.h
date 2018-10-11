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

public:
	virtual bool ShouldUseSeparateRenderTarget() const override
	{ return false; }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IStereoRendering
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual int32 GetDesiredNumberOfViews(bool bStereoRequested) const override
	{ return 1; }

protected:
	virtual bool Present(int32& InOutSyncInterval) override;
};
