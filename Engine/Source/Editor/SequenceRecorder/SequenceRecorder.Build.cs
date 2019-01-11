// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SequenceRecorder : ModuleRules
	{
		public SequenceRecorder(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/SequenceRecorder/Private",
					"Editor/SequenceRecorder/Private/Sections",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"TimeManagement",
                    "SerializedRecorderInterface",

                }
            );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"SlateCore",
					"Slate",
					"InputCore",
					"Engine",
					"UnrealEd",
					"EditorStyle",
					"Projects",
					"LevelEditor",
					"Persona",
					"WorkspaceMenuStructure",
					"PropertyEditor",
					"MovieScene",
					"MovieSceneTracks",
					"LevelSequence",
					"NetworkReplayStreaming",
					"AssetRegistry",
					"CinematicCamera",
                    "EditorWidgets",
                    "Kismet",
                    "LiveLinkInterface",
                    "SerializedRecorderInterface",
                }
                );

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				// Add __WINDOWS_WASAPI__ so that RtAudio compiles with WASAPI
				PublicDefinitions.Add("__WINDOWS_WASAPI__");
			}
		}
	}
}
