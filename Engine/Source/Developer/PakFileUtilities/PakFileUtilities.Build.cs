// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PakFileUtilities : ModuleRules
{
	public PakFileUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "PakFile", "Json", "Projects", "ApplicationCore" });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Json"
        });
    }
}
