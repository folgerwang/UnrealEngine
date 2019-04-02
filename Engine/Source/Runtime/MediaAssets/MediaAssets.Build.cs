// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaAssets : ModuleRules
	{
		public MediaAssets(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"AudioMixer",
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaUtils",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RenderCore",
					"RHI",
					"UtilityShaders",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/MediaAssets/Private",
					"Runtime/MediaAssets/Private/Assets",
					"Runtime/MediaAssets/Private/Misc",
				});

			if (Target.bBuildEditor)
			{
				PrivateIncludePathModuleNames.Add("TargetPlatform");
			}
		}
	}
}
