// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SDL2 : ModuleRules
{
	public SDL2(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SDL2Path = Target.UEThirdPartySourceDirectory + "SDL2/SDL-gui-backend/";
		string SDL2LibPath = SDL2Path + "lib/";

		// assume SDL to be built with extensions
		PublicDefinitions.Add("SDL_WITH_EPIC_EXTENSIONS=1");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicIncludePaths.Add(SDL2Path + "include");
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				// Debug version should be built with -fPIC and usable in all targets
				PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2_fPIC_Debug.a");
			}
			else if (Target.LinkType == TargetLinkType.Monolithic)
			{
				PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(SDL2LibPath + "Linux/" + Target.Architecture + "/libSDL2_fPIC.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			string OptimizationSuffix = "";
			if (Target.bCompileForSize)
			{
				OptimizationSuffix = "_Oz";
			}
			else
			{
				if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					OptimizationSuffix = "_O2";
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					OptimizationSuffix = "_O3";
				}
			}
			PublicIncludePaths.Add(SDL2Path + "include");
			SDL2LibPath += "HTML5/";
			PublicAdditionalLibraries.Add(SDL2LibPath + "libSDL2" + OptimizationSuffix + ".bc");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(SDL2Path + "include");

			SDL2LibPath += "Win64/";

			PublicAdditionalLibraries.Add(SDL2LibPath + "SDL2.lib");

			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/SDL2/Win64/SDL2.dll");
			PublicDelayLoadDLLs.Add("SDL2.dll");
		}

	}
}
