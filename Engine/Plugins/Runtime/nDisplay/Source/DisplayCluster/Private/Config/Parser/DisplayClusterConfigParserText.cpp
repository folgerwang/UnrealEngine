// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Config/Parser/DisplayClusterConfigParserText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterStrings.h"


FDisplayClusterConfigParserText::FDisplayClusterConfigParserText(IDisplayClusterConfigParserListener* pListener) :
	FDisplayClusterConfigParser(pListener)
{
}

bool FDisplayClusterConfigParserText::ParseFile(const FString& path)
{
	// Prepare path
	FString cfgPath(path);
	FPaths::NormalizeFilename(cfgPath);

	// Load data
	UE_LOG(LogDisplayClusterConfig, Log, TEXT("Parsing config file %s"), *cfgPath);
	if (FPaths::FileExists(cfgPath))
	{
		TArray<FString> data;
		if (FFileHelper::LoadANSITextFileToStrings(*cfgPath, nullptr, data) == true)
		{
			// Parse each line from config
			for (auto line : data)
			{
				line.TrimStartAndEndInline();
				ParseLine(line);
			}

			// Parsed, complete on base
			return FDisplayClusterConfigParser::ParseFile(path);
		}
	}

	// An error occurred
	return false;
}

void FDisplayClusterConfigParserText::ParseLine(const FString& line)
{
	if (line.IsEmpty() || line.StartsWith(FString(DisplayClusterStrings::cfg::spec::Comment)))
	{
		// Skip this line
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::info::Header)))
	{
		AddInfo(impl_parse<FDisplayClusterConfigInfo>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::cluster::Header)))
	{
		AddClusterNode(impl_parse<FDisplayClusterConfigClusterNode>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::window::Header)))
	{
		AddWindow(impl_parse<FDisplayClusterConfigWindow>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::screen::Header)))
	{
		AddScreen(impl_parse<FDisplayClusterConfigScreen>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::viewport::Header)))
	{
		AddViewport(impl_parse<FDisplayClusterConfigViewport>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::camera::Header)))
	{
		AddCamera(impl_parse<FDisplayClusterConfigCamera>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::scene::Header)))
	{
		AddSceneNode(impl_parse<FDisplayClusterConfigSceneNode>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::general::Header)))
	{
		AddGeneral(impl_parse<FDisplayClusterConfigGeneral>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::render::Header)))
	{
		AddRender(impl_parse<FDisplayClusterConfigRender>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::stereo::Header)))
	{
		AddStereo(impl_parse<FDisplayClusterConfigStereo>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::network::Header)))
	{
		AddNetwork(impl_parse<FDisplayClusterConfigNetwork>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::debug::Header)))
	{
		AddDebug(impl_parse<FDisplayClusterConfigDebug>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::input::Header)))
	{
		AddInput(impl_parse<FDisplayClusterConfigInput>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::inputsetup::Header)))
	{
		AddInputSetup(impl_parse<FDisplayClusterConfigInputSetup>(line));
	}
	else if (line.StartsWith(FString(DisplayClusterStrings::cfg::data::custom::Header)))
	{
		AddCustom(impl_parse<FDisplayClusterConfigCustom>(line));
	}
	else
	{
		UE_LOG(LogDisplayClusterConfig, Warning, TEXT("Unknown config token [%s]"), *line);
	}
}
