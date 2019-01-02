// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorLayerUtilitiesEditor : ModuleRules
{
	public ActorLayerUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ActorLayerUtilities",
				"Core",
				"CoreUObject",
				"EditorWidgets",
				"EditorStyle",
				"Engine",
				"Layers",
				"LevelEditor",
				"PropertyEditor",
				"SlateCore",
				"Slate",
				"UnrealEd",
			}
		);
	}
}
