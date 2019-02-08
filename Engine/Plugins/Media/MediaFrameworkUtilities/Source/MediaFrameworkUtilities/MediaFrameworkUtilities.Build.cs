// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaFrameworkUtilities : ModuleRules
	{
		public MediaFrameworkUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"OpenCVLensDistortion",
					"TimeManagement",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaAssets",
					"MediaIOCore",
					"MediaUtils",
				});

			if (Target.bBuildEditor == true)
			{
                PrivateDependencyModuleNames.AddRange(
					new string[]
					{
                        "MediaPlayerEditor",
                        "Slate",
						"SlateCore",
						"Settings",
						"UnrealEd"
					});
			}
		}
	}
}