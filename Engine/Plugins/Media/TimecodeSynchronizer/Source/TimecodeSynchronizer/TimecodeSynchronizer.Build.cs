// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TimecodeSynchronizer : ModuleRules
{
    public TimecodeSynchronizer(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "Media",
                "MediaAssets",
                "MediaUtils",
                "TimeManagement",
            });

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                "MediaPlayerEditor",
                "SlateCore",
                });
        }
    }
}
