// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MagicLeapMediaFactory : ModuleRules
	{
		public MagicLeapMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"MagicLeapMedia",
					"Media"
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MagicLeapMediaFactory/Private"
				});

			if (Target.Platform == UnrealTargetPlatform.Lumin)
			{
				DynamicallyLoadedModuleNames.Add("MagicLeapMedia");
			}
		}
	}
}
