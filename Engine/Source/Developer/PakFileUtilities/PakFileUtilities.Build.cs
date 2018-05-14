// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PakFileUtilities : ModuleRules
{
	public PakFileUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "PakFile", "Json", "Projects", "ApplicationCore" });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "Json"
        });

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry"
        });
    }
}
