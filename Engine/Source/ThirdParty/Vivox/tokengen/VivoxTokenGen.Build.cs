// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class VivoxTokenGen : ModuleRules
{
	public VivoxTokenGen(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		//string VivoxTokenGenPath = Path.Combine(Target.UEThirdPartySourceDirectory, "Vivox", "tokengen");
		string PlatformSubdir = Target.Platform.ToString();

		//bool bUseDebugBuild = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);
		//string ConfigurationSubdir = bUseDebugBuild ? "Debug" : "Release";
		if (Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.Win32)
		{
			PlatformSubdir = Path.Combine(PlatformSubdir, "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), PlatformSubdir);
			PublicAdditionalLibraries.Add("tokengen.lib");
			PrivateDependencyModuleNames.Add("OpenSSL");
		}
	}
}
