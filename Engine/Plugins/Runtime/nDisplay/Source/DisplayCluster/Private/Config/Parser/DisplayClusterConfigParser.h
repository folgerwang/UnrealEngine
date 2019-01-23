// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Config/Parser/IDisplayClusterConfigParserListener.h"
#include "Config/DisplayClusterConfigTypes.h"


/**
 * Abstract config parser
 */
class FDisplayClusterConfigParser
	: protected IDisplayClusterConfigParserListener
{
public:
	explicit FDisplayClusterConfigParser(IDisplayClusterConfigParserListener* pListener);
	virtual ~FDisplayClusterConfigParser() = 0;

public:
	// Entry point for file parsing
	virtual bool ParseFile(const FString& path);

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterConfigParserListener
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void AddInfo(const FDisplayClusterConfigInfo& InCfgInfo)                override final;
	virtual void AddClusterNode(const FDisplayClusterConfigClusterNode& InCfgCNode) override final;
	virtual void AddWindow(const FDisplayClusterConfigWindow& InCfgWindow)          override final;
	virtual void AddScreen(const FDisplayClusterConfigScreen& InCfgScreen)          override final;
	virtual void AddViewport(const FDisplayClusterConfigViewport& InCfgViewport)    override final;
	virtual void AddCamera(const FDisplayClusterConfigCamera& InCfgCamera)          override final;
	virtual void AddSceneNode(const FDisplayClusterConfigSceneNode& InCfgSNode)     override final;
	virtual void AddGeneral(const FDisplayClusterConfigGeneral& InCfgGeneral)       override final;
	virtual void AddRender(const FDisplayClusterConfigRender& InCfgRender)          override final;
	virtual void AddStereo(const FDisplayClusterConfigStereo& InCfgStereo)          override final;
	virtual void AddDebug(const FDisplayClusterConfigDebug& InCfgDebug)             override final;
	virtual void AddNetwork(const FDisplayClusterConfigNetwork& InCfgNetwork)       override final;
	virtual void AddInput(const FDisplayClusterConfigInput& InCfgInput)             override final;
	virtual void AddInputSetup(const FDisplayClusterConfigInputSetup& InCfgInputSetup)  override final;
	virtual void AddCustom(const FDisplayClusterConfigCustom& InCfgCustom)          override final;

private:
	IDisplayClusterConfigParserListener* const ConfigParserListener;
	FString CurrentConfigPath;
};
