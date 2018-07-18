// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterNodeCtrlStandalone.h"

#include "Network/DisplayClusterMessage.h"
#include "Render/IDisplayClusterStereoDevice.h"


FDisplayClusterNodeCtrlStandalone::FDisplayClusterNodeCtrlStandalone(const FString& ctrlName, const FString& nodeName) :
	FDisplayClusterNodeCtrlBase(ctrlName, nodeName)
{
}


FDisplayClusterNodeCtrlStandalone::~FDisplayClusterNodeCtrlStandalone()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterClusterSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterNodeCtrlStandalone::WaitForGameStart()
{
	// Nothing special to do here in standalone mode
}

void FDisplayClusterNodeCtrlStandalone::WaitForFrameStart()
{
	// Nothing special to do here in standalone mode
}

void FDisplayClusterNodeCtrlStandalone::WaitForFrameEnd()
{
	// Nothing special to do here in standalone mode
}

void FDisplayClusterNodeCtrlStandalone::WaitForTickEnd()
{
	// Nothing special to do here in standalone mode
}

void FDisplayClusterNodeCtrlStandalone::GetDeltaTime(float& deltaTime)
{
	// Nothing special to do here in standalone mode
}

void FDisplayClusterNodeCtrlStandalone::GetSyncData(FDisplayClusterMessage::DataType& data)
{
	// Nothing special to do here in standalone mode
}

void FDisplayClusterNodeCtrlStandalone::GetInputData(FDisplayClusterMessage::DataType& data)
{
	// Nothing special to do here in standalone mode
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterSwapSyncProtocol
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterNodeCtrlStandalone::WaitForSwapSync(double* pThreadWaitTime, double* pBarrierWaitTime)
{
	// Nothing special to do here in standalone mode
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterNodeCtrlStandalone::InitializeStereo()
{
	//@todo: initialize stereo for standalone mode

	return FDisplayClusterNodeCtrlBase::InitializeStereo();
}
