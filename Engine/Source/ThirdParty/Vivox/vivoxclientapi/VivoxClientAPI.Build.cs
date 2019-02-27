// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class VivoxClientAPI : ModuleRules
{
	public VivoxClientAPI(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDependencyModuleNames.Add("VivoxCoreSDK");

		string VivoxClientAPIPath = ModuleDirectory;
		string PlatformSubdir = Target.Platform.ToString();

		bool bUseDebugBuild = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);
		string ConfigurationSubdir = bUseDebugBuild ? "Debug" : "Release";

		PublicIncludePaths.Add(Path.Combine(VivoxClientAPIPath, "vivoxclientapi", "include"));

		if (Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(Path.Combine(VivoxClientAPIPath, PlatformSubdir, "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), ConfigurationSubdir));
			PublicAdditionalLibraries.Add("vivoxclientapi.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4
			|| Target.Platform == UnrealTargetPlatform.Switch)
		{
			PublicLibraryPaths.Add(Path.Combine(VivoxClientAPIPath, PlatformSubdir, ConfigurationSubdir));
			PublicAdditionalLibraries.Add("vivoxclientapi");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac
			|| Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add(Path.Combine(VivoxClientAPIPath, PlatformSubdir, ConfigurationSubdir, "libvivoxclientapi.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			string ToolchainName = "VS";
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
				ToolchainName += VersionName.ToString();
			}

			PublicAdditionalLibraries.Add(Path.Combine(VivoxClientAPIPath, PlatformSubdir, ToolchainName, ConfigurationSubdir, "vivoxclientapi.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(Path.Combine(VivoxClientAPIPath, Target.Platform.ToString(), ConfigurationSubdir, "ARMv7"));
			PublicLibraryPaths.Add(Path.Combine(VivoxClientAPIPath, Target.Platform.ToString(), ConfigurationSubdir, "ARM64"));
			PublicAdditionalLibraries.Add("vivoxclientapi");
		}
	}
}
