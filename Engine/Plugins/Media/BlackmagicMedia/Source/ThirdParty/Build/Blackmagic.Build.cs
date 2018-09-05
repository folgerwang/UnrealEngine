// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Blackmagic : ModuleRules
{
	public Blackmagic(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_PLATFORM=1");

			string SDKDir = ModuleDirectory;
			string LibPath = Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty/Win64");

			string LibraryName = "BlackmagicLib";

            bool bHaveDebugLib = File.Exists(Path.Combine(LibPath, "BlackmagicLibd.dll"));
            if (bHaveDebugLib && Target.Configuration == UnrealTargetConfiguration.Debug)
            {
                LibraryName = "BlackmagicLibd";
                PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=1");
			}
			else
			{
				PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=0");
			}

			PublicIncludePaths.Add(Path.Combine(SDKDir, "Include"));
			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");

			PublicDelayLoadDLLs.Add(LibraryName + ".dll");
			RuntimeDependencies.Add(Path.Combine(LibPath, LibraryName + ".dll"));
		}
		else
		{
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_PLATFORM=0");
			PublicDefinitions.Add("BLACKMAGICMEDIA_DLL_DEBUG=0");
			System.Console.WriteLine("BLACKMAGIC not supported on this platform");
		}
	}
}
