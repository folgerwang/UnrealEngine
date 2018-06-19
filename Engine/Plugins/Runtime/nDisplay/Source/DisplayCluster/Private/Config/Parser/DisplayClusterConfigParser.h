// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDisplayClusterConfigParserListener.h"
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
	virtual void AddClusterNode(const FDisplayClusterConfigClusterNode& node) override final;
	virtual void AddScreen(const FDisplayClusterConfigScreen& screen)         override final;
	virtual void AddViewport(const FDisplayClusterConfigViewport& viewport)   override final;
	virtual void AddCamera(const FDisplayClusterConfigCamera& camera)         override final;
	virtual void AddSceneNode(const FDisplayClusterConfigSceneNode& node)     override final;
	virtual void AddGeneral(const FDisplayClusterConfigGeneral& general)      override final;
	virtual void AddRender(const FDisplayClusterConfigRender& render)         override final;
	virtual void AddStereo(const FDisplayClusterConfigStereo& stereo)         override final;
	virtual void AddDebug(const FDisplayClusterConfigDebug& debug)            override final;
	virtual void AddInput(const FDisplayClusterConfigInput& input)            override final;
	virtual void AddCustom(const FDisplayClusterConfigCustom& custom)         override final;

private:
	IDisplayClusterConfigParserListener* const ConfigParserListener;
	FString CurrentConfigPath;
};

