// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Perforce : ModuleRules
{
	public Perforce(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			string Windows_P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2018.1/";

			string PlatformSubdir = Target.Platform.ToString();
			string VisualStudioVersionFolder = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			string IncludeFolder = Path.Combine(Windows_P4APIPath, "Include", PlatformSubdir, VisualStudioVersionFolder);
			PublicSystemIncludePaths.Add(IncludeFolder);

			string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" :"Release";
			string LibFolder = Path.Combine(Windows_P4APIPath, "Lib", PlatformSubdir, VisualStudioVersionFolder, ConfigPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libclient.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "librpc.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libsupp.lib"));
		}
		else
		{
			string LibFolder = "lib/";
			string LibPrefix = "";
			string LibPostfixAndExt = ".";
			string P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2015.2/";

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2014.1/";
				LibFolder += "mac";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2014.1/" ;
				LibFolder += "linux/" + Target.Architecture;
			}

			LibPrefix = P4APIPath + LibFolder + "/";
			LibPostfixAndExt = ".a";

			PublicSystemIncludePaths.Add(P4APIPath + "include");
			PublicAdditionalLibraries.Add(LibPrefix + "libclient" + LibPostfixAndExt);

			if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.Add(LibPrefix + "libp4sslstub" + LibPostfixAndExt);
			}

			PublicAdditionalLibraries.Add(LibPrefix + "librpc" + LibPostfixAndExt);
			PublicAdditionalLibraries.Add(LibPrefix + "libsupp" + LibPostfixAndExt);
		}
	}
}
