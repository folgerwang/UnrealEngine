// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigChecker.h"
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
void FDisplayClusterConfigChecker::AddClusterNode(const FDisplayClusterConfigClusterNode& node)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found cluster node: id=%s, addr=%s, role=%s, port_cs=%d, port_ss=%d, port_ce=%d"),
	//	*node.Id, *node.Addr, node.IsMaster ? TEXT("master") : TEXT("slave"), node.Port_CS, node.Port_SS, node.Port_CE);
}

void FDisplayClusterConfigChecker::AddScreen(const FDisplayClusterConfigScreen& screen)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found screen: id=%s, parent=%s, loc=%s, rot=%s, size=%s"),
	//	*screen.Id, *screen.ParentId, *screen.Loc.ToString(), *screen.Rot.ToString(), *screen.Size.ToString());
}

void FDisplayClusterConfigChecker::AddViewport(const FDisplayClusterConfigViewport& viewport)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found viewport: id=%s, loc=%s, size=%s"),
	//	*viewport.Id, *viewport.Loc.ToString(), *viewport.Size.ToString());
}

void FDisplayClusterConfigChecker::AddCamera(const FDisplayClusterConfigCamera& camera)
{
}

void FDisplayClusterConfigChecker::AddSceneNode(const FDisplayClusterConfigSceneNode& actor)
{
	//UE_LOG(LogDisplayClusterConfig, Log, TEXT("Found scene node: id=%s, parent=%s, type=%d, loc=%s, rot=%s"),
	//	*actor.Id, *actor.ParentId, static_cast<int>(actor.Type), *actor.Loc.ToString(), *actor.Rot.ToString());
}

void FDisplayClusterConfigChecker::AddGeneral(const FDisplayClusterConfigGeneral& general)
{
}

void FDisplayClusterConfigChecker::AddRender(const FDisplayClusterConfigRender& render)
{
}

void FDisplayClusterConfigChecker::AddStereo(const FDisplayClusterConfigStereo& stereo)
{
}

void FDisplayClusterConfigChecker::AddDebug(const FDisplayClusterConfigDebug& debug)
{
}

void FDisplayClusterConfigChecker::AddInput(const FDisplayClusterConfigInput& input)
{
}

void FDisplayClusterConfigChecker::AddCustom(const FDisplayClusterConfigCustom& custom)
{
}

