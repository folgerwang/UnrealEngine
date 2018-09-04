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
					"EditorStyle",
					"MediaAssets",
					"MediaPlayerEditor",
					"Projects",
					"PropertyEditor",
					"Settings",
					"Slate",
					"SlateCore",
					"TimeManagement",
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
