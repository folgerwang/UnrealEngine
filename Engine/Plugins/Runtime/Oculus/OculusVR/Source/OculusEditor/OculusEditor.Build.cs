// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OculusEditor : ModuleRules
{
	public OculusEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Projects",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"Core",
				"OculusHMD",
				"OVRPlugin",
			}
			);

		PrivateIncludePaths.AddRange(
				new string[] {
					// Relative to Engine\Plugins\Runtime\Oculus\OculusVR\Source
					"OculusEditor/Private",
					"OculusHMD/Private",
				});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
            }
            );
	}
}
