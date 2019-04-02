// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/DisplayClusterConfigTypes.h"


/**
 * Interface for parser listener. Notifies about entities found in a config file.
 */
class IDisplayClusterConfigParserListener
{
public:
	virtual ~IDisplayClusterConfigParserListener()
	{ }

	virtual void AddInfo(const FDisplayClusterConfigInfo& InCfgInfo) = 0;
	virtual void AddClusterNode(const FDisplayClusterConfigClusterNode& InCfgCNode) = 0;
	virtual void AddWindow(const FDisplayClusterConfigWindow& InCfgWindow) = 0;
	virtual void AddScreen(const FDisplayClusterConfigScreen& InCfgScreen) = 0;
	virtual void AddViewport(const FDisplayClusterConfigViewport& InCfgViewport) = 0;
	virtual void AddCamera(const FDisplayClusterConfigCamera& InCfgCamera) = 0;
	virtual void AddSceneNode(const FDisplayClusterConfigSceneNode& InCfgSNode) = 0;
	virtual void AddGeneral(const FDisplayClusterConfigGeneral& InCfgGeneral) = 0;
	virtual void AddRender(const FDisplayClusterConfigRender& InCfgRender) = 0;
	virtual void AddStereo(const FDisplayClusterConfigStereo& InCfgStereo) = 0;
	virtual void AddNetwork(const FDisplayClusterConfigNetwork& InCfgNetwork) = 0;
	virtual void AddDebug(const FDisplayClusterConfigDebug& InCfgDebug) = 0;
	virtual void AddInput(const FDisplayClusterConfigInput& InCfgInput) = 0;
	virtual void AddInputSetup(const FDisplayClusterConfigInputSetup& input) = 0;
	virtual void AddCustom(const FDisplayClusterConfigCustom& InCfgCustom) = 0;
};
