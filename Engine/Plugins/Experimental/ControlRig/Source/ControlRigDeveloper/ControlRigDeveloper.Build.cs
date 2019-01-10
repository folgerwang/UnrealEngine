// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ControlRigDeveloper : ModuleRules
    {
        public ControlRigDeveloper(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateIncludePaths.Add("ControlRigDeveloper/Private");

            // Copying some these from ControlRig.build.cs, our deps are likely leaner
            // and therefore these could be pruned if needed:
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AnimGraphRuntime",
                    "ControlRig",
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "KismetCompiler",
                    "MovieScene",
                    "MovieSceneTracks",
                    "PropertyPath",
                    "Slate",
                    "SlateCore",
					"TimeManagement"
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

                PrivateIncludePathModuleNames.Add("ControlRigEditor");
                DynamicallyLoadedModuleNames.Add("ControlRigEditor");
            }
        }
    }
}
