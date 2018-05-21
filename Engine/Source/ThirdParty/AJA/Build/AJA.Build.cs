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

			string AJASDKDir = ModuleDirectory;
			string AJALibPath = AJASDKDir + "/lib/Win64";

			string LibraryName = "AJA";
			bool bHaveDebugLib = false;
			if (bHaveDebugLib && Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				LibraryName = "AJAd";
				PublicDefinitions.Add("AJAMEDIA_DLL_DEBUG=1");
			}
			else
			{
				PublicDefinitions.Add("AJAMEDIA_DLL_DEBUG=0");
			}

			PublicIncludePaths.Add(Path.Combine(AJASDKDir, "include"));
			PublicLibraryPaths.Add(AJALibPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");

			PublicDelayLoadDLLs.Add(LibraryName + ".dll");
			RuntimeDependencies.Add(Path.Combine("$(EngineDir)/Binaries/ThirdParty/AJA/Win64", LibraryName + ".dll"));
		}
		else
		{
			PublicDefinitions.Add("AJAMEDIA_DLL_PLATFORM=0");
			PublicDefinitions.Add("AJAMEDIA_DLL_DEBUG=0");
			System.Console.WriteLine("AJA not supported on this platform");
		}
	}
}



