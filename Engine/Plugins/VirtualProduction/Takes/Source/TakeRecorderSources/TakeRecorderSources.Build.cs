// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TakeRecorderSources : ModuleRules
{
	public TakeRecorderSources(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "CinematicCamera",
                "Core",
				"CoreUObject",
				"Engine",
				"LevelSequence",
                "LevelSequenceEditor",
                "MovieScene",
				"MovieSceneTracks",
                "SceneOutliner",
				"SequenceRecorder", // For ISequenceAudioRecorder
				"SerializedRecorderInterface",
                "LevelSequence",
                "Slate",
				"SlateCore",
				"TakesCore",
				"TakeRecorder",
				"UnrealEd",
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "TakeTrackRecorders",
            }
        );

		PrivateIncludePaths.AddRange(
			new string[] {
				"TakeRecorderSources/Private",
			}
		);
	}
}
