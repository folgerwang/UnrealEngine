// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorSubsystem : ModuleRules
{
    public EditorSubsystem(ReadOnlyTargetRules Target)
         : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
            });
    }
}
