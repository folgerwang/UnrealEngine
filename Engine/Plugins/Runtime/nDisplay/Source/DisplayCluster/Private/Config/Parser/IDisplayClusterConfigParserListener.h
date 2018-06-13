// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/DisplayClusterConfigTypes.h"


/**
 * Interface for parser listener. Notifies about entities found in a config file.
 */
struct IDisplayClusterConfigParserListener
{
public:
	virtual ~IDisplayClusterConfigParserListener()
	{ }

	virtual void AddClusterNode(const FDisplayClusterConfigClusterNode& cnode) = 0;
	virtual void AddScreen(const FDisplayClusterConfigScreen& screen) = 0;
	virtual void AddViewport(const FDisplayClusterConfigViewport& viewport) = 0;
	virtual void AddCamera(const FDisplayClusterConfigCamera& camera) = 0;
	virtual void AddSceneNode(const FDisplayClusterConfigSceneNode& snode) = 0;
	virtual void AddGeneral(const FDisplayClusterConfigGeneral& general) = 0;
	virtual void AddRender(const FDisplayClusterConfigRender& render) = 0;
	virtual void AddStereo(const FDisplayClusterConfigStereo& stereo) = 0;
	virtual void AddDebug(const FDisplayClusterConfigDebug& debug) = 0;
	virtual void AddInput(const FDisplayClusterConfigInput& input) = 0;
	virtual void AddCustom(const FDisplayClusterConfigCustom& custom) = 0;
};
