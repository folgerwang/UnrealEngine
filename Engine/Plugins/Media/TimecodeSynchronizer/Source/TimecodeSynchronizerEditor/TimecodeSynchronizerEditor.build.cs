// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimecodeSynchronizerEditor : ModuleRules
{
	public TimecodeSynchronizerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("TimecodeSynchronizerEditor/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Core",
				"CoreUObject",
				"Engine",
				"EditorStyle",
				"InputCore",
                "MediaAssets",
                "MediaPlayerEditor",
                "PropertyEditor",
                "SlateCore",
				"Slate",
                "TimecodeSynchronizer",
                "UnrealEd",
			});
	}
}
