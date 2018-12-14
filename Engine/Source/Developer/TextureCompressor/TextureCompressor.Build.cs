// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureCompressor : ModuleRules
{
	public TextureCompressor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
				"TargetPlatform",
				"ImageCore"
			}
			);

		if (Target.bBuildDeveloperTools)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTextureTools");
		}
	}
}
