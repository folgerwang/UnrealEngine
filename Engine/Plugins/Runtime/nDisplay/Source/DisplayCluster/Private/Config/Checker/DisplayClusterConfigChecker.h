// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	virtual void AddClusterNode (const FDisplayClusterConfigClusterNode& InCfgCNode) override;
	virtual void AddScreen      (const FDisplayClusterConfigScreen& InCfgScreen) override;
	virtual void AddViewport    (const FDisplayClusterConfigViewport& InCfgViewport) override;
	virtual void AddCamera      (const FDisplayClusterConfigCamera& InCfgCamera) override;
	virtual void AddSceneNode   (const FDisplayClusterConfigSceneNode& InCfgSNode) override;
	virtual void AddGeneral     (const FDisplayClusterConfigGeneral& InCfgGeneral) override;
	virtual void AddRender      (const FDisplayClusterConfigRender& InCfgRender) override;
	virtual void AddStereo      (const FDisplayClusterConfigStereo& InCfgStereo) override;
	virtual void AddDebug       (const FDisplayClusterConfigDebug& InCfgDebug) override;
	virtual void AddInput       (const FDisplayClusterConfigInput& InCfgInput) override;
	virtual void AddInputSetup  (const FDisplayClusterConfigInputSetup& InCfgInputSetup) override;
	virtual void AddCustom      (const FDisplayClusterConfigCustom& InCfgCustom) override;
};
