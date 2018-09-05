// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterNativePresentHandler.h"


FDisplayClusterNativePresentHandler::FDisplayClusterNativePresentHandler()
{
}

FDisplayClusterNativePresentHandler::~FDisplayClusterNativePresentHandler()
{
}


bool FDisplayClusterNativePresentHandler::Present(int32& InOutSyncInterval)
{
	exec_BarrierWait();
	InOutSyncInterval = 1;

	return true;
}
