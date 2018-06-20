// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class AJA : ModuleRules
{
	public AJA(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("AJAMEDIA_DLL_PLATFORM=1");

			string AjaLibDir = Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty/Win64");

			string LibraryName = "AJA";
			bool bHaveDebugLib = File.Exists(Path.Combine(AjaLibDir, "AJAd.dll"));
			if (bHaveDebugLib && Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				LibraryName = "AJAd";
				PublicDefinitions.Add("AJAMEDIA_DLL_DEBUG=1");
			}
			else
			{
				PublicDefinitions.Add("AJAMEDIA_DLL_DEBUG=0");
			}

			PublicIncludePaths.Add(Path.Combine(Path.Combine(ModuleDirectory, "include")));
			PublicLibraryPaths.Add(AjaLibDir);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");

			PublicDelayLoadDLLs.Add(LibraryName + ".dll");
			RuntimeDependencies.Add(Path.Combine(AjaLibDir, LibraryName + ".dll"));
		}
		else
		{
			PublicDefinitions.Add("AJAMEDIA_DLL_PLATFORM=0");
			PublicDefinitions.Add("AJAMEDIA_DLL_DEBUG=0");
			System.Console.WriteLine("AJA not supported on this platform");
		}
	}
}



