// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Config/Parser/DisplayClusterConfigParserDebugAuto.h"
#include "Config/DisplayClusterConfigTypes.h"

#include "DisplayClusterBuildConfig.h"
#include "DisplayClusterConstants.h"
#include "DisplayClusterStrings.h"


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
	ClusterNode.WindowId   = TEXT("window_stub");
	ClusterNode.SoundEnabled = true;
	ClusterNode.EyeSwap    = false;
	AddClusterNode(ClusterNode);

	FDisplayClusterConfigWindow Window;
	Window.Id = ClusterNode.WindowId;
	Window.IsFullscreen = true;
	Window.ViewportIds.Add(FString("viewport_stub"));
	Window.WinX = 0;
	Window.WinY = 0;
	Window.ResX = 800;
	Window.ResY = 600;

	FDisplayClusterConfigViewport Viewport;
	Viewport.Id        = Window.ViewportIds[0];
	Viewport.ScreenId  = TEXT("screen_stub");
	Viewport.Loc       = FIntPoint(0, 0);
	Viewport.Size      = FIntPoint(DisplayClusterConstants::misc::DebugAutoResX, DisplayClusterConstants::misc::DebugAutoResY);
	AddViewport(Viewport);

	const float PixelDensity = 0.6f / 1920.f;

	FDisplayClusterConfigScreen Screen;
	Screen.Id   = Viewport.ScreenId;
	Screen.Loc  = FVector(0.7f, 0.f, 0.f);
	Screen.Rot  = FRotator::ZeroRotator;
	Screen.Size = FVector2D(PixelDensity * DisplayClusterConstants::misc::DebugAutoResX, PixelDensity * DisplayClusterConstants::misc::DebugAutoResY);
	AddScreen(Screen);

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

