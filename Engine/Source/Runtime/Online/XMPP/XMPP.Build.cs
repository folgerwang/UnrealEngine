// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XMPP : ModuleRules
{
	public XMPP(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("XMPP_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] 
			{
				"Runtime/Online/XMPP/Private"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"Json"
			}
		);

		bool TargetPlatformSupportsJingle = false;
		bool TargetPlatformSupportsStrophe = false;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			TargetPlatformSupportsJingle = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4 ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			TargetPlatformSupportsJingle = true;
			TargetPlatformSupportsStrophe = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.XboxOne ||
			Target.Platform == UnrealTargetPlatform.Android ||
			Target.Platform == UnrealTargetPlatform.IOS ||
			Target.Platform == UnrealTargetPlatform.Switch)
		{
			TargetPlatformSupportsStrophe = true;
		}

		if (TargetPlatformSupportsJingle)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WebRTC");
			PrivateDefinitions.Add("WITH_XMPP_JINGLE=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_XMPP_JINGLE=0");
		}

		if (TargetPlatformSupportsStrophe)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libstrophe");
			PrivateDependencyModuleNames.Add("WebSockets");
			PrivateDefinitions.Add("WITH_XMPP_STROPHE=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_XMPP_STROPHE=0");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.PS4)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
		}
	}
}
