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
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found cluster node: id=%s, addr=%s, role=%s, port_cs=%d, port_ss=%d, port_ce=%d"),
	//	*node.Id, *node.Addr, node.IsMaster ? TEXT("master") : TEXT("slave"), node.Port_CS, node.Port_SS, node.Port_CE);
}

void FDisplayClusterConfigChecker::AddScreen(const FDisplayClusterConfigScreen& InCfgScreen)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found screen: id=%s, parent=%s, loc=%s, rot=%s, size=%s"),
	//	*screen.Id, *screen.ParentId, *screen.Loc.ToString(), *screen.Rot.ToString(), *screen.Size.ToString());
}

void FDisplayClusterConfigChecker::AddViewport(const FDisplayClusterConfigViewport& InCfgViewport)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found viewport: id=%s, loc=%s, size=%s"),
	//	*viewport.Id, *viewport.Loc.ToString(), *viewport.Size.ToString());
}

void FDisplayClusterConfigChecker::AddCamera(const FDisplayClusterConfigCamera& InCfgCamera)
{
}

void FDisplayClusterConfigChecker::AddSceneNode(const FDisplayClusterConfigSceneNode& InCfgSNode)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found scene node: id=%s, parent=%s, type=%d, loc=%s, rot=%s"),
	//	*actor.Id, *actor.ParentId, static_cast<int>(actor.Type), *actor.Loc.ToString(), *actor.Rot.ToString());
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

void FDisplayClusterConfigChecker::AddCustom(const FDisplayClusterConfigCustom& InCfgCustom)
{
}

