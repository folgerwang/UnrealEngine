// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/DisplayClusterNativePresentHandler.h"

#include "Misc/DisplayClusterLog.h"


FDisplayClusterNativePresentHandler::FDisplayClusterNativePresentHandler()
	: FDisplayClusterDeviceBase(1)
{
}

FDisplayClusterNativePresentHandler::~FDisplayClusterNativePresentHandler()
{
}


bool FDisplayClusterNativePresentHandler::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	exec_BarrierWait();
	InOutSyncInterval = 1;

	return true;
}
