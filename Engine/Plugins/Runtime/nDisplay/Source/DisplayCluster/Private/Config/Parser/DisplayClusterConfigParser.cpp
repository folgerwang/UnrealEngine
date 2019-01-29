// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Config/Parser/DisplayClusterConfigParser.h"


FDisplayClusterConfigParser::FDisplayClusterConfigParser(IDisplayClusterConfigParserListener* pListener) :
	ConfigParserListener(pListener),
	CurrentConfigPath()
{
}

FDisplayClusterConfigParser::~FDisplayClusterConfigParser()
{
}


bool FDisplayClusterConfigParser::ParseFile(const FString& path)
{
	CurrentConfigPath = path;
	return !CurrentConfigPath.IsEmpty();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterConfigParserListener
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterConfigParser::AddInfo(const FDisplayClusterConfigInfo& InCfgInfo)
{
	ConfigParserListener->AddInfo(InCfgInfo);
}

void FDisplayClusterConfigParser::AddClusterNode(const FDisplayClusterConfigClusterNode& InCfgCNode)
{
	ConfigParserListener->AddClusterNode(InCfgCNode);
}

void FDisplayClusterConfigParser::AddWindow(const FDisplayClusterConfigWindow& InCfgWindow)
{
	ConfigParserListener->AddWindow(InCfgWindow);
}

void FDisplayClusterConfigParser::AddScreen(const FDisplayClusterConfigScreen& InCfgScreen)
{
	ConfigParserListener->AddScreen(InCfgScreen);
}

void FDisplayClusterConfigParser::AddViewport(const FDisplayClusterConfigViewport& InCfgViewport)
{
	ConfigParserListener->AddViewport(InCfgViewport);
}

void FDisplayClusterConfigParser::AddCamera(const FDisplayClusterConfigCamera& InCfgCamera)
{
	ConfigParserListener->AddCamera(InCfgCamera);
}

void FDisplayClusterConfigParser::AddSceneNode(const FDisplayClusterConfigSceneNode& InCfgSNode)
{
	ConfigParserListener->AddSceneNode(InCfgSNode);
}

void FDisplayClusterConfigParser::AddGeneral(const FDisplayClusterConfigGeneral& InCfgGeneral)
{
	ConfigParserListener->AddGeneral(InCfgGeneral);
}

void FDisplayClusterConfigParser::AddRender(const FDisplayClusterConfigRender& InCfgRender)
{
	ConfigParserListener->AddRender(InCfgRender);
}

void FDisplayClusterConfigParser::AddNetwork(const FDisplayClusterConfigNetwork& InCfgNetwork)
{
	ConfigParserListener->AddNetwork(InCfgNetwork);
}

void FDisplayClusterConfigParser::AddStereo(const FDisplayClusterConfigStereo& InCfgStereo)
{
	ConfigParserListener->AddStereo(InCfgStereo);
}

void FDisplayClusterConfigParser::AddDebug(const FDisplayClusterConfigDebug& InCfgDebug)
{
	ConfigParserListener->AddDebug(InCfgDebug);
}

void FDisplayClusterConfigParser::AddInput(const FDisplayClusterConfigInput& InCfgInput)
{
	ConfigParserListener->AddInput(InCfgInput);
}

void FDisplayClusterConfigParser::AddInputSetup(const FDisplayClusterConfigInputSetup& InCfgInputSetup)
{
	ConfigParserListener->AddInputSetup(InCfgInputSetup);
}

void FDisplayClusterConfigParser::AddCustom(const FDisplayClusterConfigCustom& InCfgCustom)
{
	ConfigParserListener->AddCustom(InCfgCustom);
}
