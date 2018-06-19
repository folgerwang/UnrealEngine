// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigParser.h"


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
void FDisplayClusterConfigParser::AddClusterNode(const FDisplayClusterConfigClusterNode& node)
{
	ConfigParserListener->AddClusterNode(node);
}

void FDisplayClusterConfigParser::AddScreen(const FDisplayClusterConfigScreen& screen)
{
	ConfigParserListener->AddScreen(screen);
}

void FDisplayClusterConfigParser::AddViewport(const FDisplayClusterConfigViewport& viewport)
{
	ConfigParserListener->AddViewport(viewport);
}

void FDisplayClusterConfigParser::AddCamera(const FDisplayClusterConfigCamera& camera)
{
	ConfigParserListener->AddCamera(camera);
}

void FDisplayClusterConfigParser::AddSceneNode(const FDisplayClusterConfigSceneNode& node)
{
	ConfigParserListener->AddSceneNode(node);
}

void FDisplayClusterConfigParser::AddGeneral(const FDisplayClusterConfigGeneral& general)
{
	ConfigParserListener->AddGeneral(general);
}

void FDisplayClusterConfigParser::AddRender(const FDisplayClusterConfigRender& render)
{
	ConfigParserListener->AddRender(render);
}

void FDisplayClusterConfigParser::AddStereo(const FDisplayClusterConfigStereo& stereo)
{
	ConfigParserListener->AddStereo(stereo);
}

void FDisplayClusterConfigParser::AddDebug(const FDisplayClusterConfigDebug& debug)
{
	ConfigParserListener->AddDebug(debug);
}

void FDisplayClusterConfigParser::AddInput(const FDisplayClusterConfigInput& input)
{
	ConfigParserListener->AddInput(input);
}

void FDisplayClusterConfigParser::AddCustom(const FDisplayClusterConfigCustom& custom)
{
	ConfigParserListener->AddCustom(custom);
}
