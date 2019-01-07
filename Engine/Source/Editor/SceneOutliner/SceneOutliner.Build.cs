// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SceneOutliner : ModuleRules
{
	public SceneOutliner(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject",
				"Engine",
                "ApplicationCore",
                "InputCore",
				"Slate", 
				"SlateCore",
				"EditorStyle",
				"UnrealEd",
            }
		);
	}
}
