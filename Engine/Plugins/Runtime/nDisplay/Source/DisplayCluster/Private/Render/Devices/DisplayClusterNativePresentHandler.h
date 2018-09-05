// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Devices/DisplayClusterDeviceBase.h"


/**
 * Present stub to allow to sycnhronize a cluster with native rendering pipeline (no nDisplay stereo devices used)
 */
class FDisplayClusterNativePresentHandler : public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterNativePresentHandler();
	virtual ~FDisplayClusterNativePresentHandler();

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// FRHICustomPresent
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Present(int32& InOutSyncInterval) override;	
};
