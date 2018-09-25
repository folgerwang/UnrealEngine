// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
		
public class WebSockets : ModuleRules
{
  public WebSockets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("WEBSOCKETS_PACKAGE=1");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"HTTP"
			}
		);

		bool bPlatformSupportsLibWebsockets =
				Target.Platform == UnrealTargetPlatform.Win32 ||
				Target.Platform == UnrealTargetPlatform.Win64 ||
				Target.Platform == UnrealTargetPlatform.Android ||
				Target.Platform == UnrealTargetPlatform.Mac ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
				Target.Platform == UnrealTargetPlatform.IOS ||
				Target.Platform == UnrealTargetPlatform.PS4 ||
				Target.Platform == UnrealTargetPlatform.Switch;

		bool bUsePlatformSSL = Target.Platform == UnrealTargetPlatform.Switch;

		bool bPlatformSupportsXboxWebsockets = Target.Platform == UnrealTargetPlatform.XboxOne;

		bool bShouldUseModule = 
				bPlatformSupportsLibWebsockets || 
				bPlatformSupportsXboxWebsockets;

		if (bShouldUseModule)
		{
			PublicDefinitions.Add("WITH_WEBSOCKETS=1");

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Online/WebSockets/Private",
				}
			);

			if (bPlatformSupportsLibWebsockets)
			{
				PublicDefinitions.Add("WITH_LIBWEBSOCKETS=1");

				if (bUsePlatformSSL)
				{
					PrivateDefinitions.Add("WITH_SSL=0");
					AddEngineThirdPartyPrivateStaticDependencies(Target, "libWebSockets");
				}
				else
				{
					AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "libWebSockets", "zlib");
					PrivateDependencyModuleNames.Add("SSL");
				}
			}
		}
		else
		{
			PublicDefinitions.Add("WITH_WEBSOCKETS=0");
		}
	}
}
