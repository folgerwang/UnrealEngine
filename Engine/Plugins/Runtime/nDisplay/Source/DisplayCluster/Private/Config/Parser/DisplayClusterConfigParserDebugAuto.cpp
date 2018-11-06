// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigParserDebugAuto.h"

#include "DisplayClusterBuildConfig.h"
#include "DisplayClusterConstants.h"
#include "DisplayClusterStrings.h"
#include "Config/DisplayClusterConfigTypes.h"


FDisplayClusterConfigParserDebugAuto::FDisplayClusterConfigParserDebugAuto(IDisplayClusterConfigParserListener* pListener) :
	FDisplayClusterConfigParser(pListener)
{
}

bool FDisplayClusterConfigParserDebugAuto::ParseFile(const FString& path)
{
#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
	FDisplayClusterConfigClusterNode ClusterNode;
	ClusterNode.Id         = DisplayClusterStrings::misc::DbgStubNodeId;
	ClusterNode.IsMaster   = true;
	ClusterNode.Addr       = TEXT("127.0.0.1");
	ClusterNode.Port_CS    = 41001;
	ClusterNode.Port_SS    = 41002;
	ClusterNode.ScreenId   = TEXT("screen_stub");;
	ClusterNode.ViewportId = TEXT("viewport_stub");
	ClusterNode.SoundEnabled = true;
	ClusterNode.EyeSwap    = false;
	AddClusterNode(ClusterNode);

	const float PixelDensity = 0.6f / 1920.f;

	FDisplayClusterConfigScreen Screen;
	Screen.Id   = ClusterNode.ScreenId;
	Screen.Loc  = FVector(0.7f, 0.f, 0.f);
	Screen.Rot  = FRotator::ZeroRotator;
	Screen.Size = FVector2D(PixelDensity * DisplayClusterConstants::misc::DebugAutoResX, PixelDensity * DisplayClusterConstants::misc::DebugAutoResY);
	AddScreen(Screen);

	FDisplayClusterConfigViewport Viewport;
	Viewport.Id   = ClusterNode.ViewportId;
	Viewport.Loc  = FIntPoint(0, 0);
	Viewport.Size = FIntPoint(DisplayClusterConstants::misc::DebugAutoResX, DisplayClusterConstants::misc::DebugAutoResY);
	AddViewport(Viewport);
	
	FDisplayClusterConfigCamera Camera;
	Camera.Id  = TEXT("camera_stub");
	Camera.Loc = FVector::ZeroVector;
	Camera.Rot = FRotator::ZeroRotator;
	AddCamera(Camera);

	FDisplayClusterConfigGeneral General;
	General.SwapSyncPolicy = 1;
	AddGeneral(General);

	FDisplayClusterConfigStereo Stereo;
	Stereo.EyeDist = 0.064f;
	AddStereo(Stereo);
#endif // DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
		
	return true;
}

