// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Apeiron : ModuleRules
{
    public Apeiron(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePaths.Add("Runtime/Experimental/Apeiron/Public");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "CoreUObject"
            }
		);

        PublicDefinitions.Add("COMPILE_WITHOUT_UNREAL_SUPPORT=0");
	}
}
