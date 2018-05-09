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
					"PropertyEditor",
					"Settings",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
				});

			PrivateIncludePaths.Add("AjaMediaEditor/Private");
		}
	}
}
