// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ControlRig : ModuleRules
    {
        public ControlRig(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("ControlRig/Private");
            PrivateIncludePaths.Add("ControlRig/Private/Sequencer");
            PrivateIncludePaths.Add("ControlRig/Private/Units");

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "AnimGraphRuntime",
                    "MovieScene",
                    "MovieSceneTracks",
                    "PropertyPath",
					"TimeManagement"
                }
            );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "AnimationCore",
                    "LevelSequence",
                }
            );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[]
                    {
                        "UnrealEd",
                        "BlueprintGraph",
                        "PropertyEditor",
                    }
                );
            }
        }
    }
}
