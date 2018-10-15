// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PreLoadScreen : ModuleRules
{
    public PreLoadScreen(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.Add("Runtime/PreLoadScreen/Public");
        PrivateIncludePaths.Add("Runtime/PreLoadScreen/Private");

        PublicDependencyModuleNames.AddRange(
            new string[] {
                    "Engine",
                    "ApplicationCore"
                }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                    "Core",
                    "InputCore",
                    "RenderCore",
                    "ShaderCore",
                    "CoreUObject",
                    "RHI",
                    "Slate",
                    "SlateCore",
                    "BuildPatchServices",
                    "Projects"
            }
        );


        //Need to make sure Android has Launch module so it can find and process AndroidEventManager events
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Launch"
                }
            );
        }
    }
}
