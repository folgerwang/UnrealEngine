// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraShader : ModuleRules
{
    public NiagaraShader(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Engine",
                "CoreUObject",
                "NiagaraCore"
            }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "RenderCore",
                "VectorVM",
                "RHI",
                "NiagaraCore"
            }
        );

        if (Target.bBuildEditor == true)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                "DerivedDataCache",
                "TargetPlatform",
            });

            PrivateDependencyModuleNames.AddRange(
                new string[] {
            });
        }

        PublicIncludePathModuleNames.AddRange(
            new string[] {
            });

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Niagara",
            });
    }
}
