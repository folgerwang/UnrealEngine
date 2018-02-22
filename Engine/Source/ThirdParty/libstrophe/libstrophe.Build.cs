// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libstrophe : ModuleRules
{
	public libstrophe(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string StrophePackagePath = Path.Combine(Target.UEThirdPartySourceDirectory, "libstrophe", "libstrophe-0.9.1");

		bool bIsSupported =
			Target.Platform == UnrealTargetPlatform.XboxOne ||
			Target.Platform == UnrealTargetPlatform.Android ||
			Target.Platform == UnrealTargetPlatform.IOS;

		if (bIsSupported)
		{
			PublicDefinitions.Add("WITH_XMPP_STROPHE=1");
			PublicDefinitions.Add("XML_STATIC");

			PublicSystemIncludePaths.Add(StrophePackagePath);

			string ConfigName = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

			AddEngineThirdPartyPrivateStaticDependencies(Target, "Expat");

			if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{

				// Use reflection to allow type not to exist if console code is not present
				string ToolchainName = "VS";
				System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
				if (XboxOnePlatformType != null)
				{
					System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
					ToolchainName += VersionName.ToString();
				}

				string LibraryPath = Path.Combine(StrophePackagePath, "XboxOne", ToolchainName, ConfigName);
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "strophe.lib"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				string LibraryPath = Path.Combine(StrophePackagePath, "Android", ConfigName);
				PublicLibraryPaths.Add(Path.Combine(LibraryPath, "armv7"));
				PublicLibraryPaths.Add(Path.Combine(LibraryPath, "arm64"));

				PublicAdditionalLibraries.Add("strophe");
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				// add IOS library dir
				PublicLibraryPaths.Add(Path.Combine(StrophePackagePath, "IOS", ConfigName));
				PublicAdditionalLibraries.Add("strophe");
				PublicAdditionalLibraries.Add("resolv");
			}
		}
		else
		{
			PublicDefinitions.Add("WITH_XMPP_STROPHE=0");
		}
    }
}
