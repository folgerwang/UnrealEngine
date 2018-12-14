// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosCloth : ModuleRules
{
    public ChaosCloth(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Chaos/Private");
        PublicIncludePaths.Add(ModuleDirectory + "/Public");

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "ClothingSystemRuntime",
                "ClothingSystemRuntimeInterface",
                "Engine",
                "Chaos"
            }
		);

        SetupModulePhysicsSupport(Target);
    }
}
