// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Cluster/Controller/DisplayClusterClusterNodeCtrlBase.h"
#include "DisplayClusterGlobals.h"

#include "Config/DisplayClusterConfigTypes.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "Game/IPDisplayClusterGameManager.h"
#include "Render/IPDisplayClusterRenderManager.h"
#include "Render/Devices/DisplayClusterViewportArea.h"



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

	IPDisplayClusterRenderManager* const RenderMgr = GDisplayCluster->GetPrivateRenderMgr();
	if (RenderMgr)
	{
		const IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
		if (!ConfigMgr)
		{
			return false;
		}

		FDisplayClusterConfigStereo  StereoCfg = ConfigMgr->GetConfigStereo();
		FDisplayClusterConfigGeneral GeneralCfg = ConfigMgr->GetConfigGeneral();

		FDisplayClusterConfigClusterNode LocalClusterNode;
		DisplayClusterHelpers::config::GetLocalClusterNode(LocalClusterNode);

		// Configure the device
		TArray<FDisplayClusterConfigViewport> LocalViewports = DisplayClusterHelpers::config::GetLocalViewports();
		if (LocalViewports.Num() == 0)
		{
			UE_LOG(LogDisplayClusterRender, Error, TEXT("No viewports found for this current node"));
			return false;
		}

		for (const FDisplayClusterConfigViewport& Viewport : LocalViewports)
		{
			RenderMgr->AddViewport(Viewport.Id, GDisplayCluster->GetPrivateGameMgr());
		}

		RenderMgr->SetEyesSwap(LocalClusterNode.EyeSwap);
		RenderMgr->SetInterpupillaryDistance(StereoCfg.EyeDist);
		RenderMgr->SetSwapSyncPolicy((EDisplayClusterSwapSyncPolicy)GeneralCfg.SwapSyncPolicy);
	}
	else
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Render manager not found. Stereo initialization skipped."));
	}

	return FDisplayClusterNodeCtrlBase::InitializeStereo();
}
