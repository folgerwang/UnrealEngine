// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Config/DisplayClusterConfigTypes.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterStrings.h"
#include "Misc/DisplayClusterTypesConverter.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigInfo
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigInfo::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::info::Version, *Version);
}

bool FDisplayClusterConfigInfo::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::info::Version), Version);

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigClusterNode
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigClusterNode::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%s, %s=%s, %s=%s, %s=%d, %s=%d, %s=%d, %s=%s, %s=%s]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::Id,               *Id,
		DisplayClusterStrings::cfg::data::cluster::Window,  *WindowId,
		DisplayClusterStrings::cfg::data::cluster::Addr,    *Addr,
		DisplayClusterStrings::cfg::data::cluster::Master,  DisplayClusterHelpers::str::BoolToStr(IsMaster),
		DisplayClusterStrings::cfg::data::cluster::PortCS,  Port_CS,
		DisplayClusterStrings::cfg::data::cluster::PortSS,  Port_SS,
		DisplayClusterStrings::cfg::data::cluster::PortCE,  Port_CE,
		DisplayClusterStrings::cfg::data::cluster::Sound,   DisplayClusterHelpers::str::BoolToStr(SoundEnabled),
		DisplayClusterStrings::cfg::data::cluster::EyeSwap, DisplayClusterHelpers::str::BoolToStr(EyeSwap)
	);
}

bool FDisplayClusterConfigClusterNode::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Id),                Id);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::Window),   WindowId);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::Addr),     Addr);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::Master),   IsMaster);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::PortCS),   Port_CS);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::PortSS),   Port_SS);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::PortCE),   Port_CE);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::Sound),    SoundEnabled);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::cluster::EyeSwap),  EyeSwap);

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigWindow
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigWindow::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%s, %s=%s, %s=%d, %s=%d, %s=%d, %s=%d]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::Id,                 *Id,
		DisplayClusterStrings::cfg::data::window::Viewports,  *DisplayClusterHelpers::str::ArrayToStr(ViewportIds),
		DisplayClusterStrings::cfg::data::window::Fullscreen, DisplayClusterHelpers::str::BoolToStr(IsFullscreen),
		DisplayClusterStrings::cfg::data::window::WinX,       WinX,
		DisplayClusterStrings::cfg::data::window::WinY,       WinY,
		DisplayClusterStrings::cfg::data::window::ResX,       ResX,
		DisplayClusterStrings::cfg::data::window::ResY,       ResY
	);
}

bool FDisplayClusterConfigWindow::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Id),                 Id);
	DisplayClusterHelpers::str::ExtractCommandLineArray(line, FString(DisplayClusterStrings::cfg::data::window::Viewports),  ViewportIds);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::window::Fullscreen), IsFullscreen);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::window::WinX),       WinX);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::window::WinY),       WinY);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::window::ResX),       ResX);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::window::ResY),       ResY);

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigViewport
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigViewport::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%s, %s=%d, %s=%d, %s=%d, %s=%d]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::Id,               *Id,
		DisplayClusterStrings::cfg::data::viewport::Screen, *ScreenId,
		DisplayClusterStrings::cfg::data::viewport::PosX,   Loc.X,
		DisplayClusterStrings::cfg::data::viewport::PosY,   Loc.Y,
		DisplayClusterStrings::cfg::data::viewport::Width,  Size.X,
		DisplayClusterStrings::cfg::data::viewport::Height, Size.Y
	);
}

bool FDisplayClusterConfigViewport::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Id),               Id);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::viewport::Screen), ScreenId);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::viewport::PosX),   Loc.X);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::viewport::PosY),   Loc.Y);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::viewport::Width),  Size.X);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::viewport::Height), Size.Y);

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigSceneNode
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigSceneNode::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%s, %s=%s, %s=%s, %s=%s, %s=%d]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::Id, *Id,
		DisplayClusterStrings::cfg::data::ParentId, *ParentId,
		DisplayClusterStrings::cfg::data::Loc, *Loc.ToString(),
		DisplayClusterStrings::cfg::data::Rot, *Rot.ToString(),
		DisplayClusterStrings::cfg::data::scene::TrackerId, *TrackerId,
		DisplayClusterStrings::cfg::data::scene::TrackerCh, TrackerCh);
}

bool FDisplayClusterConfigSceneNode::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Id),               Id);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::ParentId),         ParentId);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Loc),              Loc);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Rot),              Rot);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::scene::TrackerId), TrackerId);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::scene::TrackerCh), TrackerCh);
	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigScreen
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigScreen::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s]"),
		*FDisplayClusterConfigSceneNode::ToString(),
		DisplayClusterStrings::cfg::data::screen::Size, *Size.ToString());
}

bool FDisplayClusterConfigScreen::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::screen::Size), Size);
	return FDisplayClusterConfigSceneNode::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigCamera
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigCamera::ToString() const
{
	return FString::Printf(TEXT("[%s + ]"),
		*FDisplayClusterConfigSceneNode::ToString());
}

bool FDisplayClusterConfigCamera::DeserializeFromString(const FString& line)
{
	return FDisplayClusterConfigSceneNode::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigInput
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigInput::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%s, %s={%s}]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::Id, *Id,
		DisplayClusterStrings::cfg::data::input::Type, *Type,
		TEXT("params"), *Params);
}

bool FDisplayClusterConfigInput::DeserializeFromString(const FString& line)
{
	// Save full string to allow an input device to parse (polymorphic)
	Params = line;
	FString mapping;

	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Id), Id);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::input::Type), Type);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::input::Remap), mapping);

	DisplayClusterHelpers::str::DustCommandLineValue(mapping);

	TArray<FString> pairs;
	FString pair;
	while (mapping.Split(FString(","), &pair, &mapping, ESearchCase::IgnoreCase, ESearchDir::FromStart))
	{
		pairs.Add(pair);
	}

	pairs.Add(mapping);

	for (const auto& item : pairs)
	{
		FString strL, strR;

		if (item.Split(FString(":"), &strL, &strR, ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			const int32 l = FDisplayClusterTypesConverter::FromString<int32>(strL);
			const int32 r = FDisplayClusterTypesConverter::FromString<int32>(strR);

			if (l != r)
			{
				ChMap.Add(l, r);
			}
		}
	}

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}

FString FDisplayClusterConfigInputSetup::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%d, %s=%s, %s=%s]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::Id, *Id,
		DisplayClusterStrings::cfg::data::inputsetup::Channel, Channel,
		DisplayClusterStrings::cfg::data::inputsetup::Key, *Key,
		DisplayClusterStrings::cfg::data::inputsetup::Bind, *BindName);
}

bool FDisplayClusterConfigInputSetup::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::Id), Id);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::inputsetup::Channel), Channel);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::inputsetup::Key),     Key);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::inputsetup::Bind),    BindName);

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigGeneral
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigGeneral::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%d]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::general::SwapSyncPolicy, SwapSyncPolicy);
}

bool FDisplayClusterConfigGeneral::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::general::SwapSyncPolicy), SwapSyncPolicy);
	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigRender
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigRender::ToString() const
{
	return FString::Printf(TEXT("%s + "),
		*FDisplayClusterConfigBase::ToString());
}

bool FDisplayClusterConfigRender::DeserializeFromString(const FString& line)
{
	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigStereo
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigStereo::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%f]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::stereo::EyeDist, EyeDist);
}

bool FDisplayClusterConfigStereo::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::stereo::EyeDist), EyeDist);
	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigNetwork
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigNetwork::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%d, %s=%d, %s=%d, %s=%d]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::network::ClientConnectTriesAmount, ClientConnectTriesAmount,
		DisplayClusterStrings::cfg::data::network::ClientConnectRetryDelay, ClientConnectRetryDelay,
		DisplayClusterStrings::cfg::data::network::BarrierGameStartWaitTimeout, BarrierGameStartWaitTimeout,
		DisplayClusterStrings::cfg::data::network::BarrierWaitTimeout, BarrierWaitTimeout);
}

bool FDisplayClusterConfigNetwork::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::network::ClientConnectTriesAmount), ClientConnectTriesAmount);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::network::ClientConnectRetryDelay), ClientConnectRetryDelay);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::network::BarrierGameStartWaitTimeout), BarrierGameStartWaitTimeout);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::network::BarrierWaitTimeout), BarrierWaitTimeout);

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigDebug
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigDebug::ToString() const
{
	return FString::Printf(TEXT("[%s + %s=%s, %s=%s, %s=%f]"),
		*FDisplayClusterConfigBase::ToString(),
		DisplayClusterStrings::cfg::data::debug::DrawStats, DisplayClusterHelpers::str::BoolToStr(DrawStats),
		DisplayClusterStrings::cfg::data::debug::LagSim,  DisplayClusterHelpers::str::BoolToStr(LagSimulateEnabled),
		DisplayClusterStrings::cfg::data::debug::LagTime, LagMaxTime);
}

bool FDisplayClusterConfigDebug::DeserializeFromString(const FString& line)
{
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::debug::DrawStats), DrawStats);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::debug::LagSim),    LagSimulateEnabled);
	DisplayClusterHelpers::str::ExtractCommandLineValue(line, FString(DisplayClusterStrings::cfg::data::debug::LagTime),   LagMaxTime);
	return FDisplayClusterConfigBase::DeserializeFromString(line);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterConfigCustom
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterConfigCustom::ToString() const
{
	FString str = FDisplayClusterConfigBase::ToString() +  FString( + "[");
	int i = 0;

	for (auto it = Args.CreateConstIterator(); it; ++it)
	{
		str += FString::Printf(TEXT("\nCustom argument %d: %s=%s\n"), i++, *it->Key, *it->Value);
	}

	str += FString("]");

	return str;
}

bool FDisplayClusterConfigCustom::DeserializeFromString(const FString& line)
{
	// Non-typical way of specifying custom arguments (we don't know
	// the argument names) forces us to perform individual parsing approach.
	FString tmpLine = line;

	// Prepare string before parsing
	tmpLine.RemoveFromStart(DisplayClusterStrings::cfg::data::custom::Header);
	tmpLine.TrimStartAndEndInline();

	// Break into argument-value pairs
	TArray<FString> pairs;
	tmpLine.ParseIntoArray(pairs, TEXT(" "));

	// Fill data from pairs
	for (auto pair : pairs)
	{
		FString key, val;
		if (pair.Split(FString(DisplayClusterStrings::strKeyValSeparator), &key, &val))
		{
			if (key.Len() > 0 && val.Len() > 0)
			{
				Args.Add(key, val);
			}
		}
	}

	return FDisplayClusterConfigBase::DeserializeFromString(line);
}
