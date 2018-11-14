// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaIOCore : ModuleRules
	{
		public MediaIOCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaAssets",
					"MediaUtils",
					"MovieSceneCapture",
					"Projects",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
					"TimeManagement"
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}
