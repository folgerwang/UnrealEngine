// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibVpx : ModuleRules
{
	public LibVpx(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string RootPath = ModuleDirectory;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = RootPath + "/lib/Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(LibPath);

			string LibFileName = "vpxmd.lib";
			PublicAdditionalLibraries.Add(LibFileName);
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(RootPath + "/lib/Unix/" + Target.Architecture + ((Target.LinkType == TargetLinkType.Monolithic) ? "/libvpx" : "/libvpx_fPIC") + ".a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(RootPath + "/lib/Mac" + ((Target.LinkType == TargetLinkType.Monolithic) ? "/libvpx" : "/libvpx_fPIC") + ".a");
		}

		string IncludePath = RootPath + "/include";
		PublicIncludePaths.Add(IncludePath);
	}
}
