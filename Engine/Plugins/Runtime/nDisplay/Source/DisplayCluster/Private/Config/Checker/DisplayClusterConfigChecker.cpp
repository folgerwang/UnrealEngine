// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Config/Checker/DisplayClusterConfigChecker.h"
#include "Misc/DisplayClusterLog.h"


FDisplayClusterConfigChecker::FDisplayClusterConfigChecker()
{
	UE_LOG(LogDisplayClusterConfig, Verbose, TEXT("FDisplayClusterConfigManager .dtor"));
}

FDisplayClusterConfigChecker::~FDisplayClusterConfigChecker()
{
	UE_LOG(LogDisplayClusterConfig, Verbose, TEXT("FDisplayClusterConfigManager .dtor"));
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterConfigParserListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterConfigChecker::AddClusterNode(const FDisplayClusterConfigClusterNode& InCfgCNode)
{
}

void FDisplayClusterConfigChecker::AddScreen(const FDisplayClusterConfigScreen& InCfgScreen)
{
}

void FDisplayClusterConfigChecker::AddViewport(const FDisplayClusterConfigViewport& InCfgViewport)
{
}

void FDisplayClusterConfigChecker::AddCamera(const FDisplayClusterConfigCamera& InCfgCamera)
{
}

void FDisplayClusterConfigChecker::AddSceneNode(const FDisplayClusterConfigSceneNode& InCfgSNode)
{
}

void FDisplayClusterConfigChecker::AddGeneral(const FDisplayClusterConfigGeneral& InCfgGeneral)
{
}

void FDisplayClusterConfigChecker::AddRender(const FDisplayClusterConfigRender& InCfgRender)
{
}

void FDisplayClusterConfigChecker::AddStereo(const FDisplayClusterConfigStereo& InCfgStereo)
{
}

void FDisplayClusterConfigChecker::AddDebug(const FDisplayClusterConfigDebug& InCfgDebug)
{
}

void FDisplayClusterConfigChecker::AddInput(const FDisplayClusterConfigInput& InCfgInput)
{
}

void FDisplayClusterConfigChecker::AddInputSetup(const FDisplayClusterConfigInputSetup& InCfgInputSetup)
{
}

void FDisplayClusterConfigChecker::AddCustom(const FDisplayClusterConfigCustom& InCfgCustom)
{
}

