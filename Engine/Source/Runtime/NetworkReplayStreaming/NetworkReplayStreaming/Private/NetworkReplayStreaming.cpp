// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NetworkReplayStreaming.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/ConsoleManager.h"

IMPLEMENT_MODULE( FNetworkReplayStreaming, NetworkReplayStreaming );

INetworkReplayStreamingFactory& FNetworkReplayStreaming::GetFactory(const TCHAR* FactoryNameOverride)
{
	FString FactoryName = TEXT( "NullNetworkReplayStreaming" );

	if (FactoryNameOverride == nullptr)
	{
		GConfig->GetString( TEXT( "NetworkReplayStreaming" ), TEXT( "DefaultFactoryName" ), FactoryName, GEngineIni );
	}
	else
	{
		FactoryName = FactoryNameOverride;
	}

	FString CmdlineFactoryName;
	if (FParse::Value(FCommandLine::Get(), TEXT("-REPLAYSTREAMER="), CmdlineFactoryName))
	{
		FactoryName = CmdlineFactoryName;
	}

	// See if we need to forcefully fallback to the null streamer
	if ( !FModuleManager::Get().IsModuleLoaded( *FactoryName ) )
	{
		FModuleManager::Get().LoadModule( *FactoryName );
	
		if ( !FModuleManager::Get().IsModuleLoaded( *FactoryName ) )
		{
			FactoryName = TEXT( "NullNetworkReplayStreaming" );
		}
	}

	return FModuleManager::Get().LoadModuleChecked< INetworkReplayStreamingFactory >( *FactoryName );
}

int32 FNetworkReplayStreaming::GetMaxNumberOfAutomaticReplays()
{
	static const int32 DefaultMax = 10;

	int32 MaxAutomaticReplays = DefaultMax;
	GConfig->GetInt(TEXT("NetworkReplayStreaming"), TEXT("MaxNumberAutomaticReplays"), MaxAutomaticReplays, GEngineIni);

	if (!ensureMsgf(MaxAutomaticReplays >= 0, TEXT("INetworkReplayStreamer::GetMaxNumberOfAutomaticReplays: Invalid configured value, using default. %d"), MaxAutomaticReplays))
	{
		MaxAutomaticReplays = DefaultMax;
	}

	return MaxAutomaticReplays;
}

static TAutoConsoleVariable<FString> CVarReplayStreamerAutoDemoPrefix(
	TEXT("demo.ReplayStreamerAutoDemoPrefix"),
	FString(TEXT("demo")),
	TEXT("Prefix to use when generating automatic demo names.")
);

static TAutoConsoleVariable<int32> CVarReplayStreamerAutoDemoUseDateTimePostfix(
	TEXT("demo.ReplayStreamerAutoDemoUseDateTimePostfix"),
	0,
	TEXT("When enabled, uses the current time as a postfix for automatic demo names instead of indices")
);

FString FNetworkReplayStreaming::GetAutomaticReplayPrefix()
{
	return CVarReplayStreamerAutoDemoPrefix.GetValueOnAnyThread();
}

bool FNetworkReplayStreaming::UseDateTimeAsAutomaticReplayPostfix()
{
	return !!CVarReplayStreamerAutoDemoUseDateTimePostfix.GetValueOnAnyThread();
}

const FString FNetworkReplayStreaming::GetAutomaticReplayPrefixExtern() const
{
	return GetAutomaticReplayPrefix();
}

const int32 FNetworkReplayStreaming::GetMaxNumberOfAutomaticReplaysExtern() const
{
	return GetMaxNumberOfAutomaticReplays();
}