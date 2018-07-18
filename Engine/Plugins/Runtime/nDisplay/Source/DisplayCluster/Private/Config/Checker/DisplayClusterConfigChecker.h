// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/Parser/IDisplayClusterConfigParserListener.h"


/**
 * Helper class to analyze if config data is correct
 */
class FDisplayClusterConfigChecker
	: protected IDisplayClusterConfigParserListener
{
public:
	FDisplayClusterConfigChecker();
	~FDisplayClusterConfigChecker();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterConfigParserListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void AddClusterNode (const FDisplayClusterConfigClusterNode& node) override;
	virtual void AddScreen      (const FDisplayClusterConfigScreen& screen) override;
	virtual void AddViewport    (const FDisplayClusterConfigViewport& viewport) override;
	virtual void AddCamera      (const FDisplayClusterConfigCamera& camera) override;
	virtual void AddSceneNode   (const FDisplayClusterConfigSceneNode& actor) override;
	virtual void AddGeneral     (const FDisplayClusterConfigGeneral& general) override;
	virtual void AddRender      (const FDisplayClusterConfigRender& render) override;
	virtual void AddStereo      (const FDisplayClusterConfigStereo& stereo) override;
	virtual void AddDebug       (const FDisplayClusterConfigDebug& debug) override;
	virtual void AddInput       (const FDisplayClusterConfigInput& input) override;
	virtual void AddCustom      (const FDisplayClusterConfigCustom& custom) override;
};
