// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibWebM : ModuleRules
{
	public LibWebM(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string RootPath = ModuleDirectory;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = RootPath + "/lib/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(LibPath);

			string LibFileName = "libwebm.lib";
			PublicAdditionalLibraries.Add(LibFileName);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(RootPath + "/lib/Unix/" + Target.Architecture + ((Target.LinkType == TargetLinkType.Monolithic) ? "/libwebm" : "/libwebm_fPIC") + ".a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(RootPath + "/lib/Mac" + ((Target.LinkType == TargetLinkType.Monolithic) ? "/libwebm" : "/libwebm_fPIC") + ".a");
		}

		string IncludePath = RootPath + "/include";
		PublicIncludePaths.Add(IncludePath);
	}
}
