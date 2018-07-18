// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Expat : ModuleRules
{
	public Expat(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string ExpatPackagePath = Path.Combine(Target.UEThirdPartySourceDirectory, "Expat", "expat-2.2.0");

		if (Target.Platform != UnrealTargetPlatform.XboxOne &&
			Target.Platform != UnrealTargetPlatform.Android &&
			Target.Platform != UnrealTargetPlatform.IOS &&
			Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Win32 &&
			Target.Platform != UnrealTargetPlatform.PS4 &&
			Target.Platform != UnrealTargetPlatform.Mac &&
			Target.Platform != UnrealTargetPlatform.Switch)
		{
			throw new BuildException("Unexpectedly pulled in Expat module. You may need to update Expat.build.cs for platform support");
		}

		string IncludePath = Path.Combine(ExpatPackagePath, "lib");
		PublicSystemIncludePaths.Add(IncludePath);

		string ConfigName = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

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

			string LibraryPath = Path.Combine(ExpatPackagePath, "XboxOne", ToolchainName, ConfigName);
			PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "expat.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string LibraryPath = Path.Combine(ExpatPackagePath, "Android", ConfigName);
			PublicLibraryPaths.Add(Path.Combine(LibraryPath, "armv7"));
			PublicLibraryPaths.Add(Path.Combine(LibraryPath, "arm64"));

			PublicAdditionalLibraries.Add("expat");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ExpatPackagePath, "IOS", ConfigName, "libexpat.a"));
			PublicAdditionalShadowFiles.Add(Path.Combine(ExpatPackagePath, "IOS", ConfigName, "libexpat.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicLibraryPaths.Add(Path.Combine(ExpatPackagePath, Target.Platform.ToString(), "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), "Debug"));
				PublicAdditionalLibraries.Add("expatd.lib");
			}
			else
			{
				PublicLibraryPaths.Add(Path.Combine(ExpatPackagePath, Target.Platform.ToString(), "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), "Release"));
				PublicAdditionalLibraries.Add("expat.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Switch)
		{
			PublicAdditionalLibraries.Add(Path.Combine(ExpatPackagePath, Target.Platform.ToString(), ConfigName, "libexpat.a"));
		}
	}
}
