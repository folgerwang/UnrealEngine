// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraCore : ModuleRules
{
    public NiagaraCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Engine",
            }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
				"CoreUObject",
                "VectorVM",
            }
        );
    }
}
