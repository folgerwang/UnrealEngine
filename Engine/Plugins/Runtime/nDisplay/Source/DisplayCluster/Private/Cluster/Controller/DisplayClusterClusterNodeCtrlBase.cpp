// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterClusterNodeCtrlBase.h"
#include "DisplayClusterGlobals.h"

#include "Config/DisplayClusterConfigTypes.h"
#include "Misc/DisplayClusterLog.h"

#include "IPDisplayCluster.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Render/IPDisplayClusterRenderManager.h"
#include "Render/IDisplayClusterStereoDevice.h"



FDisplayClusterClusterNodeCtrlBase::FDisplayClusterClusterNodeCtrlBase(const FString& ctrlName, const FString& nodeName) :
	FDisplayClusterNodeCtrlBase(ctrlName, nodeName)
{

}

FDisplayClusterClusterNodeCtrlBase::~FDisplayClusterClusterNodeCtrlBase()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterNodeController
//////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterNodeCtrlBase
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterClusterNodeCtrlBase::InitializeStereo()
{
	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	FDisplayClusterConfigViewport ViewportCfg;
	if (!GDisplayCluster->GetPrivateConfigMgr()->GetLocalViewport(ViewportCfg))
	{
		UE_LOG(LogDisplayClusterRender, Error, TEXT("Viewport config not found"));
		return false;
	}

	//@todo: Move this logic to the render manager
	IDisplayClusterStereoDevice* const StereoDevice = GDisplayCluster->GetPrivateRenderMgr()->GetStereoDevice();
	if (StereoDevice)
	{
		
		FDisplayClusterConfigStereo  StereoCfg  = GDisplayCluster->GetPrivateConfigMgr()->GetConfigStereo();
		FDisplayClusterConfigGeneral GeneralCfg = GDisplayCluster->GetPrivateConfigMgr()->GetConfigGeneral();
		//FDisplayClusterConfigRender  RenderCfg  = GDisplayCluster->GetPrivateConfigMgr()->GetConfigRender();

		// Configure the device
		StereoDevice->SetViewportArea(ViewportCfg.Loc, ViewportCfg.Size);
		StereoDevice->SetEyesSwap(StereoCfg.EyeSwap);
		StereoDevice->SetInterpupillaryDistance(StereoCfg.EyeDist);
		StereoDevice->SetOutputFlip(ViewportCfg.FlipHorizontal, ViewportCfg.FlipVertical);
		StereoDevice->SetSwapSyncPolicy((EDisplayClusterSwapSyncPolicy)GeneralCfg.SwapSyncPolicy);
	}
	else
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Stereo device not found. Stereo initialization skipped."));
	}

	return FDisplayClusterNodeCtrlBase::InitializeStereo();
}
