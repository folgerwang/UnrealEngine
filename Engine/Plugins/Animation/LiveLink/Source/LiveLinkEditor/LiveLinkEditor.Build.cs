// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkEditor : ModuleRules
	{
		public LiveLinkEditor(ReadOnlyTargetRules Target) : base(Target)
        {

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "TargetPlatform",
                }
            );



            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "UnrealEd",
                    "Engine",
                    "Projects",
                    "DetailCustomizations",
					"HeadMountedDisplay",
                    "MovieScene",
                    "MovieSceneTools",
                    "MovieSceneTracks",
                    "WorkspaceMenuStructure",
					"SerializedRecorderInterface",
                    "EditorStyle",
                    "Sequencer",
					"SequenceRecorder",
                    "TakesCore",
                    "TakeRecorder",
				    "TakeTrackRecorders",
                    "SlateCore",
                    "Slate",
                    "InputCore",
                    //"Messaging",
                    "LiveLinkInterface",
					"LiveLinkMessageBusFramework",
                    "BlueprintGraph",
					"LiveLink",
                    "AnimGraph",
                    "Persona"
                }
			); 
		}
	}
}
