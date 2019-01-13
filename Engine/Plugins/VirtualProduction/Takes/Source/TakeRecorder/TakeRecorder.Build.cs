// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakeRecorder : ModuleRules
{
	public TakeRecorder(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"EditorStyle",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"LevelEditor",
				"LevelSequence",
				"LevelSequenceEditor",
				"MovieScene",
				"PropertyEditor",
				"TakesCore",
				"TimeManagement",
				"Settings",
				"Slate",
				"SlateCore",
				"UMG",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"Analytics",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"UMG",
                "TakeTrackRecorders",
                "SerializedRecorderInterface",
                "Sequencer",
            }
        );

		PrivateIncludePaths.AddRange(
			new string[] {
				"TakeRecorder/Private",
				"TakeRecorder/Public",
                "TakeRecorderSources/Private",
            }
        );
    }
}
