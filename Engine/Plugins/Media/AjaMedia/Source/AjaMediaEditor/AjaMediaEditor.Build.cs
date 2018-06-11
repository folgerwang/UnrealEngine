// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AjaMediaEditor : ModuleRules
	{
		public AjaMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AjaMedia",
					"AjaMediaOutput",
					"Core",
					"CoreUObject",
					"MediaAssets",
					"Projects",
					"PropertyEditor",
					"Settings",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AJA",
					"AssetTools",
				});

			PrivateIncludePaths.Add("AjaMediaEditor/Private");
		}
	}
}
