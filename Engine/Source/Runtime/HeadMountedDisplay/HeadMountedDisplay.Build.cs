// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeadMountedDisplay : ModuleRules
{
    public HeadMountedDisplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/HeadMountedDisplay/Public"
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Renderer/Private"
            }
        );

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "RHI",
                "Renderer",
                "RenderCore",
                "UtilityShaders",
                "Analytics",
                "EngineSettings",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "AugmentedReality"
            }
        );

        if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				});
		}
	}
}
